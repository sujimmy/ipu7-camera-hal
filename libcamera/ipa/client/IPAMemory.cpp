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
#include <libcamera/base/shared_fd.h>
#include <libcamera/base/unique_fd.h>

#include "libcamera/internal/framebuffer.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU)

IPAMemory::IPAMemory() : mIpaBufferId(0) {
}

IPAMemory::~IPAMemory() {}

int IPAMemory::allocateShmMem(const std::string& name, unsigned int size, int* fd, void** addr) {
    int shmFd = -1;
    void* shmAddr = nullptr;

    shmFd = shm_open(name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shmFd == -1) {
        LOG(IPAIPU, Error) << "Failed to open shm, name " << name.c_str() << " errorno "
                           << strerror(errno);
        return -1;
    }

    do {
        int ret = fcntl(shmFd, F_GETFD);
        if (ret == -1) {
            LOG(IPAIPU, Error) << "Failed to fcntl shmFd " << shmFd << " errorno " << strerror(errno);
            break;
        }

        ret = ftruncate(shmFd, size);
        if (ret == -1) {
            LOG(IPAIPU, Error) << "Failed to ftruncate shmFd " << shmFd << " errorno "
                               << strerror(errno);
            break;
        }

        struct stat sb;
        ret = fstat(shmFd, &sb);
        if (ret == -1) {
            LOG(IPAIPU, Error) << "Failed to fstat shmFd " << shmFd << " errorno " << strerror(errno);
            break;
        }

        shmAddr = mmap(0, sb.st_size, PROT_WRITE, MAP_SHARED, shmFd, 0);
        if (!shmAddr) {
            LOG(IPAIPU, Error) << "Failed to mmap shmFd " << shmFd << " errorno " << strerror(errno);
            break;
        }

        *fd = shmFd;
        *addr = shmAddr;

        return 0;
    } while (0);

    close(shmFd);
    return -1;
}

void IPAMemory::releaseShmMem(const std::string& name, unsigned int size, int fd, void* addr) {
    munmap(addr, size);
    close(fd);
    shm_unlink(name.c_str());
}

std::shared_ptr<FrameBuffer> IPAMemory::allocateBuffer(const std::string& name, unsigned int size,
                                                       void** addr) {
    int fd = -1;

    int ret = allocateShmMem(name, size, &fd, addr);
    if (ret == -1) {
        LOG(IPAIPU, Error) << "Failed to allocate buffer  " << name.c_str();
        return nullptr;
    }

    std::vector<FrameBuffer::Plane> planes;

    UniqueFD ufd(fd);
    FrameBuffer::Plane plane;

    plane.fd = SharedFD(std::move(ufd));
    plane.offset = 0;
    plane.length = size;

    planes.push_back(std::move(plane));

    mIpaBufferId++;
    return std::make_shared<FrameBuffer>(planes, mIpaBufferId);
}

void IPAMemory::freeBuffer(const std::string& name, const std::shared_ptr<FrameBuffer>& buffer,
                           void* addr) {
    const std::vector<FrameBuffer::Plane> &planes = buffer->planes();

    releaseShmMem(name, planes[0].length, planes[0].fd.get(), addr);
}

} /* namespace libcamera */
