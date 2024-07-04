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
#define LOG_TAG IntelAWBStateMachine

#include "IntelAWBStateMachine.h"

#include <libcamera/base/utils.h>

#include "CameraLog.h"
#include "Utils.h"
#include "Errors.h"

namespace libcamera {

IntelAWBStateMachine::IntelAWBStateMachine(int cameraId)
        : mCameraId(cameraId),
          mCurrentAwbMode(NULL) {
    LOG1("<id%d>%s", mCameraId, __func__);
    mCurrentAwbMode = &mAutoMode;
    CLEAR(mLastAwbControls);
    mLastAwbControls.awbMode = controls::AwbAuto;
}

IntelAWBStateMachine::~IntelAWBStateMachine() {
    LOG1("<id%d>%s", mCameraId, __func__);
}

int IntelAWBStateMachine::processState(uint8_t controlMode, uint8_t sceneMode,
                                       const AwbControls& awbControls) {
    if (controlMode == controls::Mode3AOff) {
        mCurrentAwbMode = &mOffMode;
        if (controlMode != mLastControlMode)
            LOG1("%s: Set AWB offMode: controlMode = %d, awbMode = %d", __func__, controlMode,
                 awbControls.awbMode);
    } else {
        mCurrentAwbMode = &mAutoMode;
        if (awbControls.awbMode != mLastAwbControls.awbMode)
            LOG1("%s: Set AWB autoMode: controlMode = %d, awbMode = %d", __func__, controlMode,
                 awbControls.awbMode);
    }

    mLastAwbControls = awbControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    return mCurrentAwbMode->processState(controlMode, sceneMode, awbControls);
}

int IntelAWBStateMachine::processResult(bool converged, ControlList& controls) {
    return mCurrentAwbMode->processResult(converged, controls);
}

/******************************************************************************
 * AWB MODE   -  BASE
 ******************************************************************************/
IntelAWBModeBase::IntelAWBModeBase()
        : mLastControlMode(0),
          mLastSceneMode(0),
          mCurrentAwbState(controls::draft::AwbStateInactive) {
    CLEAR(mLastAwbControls);
}

void IntelAWBModeBase::updateResult(ControlList& controls) {
    LOG2("%s: current AWB state is: %d", __func__, mCurrentAwbState);
    controls.set(controls::AwbMode, mLastAwbControls.awbMode);
    controls.set(controls::AwbLocked, static_cast<bool>(mLastAwbControls.awbLock));
    controls.set(controls::draft::AwbState, mCurrentAwbState);
}

void IntelAWBModeBase::resetState() {
    mCurrentAwbState = controls::draft::AwbStateInactive;
}

/******************************************************************************
 * AWB MODE   -  OFF
 ******************************************************************************/

IntelAWBModeOff::IntelAWBModeOff() : IntelAWBModeBase() {}

int IntelAWBModeOff::processState(uint8_t controlMode, uint8_t sceneMode,
                                  const AwbControls& awbControls) {
    LOG2("%s", __func__);

    mLastAwbControls = awbControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    if (controlMode == controls::Mode3AOff) resetState();

    return icamera::OK;
}

int IntelAWBModeOff::processResult(bool converged, ControlList& controls) {
    UNUSED(converged);

    mCurrentAwbState = controls::draft::AwbStateInactive;
    updateResult(controls);

    return icamera::OK;
}

/******************************************************************************
 * AWB MODE   -  AUTO
 ******************************************************************************/

IntelAWBModeAuto::IntelAWBModeAuto() : IntelAWBModeBase() {}

int IntelAWBModeAuto::processState(uint8_t controlMode, uint8_t sceneMode,
                                   const AwbControls& awbControls) {
    if (controlMode != mLastControlMode) {
        LOG2("%s: control mode has changed %d -> %d, reset AWB State", __func__, mLastControlMode,
             controlMode);
        resetState();
    }

    if (awbControls.awbLock) {
        mCurrentAwbState = controls::draft::AwbLocked;
    } else if (awbControls.awbMode != mLastAwbControls.awbMode ||
               (controlMode == controls::Mode3AUseSceneMode && sceneMode != mLastSceneMode)) {
        resetState();
    } else {
        switch (mCurrentAwbState) {
            case controls::draft::AwbLocked:
                mCurrentAwbState = controls::draft::AwbStateInactive;
                break;
            case controls::draft::AwbStateInactive:
            case controls::draft::AwbStateSearching:
            case controls::draft::AwbConverged:
                // do nothing
                break;
            default:
                LOGE("Invalid AWB state!, State set to INACTIVE");
                mCurrentAwbState = controls::draft::AwbStateInactive;
        }
    }
    mLastAwbControls = awbControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    return icamera::OK;
}

int IntelAWBModeAuto::processResult(bool converged, ControlList& controls) {
    int32_t awbState = mCurrentAwbState;

    switch (mCurrentAwbState) {
        case controls::draft::AwbLocked:
            // do nothing
            break;
            case controls::draft::AwbStateInactive:
            case controls::draft::AwbStateSearching:
            case controls::draft::AwbConverged:
            if (converged)
                mCurrentAwbState = controls::draft::AwbConverged;
            else
                mCurrentAwbState = controls::draft::AwbStateSearching;
            break;
        default:
            LOGE("invalid AWB state!, State set to INACTIVE");
            mCurrentAwbState = controls::draft::AwbStateInactive;
    }

    updateResult(controls);
    if (awbState != mCurrentAwbState) {
        LOG1("%s: AWB state has changed %u -> %u", __func__, awbState, mCurrentAwbState);
    }
    return icamera::OK;
}

}  // namespace libcamera
