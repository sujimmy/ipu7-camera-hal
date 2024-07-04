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

#pragma once

#include <libcamera/base/mutex.h>
#include <libcamera/base/signal.h>

namespace libcamera {

class FrameBuffer;

class IPAMemory {
 public:
    IPAMemory();
    ~IPAMemory();

    std::shared_ptr<FrameBuffer> allocateBuffer(const std::string& name, unsigned int size,
                                                void** addr);
    void freeBuffer(const std::string& name, const std::shared_ptr<FrameBuffer>& buffer,
                    void* addr);
 private:
    int allocateShmMem(const std::string& name, unsigned int size, int* fd, void** addr);
    void releaseShmMem(const std::string& name, unsigned int size, int fd, void* addr);

    unsigned int mIpaBufferId;
};

} /* namespace libcamera */
