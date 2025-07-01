/*
 * Copyright (C) 2022-2025 Intel Corporation
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

#ifndef CAMERA_BUFFER_POOL_H
#define CAMERA_BUFFER_POOL_H

#include <mutex>
#include <unordered_map>

#include "CameraBuffer.h"
#include "iutils/Errors.h"

namespace icamera {

/**
 * \class CameraBufferPool
 *
 * This class is used to manage a memory pool based on CameraBuffer
 * It needs to follow the calling sequence:
 * createBufferPool -> acquireBuffer -> returnBuffer
 */
class CameraBufferPool {
 public:
    CameraBufferPool();
    ~CameraBufferPool();

    status_t createBufferPool(int cameraId, uint32_t numBufs, const stream_t& stream);
    void destroyBufferPool();
    std::shared_ptr<CameraBuffer> acquireBuffer();
    void returnBuffer(std::shared_ptr<CameraBuffer> buffer);

 private:
    std::unordered_map<std::shared_ptr<CameraBuffer>, bool> mBuffers;
    // first: CameraBuffer, second: true as buffer in used
    std::mutex mLock;  // lock the mBuffers
};
}  // namespace icamera

#endif // CAMERA_BUFFER_POOL_H
