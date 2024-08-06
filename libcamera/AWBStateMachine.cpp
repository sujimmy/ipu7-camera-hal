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

#include "AWBStateMachine.h"
#include <libcamera/base/utils.h>

#include <libcamera/base/log.h>

namespace libcamera {
LOG_DECLARE_CATEGORY(IPU7MetaData)

AWBStateMachine::AWBStateMachine(int cameraId)
        : mCameraId(cameraId),
          mCurrentAwbMode(NULL) {
    LOG(IPU7MetaData, Debug) << "<id" << mCameraId << ">" << __func__;
    mCurrentAwbMode = &mAutoMode;
    CLEAR(mLastAwbControls);
    mLastAwbControls.awbMode = controls::AwbAuto;
}

AWBStateMachine::~AWBStateMachine() {
    LOG(IPU7MetaData, Debug) << "<id " << mCameraId << ">" << __func__;
}

int AWBStateMachine::processState(uint8_t controlMode, uint8_t sceneMode,
                                       const AwbControls& awbControls) {
    if (controlMode == controls::Mode3AOff) {
        mCurrentAwbMode = &mOffMode;
        if (controlMode != mLastControlMode)
            LOG(IPU7MetaData, Debug) << __func__ << ": Set AWB offMode: controlMode = "
                                     << controlMode << ", awbMode = " << awbControls.awbMode;
    } else {
        mCurrentAwbMode = &mAutoMode;
        if (awbControls.awbMode != mLastAwbControls.awbMode)
            LOG(IPU7MetaData, Debug) << __func__ << ": Set AWB autoMode: controlMode = "
                                     << controlMode << ", awbMode = " << awbControls.awbMode;
    }

    mLastAwbControls = awbControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    return mCurrentAwbMode->processState(controlMode, sceneMode, awbControls);
}

int AWBStateMachine::processResult(bool converged, ControlList& controls) {
    return mCurrentAwbMode->processResult(converged, controls);
}

/******************************************************************************
 * AWB MODE   -  BASE
 ******************************************************************************/
AWBModeBase::AWBModeBase()
        : mLastControlMode(0),
          mLastSceneMode(0),
          mCurrentAwbState(controls::draft::AwbStateInactive) {
    CLEAR(mLastAwbControls);
}

void AWBModeBase::updateResult(ControlList& controls) {
    LOG(IPU7MetaData, Debug) << __func__ << ": current AWB state is: " << mCurrentAwbState;
    controls.set(controls::AwbMode, mLastAwbControls.awbMode);
    controls.set(controls::AwbLocked, static_cast<bool>(mLastAwbControls.awbLock));
    controls.set(controls::draft::AwbState, mCurrentAwbState);
}

void AWBModeBase::resetState() {
    mCurrentAwbState = controls::draft::AwbStateInactive;
}

/******************************************************************************
 * AWB MODE   -  OFF
 ******************************************************************************/

AWBModeOff::AWBModeOff() : AWBModeBase() {}

int AWBModeOff::processState(uint8_t controlMode, uint8_t sceneMode,
                                  const AwbControls& awbControls) {
    LOG(IPU7MetaData, Debug) << __func__;

    mLastAwbControls = awbControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    if (controlMode == controls::Mode3AOff) resetState();

    return 0;
}

int AWBModeOff::processResult(bool converged, ControlList& controls) {
    UNUSED(converged);

    mCurrentAwbState = controls::draft::AwbStateInactive;
    updateResult(controls);

    return 0;
}

/******************************************************************************
 * AWB MODE   -  AUTO
 ******************************************************************************/

AWBModeAuto::AWBModeAuto() : AWBModeBase() {}

int AWBModeAuto::processState(uint8_t controlMode, uint8_t sceneMode,
                                   const AwbControls& awbControls) {
    if (controlMode != mLastControlMode) {
        LOG(IPU7MetaData, Debug) << __func__ << ": control mode has changed " << mLastControlMode
                                 << " -> " << controlMode << ", reset AWB State";
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
                LOG(IPU7MetaData, Error) << "Invalid AWB state!, State set to INACTIVE";
                mCurrentAwbState = controls::draft::AwbStateInactive;
        }
    }
    mLastAwbControls = awbControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    return 0;
}

int AWBModeAuto::processResult(bool converged, ControlList& controls) {
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
            LOG(IPU7MetaData, Error) << "Invalid AWB state!, State set to INACTIVE";
            mCurrentAwbState = controls::draft::AwbStateInactive;
    }

    updateResult(controls);
    if (awbState != mCurrentAwbState) {
        LOG(IPU7MetaData, Debug) << __func__ << " AWB state has changed " << awbState
                                 << " -> " << mCurrentAwbState;
    }
    return 0;
}

}  // namespace libcamera
