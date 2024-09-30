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

#include "Camera3AMetadata.h"

#include <libcamera/base/log.h>

#include "Utils.h"

namespace libcamera {
LOG_DEFINE_CATEGORY(IPU7MetaData)

Camera3AMetadata::Camera3AMetadata(int cameraId) : mCameraId(cameraId) {
    LOG(IPU7MetaData, Debug) << __func__;

    mAEStateMachine = new AEStateMachine(mCameraId);
    mAFStateMachine = new AFStateMachine(mCameraId);
    mAWBStateMachine = new AWBStateMachine(mCameraId);
}

Camera3AMetadata::~Camera3AMetadata() {
    LOG(IPU7MetaData, Debug) << __func__;

    delete mAEStateMachine;
    delete mAFStateMachine;
    delete mAWBStateMachine;
}

void Camera3AMetadata::process3Astate(const icamera::AiqResult* aiqResult,
                                      const icamera::DataContext* dataContext,
                                      const ControlList& controls, ControlList& metadata) {
    LOG(IPU7MetaData, Debug) << __func__;

    // process AE
    AeControls aeControls = {controls::AeModeOn, dataContext->mAiqParams.aeForceLock,
                             controls::draft::AePrecaptureTriggerIdle, 0};

    aeControls.aeMode = controls.get(controls::AeMode).value_or(controls::AeModeOn);
    aeControls.aePreCaptureTrigger = controls.get(controls::draft::AePrecaptureTrigger)
                                         .value_or(controls::draft::AePrecaptureTriggerIdle);

    uint8_t controlMode = controls.get(controls::Mode3A).value_or(controls::Mode3AAuto);
    uint8_t sceneMode = controls.get(controls::SceneMode).value_or(controls::SceneModeDisabled);
    mAEStateMachine->processState(controlMode, sceneMode, aeControls);
    mAEStateMachine->processResult(aiqResult->mAeResults.exposures[0].converged, metadata);

    // process AF
    uint8_t afTrigger = static_cast<uint8_t>(controls::AfTriggerIdle);
    const auto& trigger = controls.get(controls::AfTrigger);
    if (trigger) afTrigger = *trigger;

    uint8_t afMode = static_cast<uint8_t>(controls::AfModeAuto);
    const auto& mode = controls.get(controls::AfMode);
    if (mode) afMode = *mode;
    mAFStateMachine->processTriggers(afTrigger, afMode);

    // get AF state
    icamera::camera_af_state_t internalAfState =
        (aiqResult->mAfResults.status == ia_aiq_af_status_local_search)
            ? icamera::AF_STATE_LOCAL_SEARCH
            : (aiqResult->mAfResults.status == ia_aiq_af_status_extended_search)
                  ? icamera::AF_STATE_EXTENDED_SEARCH
                  : ((aiqResult->mAfResults.status == ia_aiq_af_status_success) &&
                     aiqResult->mAfResults.final_lens_position_reached)
                        ? icamera::AF_STATE_SUCCESS
                        : (aiqResult->mAfResults.status == ia_aiq_af_status_fail)
                              ? icamera::AF_STATE_FAIL
                              : icamera::AF_STATE_IDLE;
    bool lensMoving = false;
    if (internalAfState == icamera::AF_STATE_LOCAL_SEARCH ||
        internalAfState == icamera::AF_STATE_EXTENDED_SEARCH) {
        lensMoving = (aiqResult->mAfResults.final_lens_position_reached == false);
    } else if (internalAfState == icamera::AF_STATE_SUCCESS &&
               dataContext->mAiqParams.afMode == icamera::AF_MODE_OFF) {
        /* In manual focus mode, AF_STATE_SUCCESS is set immediately after running algo,
         * but lens is moving and will stop moving in next frame.
         */
        lensMoving = (aiqResult->mLensPosition != aiqResult->mAfResults.next_lens_position);
    }

    mAFStateMachine->processResult(internalAfState, lensMoving, metadata);

    // AWB
    AwbControls awbControls = {controls::AwbAuto, 0, 0, 0};
    const auto& awbMode = controls.get(controls::AwbMode);
    if (awbMode) awbControls.awbMode = *awbMode;

    const auto& awbLock = controls.get(controls::AwbLocked);
    if (awbLock) awbControls.awbLock = *awbLock;

    mAWBStateMachine->processState(controlMode, sceneMode, awbControls);

    // get AWB state
    icamera::camera_awb_state_t awbState =
        (fabs(aiqResult->mAwbResults.distance_from_convergence) < 0.001)
            ? icamera::AWB_STATE_CONVERGED
            : icamera::AWB_STATE_NOT_CONVERGED;
    mAWBStateMachine->processResult(awbState == icamera::AWB_STATE_CONVERGED, metadata);
}

}  // namespace libcamera
