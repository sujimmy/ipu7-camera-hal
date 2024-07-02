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
#define LOG_TAG IntelAFStateMachine

#include "IntelAFStateMachine.h"

#include <libcamera/base/log.h>

#include "CameraLog.h"
#include "Errors.h"

namespace libcamera {
LOG_DECLARE_CATEGORY(IPU7MetaData)
/**
 * AF timeouts. Together these will make:
 * timeout if: [MIN_AF_TIMEOUT - MAX_AF_FRAME_COUNT_TIMEOUT - MAX_AF_TIMEOUT]
 * which translates to 2-4 seconds with the current values. Actual timeout value
 * will depend on the FPS. E.g. >30FPS = 2s, 20FPS = 3s, <15FPS = 4s.
 */

/**
 * MAX_AF_TIMEOUT
 * Maximum time we allow the AF to iterate without a result.
 * This timeout is the last resort, for very low FPS operation.
 * Units are in microseconds.
 * 4 seconds is a compromise between CTS & ITS. ITS allows for 10 seconds for
 * 3A convergence. CTS1 allows only 5, but it doesn't require convergence, just
 * a conclusion. We reserve one second for latencies to be safe. This makes the
 * timeout 5 (cts1) - 1 (latency safety) = 4 seconds = 4000000us.
 */
static const long int MAX_AF_TIMEOUT = 4000000;  // 4 seconds

/**
 * MIN_AF_TIMEOUT
 * For very high FPS use cases, we want to anyway allow some time for moving the
 * lens.
 */
static const long int MIN_AF_TIMEOUT = 2000000;  // 2 seconds

/**
 * MAX_AF_FRAME_COUNT_TIMEOUT
 * Maximum time we allow the AF to iterate without a result.
 * Based on frames, as the AF algorithm itself needs frames for its operation,
 * not just time, and the FPS varies.
 * This is the timeout for normal operation, and translates to 2 seconds
 * if FPS is 30.
 */
static const int MAX_AF_FRAME_COUNT_TIMEOUT = 60;  // 2 seconds if 30fps

IntelAFStateMachine::IntelAFStateMachine(int cameraId) : mCameraId(cameraId) {
    LOG1("<id%d>%s", mCameraId, __func__);
    mCurrentAfMode = &mAutoMode;
    mLastAfControls = {controls::AfModeAuto, controls::AfTriggerIdle};
}

IntelAFStateMachine::~IntelAFStateMachine() {
    LOG1("<id%d>%s", mCameraId, __func__);
}

int IntelAFStateMachine::processTriggers(uint8_t afTrigger, uint8_t afMode) {
    if (afMode != mLastAfControls.afMode) {
        LOG1("Change of AF mode from %d to %d", mLastAfControls.afMode, afMode);
        switch (afMode) {
            case controls::AfModeAuto:
            case controls::AfModeMacro:
                mCurrentAfMode = &mAutoMode;
                break;
            case controls::AfModeContinuosVideo:
            case controls::AfModeContinuosPicture:
                mCurrentAfMode = &mContinuousPictureMode;
                break;
            case controls::AfModeOff:
                mCurrentAfMode = &mOffMode;
                break;
            default:
                LOGE("INVALID AF mode requested defaulting to AUTO");
                mCurrentAfMode = &mAutoMode;
                break;
        }
        mCurrentAfMode->resetState();
    }
    mLastAfControls.afTrigger = afTrigger;
    mLastAfControls.afMode = afMode;
    LOG2("%s: afMode %d", __func__, mLastAfControls.afMode);

    return mCurrentAfMode->processTriggers(afTrigger, afMode);
}

int IntelAFStateMachine::processResult(int internalAfState, bool lensMoving,
                                       ControlList& controls) {
    return mCurrentAfMode->processResult(internalAfState, lensMoving, controls);
}

/******************************************************************************
 * AF MODE   -  BASE
 ******************************************************************************/
IntelAfModeBase::IntelAfModeBase()
        : mLastAfControls{controls::AfModeAuto, controls::AfTriggerIdle},
          mCurrentAfState(controls::AfStateIdle),
          mLensState(controls::LensStateStationary),
          mLastActiveTriggerTime(0),
          mFramesSinceTrigger(0) {}

/**
 * processTriggers
 *
 * This method is called BEFORE auto focus algorithm has RUN
 * Input parameters are pre-filled by the Intel3APlus::fillAfInputParams()
 * by parsing the request settings.
 * Other parameters from the capture request settings not filled in the input
 * params structure is passed as argument
 */
int IntelAfModeBase::processTriggers(uint8_t afTrigger, uint8_t afMode) {
    LOG2("%s", __func__);

    if (afTrigger == controls::AfTriggerStart) {
        resetTrigger(icamera::CameraUtils::systemTime() / 1000);
        LOG(IPU7MetaData, Info) << "AF TRIGGER START";
    } else if (afTrigger == controls::AfTriggerCancel) {
        LOG(IPU7MetaData, Info) << "AF TRIGGER CANCEL";
        resetTrigger(0);
    }
    mLastAfControls.afTrigger = afTrigger;
    mLastAfControls.afMode = afMode;

    return icamera::OK;
}

void IntelAfModeBase::updateResult(ControlList& controls) {
    LOG2("%s afMode = %d state = %d lens state: %d", __func__, mLastAfControls.afMode,
         mCurrentAfState, mLensState);

    controls.set(controls::AfMode, mLastAfControls.afMode);
    controls.set(controls::AfTrigger, mLastAfControls.afTrigger);
    controls.set(controls::AfState, mCurrentAfState);
    controls.set(controls::LensState, mLensState);
}

void IntelAfModeBase::resetTrigger(usecs_t triggerTime) {
    mLastActiveTriggerTime = triggerTime;
    mFramesSinceTrigger = 0;
}

void IntelAfModeBase::resetState() {
    mCurrentAfState = controls::AfTriggerIdle;
}

void IntelAfModeBase::checkIfFocusTimeout() {
    // give up if AF was iterating for too long
    if (mLastActiveTriggerTime != 0) {
        mFramesSinceTrigger++;
        usecs_t now = icamera::CameraUtils::systemTime() / 1000;
        usecs_t timeSinceTriggered = now - mLastActiveTriggerTime;
        if (mCurrentAfState != controls::AfStateFocused) {
            /**
             * Timeout IF either time has passed beyond MAX_AF_TIMEOUT
             *                         OR
             * Enough frames have been processed and time has passed beyond
             * MIN_AF_TIMEOUT
             */
            if (timeSinceTriggered > MAX_AF_TIMEOUT ||
                (mFramesSinceTrigger > MAX_AF_FRAME_COUNT_TIMEOUT &&
                 timeSinceTriggered > MIN_AF_TIMEOUT)) {
                resetTrigger(0);
                mCurrentAfState = controls::AfStateFailed;
            }
        }
    }
}

/******************************************************************************
 * AF MODE   -  OFF
 ******************************************************************************/

IntelAFModeOff::IntelAFModeOff() : IntelAfModeBase() {}

int IntelAFModeOff::processTriggers(uint8_t afTrigger, uint8_t afMode) {
    LOG2("%s", __func__);

    mLastAfControls.afTrigger = afTrigger;
    mLastAfControls.afMode = afMode;

    return icamera::OK;
}

int IntelAFModeOff::processResult(int internalAfState, bool lensMoving, ControlList& controls) {
    UNUSED(internalAfState);
    LOG2("%s", __func__);

    mCurrentAfState = controls::AfStateIdle;
    mLensState = lensMoving ? controls::LensStateMoving : controls::LensStateStationary;
    updateResult(controls);

    return icamera::OK;
}

/******************************************************************************
 * AF MODE   -  AUTO
 ******************************************************************************/

IntelAFModeAuto::IntelAFModeAuto() : IntelAfModeBase() {}

int IntelAFModeAuto::processTriggers(uint8_t afTrigger, uint8_t afMode) {
    LOG2("%s afMode:%d trigger:%d", __func__, afMode, afTrigger);

    IntelAfModeBase::processTriggers(afTrigger, afMode);

    // Override AF state if we just got an AF TRIGGER Start
    // This is only valid for the AUTO/MACRO state machine
    if (mLastAfControls.afTrigger == controls::AfTriggerStart) {
        mCurrentAfState = controls::AfStateScanning;
        LOG1("@%s AF state ACTIVE_SCAN (trigger start)", __PRETTY_FUNCTION__);
    } else if (mLastAfControls.afTrigger == controls::AfTriggerCancel) {
        mCurrentAfState = controls::AfStateIdle;
        LOG1("@%s AF state INACTIVE (trigger cancel)", __PRETTY_FUNCTION__);
    }

    return icamera::OK;
}

int IntelAFModeAuto::processResult(int internalAfState, bool lensMoving, ControlList& controls) {
    mLensState = lensMoving ? controls::LensStateMoving : controls::LensStateStationary;
    if (mLastActiveTriggerTime != 0) {
        switch (internalAfState) {
            case icamera::AF_STATE_LOCAL_SEARCH:
            case icamera::AF_STATE_EXTENDED_SEARCH:
                LOG2("@%s AF state SCANNING", __PRETTY_FUNCTION__);
                break;
            case icamera::AF_STATE_SUCCESS:
                mCurrentAfState = controls::AfStateFocused;
                resetTrigger(0);
                LOG2("@%s AF state FOCUSED_LOCKED", __PRETTY_FUNCTION__);
                break;
            case icamera::AF_STATE_FAIL:
                mCurrentAfState = controls::AfStateFailed;
                resetTrigger(0);
                LOG2("@%s AF state FAILED", __PRETTY_FUNCTION__);
                break;
            case icamera::AF_STATE_IDLE:
            default:
                LOG2("@%s AF state IDLE", __PRETTY_FUNCTION__);
                break;
        }
    }

    checkIfFocusTimeout();
    updateResult(controls);

    return icamera::OK;
}

/******************************************************************************
 * AF MODE   -  CONTINUOUS PICTURE
 ******************************************************************************/

IntelAFModeContinuousPicture::IntelAFModeContinuousPicture() : IntelAfModeBase() {
    mCurrentAfState = controls::AfStateScanning;
}

int IntelAFModeContinuousPicture::processTriggers(uint8_t afTrigger, uint8_t afMode) {
    LOG2("%s afMode:%d trigger:%d", __func__, afMode, afTrigger);
    IntelAfModeBase::processTriggers(afTrigger, afMode);
    // Override AF state if we just got an AF TRIGGER CANCEL
    if (mLastAfControls.afTrigger == controls::AfTriggerCancel) {
        /* Scan is supposed to be restarted, which we try by triggering a new
         * scan. (see IntelAFStateMachine::processTriggers)
         * This however, doesn't do anything at all, because AIQ does not
         * want to play ball, at least yet.
         *
         * We can skip state transitions when allowed by the state
         * machine documentation, so skip INACTIVE, also skip PASSIVE_SCAN if
         * possible and go directly to either PASSIVE_FOCUSED or UNFOCUSED
         */
        switch (mCurrentAfState) {
            case controls::AfStateFocused:
                mCurrentAfState = controls::AfStateFocused;
                break;
            default:
                mCurrentAfState = controls::AfStateIdle;
                break;
        }
    }

    if (mLastAfControls.afTrigger == controls::AfTriggerStart) {
        mCurrentAfState = controls::AfStateScanning;
    }

    return icamera::OK;
}

int IntelAFModeContinuousPicture::processResult(int internalAfState, bool lensMoving,
                                                ControlList& controls) {
    mLensState = lensMoving ? controls::LensStateMoving : controls::LensStateStationary;
    // state transition from locked state are only allowed via triggers, which
    // are handled in the currentAFMode processTriggers() and below in this
    // function.
    if (mCurrentAfState != controls::AfStateFocused && mCurrentAfState != controls::AfStateFailed) {
        switch (internalAfState) {
            case icamera::AF_STATE_LOCAL_SEARCH:
            case icamera::AF_STATE_EXTENDED_SEARCH:
                LOG2("@%s AF state SCANNING", __PRETTY_FUNCTION__);
                mCurrentAfState = controls::AfStateScanning;
                break;
            case icamera::AF_STATE_SUCCESS:
                mCurrentAfState = controls::AfStateFocused;
                LOG2("@%s AF state FOCUSED", __PRETTY_FUNCTION__);
                if (mLastActiveTriggerTime != 0) resetTrigger(0);
                break;
            case icamera::AF_STATE_FAIL:
                mCurrentAfState = controls::AfStateFailed;
                LOG2("@%s AF state FAILED", __PRETTY_FUNCTION__);
                if (mLastActiveTriggerTime != 0) resetTrigger(0);
                break;
            case icamera::AF_STATE_IDLE:
            default:
                break;
        }
    }

    checkIfFocusTimeout();
    updateResult(controls);

    return icamera::OK;
}

}  // namespace libcamera
