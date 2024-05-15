/*
 * Copyright (C) 2022 Intel Corporation
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

#include "BufferAllocator.h"

#include <unistd.h>

#include "iutils/CameraLog.h"
#include "iutils/Errors.h"
#include "iutils/Utils.h"

namespace icamera {

namespace BufferAllocator {

buffer_handle_t allocateGbmBuffer(int width, int height, int format, int gbmUsage) {
    LOG2("@%s, width:%d, height:%d, format:0x%x, usage:0x%x", __func__, width, height, format,
         gbmUsage);

    int gbmFmt = V4l2FormatToHALFormat(format);
    int w = width;
    int h = height;
    if (gbmFmt == HAL_PIXEL_FORMAT_BLOB) {
        w = CameraUtils::getFrameSize(V4L2_PIX_FMT_NV12, width, height, false, false, false);
        h = 1;
    }

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(bufManager == nullptr, nullptr, "Get CameraBufferManager instance failed!");

    buffer_handle_t handle;
    uint32_t stride = 0;
    int ret = bufManager->Allocate(w, h, gbmFmt, gbmUsage, &handle, &stride);
    CheckAndLogError(ret != 0, nullptr, "Allocate handle failed! ret:%d", ret);

    return handle;
}

void freeGbmBuffer(buffer_handle_t handle) {
    CheckAndLogError(!handle, VOID_VALUE, "@%s, handle is invalid", __func__);
    LOG2("@%s, free GBM buf:%p", __func__, handle);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(bufManager == nullptr, VOID_VALUE, "Get CameraBufferManager instance failed!");

    bufManager->Free(handle);
}

void* lock(int width, int height, int format, buffer_handle_t handle) {
    CheckAndLogError(!handle, nullptr, "@%s, handle is invalid", __func__);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(bufManager == nullptr, nullptr, "Get CameraBufferManager instance failed!");

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

int unlock(buffer_handle_t handle) {
    CheckAndLogError(!handle, UNKNOWN_ERROR, "@%s, handle is invalid", __func__);
    LOG2("@%s, handle:%p", __func__, handle);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(bufManager == nullptr, UNKNOWN_ERROR,
                     "Get CameraBufferManager instance failed!");

    int ret = bufManager->Unlock(handle);
    CheckAndLogError(ret, UNKNOWN_ERROR, "@%s: call Unlock fail, handle:%p, ret:%d", __func__,
                     handle, ret);

    return OK;
}

int getSize(buffer_handle_t handle) {
    CheckAndLogError(!handle, 0, "@%s, handle is invalid", __func__);
    LOG2("@%s, handle:%p", __func__, handle);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(bufManager == nullptr, 0, "Get CameraBufferManager instance failed!");

    int len = 0;
    uint32_t planeNum = bufManager->GetNumPlanes(handle);
    for (uint32_t i = 0; i < planeNum; i++) {
        len += bufManager->GetPlaneSize(handle, i);
    }
    LOG2("@%s, planeNum:%d, size:%d", __func__, planeNum, len);

    return len;
}

int getStride(buffer_handle_t handle) {
    CheckAndLogError(!handle, 0, "@%s, handle is invalid", __func__);
    LOG2("@%s, handle:%p", __func__, handle);

    cros::CameraBufferManager* bufManager = cros::CameraBufferManager::GetInstance();
    CheckAndLogError(bufManager == nullptr, 0, "Get CameraBufferManager instance failed!");

    return bufManager->GetPlaneStride(handle, 0);
}

int V4l2FormatToHALFormat(int v4l2Format) {
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

}  // namespace BufferAllocator
}  // namespace icamera
