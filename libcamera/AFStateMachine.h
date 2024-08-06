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

#pragma once
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <libcamera/property_ids.h>

#include "Utils.h"

namespace libcamera {

typedef int64_t usecs_t;

/**
 * \struct AfControls
 *
 * Control Modes saved and passed back to control unit after reading
 *
 */
struct AfControls {
    uint8_t afMode;    /**< AF_MODE */
    uint8_t afTrigger; /**< AF_TRIGGER */
};

/**
 * \class AFModeBase
 *
 * Base class for all the AutoFocus modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.afState
 *
 */
class AFModeBase {
 public:
    AFModeBase();
    virtual ~AFModeBase() {}

    virtual int processTriggers(uint8_t afTrigger, uint8_t afMode) = 0;
    virtual int processResult(int afState, bool lensMoving, ControlList& controls) = 0;

    void resetState(void);
    void resetTrigger(usecs_t triggerTime);
    void updateResult(ControlList& controls);

 protected:
    void checkIfFocusTimeout();

 protected:
    AfControls mLastAfControls;
    uint8_t mCurrentAfState;
    int32_t mLensState;
    usecs_t mLastActiveTriggerTime; /**< in useconds */
    uint32_t mFramesSinceTrigger;
};

/**
 * \class AFModeAuto
 * Derived class from AFModeBase for Auto mode
 *
 */
class AFModeAuto : public AFModeBase {
 public:
    AFModeAuto();
    virtual int processTriggers(uint8_t afTrigger, uint8_t afMode);
    virtual int processResult(int afState, bool lensMoving, ControlList& controls);
};

/**
 * \class AFModeContinuousPicture
 * Derived class from AFModeBase for Continuous AF mode
 *
 */
class AFModeContinuousPicture : public AFModeBase {
 public:
    AFModeContinuousPicture();
    virtual int processTriggers(uint8_t afTrigger, uint8_t afMode);
    virtual int processResult(int afState, bool lensMoving, ControlList& controls);
};

/**
 * \class AFModeOff
 * Derived class from AFModeBase for OFF mode
 *
 */
class AFModeOff : public AFModeBase {
 public:
    AFModeOff();
    virtual int processTriggers(uint8_t afTrigger, uint8_t afMode);
    virtual int processResult(int afState, bool lensMoving, ControlList& controls);
};

/**
 * \class AFStateMachine
 *
 * This class adapts the Android V3 AF triggers and state transitions to
 * the ones implemented by the Intel AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the AFModeBase
 *
 */
class AFStateMachine {
 public:
    AFStateMachine(int cameraId);
    virtual ~AFStateMachine();

    int processTriggers(uint8_t afTrigger, uint8_t afMode);
    int processResult(int afState, bool lensMoving, ControlList& controls);

 private:
    // prevent copy constructor and assignment operator
    DISALLOW_COPY_AND_ASSIGN(AFStateMachine);

 private: /* members*/
    int mCameraId;
    AfControls mLastAfControls;
    AFModeBase* mCurrentAfMode;

    std::vector<uint8_t> mAvailableModes;

    AFModeOff mOffMode;
    AFModeAuto mAutoMode;

    AFModeContinuousPicture mContinuousPictureMode;
};

}  // namespace libcamera
