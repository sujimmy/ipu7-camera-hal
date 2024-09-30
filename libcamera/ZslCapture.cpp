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

#include "ZslCapture.h"

#include <libcamera/base/log.h>
#include <libcamera/control_ids.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(IPU7)

ZslCapture::ZslCapture() {
    LOG(IPU7, Debug) << "Construct " << __func__;
}

ZslCapture::~ZslCapture() {
    LOG(IPU7, Debug) << "Deconstruct " << __func__;
}

bool ZslCapture::isManualExposureSettings(const ControlList& controls) {
    uint8_t controlMode = controls.get(controls::Mode3A).value_or(controls::Mode3AAuto);
    bool aeEnabled = controls.get(controls::AeEnable).value_or(true);
    auto aeMode = controls.get(controls::AeMode).value_or(controls::AeModeOn);

    bool manualExpo = !aeEnabled || controlMode == controls::Mode3AOff ||
                      aeMode == controls::AeModeOff;

    bool aeLocked = controls.get(controls::AeLocked).value_or(false);

    return manualExpo || aeLocked;
}

void ZslCapture::registerFrameInfo(unsigned int frameNumber, const ControlList& controls) {
    ZslInfo info;

    /* Check if manual exposure or ae locked */
    info.isManualExposure = isManualExposureSettings(controls);

    MutexLocker locker(mMutex);
    mZslInfoMap[frameNumber] = info;
}

void ZslCapture::updateTimeStamp(unsigned int frameNumber, uint64_t timestamp) {
    MutexLocker locker(mMutex);

    if (mZslInfoMap.find(frameNumber) != mZslInfoMap.end()) {
        mZslInfoMap[frameNumber].timestamp = timestamp;
    }
}

void ZslCapture::updateSequence(unsigned int frameNumber, int64_t sequence) {
    MutexLocker locker(mMutex);

    if (mZslInfoMap.find(frameNumber) != mZslInfoMap.end()) {
        mZslInfoMap[frameNumber].sequence = sequence;
    }
}

void ZslCapture::update3AStatus(unsigned int frameNumber, const ControlList& metadata) {
    MutexLocker locker(mMutex);

    if (mZslInfoMap.find(frameNumber) != mZslInfoMap.end()) {
        /* Reset it to false */
        mZslInfoMap[frameNumber].isAeStable = false;
        mZslInfoMap[frameNumber].isAfStable = false;
        mZslInfoMap[frameNumber].isAwbStable = false;

        uint8_t aeState =
            metadata.get(controls::draft::AeState).value_or(controls::draft::AeStateInactive);
        if (aeState == controls::draft::AeStateConverged) {
            mZslInfoMap[frameNumber].isAeStable = true;
        }

        uint8_t afState = metadata.get(controls::AfState).value_or(controls::AfTriggerIdle);
        if (afState == controls::AfStateFocused) {
            mZslInfoMap[frameNumber].isAfStable = true;
        }

        uint8_t awbState =
            metadata.get(controls::draft::AwbState).value_or(controls::draft::AwbStateInactive);
        if (awbState == controls::draft::AwbConverged) {
            mZslInfoMap[frameNumber].isAwbStable = true;
        }
    }

    if (mZslInfoMap.size() > kMaxZslRequest) {
        mZslInfoMap.erase(mZslInfoMap.begin());
    }
}

void ZslCapture::getZslSequenceAndTimestamp(uint64_t& timestamp, int64_t& sequence) {
    MutexLocker locker(mMutex);

    timestamp = 0;
    sequence = -1;
    // Todo handle later

    LOG(IPU7, Debug) << "ZSL timestamp " << timestamp << " sequence " << sequence;
}

} /* namespace libcamera */
