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

#include "IPAMemory.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include <libcamera/base/log.h>

#include "libcamera/internal/framebuffer.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU)

IPAMemory::IPAMemory() : mIpaBufferId(0) {}

IPAMemory::~IPAMemory() {}

SharedFD IPAMemory::allocateShmMem(const std::string& name, unsigned int size, void** addr) {
    SharedFD fd(shm_open(name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR));

    do {
        if (!fd.isValid()) {
            LOG(IPAIPU, Error) << "Failed to open shm, name " << name.c_str() << " errorno "
                               << strerror(errno);
            break;
        }

        int ret = fcntl(fd.get(), F_GETFD);
        if (ret == -1) {
            LOG(IPAIPU, Error) << "Failed to fcntl shmFd " << fd.get() << " errorno "
                               << strerror(errno);
            break;
        }

        ret = ftruncate(fd.get(), size);
        if (ret == -1) {
            LOG(IPAIPU, Error) << "Failed to ftruncate shmFd " << fd.get() << " errorno "
                               << strerror(errno);
            break;
        }

        struct stat sb;
        ret = fstat(fd.get(), &sb);
        if (ret == -1) {
            LOG(IPAIPU, Error) << "Failed to fstat shmFd " << fd.get() << " errorno "
                               << strerror(errno);
            break;
        }

        void* shmAddr = mmap(0, sb.st_size, PROT_WRITE, MAP_SHARED, fd.get(), 0);
        if (!shmAddr) {
            LOG(IPAIPU, Error) << "Failed to mmap shmFd " << fd.get() << " errorno "
                               << strerror(errno);
            break;
        }
        *addr = shmAddr;

        return fd;
    } while (0);

    return SharedFD(-1);
}

void IPAMemory::releaseShmMem(const std::string& name, unsigned int size, void* addr) {
    munmap(addr, size);
    shm_unlink(name.c_str());
}

std::shared_ptr<FrameBuffer> IPAMemory::allocateBuffer(const std::string& name, unsigned int size,
                                                       void** addr) {
    SharedFD fd = allocateShmMem(name, size, addr);
    if (!fd.isValid()) {
        LOG(IPAIPU, Error) << "Failed to allocate buffer  " << name.c_str();
        return nullptr;
    }

    std::vector<FrameBuffer::Plane> planes;
    FrameBuffer::Plane plane;

    plane.fd = fd;
    plane.offset = 0;
    plane.length = size;

    planes.push_back(std::move(plane));

    mIpaBufferId++;
    return std::make_shared<FrameBuffer>(planes, mIpaBufferId);
}

void IPAMemory::freeBuffer(const std::string& name, const std::shared_ptr<FrameBuffer>& buffer,
                           void* addr) {
    const std::vector<FrameBuffer::Plane> &planes = buffer->planes();
    releaseShmMem(name, planes[0].length, addr);
}

} /* namespace libcamera */
