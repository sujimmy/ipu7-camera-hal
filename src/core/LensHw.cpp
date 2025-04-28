/*
 * Copyright (C) 2016-2021 Intel Corporation
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

#define LOG_TAG LensHw

#include "LensHw.h"
#include "iutils/CameraLog.h"
#include "V4l2DeviceFactory.h"
#include "PlatformData.h"

namespace icamera {

LensHw::LensHw(int cameraId):
    mCameraId(cameraId),
    mLensSubdev(nullptr),
    mLastLensPosition(0),
    mLensMovementStartTime(0) {
}

LensHw::~LensHw() {
}

void LensHw::start() {
    std::string lensName = PlatformData::getLensName(mCameraId);
    if (lensName.empty()) {
        LOG1("<id%d>@%s No HW Lens", mCameraId, __func__);
        return;
    }

    LOG1("<id%d>@%s, lens name:%s", mCameraId, __func__, lensName.c_str());
    std::string subDevName;
    CameraUtils::getSubDeviceName(lensName.c_str(), subDevName);
    if (!subDevName.empty()) {
        mLensSubdev = V4l2DeviceFactory::getSubDev(mCameraId, subDevName);
        mLensName=lensName;
        return;
    }

    LOGW("<id%d>@%s, Failed to start lens. name:%s", mCameraId, __func__, lensName.c_str());
}

void LensHw::stop() {
    if (mLensSubdev == nullptr) {
        return;
    }

    // close the lens sub device
    std::string subDevName;
    CameraUtils::getSubDeviceName(mLensName.c_str(), subDevName);
    if (!subDevName.empty()) {
        V4l2DeviceFactory::releaseSubDev(mCameraId, subDevName);
        mLensSubdev = nullptr;
    }
}

/**
 * focus with absolute value
 */
int LensHw::setFocusPosition(int position) {
    CheckAndLogError(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    mLastLensPosition = position;

    struct timespec t = {};
    clock_gettime(CLOCK_MONOTONIC, &t);

    mLensMovementStartTime = ((long)t.tv_sec) * 1000000 + (long)t.tv_nsec / 1000;

    LOG2("@%s: %d, time %lld", __func__, position, mLensMovementStartTime);
    return mLensSubdev->SetControl(V4L2_CID_FOCUS_ABSOLUTE, position);
}

/**
 * getLatestPosition
 *
 * returns the latest position commanded to the lens actuator and when this
 * was issued.
 * This method does not query the driver.
 *
 * \param: lensPosition[OUT]: lens position last applied
 * \param: time[OUT]: time in micro seconds when the lens move command was sent.
 */
int LensHw::getLatestPosition(int& lensPosition, unsigned long long& time) {
    lensPosition = mLastLensPosition;
    time = mLensMovementStartTime;
    return OK;
}
}   // namespace icamera
