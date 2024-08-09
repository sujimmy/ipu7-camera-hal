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
 * \struct AwbControls
 *
 * Control Modes saved and passed back to control unit after reading
 *
 */
struct AwbControls {
    uint8_t awbMode;                       /**< AWB_MODE */
    uint8_t awbLock;                       /**< AWB_LOCK */
    uint8_t colorCorrectionMode;           /**< COLOR_CORRECTION_MODE */
    uint8_t colorCorrectionAberrationMode; /**< COLOR_CORRECTION_ABERRATION_MODE */
};

/**
 * \class AWBModeBase
 *
 * Base class for all the Auto white balance modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.awbState
 *
 */
class AWBModeBase {
 public:
    AWBModeBase();
    virtual ~AWBModeBase() {}

    virtual int processState(uint8_t controlMode, uint8_t sceneMode,
                             const AwbControls& awbControls) = 0;

    virtual int processResult(bool converged, ControlList& controls) = 0;

    void resetState(void);

 protected:
    void updateResult(ControlList& controls);

 protected:
    AwbControls mLastAwbControls;
    uint8_t mLastControlMode;
    uint8_t mLastSceneMode;
    int32_t mCurrentAwbState;
};

/**
 * \class AWBModeAuto
 * Derived class from AWBModeBase for Auto mode
 *
 */
class AWBModeAuto : public AWBModeBase {
 public:
    AWBModeAuto();
    virtual int processState(uint8_t controlMode, uint8_t sceneMode,
                             const AwbControls& awbControls);

    virtual int processResult(bool converged, ControlList& controls);
};

/**
 * \class AWBModeOFF
 * Derived class from AWBModeBase for OFF mode
 *
 */
class AWBModeOff : public AWBModeBase {
 public:
    AWBModeOff();
    virtual int processState(uint8_t controlMode, uint8_t sceneMode,
                             const AwbControls& awbControls);

    virtual int processResult(bool converged, ControlList& controls);
};

/**
 * \class AWBStateMachine
 *
 * This class adapts the Android V3 AWB triggers and state transitions to
 * the ones implemented by the Intel AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the AWBModeBase
 *
 */
class AWBStateMachine {
 public:
    AWBStateMachine(int CameraId);
    virtual ~AWBStateMachine();

    int processState(uint8_t controlMode, uint8_t sceneMode, const AwbControls& awbControls);

    int processResult(bool converged, ControlList& controls);

 private:
    // prevent copy constructor and assignment operator
    DISALLOW_COPY_AND_ASSIGN(AWBStateMachine);

 private: /* members*/
    int mCameraId;
    AwbControls mLastAwbControls;
    uint8_t mLastControlMode;
    uint8_t mLastSceneMode;
    AWBModeBase* mCurrentAwbMode;

    AWBModeOff mOffMode;
    AWBModeAuto mAutoMode;
};

}  // namespace libcamera
