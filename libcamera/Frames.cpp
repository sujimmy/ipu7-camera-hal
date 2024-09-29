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

#include "CameraContext.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPU7)

IPUFrames::IPUFrames() {
    for (uint8_t i = 0; i < kMaxProcessingRequest; i++) {
        mAvailableRequestBuffers.push(&mRequestBuffers[i]);
    }

    mResultsHandler.init(this);
}

IPUFrames::~IPUFrames() {}

void IPUFrames::clear() {
    MutexLocker locker(mMutex);

    mProcessingRequests.clear();
}

Info* IPUFrames::create(Request* request) {
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
    info->isStill = false;

    mProcessingRequests[id] = info;

    return info;
}

void IPUFrames::remove(Info* info) {
    unsigned int id = info->id;

    MutexLocker locker(mMutex);

    mAvailableRequestBuffers.push(info);

    mProcessingRequests.erase(id);
}

Info* IPUFrames::find(unsigned int frameNumber) {
    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        return info;
    }

    return nullptr;
}

bool IPUFrames::getBuffer(Info* info, const icamera::stream_t& halStream, FrameBuffer* frameBuffer,
                           icamera::camera_buffer_t* buf) {
    if (!info || !frameBuffer || !buf) return false;

    const std::vector<FrameBuffer::Plane> &planes = frameBuffer->planes();
    if (planes.size() == 0 || planes[0].fd.get() < 0) return false;

    buf->s = halStream;
    buf->s.memType = V4L2_MEMORY_DMABUF;
    buf->frameNumber = info->id;
    buf->dmafd = planes[0].fd.get();
    buf->flags = icamera::BUFFER_FLAG_DMA_EXPORT;

    info->outBuffers[halStream.id] = frameBuffer;

    LOG(IPU7, Debug) << "id " << info->id << " dma fd " << buf->dmafd;
    return true;
}

void IPUFrames::returnRequest(Info* info) {
}

void IPUFrames::shutterReady(unsigned int frameNumber) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->shutterReady = true;

        return;
    }

    LOG(IPU7, Warning) << "id " << frameNumber << " for shutter isn't found";
}

void IPUFrames::metadataReady(unsigned int frameNumber, int64_t sequence) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->metadataReady = true;

        return;
    }

    LOG(IPU7, Warning) << "id " << frameNumber << " for metadata isn't found";
}

void IPUFrames::bufferReady(unsigned int frameNumber, unsigned int streamId) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->outBuffers.erase(streamId);

        return;
    }

    LOG(IPU7, Warning) << "id " << frameNumber << " for buffer isn't found";
}

Info* IPUFrames::requestComplete(unsigned int frameNumber) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        if (info->shutterReady && info->metadataReady && !info->outBuffers.size()) {
            return info;
        }
    }

    return nullptr;
}

IPUResults::IPUResults() : mState(State::Stopped) {
    icamera::camera_callback_ops_t::notify = IPUResults::notifyCallback;
}

IPUResults::~IPUResults() {
    {
        MutexLocker locker(mMutex);
        mState = State::Stopped;
        mEventCondition.notify_one();
    }

    wait();

    LOG(IPU7, Debug) << "Result thread stopped";
}

void IPUResults::init(IPUFrames* frames) {
    mIPUFrames = frames;

    {
        MutexLocker locker(mMutex);
        mState = State::Running;
    }

    start();

    LOG(IPU7, Debug) << "Result thread started";
}

void IPUResults::sendEvent(const icamera::camera_msg_data_t& data) {
    MutexLocker locker(mMutex);

    mEventQueue.push(data);
    mEventCondition.notify_one();
}

void IPUResults::run() {
    LOG(IPU7, Debug) << "Enter result thread loop";

    while(true) {
        icamera::camera_msg_data_t data;
        {
            MutexLocker locker(mMutex);

            auto isEmpty = ([&]() LIBCAMERA_TSA_REQUIRES(mMutex) {
                return mState != State::Running || !mEventQueue.empty();
            });

            auto duration = std::chrono::duration<float, std::ratio<2 / 1>>(10);
            if (mEventQueue.empty()) {
                mEventCondition.wait_for(locker, duration, isEmpty);
            }

            if (mState != State::Running) break;

            if (mEventQueue.empty()) continue;

            data = std::move(mEventQueue.front());
            mEventQueue.pop();
        }

        handleEvent(data);
    }

    LOG(IPU7, Debug) << "Exit result thread loop";
}

void IPUResults::notifyCallback(const icamera::camera_callback_ops_t* cb,
                                 const icamera::camera_msg_data_t& data) {
    if (!cb) return;

    IPUResults* callback = const_cast<IPUResults*>(static_cast<const IPUResults*>(cb));

    callback->sendEvent(data);
}

void IPUResults::handleEvent(const icamera::camera_msg_data_t& data) {
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

void IPUResults::shutterDone(unsigned int frameNumber, uint64_t timestamp) {
    mShutterReady.emit(frameNumber, timestamp);
}

void IPUResults::metadataDone(unsigned int frameNumber, int64_t sequence) {
    mMetadataAvailable.emit(frameNumber, sequence);
}

void IPUResults::bufferDone(unsigned int streamId) {
    mBufferAvailable.emit(streamId);
}

} /* namespace libcamera */
