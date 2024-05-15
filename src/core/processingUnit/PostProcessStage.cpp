/*
 * Copyright (C) 2022 Intel Corporation.
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

#define LOG_TAG PostProcessStage

#include "src/core/processingUnit/PostProcessStage.h"

#include "PlatformData.h"
#include "iutils/CameraLog.h"

namespace icamera {

PostProcessStage::PostProcessStage(int cameraId, int stageId, const std::string& stageName)
        : IPipeStage(stageName.c_str(), stageId),
          mCameraId(cameraId),
          mInputPort(INVALID_PORT),
          mOutputBuffersNum(0) {}
PostProcessStage::~PostProcessStage() {}

void PostProcessStage::setFrameInfo(const std::map<uuid, stream_t>& inputInfo,
                                    const std::map<uuid, stream_t>& outputInfo) {
    CheckWarningNoReturn(inputInfo.size() > 1, "Only support one input");
    BufferQueue::setFrameInfo(inputInfo, outputInfo);

    mPostProcessors.clear();
    mInputPort = mInputFrameInfo.begin()->first;  // Only support one input currently
    stream_t input = mInputFrameInfo[mInputPort];
    // Work around: Now Graphs provide fourcc format, but here we should use v4l2 format.
    // TODO: Will pass v4l2 format and IpuPipeStage should change it to fourcc format
    input.format = CameraUtils::getV4L2Format(input.format);
    for (auto& info : mOutputFrameInfo) {
        stream_t output = info.second;
        output.format = CameraUtils::getV4L2Format(output.format);
        mPostProcessors[info.first] =
            std::unique_ptr<SwPostProcessUnit>(new SwPostProcessUnit(mCameraId));
        mPostProcessors[info.first]->configure(input, output);

        LOG1("%s created, out port %u, post type %d", getName(), info.first,
             mPostProcessors[info.first]->getPostProcessType());
    }

    mOutputBuffersNum = mOutputFrameInfo.size();
}

int PostProcessStage::qbuf(uuid port, const std::shared_ptr<CameraBuffer>& camBuffer) {
    AutoMutex l(mBufferQueueLock);
    CheckAndLogError(mPendingOutBuffers.find(port) != mPendingOutBuffers.end(), INVALID_OPERATION,
                     "%s: already have buffer for port %x", getName(), port);
    CheckAndLogError(mPostProcessors.find(port) == mPostProcessors.end(), INVALID_OPERATION,
                     "%s: unsupported port %x", getName(), port);

    mPendingOutBuffers[port] = camBuffer;
    // Wait for all output buffers come for one request.
    // Assume no buffer of request n+1 comes before stage gets all output buffers of request n.
    if (mPendingOutBuffers.size() < mOutputBuffersNum) return OK;

    int64_t sequence = -1;
    for (auto& item : mPendingOutBuffers) {
        CameraBufQ& output = mOutputQueue[item.first];
        output.push(item.second);
        if (item.second) sequence = item.second->getSettingSequence();
    }

    // Select input buffers for the request
    std::shared_ptr<CameraBuffer> inBuffer;
    if (!fetchRequestBuffer(sequence, inBuffer)) return INVALID_OPERATION;
    mBufferProducer->qbuf(mInputPort, inBuffer);

    mOutputAvailableSignal.signal();
    mPendingOutBuffers.clear();
    return OK;
}

bool PostProcessStage::fetchRequestBuffer(int64_t sequence,
                                          std::shared_ptr<CameraBuffer>& inBuffer) {
    bool useInternal = false;

    // Check if internal buffer is need
    for (auto& item : mPendingOutBuffers) {
        if (!item.second) continue;

        if (inBuffer)
            useInternal = true;  // Request multiple streams output
        else
            inBuffer = item.second;
        if (!mPostProcessors[item.first]->isBypassed(sequence)) useInternal = true;

        if (useInternal) break;
    }

    LOG2("<seq%ld>%s: %s, inBuffer %p, use internal buffer? %d", sequence, getName(), __func__,
         inBuffer.get(), useInternal);
    if (!useInternal && inBuffer) return true;

    CameraBufVector& bufV = mInternalBuffers[mInputPort];
    CheckAndLogError(bufV.empty(), false, "%s: queued %d, no avaiable buffer", getName(),
                     mQueuedInputBuffers.size());
    inBuffer = bufV.back();
    bufV.pop_back();
    mQueuedInputBuffers.push(inBuffer);
    inBuffer->setSettingSequence(sequence);
    return true;
}

bool PostProcessStage::process(int64_t triggerId) {
    PERF_CAMERA_ATRACE_PARAM1(getName(), triggerId);
    std::map<uuid, std::shared_ptr<CameraBuffer> > inBuffers;
    std::map<uuid, std::shared_ptr<CameraBuffer> > outBuffers;

    {
        AutoMutex l(mBufferQueueLock);
        if (getFreeBuffersInQueue(inBuffers, outBuffers) != OK) return true;
    }

    std::shared_ptr<CameraBuffer> inBuffer = inBuffers.begin()->second;
    v4l2_buffer_t inV4l2Buf = *inBuffer->getV4L2Buffer().Get();
    int64_t sequence = inBuffer->getSequence();
    for (auto& output : outBuffers) {
        if (!output.second) continue;

        uuid outPort = output.first;
        LOG2("<seq%ld>%s: handle port %x in async", sequence, getName(), outPort);

        bool needLock = (inBuffer != output.second);  // need access to output buffer
        if (needLock) output.second->lock();
        int32_t ret = mPostProcessors[outPort]->doPostProcessing(inBuffer, output.second);
        CheckWarningNoReturn(ret != OK, false, "%s: Process errorfor port %d", getName(), outPort);
        if (needLock) output.second->unlock();

        updateInfoAndSendEvents(inV4l2Buf, output.second, outPort);
    }

    returnBuffers(inBuffers, outBuffers);
    return true;
}

void PostProcessStage::updateInfoAndSendEvents(const v4l2_buffer_t& inV4l2Buf,
                                               std::shared_ptr<CameraBuffer> outBuffer,
                                               int32_t outPort) {
    outBuffer->updateV4l2Buffer(inV4l2Buf);

    EventData bufferEvent;
    bufferEvent.type = EVENT_STAGE_BUF_READY;
    bufferEvent.data.stageBufReady.sequence = inV4l2Buf.sequence;
    bufferEvent.data.stageBufReady.uuid = outPort;
    bufferEvent.buffer = outBuffer;
    notifyListeners(bufferEvent);
}

void PostProcessStage::returnBuffers(std::map<uuid, std::shared_ptr<CameraBuffer> >& inBuffers,
                                     std::map<uuid, std::shared_ptr<CameraBuffer> >& outBuffers) {
    // Check and return internal input buffer
    if (inBuffers.find(mInputPort) != inBuffers.end() && !mQueuedInputBuffers.empty()) {
        AutoMutex l(mBufferQueueLock);
        if (mQueuedInputBuffers.front() == inBuffers[mInputPort]) {
            mInternalBuffers[mInputPort].push_back(mQueuedInputBuffers.front());
            mQueuedInputBuffers.pop();
            inBuffers.erase(mInputPort);
        }
    }

    // Don't return input buffer to producer here because it happens only when stage gets outputs
    inBuffers.clear();
    BufferQueue::returnBuffers(inBuffers, outBuffers);
}

int32_t PostProcessStage::allocateBuffers() {
    mInternalBuffers.clear();
    while (!mQueuedInputBuffers.empty()) mQueuedInputBuffers.pop();
    CheckAndLogError(!mBufferProducer, BAD_VALUE, "@%s: Buffer Producer is nullptr", __func__);
#ifdef CAL_BUILD
    int memoryType = V4L2_MEMORY_DMABUF;
#else
    int memoryType = V4L2_MEMORY_USERPTR;
#endif

    if (mInputFrameInfo.empty()) return OK;

    // Only support one input currently
    const stream_t& input = mInputFrameInfo[mInputPort];
    LOG1("%s fmt:%s (%dx%d)", __func__, CameraUtils::format2string(input.format).c_str(),
         input.width, input.height);

    int32_t size = CameraUtils::getFrameSize(input.format, input.width, input.height);
    for (int i = 0; i < MAX_BUFFER_COUNT; i++) {
        std::shared_ptr<CameraBuffer> camBuffer =
            CameraBuffer::create(memoryType, size, i, input.format, input.width, input.height);
        CheckAndLogError(!camBuffer, NO_MEMORY, "Allocate producer userptr buffer failed");

        camBuffer->setUserBufferFlags(BUFFER_FLAG_SW_READ);
        mInternalBuffers[mInputPort].push_back(camBuffer);
    }
    return OK;
}

int PostProcessStage::start() {
    return allocateBuffers();
}

}  // namespace icamera
