/*
 * Copyright (C) 2024 Intel Corporation
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

#include "Frames.h"

#include <linux/videodev2.h>

#include <libcamera/base/log.h>
#include <libcamera/request.h>
#include <libcamera/control_ids.h>

#include "libcamera/internal/framebuffer.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPU7)

IPU7Frames::IPU7Frames() {
    for (uint8_t i = 0; i < kMaxProcessingRequest; i++) {
        mAvailableRequestBuffers.push(&mRequestBuffers[i]);
    }
}

IPU7Frames::~IPU7Frames() {}

void IPU7Frames::clear() {
    MutexLocker locker(mMutex);

    mProcessingRequests.clear();
}

Info* IPU7Frames::create(Request* request) {
    unsigned int id = request->sequence();

    MutexLocker locker(mMutex);

    if (mAvailableRequestBuffers.empty()) {
        return nullptr;
    }

    Info* info = mAvailableRequestBuffers.front();
    mAvailableRequestBuffers.pop();

    info->id = id;
    info->request = request;
    info->metadataReady = false;
    info->shutterReady = false;

    mProcessingRequests[id] = info;

    return info;
}

void IPU7Frames::remove(Info* info) {
    unsigned int id = info->id;

    MutexLocker locker(mMutex);

    mAvailableRequestBuffers.push(info);

    mProcessingRequests.erase(id);
}

void IPU7Frames::processRequest(Info* info) {
}

bool IPU7Frames::getBuffer(Info* info, const icamera::stream_t& halStream, FrameBuffer* frameBuffer,
                           icamera::camera_buffer_t* buf) {
    if (!info || !frameBuffer || !buf) return false;
    const std::vector<FrameBuffer::Plane> &planes = frameBuffer->planes();
    if (planes.size() == 0 || planes[0].fd.get() < 0) return false;

    buf->s = halStream;
    buf->frameNumber = info->id;
    buf->dmafd = planes[0].fd.get();
    buf->flags = V4L2_MEMORY_DMABUF;

    info->outBuffers.insert(halStream.id);

    return true;
}

void IPU7Frames::returnRequest(Info* info) {
}

void IPU7Frames::updateShutterInfo(unsigned int frameNumber, uint64_t timestamp) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->request->metadata().set(controls::SensorTimestamp, timestamp);
        info->shutterReady = true;

        return;
    }

    LOG(IPU7, Warning) << "id " << frameNumber << " for shutter isn't found";
}

void IPU7Frames::upateMetadataInfo(unsigned int frameNumber, int64_t sequence) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->metadataReady = true;

        return;
    }

    LOG(IPU7, Warning) << "id " << frameNumber << " for metadata isn't found";
}

IPU7Results::IPU7Results(IPU7Frames* frames)
    : mIPU7Frames(frames) {
    icamera::camera_callback_ops_t::notify = IPU7Results::notifyCallback;
}

IPU7Results::~IPU7Results() {
}

void IPU7Results::notifyCallback(const icamera::camera_callback_ops_t* cb,
                                 const icamera::camera_msg_data_t& data) {
    if (!cb) return;

    IPU7Results* callback = const_cast<IPU7Results*>(static_cast<const IPU7Results*>(cb));

    callback->handleEvent(data);
}

void IPU7Results::handleEvent(const icamera::camera_msg_data_t& data) {
    switch(data.type) {
        case icamera::CAMERA_ISP_BUF_READY: {
            shutterDone(data.data.buffer_ready.frameNumber, data.data.buffer_ready.timestamp);
            break;
        }
        case icamera::CAMERA_METADATA_READY: {
            metadataDone(data.data.metadata_ready.frameNumber, data.data.metadata_ready.sequence);
            break;
        }
        case icamera::CAMERA_FRAME_DONE: {
            bufferDone(data.data.frame_ready.streamId);
            break;
        }
        default: {
            break;
        }
    }
}

void IPU7Results::shutterDone(unsigned int frameNumber, uint64_t timestamp) {
    mIPU7Frames->updateShutterInfo(frameNumber, timestamp);
}

void IPU7Results::metadataDone(unsigned int frameNumber, int64_t sequence) {
    mMetadataAvailable.emit(frameNumber, sequence);
}

void IPU7Results::bufferDone(unsigned int streamId) {
    mBufferAvailable.emit(streamId);
}

} /* namespace libcamera */
