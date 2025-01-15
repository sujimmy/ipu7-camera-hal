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

#include <libcamera/base/mutex.h>
#include <libcamera/base/thread.h>

#include <vector>

#include <v4l2_device.h>
#include "CameraEvent.h"
#include "iutils/Utils.h"

namespace libcamera {

class HwPrivacyControl : Thread {
 public:
    HwPrivacyControl(int cameraId);
    ~HwPrivacyControl();
    int start();
    int stop();
    bool init();
    bool getPrivacyStatus();

 protected:
    virtual void run() override;

 private:
    int mCameraId;
    bool mInitialized;

    icamera::V4L2Subdevice* mHwPriModeSubDev;
    bool mThreadRunning;

    Mutex mLock;
    bool mState;

    DISALLOW_COPY_AND_ASSIGN(HwPrivacyControl);
};

}  // namespace libcamera
