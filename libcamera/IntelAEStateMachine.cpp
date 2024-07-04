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

#include "IntelAEStateMachine.h"

#include <libcamera/base/log.h>

#include "Errors.h"
#include "Utils.h"

namespace libcamera {
LOG_DECLARE_CATEGORY(IPU7MetaData)

IntelAEStateMachine::IntelAEStateMachine(int cameraId) : mCameraId(cameraId), mCurrentAeMode(NULL) {
    LOG(IPU7MetaData, Debug) << "id" << mCameraId << " " << __func__;
    mCurrentAeMode = &mAutoMode;
    CLEAR(mLastAeControls);
}

IntelAEStateMachine::~IntelAEStateMachine() {
    LOG(IPU7MetaData, Debug) << "id" << mCameraId << " " << __func__;
}

/**
 * Process states in input stage before the AE is run.
 * It is initializing the current state if input
 * parameters have an influence.
 *
 * \param[IN] controlMode: control.controlMode
 * \param[IN] sceneMode: control.sceneMode
 * \param[IN] aeControls: set of control.<ae>
 */
int IntelAEStateMachine::processState(uint8_t controlMode, uint8_t sceneMode,
                                      const AeControls& aeControls) {
    if (controlMode == controls::Mode3AOff) {
        LOG(IPU7MetaData, Info) << " Set AE offMode";
        mCurrentAeMode = &mOffMode;
    } else {
        // Mode3AAuto
        if (aeControls.aeMode == controls::AeModeOff) {
            LOG(IPU7MetaData, Info) << " Set AE offMode with AE mode " << aeControls.aeMode;
            mCurrentAeMode = &mOffMode;
        } else {
            LOG(IPU7MetaData, Info) << " Set AE autoMode with AE mode " << aeControls.aeMode;
            mCurrentAeMode = &mAutoMode;
        }
    }

    mLastAeControls = aeControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    return mCurrentAeMode->processState(controlMode, sceneMode, aeControls);
}

/**
 * Process results and define output state after the AE is run
 *
 * \param[IN] aeConverged: from the ae result
 * \param[IN] results: cameraMetadata to write dynamic tags.
 */
int IntelAEStateMachine::processResult(bool aeConverged, ControlList& metadata) {
    return mCurrentAeMode->processResult(aeConverged, metadata);
}

/******************************************************************************
 * AE MODE   -  BASE
 ******************************************************************************/
IntelAEModeBase::IntelAEModeBase()
        : mLastControlMode(0),
          mLastSceneMode(0),
          mEvChanged(false),
          mLastAeConvergedFlag(false),
          mAeRunCount(0),
          mAeConvergedCount(0),
          mCurrentAeState(0) {
    CLEAR(mLastAeControls);
}

void IntelAEModeBase::updateResult(ControlList& metadata) {
    metadata.set(controls::AeMode, mLastAeControls.aeMode);
    metadata.set(controls::AeLocked, mLastAeControls.aeLock);
    metadata.set(controls::draft::AePrecaptureTrigger, mLastAeControls.aePreCaptureTrigger);
    metadata.set(controls::draft::AeState, mCurrentAeState);
}

void IntelAEModeBase::resetState() {
    mCurrentAeState = controls::AeModeOff;
    mLastAeConvergedFlag = false;
    mAeRunCount = 0;
    mAeConvergedCount = 0;
}

/******************************************************************************
 * AE MODE   -  OFF
 ******************************************************************************/

IntelAEModeOff::IntelAEModeOff() : IntelAEModeBase() {}

int IntelAEModeOff::processState(uint8_t controlMode, uint8_t sceneMode,
                                 const AeControls& aeControls) {
    LOG(IPU7MetaData, Debug) << __func__;
    mLastAeControls = aeControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    if (controlMode == controls::Mode3AOff || aeControls.aeMode == controls::AeModeOff) {
        resetState();
    } else {
        LOG(IPU7MetaData, Error) << "AE State machine should not be OFF! - Fix bug";
        return icamera::UNKNOWN_ERROR;
    }

    return icamera::OK;
}

int IntelAEModeOff::processResult(bool aeConverged, ControlList& metadata) {
    UNUSED(aeConverged);
    LOG(IPU7MetaData, Debug) << __func__;
    updateResult(metadata);

    return icamera::OK;
}

/******************************************************************************
 * AE MODE   -  AUTO
 ******************************************************************************/

IntelAEModeAuto::IntelAEModeAuto() : IntelAEModeBase() {}

int IntelAEModeAuto::processState(uint8_t controlMode, uint8_t sceneMode,
                                  const AeControls& aeControls) {
    if (controlMode != mLastControlMode) {
        LOG(IPU7MetaData, Debug) << "control mode has changed " << controlMode;
        resetState();
    }
    if (aeControls.aeLock == true) {
        // If ev compensation changes, we have to let the AE run until
        // convergence. Thus we need to figure out changes in compensation and
        // only change the state immediately to locked,
        // IF the EV did not change.
        mEvChanged = (mLastAeControls.evCompensation != aeControls.evCompensation) ? true : false;

        if (!mEvChanged) mCurrentAeState = controls::draft::AeStateLocked;
    } else if (aeControls.aeMode != mLastAeControls.aeMode ||
               (controlMode == controls::Mode3AUseSceneMode && sceneMode != mLastSceneMode)) {
        resetState();
    } else {
        switch (mCurrentAeState) {
            case controls::draft::AeStateLocked:
                mCurrentAeState = controls::draft::AeStateInactive;
                break;
            case controls::draft::AeStateSearching:
            case controls::draft::AeStateInactive:
            case controls::draft::AeStateConverged:
            case controls::draft::AeStateFlashRequired:
            case controls::draft::AeStatePrecapture:
                if (aeControls.aePreCaptureTrigger == controls::draft::AePrecaptureTriggerStart)
                    mCurrentAeState = controls::draft::AeStatePrecapture;

                if (aeControls.aePreCaptureTrigger == controls::draft::AePrecaptureTriggerCancel)
                    mCurrentAeState = controls::draft::AeStateInactive;
                break;
            default:
                LOG(IPU7MetaData, Error) << "Invalid AE state!, State set to INACTIVE";
                mCurrentAeState = controls::draft::AeStateInactive;
                break;
        }
    }
    mLastAeControls = aeControls;
    mLastSceneMode = sceneMode;
    mLastControlMode = controlMode;

    return icamera::OK;
}

int IntelAEModeAuto::processResult(bool aeConverged, ControlList& metadata) {
    uint8_t aeState = mCurrentAeState;
    switch (mCurrentAeState) {
        case controls::draft::AeStateLocked:
            // do nothing
            break;
        case controls::draft::AeStateSearching:
        case controls::draft::AeStateInactive:
        case controls::draft::AeStateConverged:
        case controls::draft::AeStateFlashRequired:
        case controls::draft::AeStatePrecapture:
            if (aeConverged) {
                mEvChanged = false;  // converged -> reset
                if (mLastAeControls.aeLock) {
                    mCurrentAeState = controls::draft::AeStateLocked;
                } else {
                    mCurrentAeState = controls::draft::AeStateConverged;
                }
            }

            if (mCurrentAeState != controls::draft::AeStatePrecapture && !aeConverged) {
                mCurrentAeState = controls::draft::AeStateSearching;
            }
            break;
        default:
            LOG(IPU7MetaData, Error) << "Invalid AE state!, State set to INACTIVE";
            mCurrentAeState = controls::draft::AeStateInactive;
            break;
    }

    if (aeConverged) {
        if (mLastAeConvergedFlag == true) {
            mAeConvergedCount++;
            LOG(IPU7MetaData, Debug)
                << "AE converged for " << std::to_string(mAeConvergedCount) << " frames";
        } else {
            mAeConvergedCount = 1;
            LOG(IPU7MetaData, Debug) << "AE converging -> converged, after running AE for "
                                     << std::to_string(mAeRunCount) << " times";
        }
    } else {
        if (mLastAeConvergedFlag == true) {
            mAeRunCount = 1;
            mAeConvergedCount = 0;
        } else {
            mAeRunCount++;
            LOG(IPU7MetaData, Debug)
                << "AE converging for " << std::to_string(mAeRunCount) << " frames";
        }
    }
    mLastAeConvergedFlag = aeConverged;

    updateResult(metadata);

    if (aeState != mCurrentAeState || aeConverged != mLastAeConvergedFlag) {
        LOG(IPU7MetaData, Debug) << "AE state has changed " << aeState << " -> " << mCurrentAeState
                                 << " and ae converged has changed " << mLastAeConvergedFlag
                                 << " -> " << aeConverged;
    }
    return icamera::OK;
}

}  // namespace libcamera
