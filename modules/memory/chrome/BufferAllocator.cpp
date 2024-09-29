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

#define LOG_TAG BufferAllocator

#include "modules/memory/BufferAllocator.h"

#include <unistd.h>
#include <hardware/gralloc.h>

#include "iutils/CameraLog.h"
#include "iutils/Errors.h"
#include "iutils/Utils.h"

namespace icamera {

int BufferAllocator::allocateDmaBuffer(camera_buffer_t* ubuffer) {
    if (!ubuffer) return BAD_VALUE;

    LOG2("@%s, width:%d, height:%d, format:0x%x", __func__, ubuffer->s.width, ubuffer->s.height,
         ubuffer->s.format);
    int usage =
        GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_HW_CAMERA_MASK;
    int gbmFmt = V4l2FormatToHALFormat(ubuffer->s.format);
    int w = ubuffer->s.width;
    int h = ubuffer->s.height;
    if (gbmFmt == HAL_PIXEL_FORMAT_BLOB) {
        w = CameraUtils::getFrameSize(V4L2_PIX_FMT_NV12, w, h, false, false, false);
        h = 1;
    }

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(!bufManager, NO_INIT, "Get CameraBufferManager instance failed!");

    buffer_handle_t handle;
    uint32_t stride = 0;
    int ret = bufManager->Allocate(w, h, gbmFmt, usage, &handle, &stride);
    CheckAndLogError(ret != 0, NO_MEMORY, "Allocate handle failed! ret:%d", ret);

    mHandle = handle;
    ubuffer->dmafd = handle->data[0];

    void* addr = lock(w, h, ubuffer->s.format, handle);
    if (!addr) {
        freeDmaBuffer();
        LOGE("@%s: Failed to lock buffer, handle:%p", __func__, handle);
        return UNKNOWN_ERROR;
    }

    mUsrAddr = addr;
    ubuffer->addr = addr;
    ubuffer->privateHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));

    uint32_t planeNum = bufManager->GetNumPlanes(handle);
    for (uint32_t i = 0; i < planeNum; i++) {
        ubuffer->s.size += bufManager->GetPlaneSize(handle, i);
    }
    LOG2("@%s, planeNum:%d, size:%d", __func__, planeNum, ubuffer->s.size);

    ubuffer->s.stride = bufManager->GetPlaneStride(handle, 0);

    return OK;
}

void BufferAllocator::freeDmaBuffer() {
    CheckAndLogError(!mHandle, VOID_VALUE, "@%s, handle is invalid", __func__);
    LOG2("@%s, free GBM buf:%p", __func__, mHandle);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(!bufManager, VOID_VALUE, "Get CameraBufferManager instance failed!");

    if (mUsrAddr) {
        int ret = bufManager->Unlock(mHandle);
        if (ret) {
            LOGE("Unlock fail, handle:%p, ret:%d", mHandle, ret);
        }
    }

    bufManager->Free(mHandle);

    mHandle = nullptr;
}

void* BufferAllocator::lock(int width, int height, int format, buffer_handle_t handle) {
    CheckAndLogError(!handle, nullptr, "@%s, handle is invalid", __func__);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(!bufManager, nullptr, "Get CameraBufferManager instance failed!");

    void* dataPtr = nullptr;
    int ret = 0;
    uint32_t planeNum = bufManager->GetNumPlanes(handle);
    if (planeNum == 1) {
        int gbmFmt = V4l2FormatToHALFormat(format);
        int stride = bufManager->GetPlaneStride(handle, 0);
        ret = (gbmFmt == HAL_PIXEL_FORMAT_BLOB)
                  ? bufManager->Lock(handle, 0, 0, 0, stride, 1, &dataPtr)
                  : bufManager->Lock(handle, 0, 0, 0, width, height, &dataPtr);
    } else if (planeNum > 1) {
        struct android_ycbcr ycbrData;
        ret = bufManager->LockYCbCr(handle, 0, 0, 0, width, height, &ycbrData);
        dataPtr = ycbrData.y;
    } else {
        LOGE("ERROR @%s: planeNum is 0", __func__);
        return nullptr;
    }
    CheckAndLogError(ret, nullptr, "@%s: Failed to lock buffer, mHandle:%p planeNum: %d", __func__,
                     handle, planeNum);

    LOG2("@%s, call Lock success, handle:%p, planeNum:%d, dataPtr:%p", __func__, handle, planeNum,
         dataPtr);

    return dataPtr;
}

int BufferAllocator::V4l2FormatToHALFormat(int v4l2Format) {
    int format = HAL_PIXEL_FORMAT_YCBCR_420_888;
    switch (v4l2Format) {
        case V4L2_PIX_FMT_P010:
            format = HAL_PIXEL_FORMAT_YCBCR_P010;
            break;
        case V4L2_PIX_FMT_JPEG:
            format = HAL_PIXEL_FORMAT_BLOB;
            break;
        default:
            break;
    }

    return format;
}

}  // namespace icamera
