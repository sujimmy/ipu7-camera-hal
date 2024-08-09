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

#ifdef CAL_BUILD
#include <cros-camera/v4l2_device.h>
#else
#include <v4l2_device.h>
#endif
#include <vector>

#include "CameraEvent.h"
#include "iutils/Thread.h"
#include "iutils/Utils.h"

namespace libcamera {
using namespace icamera;

class HwPrivacyControl : public icamera::Thread {
 public:
    HwPrivacyControl(int cameraId);
    ~HwPrivacyControl();
    int start();
    int stop();
    bool init();
    bool getPrivacyStatus();

 private:
    virtual bool threadLoop() override;

 private:
    int mCameraId;
    bool mInitialized;

    icamera::V4L2Subdevice* mHwPriModeSubDev;
    bool mThreadRunning;

    std::mutex sLock;
    bool mState;

    DISALLOW_COPY_AND_ASSIGN(HwPrivacyControl);
};

}  // namespace libcamera
