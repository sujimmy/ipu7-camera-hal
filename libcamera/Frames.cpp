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

IPUFrames::IPUFrames(bool zslEnable) : mZslCapture(nullptr) {
    if (zslEnable) {
        mZslCapture = new ZslCapture;
    }

    for (uint8_t i = 0; i < kMaxProcessingRequest; i++) {
        mAvailableRequestBuffers.push(&mRequestBuffers[i]);
    }
}

IPUFrames::~IPUFrames() {
    delete mZslCapture;
}

void IPUFrames::clear() {
    MutexLocker locker(mMutex);

    mProcessingRequests.clear();
}

Info* IPUFrames::create(Request* request) {
    if (!request) {
        LOG(IPU7, Error) << "request is nullptr";
        return nullptr;
    }

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

    if (mZslCapture) mZslCapture->registerFrameInfo(id, request->controls());

    return info;
}

void IPUFrames::recycle(Info* info) {
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
    buf->sequence = -1;
    buf->timestamp = 0;

    if (mZslCapture && info->isStill) {
        mZslCapture->getZslSequenceAndTimestamp(info->request->controls(), buf->timestamp,
                                                buf->sequence);
    }

    info->outBuffers[halStream.id] = frameBuffer;

    LOG(IPU7, Debug) << "id " << info->id << " dma fd " << buf->dmafd;
    return true;
}

void IPUFrames::shutterReady(unsigned int frameNumber, uint64_t timestamp) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->shutterReady = true;

        if (mZslCapture) mZslCapture->updateTimeStamp(frameNumber, timestamp);

        return;
    }

    LOG(IPU7, Warning) << "id " << frameNumber << " for shutter isn't found";
}

void IPUFrames::metadataReady(unsigned int frameNumber, int64_t sequence,
                              const ControlList& metadata) {
    MutexLocker locker(mMutex);

    if (mProcessingRequests.find(frameNumber) != mProcessingRequests.end()) {
        Info* info = mProcessingRequests[frameNumber];

        info->metadataReady = true;

        if (mZslCapture) {
            mZslCapture->updateSequence(frameNumber, sequence);
            mZslCapture->update3AStatus(frameNumber, metadata);
        }

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

} /* namespace libcamera */
