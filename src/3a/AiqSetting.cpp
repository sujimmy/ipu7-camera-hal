/*
 * Copyright (C) 2015-2024 Intel Corporation.
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

#define LOG_TAG AiqSetting

#include <algorithm>

#include "iutils/Utils.h"
#include "iutils/Errors.h"
#include "iutils/CameraLog.h"

#include "AiqSetting.h"

namespace icamera {

void aiq_parameter_t::reset() {
    frameUsage = FRAME_USAGE_VIDEO;
    aeMode = AE_MODE_AUTO;
    aeForceLock = false;
    awbMode = AWB_MODE_AUTO;
    awbForceLock = false;
    afMode = AF_MODE_AUTO;
    afTrigger = AF_TRIGGER_IDLE;
    sceneMode = SCENE_MODE_AUTO;
    manualExpTimeUs = -1;
    manualGain = -1;
    manualIso = 0;
    evSetting = 0;
    evShift = 0;
    evStep = {1, 3};
    evRange = {-6, 6};
    fps = 30;
    aeFpsRange = { 10.0, 60.0 };
    antibandingMode = ANTIBANDING_MODE_AUTO;
    cctRange = { 0, 0 };
    whitePoint = { 0, 0 };
    awbManualGain = { 0, 0, 0 };
    awbGainShift = { 0, 0, 0 };
    CLEAR(manualColorMatrix);
    CLEAR(manualColorGains);
    aeRegions.clear();
    blcAreaMode = BLC_AREA_MODE_OFF;
    aeConvergeSpeedMode = CONVERGE_SPEED_MODE_AIQ;
    awbConvergeSpeedMode = CONVERGE_SPEED_MODE_AIQ;
    aeConvergeSpeed = CONVERGE_NORMAL;
    awbConvergeSpeed = CONVERGE_NORMAL;
    run3ACadence = 1;
    ltmStrength = 100;
    weightGridMode = WEIGHT_GRID_AUTO;
    aeDistributionPriority = DISTRIBUTION_AUTO;
    CLEAR(customAicParam);
    yuvColorRangeMode = CAMERA_FULL_MODE_YUV_COLOR_RANGE;
    exposureTimeRange.min = -1;
    exposureTimeRange.max = -1;
    sensitivityGainRange.min = -1;
    sensitivityGainRange.max = -1;
    videoStabilizationMode = VIDEO_STABILIZATION_MODE_OFF;
    tuningMode = TUNING_MODE_VIDEO;
    ldcMode = LDC_MODE_OFF;
    rscMode = RSC_MODE_OFF;
    flipMode = FLIP_MODE_NONE;
    digitalZoomRatio = 1.0f;

    lensPosition = 0;
    lensMovementStartTimestamp = 0;
    makernoteMode = MAKERNOTE_MODE_OFF;
    minFocusDistance = 0.0f;
    focusDistance = 0.0f;
    shadingMode = SHADING_MODE_FAST;
    lensShadingMapMode = LENS_SHADING_MAP_MODE_OFF;
    lensShadingMapSize = {0, 0};
    testPatternMode = TEST_PATTERN_OFF;

    tonemapMode = TONEMAP_MODE_FAST;
    tonemapPresetCurve = TONEMAP_PRESET_CURVE_SRGB;
    tonemapGamma = 0.0f;
    tonemapCurves.rSize = 0;
    tonemapCurves.gSize = 0;
    tonemapCurves.bSize = 0;
    tonemapCurves.rCurve = &tonemapCurveMem[0];
    tonemapCurves.gCurve = &tonemapCurveMem[DEFAULT_TONEMAP_CURVE_POINT_NUM];
    tonemapCurves.bCurve = &tonemapCurveMem[DEFAULT_TONEMAP_CURVE_POINT_NUM * 2];
    callbackRgbs = false;
    callbackTmCurve = false;

    powerMode = CAMERA_HIGH_QUALITY;
    effectMode = CAM_EFFECT_NONE;
    totalExposureTarget = 0;

    CLEAR(resolution);
}

void aiq_parameter_t::dump() {
    if (!Log::isLogTagEnabled(GET_FILE_SHIFT(AiqSetting))) return;

    LOG3("Application parameters:");
    LOG3("3A mode: ae %d, awb %d, af %d, scene %d", aeMode, awbMode, afMode, sceneMode);
    LOG3("lock: ae %d, awb %d, af trigger:%d", aeForceLock, awbForceLock, afTrigger);
    LOG3("converge speed mode: ae %d, awb %d", aeConvergeSpeedMode, awbConvergeSpeedMode);
    LOG3("converge speed: ae %d, awb %d", aeConvergeSpeed, awbConvergeSpeed);

    LOG3("EV:%f(%d), range (%f-%f), step %d/%d", evShift, evSetting, evRange.min, evRange.max,
         evStep.numerator, evStep.denominator);
    LOG3("manualExpTimeUs:%ld, time range (%f-%f)", manualExpTimeUs,
         exposureTimeRange.min, exposureTimeRange.max);
    LOG3("manualGain %f, manualIso %d, gain range (%f-%f)", manualGain, manualIso,
         sensitivityGainRange.min, sensitivityGainRange.max);
    LOG3("FPS %f, range (%f-%f)", fps, aeFpsRange.min, aeFpsRange.max);
    for (auto &region : aeRegions) {
        LOG3("ae region (%d, %d, %d, %d, %d)",
             region.left, region.top, region.right, region.bottom, region.weight);
    }
    LOG3("Antibanding mode:%d", antibandingMode);
    LOG3("AE Distribution Priority:%d", aeDistributionPriority);

    LOG3("cctRange:(%f-%f)", cctRange.min, cctRange.max);
    LOG3("manual awb: white point:(%d,%d)", whitePoint.x, whitePoint.y);
    LOG3("manual awb gain:(%d,%d,%d), gain shift:(%d,%d,%d)",
         awbManualGain.r_gain, awbManualGain.g_gain, awbManualGain.b_gain,
         awbGainShift.r_gain, awbGainShift.g_gain, awbGainShift.b_gain);
    for (int i = 0; i < 3; i++) {
        LOG3("manual color matrix: [%.3f %.3f %.3f]",
             manualColorMatrix.color_transform[i][0],
             manualColorMatrix.color_transform[i][1],
             manualColorMatrix.color_transform[i][2]);
    }
    LOG3("manual color gains in rggb:(%.3f,%.3f,%.3f,%.3f)",
         manualColorGains.color_gains_rggb[0], manualColorGains.color_gains_rggb[1],
         manualColorGains.color_gains_rggb[2], manualColorGains.color_gains_rggb[3]);

    for (auto &region : afRegions) {
        LOG3("af region (%d, %d, %d, %d, %d)",
             region.left, region.top, region.right, region.bottom, region.weight);
    }
    LOG3("manual focus distance: %f, min focus distance: %f", focusDistance, minFocusDistance);
    LOG3("Focus position %d, start timestamp %llu", lensPosition, lensMovementStartTimestamp);

    LOG3("digitalZoomRatio %f", digitalZoomRatio);

    LOG3("custom AIC parameter length:%u", customAicParam.length);
    if (customAicParam.length > 0) {
        LOG3("custom AIC parameter data:%s", customAicParam.data);
    }
    if (tuningMode != TUNING_MODE_MAX) {
        LOG3("camera mode:%d", tuningMode);
    }
    LOG3("blc area mode:%d", blcAreaMode);
    LOG3("ltm strength:(%u)", ltmStrength);
    LOG3("weight grid mode:%d", weightGridMode);
    LOG3("Yuv Color Range Mode:%d", yuvColorRangeMode);
    LOG3("DVS mode %d", videoStabilizationMode);

    LOG3("makernoteMode %d", makernoteMode);
    LOG3("shadingMode %d, lensShadingMapMode %d, size %dx%d", shadingMode,
         lensShadingMapMode, lensShadingMapSize.x, lensShadingMapSize.y);

    LOG3("ldcMode %d, rscMode %d, flipMode %d", ldcMode, ldcMode, flipMode);

    LOG3("run3ACadence %d", run3ACadence);
    LOG3("tonemap mode %d, preset curve %d, gamma %f, curve points %d",
          tonemapMode, tonemapPresetCurve, tonemapGamma, tonemapCurves.gSize);
    LOG3("testPatternMode %d", testPatternMode);
    LOG3("power mode %d", powerMode);
    LOG3("totalExposureTarget %ld", totalExposureTarget);

    LOG3("callback RGBS stats %s", callbackRgbs ? "true" : "false");
    LOG3("callback Tonemap curve: %s", callbackTmCurve ? "true" : "false");

    for (auto &region : awbRegions) {
        LOG3("awb region (%d, %d, %d, %d, %d)",
             region.left, region.top, region.right, region.bottom, region.weight);
    }
    LOG3("effect mode %d", effectMode);
}

} /* namespace icamera */
