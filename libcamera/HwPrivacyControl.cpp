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

#include "HwPrivacyControl.h"

#include <libcamera/base/log.h>
#include <poll.h>

#include <string>

#include "PlatformData.h"
#include "V4l2DeviceFactory.h"

namespace libcamera {
LOG_DECLARE_CATEGORY(IPU7Privacy)

#define SUBDEV_NAME "/dev/v4l-subdev0"

HwPrivacyControl::HwPrivacyControl(int cameraId)
        : mCameraId(cameraId),
          mInitialized(false),
          mHwPriModeSubDev(nullptr),
          mThreadRunning(false),
          mState(false) {}

HwPrivacyControl::~HwPrivacyControl() {
    LOG(IPU7Privacy, Debug) << __func__;
}

bool HwPrivacyControl::init() {
    mHwPriModeSubDev = icamera::V4l2DeviceFactory::getSubDev(mCameraId, SUBDEV_NAME);
#ifdef CAL_BUILD
    int ret = mHwPriModeSubDev->SubscribeEvent(V4L2_EVENT_CTRL);
#else
    int ret = mHwPriModeSubDev->SubscribeEvent(V4L2_EVENT_CTRL, V4L2_CID_PRIVACY);
#endif

    if (ret != icamera::OK) {
        LOG(IPU7Privacy, Info)
            << "Failed to subscribe sync event V4L2_EVENT_CTRL, Privacy Mode not supported";
        mHwPriModeSubDev = nullptr;
        return false;
    }
    int privacy = -1;
    int status = mHwPriModeSubDev->GetControl(V4L2_CID_PRIVACY, &privacy);
    if (status != icamera::OK) {
        LOG(IPU7Privacy, Error) << "Couldn't get V4L2_CID_PRIVACY, status: "
                                << std::to_string(status);
        mHwPriModeSubDev = nullptr;
        return false;
    }

    mState = (privacy == 1);
    mInitialized = true;

    return true;
}

int HwPrivacyControl::start() {
    LOG(IPU7Privacy, Debug) << __func__;

    if (!mInitialized) return icamera::NO_INIT;

    mThreadRunning = true;
    Thread::start();

    return icamera::OK;
}

int HwPrivacyControl::stop() {
    LOG(IPU7Privacy, Debug) << __func__;
    if (!mInitialized) return icamera::NO_INIT;

    mThreadRunning = false;
    wait();

    if (mHwPriModeSubDev) {
        mHwPriModeSubDev->UnsubscribeEvent(V4L2_EVENT_CTRL);
        icamera::V4l2DeviceFactory::releaseSubDev(mCameraId, SUBDEV_NAME);
        mHwPriModeSubDev = nullptr;
    }

    return icamera::OK;
}

bool HwPrivacyControl::getPrivacyStatus() {
    MutexLocker locker(mLock);
    return mState;
}

void HwPrivacyControl::run() {
    int ret = 0;
    const int pollTimeoutMs = 100;

    std::vector<icamera::V4L2Device*> pollDevs;
    pollDevs.push_back(mHwPriModeSubDev);
    icamera::V4L2DevicePoller poller{pollDevs, -1};

    std::vector<icamera::V4L2Device*> readyDevices;

    while (true) {
        if (!mThreadRunning) return;

        ret = poller.Poll(pollTimeoutMs, POLLPRI | POLLIN | POLLOUT | POLLERR, &readyDevices);
        if (ret == 0) continue;  // time out

        if (ret < 0) {
            LOG(IPU7Privacy, Error) << "Poll error, ret: " << std::to_string(ret);
            return;
        }

        // Get privacy mode state
        struct v4l2_event event;
        CLEAR(event);
        ret = mHwPriModeSubDev->DequeueEvent(&event);
        if (ret == icamera::OK) {
            MutexLocker locker(mLock);
            mState = (event.u.ctrl.value == 1);
        }
    }
}
}  // namespace libcamera
