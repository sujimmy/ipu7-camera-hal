/*
 * Copyright (C) 2022-2024 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG PSysDevice

#include "PSysDevice.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "CameraLog.h"
#include "Errors.h"
#include "Utils.h"

namespace icamera {

const char* DRIVER_NAME = "/dev/ipu7-psys0";

PSysDevice::PSysDevice(int cameraId)
        : mPollThread(nullptr),
          mCameraId(cameraId),
          mFd(-1),
          mGraphId(INVALID_GRAPH_ID) {
    LOG1("<%id> Construct PSysDevice", mCameraId);

    CLEAR(mFrameId);
    memset(&mFrameIdToSeqMap, -1, sizeof(mFrameIdToSeqMap));
    mGraphNode = new graph_node[MAX_GRAPH_NODES];
    for (uint8_t i = 0; i < MAX_GRAPH_NODES; i++) {
        mTaskBuffers[i] = new ipu_psys_term_buffers[MAX_GRAPH_TERMINALS];
    }

    mPollThread = new PollThread(this);
}

PSysDevice::~PSysDevice() {
    LOG1("Destroy PSysDevice");

    // Unregister PSYS buffer
    for (auto& it : mPtrToTermBufMap) {
        unregisterBuffer(&it.second);
    }

    if (mFd >= 0) {
        int ret = ::close(mFd);
        if (ret < 0) {
            LOGE("Failed to close psys device %s, ret %d", strerror(errno), ret);
        }
    }

    mPollThread->requestExitAndWait();
    mPollThread->join();
    delete mPollThread;

    delete[] mGraphNode;
    for (uint8_t i = 0; i < MAX_GRAPH_NODES; i++) {
        delete[] mTaskBuffers[i];
    }
}

int PSysDevice::init() {
    mFd = open(DRIVER_NAME, 0, O_RDWR | O_NONBLOCK);
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "Failed to open psys device %s", strerror(errno));

    mPollThread->run("PSysDevice", PRIORITY_URGENT_AUDIO);

    return OK;
}

int PSysDevice::addGraph(const PSysGraph& graph) {
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "psys device wasn't opened");

    ipu_psys_graph_info graphDrv;
    CLEAR(graphDrv);

    graphDrv.graph_id = INVALID_GRAPH_ID;
    graphDrv.num_nodes = 0;
    graphDrv.nodes = mGraphNode;
    memset(mGraphNode, 0, sizeof(graph_node) * MAX_GRAPH_NODES);

    for (const auto& node : graph.nodes) {
        graph_node& drvNode = graphDrv.nodes[node.nodeCtxId];

        drvNode.node_ctx_id = static_cast<uint8_t>(node.nodeCtxId);
        drvNode.node_rsrc_id = static_cast<uint8_t>(node.nodeRsrcId);

        MEMCPY_S(&drvNode.profiles[0].teb, sizeof(drvNode.profiles[0].teb), &node.bitmaps.teb,
                 sizeof(node.bitmaps.teb));
        MEMCPY_S(&drvNode.profiles[0].deb, sizeof(drvNode.profiles[0].deb), &node.bitmaps.deb,
                 sizeof(node.bitmaps.deb));
        MEMCPY_S(&drvNode.profiles[0].rbm, sizeof(drvNode.profiles[0].rbm), &node.bitmaps.rbm,
                 sizeof(node.bitmaps.rbm));
        MEMCPY_S(&drvNode.profiles[0].reb, sizeof(drvNode.profiles[0].reb), &node.bitmaps.reb,
                 sizeof(node.bitmaps.reb));

        uint8_t termIndex = 0;
        for (const auto& term : node.terminalConfig) {
            const uint8_t termId = term.first;
            drvNode.terminals[termIndex].term_id = termId;
            drvNode.terminals[termIndex].buf_size = term.second.payloadSize;
            termIndex++;
        }

        drvNode.num_terms = termIndex;
        graphDrv.num_nodes++;
    }

    uint8_t linkIndex = 0;
    for (const auto& link : graph.links) {
        graph_link& drvLink = graphDrv.links[linkIndex];

        drvLink.ep_src.node_ctx_id = static_cast<uint8_t>(link.srcNodeCtxId);
        drvLink.ep_src.term_id = static_cast<uint8_t>(link.srcTermId);
        drvLink.ep_dst.node_ctx_id = static_cast<uint8_t>(link.dstNodeCtxId);
        drvLink.ep_dst.term_id = static_cast<uint8_t>(link.dstTermId);

        drvLink.foreign_key = IPU_PSYS_FOREIGN_KEY_NONE;
        drvLink.streaming_mode = link.streamingMode;
        drvLink.pbk_id = IPU_PSYS_LINK_PBK_ID_NONE;
        drvLink.pbk_slot_id = IPU_PSYS_LINK_PBK_SLOT_ID_NONE;
        drvLink.delayed_link = link.delayedLink;

        linkIndex++;
    }

    int ret = ::ioctl(mFd, static_cast<int>(IPU_IOC_GRAPH_OPEN), &graphDrv);
    CheckAndLogError(ret != 0 || graphDrv.graph_id == INVALID_GRAPH_ID, INVALID_OPERATION,
                     "Failed to open graph %s", strerror(errno));

    mGraphId = graphDrv.graph_id;

    return OK;
}

int PSysDevice::closeGraph() {
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "psys device wasn't opened");

    if (mGraphId != INVALID_GRAPH_ID) {
        int ret = ::ioctl(mFd, static_cast<int>(IPU_IOC_GRAPH_CLOSE), &mGraphId);
        CheckAndLogError(ret != 0, INVALID_OPERATION, "Failed to close graph %s", strerror(errno));
        mGraphId = INVALID_GRAPH_ID;
    }

    return OK;
}

int PSysDevice::addTask(const PSysTask& task) {
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "psys device wasn't opened");

    ipu_psys_task_request taskData;
    CLEAR(taskData);

    taskData.graph_id = mGraphId;
    taskData.node_ctx_id = task.nodeCtxId;
    taskData.frame_id = mFrameId[task.nodeCtxId];
    taskData.task_buffers = mTaskBuffers[task.nodeCtxId];
    memset(mTaskBuffers[task.nodeCtxId], 0, sizeof(ipu_psys_term_buffers) * MAX_GRAPH_TERMINALS);
    taskData.term_buf_count = 0;

    for (const auto& item : task.terminalBuffers) {
        taskData.task_buffers[taskData.term_buf_count].term_id = item.first;
        taskData.task_buffers[taskData.term_buf_count].term_buf = item.second.psysBuf;
        taskData.term_buf_count++;
    }

    {
        std::lock_guard<std::mutex> l(mDataLock);
        uint8_t idx = taskData.frame_id % MAX_TASK_NUM;
        CheckWarningNoReturn(mFrameIdToSeqMap[task.nodeCtxId][idx] >= 0,
                             "context %d sequence %lld not done", task.nodeCtxId,
                             mFrameIdToSeqMap[task.nodeCtxId][idx]);

        mFrameIdToSeqMap[task.nodeCtxId][idx] = task.sequence;
        if (mFrameId[task.nodeCtxId] >= MAX_DRV_FRAME_ID) {
            mFrameId[task.nodeCtxId] = 0;
        } else {
            ++mFrameId[task.nodeCtxId];
        }
    }

    int ret = ioctl(mFd, static_cast<int>(IPU_IOC_TASK_REQUEST), &taskData);
    CheckAndLogError(ret != 0, INVALID_OPERATION, "Failed to add task %s", strerror(errno));

    return OK;
}

int PSysDevice::wait(ipu_psys_event& event) {
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "psys device wasn't opened");

    int ret = ioctl(mFd, static_cast<int>(IPU_IOC_DQEVENT), &event);
    CheckAndLogError(ret != 0, INVALID_OPERATION, "Failed to dequeue event %s", strerror(errno));

    return OK;
}

void PSysDevice::updatePsysBufMap(TerminalBuffer* buf) {
    std::lock_guard<std::mutex> l(mDataLock);
    if (buf->flags & IPU_BUFFER_FLAG_USERPTR) {
        mPtrToTermBufMap[buf->userPtr] = *buf;
    } else if (buf->flags & IPU_BUFFER_FLAG_DMA_HANDLE) {
        mFdToTermBufMap[static_cast<int>(buf->handle)] = *buf;
    }
}

bool PSysDevice::getPsysBufMap(TerminalBuffer* buf) {
    std::lock_guard<std::mutex> l(mDataLock);
    if (buf->flags & IPU_BUFFER_FLAG_USERPTR) {
        if (mPtrToTermBufMap.find(buf->userPtr) != mPtrToTermBufMap.end()) {
            buf->psysBuf = mPtrToTermBufMap[buf->userPtr].psysBuf;
            return true;
        }
    } else if (buf->flags & IPU_BUFFER_FLAG_DMA_HANDLE) {
        if (mFdToTermBufMap.find(static_cast<int>(buf->handle)) != mFdToTermBufMap.end()) {
            buf->psysBuf = mFdToTermBufMap[static_cast<int>(buf->handle)].psysBuf;
            return true;
        }
    }

    return false;
}

int PSysDevice::registerBuffer(TerminalBuffer* buf) {
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "psys device wasn't opened");
    CheckAndLogError(!buf, INVALID_OPERATION, "buf is nullptr");

    // If already registered, just return
    if (getPsysBufMap(buf)) return OK;

    int ret = OK;
    buf->psysBuf.len = buf->size;
    if (buf->flags & IPU_BUFFER_FLAG_USERPTR) {
        buf->psysBuf.base.userptr = buf->userPtr;
        buf->psysBuf.flags |= IPU_BUFFER_FLAG_USERPTR;

        ret = ioctl(mFd, static_cast<int>(IPU_IOC_GETBUF), &buf->psysBuf);
        CheckAndLogError(ret != 0, INVALID_OPERATION, "Failed to get buffer %s", strerror(errno));

        if (!(buf->psysBuf.flags & IPU_BUFFER_FLAG_DMA_HANDLE)) {
            LOGW("IOC_GETBUF succeed but did not return dma handle");
            return INVALID_OPERATION;
        } else if (buf->psysBuf.flags & IPU_BUFFER_FLAG_USERPTR) {
            LOGW("IOC_GETBUF succeed but did not consume the userptr flag");
            return INVALID_OPERATION;
        }
    } else if (buf->flags & IPU_BUFFER_FLAG_DMA_HANDLE) {
        buf->psysBuf.base.fd = static_cast<int>(buf->handle);
        buf->psysBuf.flags |= IPU_BUFFER_FLAG_DMA_HANDLE;
    }

    if (buf->flags & IPU_BUFFER_FLAG_NO_FLUSH) {
        buf->psysBuf.flags |= IPU_BUFFER_FLAG_NO_FLUSH;
    }

    buf->psysBuf.data_offset = 0;
    buf->psysBuf.bytes_used = buf->psysBuf.len;

    ret = ioctl(mFd, static_cast<int>(IPU_IOC_MAPBUF),
                reinterpret_cast<void*>((intptr_t)buf->psysBuf.base.fd));
    CheckAndLogError(ret != 0, INVALID_OPERATION, "Failed to map buffer %s", strerror(errno));

    // Save PSYS buf
    updatePsysBufMap(buf);

    LOG2("%s, mapbuffer flags %x, ptr %p, fd %d, size %d", __func__, buf->flags, buf->userPtr,
         buf->psysBuf.base.fd, buf->size);

    return OK;
}

int PSysDevice::unregisterBuffer(TerminalBuffer* buf) {
    CheckAndLogError(mFd < 0, INVALID_OPERATION, "psys device wasn't opened");
    CheckAndLogError(!buf, INVALID_OPERATION, "buf is nullptr");

    if (buf->flags & IPU_BUFFER_FLAG_DMA_HANDLE) {
        LOGW("cannot unmap buffer fd %d", buf->psysBuf.base.fd);
        return OK;
    }

    int ret = ioctl(mFd, static_cast<int>(IPU_IOC_UNMAPBUF),
                    reinterpret_cast<void*>((intptr_t)buf->psysBuf.base.fd));
    if (ret != 0) {
        LOGW("Failed to unmap buffer %s", strerror(errno));
    }

    if (buf->flags & IPU_BUFFER_FLAG_USERPTR) {
        ret = close(buf->psysBuf.base.fd);
        CheckAndLogError(ret < 0, INVALID_OPERATION, "Failed to close fd %d, error %s",
                         buf->psysBuf.base.fd, strerror(errno));
    }

    return OK;
}

void PSysDevice::registerPSysDeviceCallback(uint8_t contextId, IPSysDeviceCallback* callback) {
    std::lock_guard<std::mutex> l(mDataLock);

    mPSysDeviceCallbackMap[contextId] = callback;
}

int PSysDevice::poll(short events, int timeout)
{
    struct pollfd fds = { mFd, events, 0 };

    return ::poll(&fds, 1, timeout);
}

void PSysDevice::handleEvent(const ipu_psys_event& event) {
    std::lock_guard<std::mutex> l(mDataLock);
    if (mPSysDeviceCallbackMap.find(event.node_ctx_id) == mPSysDeviceCallbackMap.end()) {
        LOGW("context id %u isn't found", event.node_ctx_id);
        return;
    }

    uint8_t idx = event.frame_id % MAX_TASK_NUM;
    if (mFrameIdToSeqMap[event.node_ctx_id][idx] < 0) {
        LOGW("frame id %u isn't found", event.frame_id);
        return;
    }

    int64_t sequence = mFrameIdToSeqMap[event.node_ctx_id][idx];
    mPSysDeviceCallbackMap[event.node_ctx_id]->bufferDone(sequence);
    mFrameIdToSeqMap[event.node_ctx_id][idx] = -1;
    LOG2("context id %u, frame id %u is done", event.node_ctx_id, event.frame_id);
}

PSysDevice::PollThread::PollThread(PSysDevice* psysDevice)
        : mPSysDevice(psysDevice) {
}

bool PSysDevice::PollThread::threadLoop() {
    int ret = mPSysDevice->poll(POLLIN | POLLHUP | POLLERR, kEventTimeout * SLOWLY_MULTIPLIER);

    if (isExiting()) return false;

    if (ret == POLLIN) {
        ipu_psys_event event;
        CLEAR(event);

        ret = mPSysDevice->wait(event);
        if (ret == OK) {
            mPSysDevice->handleEvent(event);
        }
    } else {
        LOG2("%s, device poll timeout", __func__);
    }

    return true;
}

}  // namespace icamera
