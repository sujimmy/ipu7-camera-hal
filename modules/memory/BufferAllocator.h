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

#pragma once

#include "ParamDataType.h"
#ifdef LIBCAMERA_BUILD
#include "libcamera/internal/dma_buf_allocator.h"

namespace icamera {
using namespace libcamera;

class BufferAllocator {
 public:
    BufferAllocator() {}
    virtual ~BufferAllocator() {}

    int allocateDmaBuffer(camera_buffer_t* ubuffer);
    // buffer will be released when UniqueFD is destructed
    void freeDmaBuffer() {}
    static DmaBufAllocator sDmaBufAllocator;

 private:
    UniqueFD mUniqueBufferFD;
};

}  // namespace icamera
#else
#ifdef CAL_BUILD
#include <cros-camera/camera_buffer_manager.h>

namespace icamera {

class BufferAllocator {
 public:
    BufferAllocator() : mHandle(nullptr), mUsrAddr(nullptr) {}
    virtual ~BufferAllocator() {}

    int allocateDmaBuffer(camera_buffer_t* ubuffer);
    void freeDmaBuffer();

 private:
    void* lock(int width, int height, int format, buffer_handle_t handle);
    int V4l2FormatToHALFormat(int v4l2Format);

 private:
    buffer_handle_t mHandle;
    void* mUsrAddr;
};

}  // namespace icamera
#else

namespace icamera {

class BufferAllocator {
 public:
    BufferAllocator() {}
    virtual ~BufferAllocator() {}

    int allocateDmaBuffer(camera_buffer_t* ubuffer) { return -1; }
    void freeDmaBuffer() {}
};

}  // namespace icamera

#endif
#endif
