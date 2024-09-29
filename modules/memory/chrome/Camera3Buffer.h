/*
 * Copyright (C) 2019-2024 Intel Corporation
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

#include <cros-camera/camera_buffer_manager.h>
#include <hardware/camera3.h>

#include <memory>

#include "ParamDataType.h"
#include "iutils/Errors.h"

namespace camera3 {

// Forward declaration to  avoid extra include
class Camera3Stream;

enum Camera3BufferType {
    BUF_TYPE_HANDLE,
    BUF_TYPE_MALLOC,
    BUF_TYPE_MMAP,
};

/**
 * \class Camera3Buffer
 *
 * This class is the buffer abstraction in the HAL. It can store buffers
 * provided by the framework or buffers allocated by the HAL.
 * Allocation in the HAL can be done via gralloc, malloc or mmap
 * in case of mmap the memory cannot be freed
 */
class Camera3Buffer {
 public:
    /**
     * default constructor
     * Used for buffers coming from the framework. The wrapper is initialized
     * using the method init
     */
    Camera3Buffer();

    /**
     * no need to delete a buffer since it is RefBase'd. Buffer will be deleted
     * when no reference to it exist.
     */
    ~Camera3Buffer();

    /**
     * constructor for the HAL-allocated buffer
     * These are used via the utility methods in the MemoryUtils namespace
     */
    Camera3Buffer(int w, int h, int stride, int v4l2fmt, void* usrPtr, int cameraId,
                  int dataSizeOverride = 0);

    /**
     * initialization for the wrapper around the framework buffers
     */
    icamera::status_t init(const camera3_stream_buffer* aBuffer, int cameraId);

    /**
     * initialization for the fake framework buffer (allocated by the HAL)
     */
    icamera::status_t init(const camera3_stream_t* stream, buffer_handle_t buffer, int cameraId);
    /**
     * deinitialization for the wrapper around the framework buffers
     */
    icamera::status_t deinit();

    /**
     * dump functions
     */
    void dumpImage(int frameNumber, const int type, int format);
    void dumpImage(const void* data, int frameNumber, const int size, int width, int height,
                   int format) const;

    /**
     * lock the buffer and get buffer addr from handle
     */
    icamera::status_t lock();
    icamera::status_t unlock();

    /**
     * Fence
     */
    icamera::status_t waitOnAcquireFence();
    icamera::status_t getFence(camera3_stream_buffer* buf);

    /**
     * APIs for getting private member
     */
    int width() { return mHalBuffer.s.width; }
    int height() { return mHalBuffer.s.height; }
    int stride() { return mHalBuffer.s.stride; }
    unsigned int size() { return mHalBuffer.s.size; }
    int v4l2Fmt() { return mHalBuffer.s.format; }
    void* data() { return mHalBuffer.addr; }
    void setTimeStamp(uint64_t timestamp) { mHalBuffer.timestamp = timestamp; }
    int format() { return mFormat; }
    int usage() { return mUsage; }
    buffer_handle_t* getBufferHandle() { return mHandlePtr; }
    bool isLocked() const { return mLocked; }
    int status() { return mUserBuffer.status; }
    icamera::camera_buffer_t getHalBuffer() { return mHalBuffer; }

 private:
    icamera::status_t registerBuffer();
    icamera::status_t deregisterBuffer();
    icamera::status_t lockBuf();

 private:
    /*!< Original structure passed by request */
    camera3_stream_buffer_t mUserBuffer = {0, 0, 0, -1, -1};
    int mFormat;  /*!<  HAL PIXEL fmt */
    bool mInit;   /*!< Boolean to check the integrity of the
                       buffer when it is created*/
    bool mLocked; /*!< Use to track the lock status */
    int mUsage;
    Camera3BufferType mType;

    buffer_handle_t mHandle = {};
    buffer_handle_t* mHandlePtr;
    int mCameraId;
    icamera::camera_buffer_t mHalBuffer;

 private:
    bool mRegistered; /*!< Use to track the buffer register status */
    cros::CameraBufferManager* mGbmBufferManager;
};

namespace MemoryUtils {

std::shared_ptr<Camera3Buffer> allocateHeapBuffer(int w, int h, int stride, int v4l2Fmt,
                                                  int cameraId, int dataSizeOverride = 0);

std::shared_ptr<Camera3Buffer> allocateHandleBuffer(int w, int h, int v4l2Fmt, int usage,
                                                    int cameraId);
}  // namespace MemoryUtils

}  // namespace camera3
