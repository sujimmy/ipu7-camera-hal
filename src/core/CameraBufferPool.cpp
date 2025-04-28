/*
 * Copyright (C) 2022-2024 Intel Corporation
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

#define LOG_TAG CameraBufferPool

#include "CameraBufferPool.h"
#include "iutils/Utils.h"
#include "iutils/CameraLog.h"

namespace icamera {

CameraBufferPool::CameraBufferPool() {
    LOG2("@%s", __func__);
}

CameraBufferPool::~CameraBufferPool() {
    LOG2("@%s", __func__);
    destroyBufferPool();
}

status_t CameraBufferPool::createBufferPool(int cameraId, uint32_t numBufs,
                                            const stream_t& stream) {
    LOG1("@%s number of buffers:%d", __func__, numBufs);
    std::lock_guard<std::mutex> l(mLock);
    mBuffers.clear();

    for (uint32_t i = 0; i < numBufs; i++) {
        std::shared_ptr<CameraBuffer> buffer = CameraBuffer::create(stream.memType,
            stream.size, i, stream.format, stream.width, stream.height);
        if (!buffer) {
            mBuffers.clear();
            LOGE("failed to alloc %d internal buffers", i);
            return NO_MEMORY;
        }

        // Initialize the buffer status to free
        mBuffers[buffer] = false;
    }

    return OK;
}

void CameraBufferPool::destroyBufferPool() {
    LOG1("@%s Internal buffers size:%zu", __func__, mBuffers.size());

    std::lock_guard<std::mutex> l(mLock);
    mBuffers.clear();
}

std::shared_ptr<CameraBuffer> CameraBufferPool::acquireBuffer() {
    std::lock_guard<std::mutex> l(mLock);
    for (auto& buf : mBuffers) {
        if (!buf.second) {
            buf.second = true;
            return buf.first;
        }
    }

    LOGE("%s all the internal buffers are busy", __func__);
    return nullptr;
}

void CameraBufferPool::returnBuffer(std::shared_ptr<CameraBuffer> buffer) {
    std::lock_guard<std::mutex> l(mLock);
    for (auto& buf : mBuffers) {
        if (buf.second && (buf.first == buffer)) {
            buf.second = false;
            return;
        }
    }

    LOGE("%s, the internal buffer addr:%p not found", __func__, buffer->getBufferAddr());
}
}  // namespace icamera
