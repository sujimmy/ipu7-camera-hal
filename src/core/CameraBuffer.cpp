/*
 * Copyright (C) 2015-2022 Intel Corporation.
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

#define LOG_TAG CameraBuffer

#include "CameraBuffer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <memory>
#include <vector>
#ifdef CAL_BUILD
#include <hardware/gralloc.h>
#endif

#include "PlatformData.h"
#include "iutils/CameraLog.h"
#include "iutils/Utils.h"

namespace icamera {
CameraBuffer::CameraBuffer(int memory, uint32_t size, int index)
        : mAllocatedMemory(false),
          mU(nullptr),
          mSettingSequence(-1),
#ifdef CAL_BUILD
          mHandle(nullptr),
#endif
          mMmapAddrs(nullptr),
          mDmaFd(-1) {
    LOG2("%s: construct buffer with memory:%d, size:%d, index:%d",  __func__, memory, size, index);

    mU = new camera_buffer_t;
    CLEAR(*mU);
    mBufferflag = BUFFER_FLAG_INTERNAL;
    mU->flags = BUFFER_FLAG_INTERNAL;
    mU->sequence = -1;

    mV.SetMemory(memory);
    mV.SetIndex(index);
    mV.SetType(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    mV.SetLength(size, 0);
    mV.SetFlags(mV.Flags() | V4L2_BUF_FLAG_NO_CACHE_INVALIDATE | V4L2_BUF_FLAG_NO_CACHE_CLEAN);
}

CameraBuffer::~CameraBuffer() {
    freeMemory();

    if (mBufferflag & BUFFER_FLAG_INTERNAL) {
        delete mU;
    }
}

// Helper function to construct a internal CameraBuffer with memory type
std::shared_ptr<CameraBuffer> CameraBuffer::create(int memory, unsigned int size, int index,
                                                   int srcFmt, int srcWidth, int srcHeight) {
    LOG1("%s, width:%d, height:%d, memory type:%d, size:%d, format:%d, index:%d", __func__,
         srcWidth, srcHeight, memory, size, srcFmt, index);
    std::shared_ptr<CameraBuffer> camBuffer =
        std::make_shared<CameraBuffer>(memory, size, index);
    CheckAndLogError(!camBuffer, nullptr, "@%s: fail to alloc CameraBuffer", __func__);

    camBuffer->setUserBufferInfo(srcFmt, srcWidth, srcHeight);
    int ret = camBuffer->allocateMemory();
    CheckAndLogError(ret != OK, nullptr, "Allocate memory failed ret %d", ret);

    return camBuffer;
}

// Helper function to construct a CameraBuffer from camera_buffer_t pointer
std::shared_ptr<CameraBuffer> CameraBuffer::create(int memory, int size, int index,
                                                   camera_buffer_t* ubuffer) {
    CheckAndLogError(!ubuffer, nullptr, "ubuffer is nullptr");
    LOG1("%s, memory type:%d, size:%d, index:%d", __func__, memory, size, index);

    std::shared_ptr<CameraBuffer> camBuffer =
        std::make_shared<CameraBuffer>(memory, size, index);
    CheckAndLogError(!camBuffer, nullptr, "@%s: fail to alloc CameraBuffer", __func__);

    camBuffer->setUserBufferInfo(ubuffer);
    camBuffer->updateFlags();

    return camBuffer;
}

// Helper function to construct a CameraBuffer from void* pointer
std::shared_ptr<CameraBuffer> CameraBuffer::create(int srcWidth, int srcHeight, int size,
                                                   int srcFmt, int index, void* buffer) {
    CheckAndLogError(!buffer, nullptr, "buffer is nullptr");
    LOG1("%s, width:%d, height:%d, size:%d, format:%d, index:%d", __func__, srcWidth, srcHeight,
         size, srcFmt, index);

    std::shared_ptr<CameraBuffer> camBuffer =
        std::make_shared<CameraBuffer>(V4L2_MEMORY_USERPTR, size, index);
    CheckAndLogError(!camBuffer, nullptr, "Fail to alloc CameraBuffer");

    camBuffer->setUserBufferInfo(srcFmt, srcWidth, srcHeight, buffer);

    return camBuffer;
}

#ifdef CAL_BUILD
int CameraBuffer::allocateGbmBuffer() {
    int usage =
        GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_HW_CAMERA_MASK;
    buffer_handle_t handle =
        BufferAllocator::allocateGbmBuffer(mU->s.width, mU->s.height, mU->s.format, usage);
    CheckAndLogError(!handle, UNKNOWN_ERROR, "Allocate handle failed!");
    mHandle = handle;
    mU->dmafd = handle->data[0];

    void* addr = BufferAllocator::lock(mU->s.width, mU->s.height, mU->s.format, handle);
    if (!addr) {
        icamera::BufferAllocator::freeGbmBuffer(mHandle);
        LOGE("@%s: Failed to lock buffer, handle:%p", __func__, handle);
        return UNKNOWN_ERROR;
    }
    mU->addr = addr;

    mU->s.size = icamera::BufferAllocator::getSize(mHandle);
    mU->s.stride = icamera::BufferAllocator::getStride(mHandle);
    mV.SetLength(mU->s.size, 0);

    return OK;
}

void CameraBuffer::freeGbmBuffer() {
    if (mHandle) {
        if (mU->addr) {
            icamera::BufferAllocator::unlock(mHandle);
            mU->addr = nullptr;
        }
        icamera::BufferAllocator::freeGbmBuffer(mHandle);
        mU->dmafd = -1;
        mHandle = nullptr;
    }
}
#endif

// Internal frame Buffer
void CameraBuffer::setUserBufferInfo(int format, int width, int height) {
    mU->s.width = width;
    mU->s.height = height;
    mU->s.format = format;
    if (format != -1) {
        mU->s.stride = CameraUtils::getStride(format, width);
    }
}

// Memory type is V4L2_MEMORY_USERPTR which can use it
void CameraBuffer::setUserBufferInfo(int format, int width, int height, void* usrPtr) {
    setUserBufferInfo(format, width, height);
    mV.SetUserptr(reinterpret_cast<uintptr_t>(usrPtr), 0);
}

// Called when a buffer is from the application
void CameraBuffer::setUserBufferInfo(camera_buffer_t* ubuffer) {
    CheckAndLogError(ubuffer == nullptr, VOID_VALUE, "%s: ubuffer is nullptr", __func__);

    if (mU->flags & BUFFER_FLAG_INTERNAL) delete mU;
    mU = ubuffer;
    mBufferflag = mU->flags;

    mV.SetSequence(0);
    struct timeval ts = {0};
    mV.SetTimestamp(ts);

    // update the v4l2 buffer memory with user info
    switch (ubuffer->s.memType) {
        case V4L2_MEMORY_USERPTR:
            mV.SetUserptr(reinterpret_cast<uintptr_t>(ubuffer->addr), 0);
            break;
        case V4L2_MEMORY_DMABUF:
#ifdef CAL_BUILD
            // For user buffer, it's memory type is V4L2_MEMORY_DMABUF,
            // and the buffer handle is stored in reserved of ubuffer
            mHandle =
                reinterpret_cast<buffer_handle_t>(static_cast<uintptr_t>(ubuffer->reserved));
            break;
#endif
        case V4L2_MEMORY_MMAP:
            /* do nothing */
            break;
        default:
            LOGE("iomode %d is not supported yet.", mV.Memory());
            break;
    }

    if (mU->s.streamType == CAMERA_STREAM_INPUT || ubuffer->sequence >= 0) {
        // update timestamp if raw buffer is selected by user and timestamp is set
        if (ubuffer->timestamp > 0) {
            struct timeval timestamp = {0};
            timestamp.tv_sec = ubuffer->timestamp / 1000000000LL;
            timestamp.tv_usec = (ubuffer->timestamp - timestamp.tv_sec * 1000000000LL) / 1000LL;
            mV.SetTimestamp(timestamp);
        }
        mV.SetSequence(ubuffer->sequence);
        LOG2("%s, input buffer sequence %ld, timestamp %ld", __func__, ubuffer->sequence,
             ubuffer->timestamp);
    }
}

void CameraBuffer::updateV4l2Buffer(const v4l2_buffer_t& v4l2buf) {
    mV.SetField(v4l2buf.field);
    mV.SetTimestamp(v4l2buf.timestamp);
    mV.SetSequence(v4l2buf.sequence);
    mV.SetRequestFd(v4l2buf.request_fd);
}

/*export mmap buffer as dma_buf fd stored in mV and mU*/
int CameraBuffer::exportMmapDmabuf(V4L2VideoNode* vDevice) {
    std::vector<int> fds;

    int ret = vDevice->ExportFrame(mV.Index(), &fds);
    CheckAndLogError(ret != OK || fds.size() != 1, -1,
                     "exportMmapDmabuf failed, ret %d, fds size:%zu", ret, fds.size());
    mU->dmafd = fds[0];

    return OK;
}

int CameraBuffer::allocateMemory(V4L2VideoNode* vDevice) {
    int ret = BAD_VALUE;
    switch (mV.Memory()) {
        case V4L2_MEMORY_USERPTR:
            ret = allocateUserPtr();
            break;
        case V4L2_MEMORY_MMAP:
            exportMmapDmabuf(vDevice);
            ret = allocateMmap(vDevice);
            break;
        case V4L2_MEMORY_DMABUF:
#ifdef CAL_BUILD
            ret = allocateGbmBuffer();
#endif
            break;
        default:
            LOGE("memory type %d is incorrect for allocateMemory.", mV.Memory());
            break;
    }

    if (ret == OK) mAllocatedMemory = true;

    return ret;
}

void CameraBuffer::freeMemory() {
    if (!mAllocatedMemory) return;

    switch (mV.Memory()) {
        case V4L2_MEMORY_USERPTR:
            freeUserPtr();
            break;
        case V4L2_MEMORY_MMAP:
            freeMmap();
            break;
        case V4L2_MEMORY_DMABUF:
#ifdef CAL_BUILD
            freeGbmBuffer();
#endif
            break;
        default:
            LOGE("Free camera buffer failed, due to memory %d type is not implemented yet.",
                 mV.Memory());
    }
}

int CameraBuffer::allocateUserPtr() {
    void* buffer = nullptr;
    int ret = posix_memalign(&buffer, getpagesize(), mV.Length(0));
    CheckAndLogError(ret != 0, -1, "%s, posix_memalign fails, ret:%d", __func__, ret);
    mV.SetUserptr(reinterpret_cast<uintptr_t>(buffer), 0);
    return OK;
}

void CameraBuffer::freeUserPtr() {
    void* ptr = reinterpret_cast<void*>(mV.Userptr(0));
    ::free(ptr);
    mV.SetUserptr(reinterpret_cast<uintptr_t>(nullptr), 0);
}

int CameraBuffer::allocateMmap(V4L2VideoNode* dev) {
    std::vector<void*> addrs;
    int ret = dev->MapMemory(mV.Index(), PROT_READ | PROT_WRITE, MAP_SHARED, &addrs);
    CheckAndLogError(ret != OK || addrs.size() != 1, -1,
                     "allocateMmap failed, ret %d, addr size:%zu", ret, addrs.size());
    mU->addr = addrs[0];

    return OK;
}

void CameraBuffer::freeMmap() {
    if (mU->dmafd != -1) {
        ::close(mU->dmafd);
        mU->dmafd = -1;
    }
    if (mU->addr) {
        int ret = ::munmap(mU->addr, mV.Length(0));
        CheckAndLogError(ret != 0, VOID_VALUE, "failed to munmap buffer");
    }
}

void* CameraBuffer::mapDmaBufferAddr(int fd, unsigned int bufferSize) {
    CheckAndLogError(fd < 0 || !bufferSize, nullptr, "%s, fd:0x%x, bufferSize:%u", __func__, fd,
                     bufferSize);
    return ::mmap(nullptr, bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
}

void CameraBuffer::unmapDmaBufferAddr(void* addr, unsigned int bufferSize) {
    CheckAndLogError(addr == nullptr || !bufferSize, VOID_VALUE, "%s, addr:%p, bufferSize:%u",
                     __func__, addr, bufferSize);
    munmap(addr, bufferSize);
}

bool CameraBuffer::lock() {
#ifdef CAL_BUILD
    if (getMemory() == V4L2_MEMORY_DMABUF && mU && !mU->addr) {
        mU->addr = BufferAllocator::lock(mU->s.width, mU->s.height, mU->s.format, mHandle);
        mU->s.size = icamera::BufferAllocator::getSize(mHandle);
        mU->s.stride = icamera::BufferAllocator::getStride(mHandle);
        mV.SetLength(mU->s.size, 0);
    }

    return (mU->addr != nullptr);
#else
    return true;
#endif
}

void CameraBuffer::unlock() {
#ifdef CAL_BUILD
    if (getMemory() == V4L2_MEMORY_DMABUF && mU && mU->addr) {
        BufferAllocator::unlock(mHandle);
        mU->addr = nullptr;
    }
#endif
}

void CameraBuffer::updateUserBuffer(void) {
    mU->timestamp = TIMEVAL2NSECS(getTimestamp());
    mU->s.field = getField();

    // Use valid setting sequence to align shutter/parameter with buffer
    mU->sequence = (mSettingSequence < 0) ? getSequence() : mSettingSequence;
}

void CameraBuffer::updateFlags(void) {
    int flag = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE | V4L2_BUF_FLAG_NO_CACHE_CLEAN;
    bool set = true;

    // clear the flags if the buffers is accessed by the SW
    if ((mU->flags & BUFFER_FLAG_SW_READ) || (mU->flags & BUFFER_FLAG_SW_WRITE)) {
        set = false;
    }

    mV.SetFlags(set ? (mV.Flags() | flag) : (mV.Flags() & (~flag)));
}

bool CameraBuffer::isFlagsSet(int flag) {
    return ((mU->flags & flag) ? true : false);
}

int CameraBuffer::getFd() {
    switch (mV.Memory()) {
        case V4L2_MEMORY_USERPTR:
            return mV.Fd(0);
        case V4L2_MEMORY_DMABUF:
        case V4L2_MEMORY_MMAP:
            return mU->dmafd;
        default:
            LOGE("iomode %d is not supported yet.", mV.Memory());
            return -1;
    }
}

void* CameraBuffer::getBufferAddr() {
    switch (mV.Memory()) {
        case V4L2_MEMORY_USERPTR:
            return reinterpret_cast<void*>(mV.Userptr(0));
        case V4L2_MEMORY_DMABUF:
        case V4L2_MEMORY_MMAP:
            return mU->addr;
        default:
            LOGE("%s: Not supported memory type %u", __func__, mV.Memory());
            return nullptr;
    }
}

}  // namespace icamera
