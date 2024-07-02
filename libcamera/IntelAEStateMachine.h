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

/**
 * \struct AeControls
 *
 * Control Modes saved and passed back to control unit after reading
 *
 */
struct AeControls {
    int32_t aeMode;              /**< AE_MODE */
    bool aeLock;                 /**< AE_LOCK */
    int32_t aePreCaptureTrigger; /**< PRECAPTURE_TRIGGER */
    int32_t evCompensation;      /**< AE_EXPOSURE_COMPENSATION */
};

/**
 * \class IntelAEModeBase
 *
 * Base class for all the Autoexposure modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.aeState
 *
 */
class IntelAEModeBase {
 public:
    IntelAEModeBase();
    virtual ~IntelAEModeBase() {}

    virtual int processState(uint8_t controlMode, uint8_t sceneMode,
                             const AeControls& aeControls) = 0;
    virtual int processResult(bool aeConverged, ControlList& controls) = 0;
    void resetState(void);

 protected:
    void updateResult(ControlList& controls);

 protected:
    AeControls mLastAeControls;
    uint8_t mLastControlMode;
    uint8_t mLastSceneMode;
    bool mEvChanged; /**< set and kept to true when ev changes until
                          converged */

    bool mLastAeConvergedFlag;
    uint8_t mAeRunCount;
    uint8_t mAeConvergedCount;
    uint8_t mCurrentAeState;
};

/**
 * \class IntelAEModeAuto
 * Derived class from IntelAEModeBase for Auto mode
 *
 */
class IntelAEModeAuto : public IntelAEModeBase {
 public:
    IntelAEModeAuto();
    virtual int processState(uint8_t controlMode, uint8_t sceneMode, const AeControls& aeControls);
    virtual int processResult(bool aeConverged, ControlList& controls);
};

/**
 * \class IntelAEModeOFF
 * Derived class from IntelAEModeBase for OFF mode
 *
 */
class IntelAEModeOff : public IntelAEModeBase {
 public:
    IntelAEModeOff();
    virtual int processState(uint8_t controlMode, uint8_t sceneMode, const AeControls& aeControls);
    virtual int processResult(bool aeConverged, ControlList& controls);
};

/**
 * \class IntelAEStateMachine
 *
 * This class adapts the Android V3 AE triggers and state transitions to
 * the ones implemented by the Intel AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the IntelAEModeBase
 *
 */
class IntelAEStateMachine {
 public:
    IntelAEStateMachine(int cameraId);
    virtual ~IntelAEStateMachine();

    int processState(uint8_t controlMode, uint8_t sceneMode, const AeControls& aeControls);

    int processResult(bool aeConverged, ControlList& controls);

 private:
    // prevent copy constructor and assignment operator
    DISALLOW_COPY_AND_ASSIGN(IntelAEStateMachine);

 private: /* members*/
    int mCameraId;
    AeControls mLastAeControls;
    uint8_t mLastControlMode;
    uint8_t mLastSceneMode;

    IntelAEModeBase* mCurrentAeMode;

    IntelAEModeOff mOffMode;
    IntelAEModeAuto mAutoMode;
};

}  // namespace libcamera
