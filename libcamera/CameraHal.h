/*
 * Copyright (C) 2024 Intel Corporation.
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

#include <libcamera/request.h>
#include <libcamera/formats.h>

#include "CameraDevice.h"
#include "ParamDataType.h"
#include "Camera3AMetadata.h"

namespace libcamera {

class CameraHal {
 public:
    static void setup();
    static void tearDown();

    CameraHal(int32_t cameraId);
    virtual ~CameraHal();
    virtual bool validate(const icamera::stream_t& stream);
    virtual int configure(icamera::stream_config_t *streamList);
    virtual void callbackRegister(const icamera::camera_callback_ops_t* callback);
    virtual int start();
    virtual int stop();
    virtual void processControls(Request* request, bool isStill = false);
    virtual void updateMetadataResult(int64_t sequence, const ControlList& controls,
                                      ControlList& metadata);
    virtual int qbuf(icamera::camera_buffer_t** ubuffer, int bufferNum);
    virtual int dqbuf(int streamId, icamera::camera_buffer_t** ubuffer);

    int32_t getCameraId() const { return mCameraId; }
    std::vector<SizeRange> availableStreamSizes(const PixelFormat &pixelFormat) const;

 protected:
    enum CameraHalStatus {
        HAL_UNKNOWN = 0,
        HAL_INIT
    };

    int32_t mCameraId;
    CameraHalStatus mHalStatus;

 private:
    icamera::CameraDevice* mCameraDevice;
    Camera3AMetadata* mCamera3AMetadata;
};

} /* namespace libcamera */
