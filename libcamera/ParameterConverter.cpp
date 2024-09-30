/*
 * Copyright (C) 2024 Intel Corporation.
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

#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <libcamera/property_ids.h>
#include <libcamera/base/log.h>

#include "ParameterConverter.h"
#include "PlatformData.h"
#include "ParamDataType.h"

namespace libcamera {
using namespace icamera;
LOG_DECLARE_CATEGORY(IPU7)

#define FLICKER_50HZ_PERIOD 10000
#define FLICKER_60HZ_PERIOD 8333
#define LENS_FACING_FRONT 0

template <typename T>
struct ValuePair {
    int ctrlValue;
    T halValue;
};

static const ValuePair<camera_awb_mode_t> awbModeMap[] = {
    {controls::AwbAuto, AWB_MODE_AUTO},
    {controls::AwbIncandescent, AWB_MODE_INCANDESCENT},
    //{controls::AwbTungsten, },
    {controls::AwbFluorescent, AWB_MODE_FLUORESCENT},
    //{controls::AwbIndoor, },
    {controls::AwbDaylight, AWB_MODE_DAYLIGHT},
    {controls::AwbCloudy, AWB_MODE_FULL_OVERCAST},
    //{controls::AwbCustom, },
};

static const ValuePair<camera_af_mode_t> afModeMap[] = {
    {controls::AfModeOff, AF_MODE_OFF},
    {controls::AfModeAuto, AF_MODE_AUTO},
    {controls::AfModeMacro, AF_MODE_MACRO},
    {controls::AfModeContinuosVideo, AF_MODE_CONTINUOUS_VIDEO},
    {controls::AfModeContinuosPicture, AF_MODE_CONTINUOUS_PICTURE},
};

static const ValuePair<camera_test_pattern_mode_t> testPatternMap[] = {
    {controls::draft::TestPatternModeOff, TEST_PATTERN_OFF},
    {controls::draft::TestPatternModeSolidColor, SOLID_COLOR},
    {controls::draft::TestPatternModeColorBars, COLOR_BARS},
    {controls::draft::TestPatternModeColorBarsFadeToGray, COLOR_BARS_FADE_TO_GRAY},
    {controls::draft::TestPatternModePn9, PN9},
    {controls::draft::TestPatternModeCustom1, TEST_PATTERN_CUSTOM1},
};

static const ValuePair<camera_tonemap_mode_t> tonemapModesTable[] = {
    {controls::ToneMapModeContrastCurve, TONEMAP_MODE_CONTRAST_CURVE},
    {controls::TonemapModeFast, TONEMAP_MODE_FAST},
    {controls::TonemapModeHighQuality, TONEMAP_MODE_HIGH_QUALITY},
    {controls::TonemapModeGammaValue, TONEMAP_MODE_GAMMA_VALUE},
    {controls::TonemapModePresetCurve, TONEMAP_MODE_PRESET_CURVE},
};

static const ValuePair<camera_antibanding_mode_t> antibandingModeMap[] = {
    {controls::AeAntiBandingModeOff, ANTIBANDING_MODE_OFF},
    {controls::AeAntiBandingMode50Hz, ANTIBANDING_MODE_50HZ},
    {controls::AeAntiBandingMode60Hz, ANTIBANDING_MODE_60HZ},
    {controls::AeAntiBandingModeAuto, ANTIBANDING_MODE_AUTO},
};

static const ValuePair<camera_statistics_face_detect_mode_t> faceDetectModeMap[] = {
    {controls::FaceDetectModeOff, CAMERA_STATISTICS_FACE_DETECT_MODE_OFF},
    {controls::FaceDetectModeSimple, CAMERA_STATISTICS_FACE_DETECT_MODE_SIMPLE},
};

template <typename T>
static bool getCtlValue(T halValue, const ValuePair<T>* table, int tableCount, int* ctrlValue) {
    if (!table || !ctrlValue) return false;

    for (int i = 0; i < tableCount; i++) {
        if (halValue == table[i].halValue) {
            *ctrlValue = table[i].ctrlValue;
            return true;
        }
    }
    return false;
}

template <typename T>
static bool getHalValue(int ctrlValue, const ValuePair<T>* table, int tableCount, T* halValue) {
    if (!table || !halValue) return false;

    for (int i = 0; i < tableCount; i++) {
        if (ctrlValue == table[i].ctrlValue) {
            *halValue = table[i].halValue;
            return true;
        }
    }
    return false;
}

template <typename T, size_t N>
static bool isValueSupported(int32_t value, const std::array<T, N>& array) {
    for (auto &item : array) {
        if (value == item.template get<int32_t>()) return true;
    }
    return false;
}

void ParameterConverter::fillLensStaticMetadata(int cameraId, ControlInfoMap::Map& controls) {
    std::string str = "shading.availableModes";
    auto vSM = PlatformData::getByteStaticMetadata(cameraId, str);
    std::vector<ControlValue> lensSM;
    for (auto mode : vSM) {
        if(isValueSupported(mode, controls::draft::LensShadingMapModeValues))
            lensSM.push_back(static_cast<int32_t>(mode));
    }
    controls[&controls::draft::LensShadingMapMode] = ControlInfo(lensSM);

    str = "lens.info.availableFocalLengths";
    auto vFocalLength = icamera::PlatformData::getFloatStaticMetadata(cameraId, str);
    std::vector<ControlValue> fLength;
    for (auto len : vFocalLength) {
        fLength.push_back(static_cast<float>(len));
    }
    controls[&controls::LensFocalLength] = ControlInfo(fLength);

    str = "lens.info.minimumFocusDistance";
    auto vMFD = icamera::PlatformData::getFloatStaticMetadata(cameraId, str);
    float minimumFocusDistance = vMFD.size() > 0 ? vMFD[0] : 10.0f;

    float infiniteFocusDistance = 0.1f;
    controls[&controls::LensFocusDistance] = ControlInfo(
        infiniteFocusDistance, std::max(infiniteFocusDistance, minimumFocusDistance), 1.0f);

    // focusDistanceCalibration
    // shadingMapSize
    // hyperfocalDistance

    controls[&controls::LensPosition] = ControlInfo(0.0f, 1000.0f);
}

void ParameterConverter::initializeCapabilities(int cameraId, const ControlList& properties,
                                                ControlInfoMap::Map& controls) {
    auto metadata = PlatformData::getStaticMetadata(cameraId);

    // AE
    controls[&controls::AeEnable] = ControlInfo(false, true, true);
    controls[&controls::AeLocked] = ControlInfo(true, false);

    std::vector<ControlValue> AeModes{
        static_cast<int32_t>(controls::AeModeOff),
        static_cast<int32_t>(controls::AeModeOn),
    };
    controls[&controls::AeMode] = ControlInfo(AeModes);

    // controls::AeMeteringMode
    // controls::AeConstraintMode
    // controls::AeExposureMode

    if (metadata->mEvRange.size() == 2 && metadata->mEvStep.size() == 2 && metadata->mEvStep[1]) {
        float step = static_cast<float>(metadata->mEvStep[0]) / metadata->mEvStep[1];
        float minV = step * metadata->mEvRange[0];
        float maxV = step * metadata->mEvRange[1];
        controls[&controls::ExposureValue] = ControlInfo(minV, maxV, 0.0f);
    }

    std::string str = "sensor.info.exposureTimeRange";
    auto et = PlatformData::getInt64StaticMetadata(cameraId, str);
    if (et.size() == 2) {
        controls[&controls::ExposureTime] = ControlInfo(static_cast<int32_t>(et[0]),
                                                        static_cast<int32_t>(et[1]), 33333);  // us
    }

    str = "sensor.info.sensitivityRange";
    auto range = PlatformData::getInt32StaticMetadata(cameraId, str);
    str = "sensor.maxAnalogSensitivity";
    auto ag = PlatformData::getInt32StaticMetadata(cameraId, str);
    if (range.size() == 2 && ag.size() == 1) {
        /* libcamera Android layer translates AnalogueGain to SENSITIVITY_RANGE and SENSITIVITY
        ** ANDROID_SENSOR_INFO_SENSITIVITY_RANGE is set to [range[0], range[1]]
        ** ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY is set to range[1]
        ** According to our design, the range max is ag*dg, we set the AnalogueGain max to range[1]
        ** as a WA
        */
        float gainMin = static_cast<float>(range[0]);
        float sensitivityMax = static_cast<float>(range[1]);
        controls[&controls::AnalogueGain] = ControlInfo(gainMin, sensitivityMax, gainMin);
        // dGain is not used by libcamera currently, default aGainMax * dGainMax = sensitivityMax
        controls[&controls::DigitalGain] = ControlInfo(1.0f, sensitivityMax / ag[0], 1.0f);
    }

    std::vector<ControlValue> antibandingModes;
    for (auto mode : metadata->mSupportedAntibandingMode) {
        int32_t ctrlValue = 0;
        if (getCtlValue(mode, antibandingModeMap, ARRAY_SIZE(antibandingModeMap), &ctrlValue)) {
            antibandingModes.push_back(ctrlValue);
        }
    }

    if (!antibandingModes.empty())
        controls[&controls::AeAntiBandingMode] = ControlInfo(antibandingModes);

    // AWB
    controls[&controls::AwbEnable] = ControlInfo(true, false, true);
    controls[&controls::AwbLocked] = ControlInfo(true, false);

    std::vector<ControlValue> awbModes;
    for (auto mode : metadata->mSupportedAwbMode) {
        int32_t ctrlValue = 0;
        if (getCtlValue(mode, awbModeMap, ARRAY_SIZE(awbModeMap), &ctrlValue)) {
            awbModes.push_back(ctrlValue);
        }
    }
    if (!awbModes.empty())
        controls[&controls::AwbMode] = ControlInfo(awbModes);

     controls[&controls::draft::AePrecaptureTrigger] =
        ControlInfo(controls::draft::AePrecaptureTriggerValues);

    // controls::ColourGains
    // controls::ColourTemperature
    controls[&controls::ColorCorrectionGains] = ControlInfo(0.0f, 100.0f);
    controls[&controls::ColourCorrectionMatrix] = ControlInfo(-100.0f, 100.0f);
    controls[&controls::ColorCorrectionMode] = ControlInfo(controls::ColorCorrectionModeValues);

    // AF
    std::vector<ControlValue> afModes;
    for (auto mode : metadata->mSupportedAfMode) {
        int32_t ctrlValue = 0;
        if (getCtlValue(mode, afModeMap, ARRAY_SIZE(afModeMap), &ctrlValue)) {
            afModes.push_back(ctrlValue);
        }
    }
    if (!afModes.empty())
        controls[&controls::AfMode] = ControlInfo(afModes);

    controls[&controls::AfTrigger] = ControlInfo(controls::AfTriggerValues);

    str = "control.maxRegions";
    auto regions = PlatformData::getInt32StaticMetadata(cameraId, str);
    std::vector<ControlValue> afMetering;
    afMetering.push_back(static_cast<int32_t>(controls::AfMeteringAuto));
    if (regions.size() == 3 && regions[2] > 0) {
        afMetering.push_back(static_cast<int32_t>(controls::AfMeteringWindows));
    }
    controls[&controls::AfMetering] = ControlInfo(afMetering);
    controls[&controls::AfWindows] = ControlInfo(Rectangle{}, Rectangle{}, Rectangle{});
    // controls::AfRange
    // controls::AfSpeed

    // Advanced
    // controls::Brightness
    // controls::Contrast
    // controls::Lux
    // controls::Saturation
    // controls::SensorBlackLevels
    // controls::Sharpness
    // controls::FocusFoM

    str = "edge.availableEdgeModes";
    auto vEM = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    std::vector<ControlValue> edgeModes;
    for (auto mode : vEM) {
        if(isValueSupported(mode, controls::EdgeModeValues))
            edgeModes.push_back(static_cast<int32_t>(mode));
    }
    if (!edgeModes.empty())
        controls[&controls::EdgeMode] = ControlInfo(edgeModes);

    str = "noiseReduction.availableNoiseReductionModes";
    auto vNRM = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    std::vector<ControlValue> nrModes;
    for (auto mode : vNRM) {
        if(isValueSupported(mode, controls::draft::NoiseReductionModeValues))
            nrModes.push_back(static_cast<int32_t>(mode));
    }
    if (!nrModes.empty()) {
        controls[&controls::draft::NoiseReductionMode] = ControlInfo(nrModes);
    }

    std::vector<ControlValue> aberrationModes;
    aberrationModes.push_back(static_cast<int32_t>(controls::draft::ColorCorrectionAberrationOff));
    controls[&controls::draft::ColorCorrectionAberrationMode] = ControlInfo(aberrationModes);

    // tone map control
    str = "tonemap.availableToneMapModes";
    auto vTMM = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    std::vector<ControlValue> toneMapModes;
    for (auto mode : vTMM) {
        if(isValueSupported(mode, controls::TonemapModeValues))
            toneMapModes.push_back(static_cast<int32_t>(mode));
    }
    controls[&controls::TonemapMode] = ControlInfo(toneMapModes);
    controls[&controls::TonemapCurveRed] = ControlInfo(0.0f, 1.0f);
    controls[&controls::TonemapCurveGreen] = ControlInfo(0.0f, 1.0f);
    controls[&controls::TonemapCurveBlue] = ControlInfo(0.0f, 1.0f);

    // Sensor
    // us, <min, max, default>
    int64_t frameDurations[3];
    frameDurations[0] = 33333;
    frameDurations[1] = 66666;
    frameDurations[2] = 33333;
    controls[&controls::FrameDurationLimits] = ControlInfo(frameDurations[0], frameDurations[1],
                                                           frameDurations[2]);
    controls[&controls::FrameDuration] =
        ControlInfo(frameDurations[0] * 1000, frameDurations[1] * 1000, frameDurations[2] * 1000);

    // controls::SensorTemperature
    // controls::draft::SensorRollingShutterSkew

    // Lens
    fillLensStaticMetadata(cameraId, controls);

    // Others
    str = "request.pipelineMaxDepth";
    auto vPMD = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    if (vPMD.size() == 1) {
        int32_t depth = vPMD[0];
        controls[&controls::draft::PipelineDepth] = ControlInfo(depth, depth, depth);
    }

    str = "sync.maxLatency";
    auto vL = icamera::PlatformData::getInt32StaticMetadata(cameraId, str);
    if (vL.size() == 1) {
        int32_t maxLatency = vL[0];
        controls[&controls::draft::MaxLatency] = ControlInfo(maxLatency, maxLatency, maxLatency);
    }

    str = "sensor.availableTestPatternModes";
    auto vTSM = icamera::PlatformData::getInt32StaticMetadata(cameraId, str);
    std::vector<ControlValue> testPM;
    for (auto mode : vTSM) {
        int32_t ctrlValue = 0;
        if (getCtlValue(static_cast<camera_test_pattern_mode_t>(mode), testPatternMap,
                        ARRAY_SIZE(testPatternMap), &ctrlValue)) {
            testPM.push_back(ctrlValue);
        }
    }
    if (!testPM.empty()) {
        controls[&controls::draft::TestPatternMode] = ControlInfo(testPM);
    }

    // Fixme, assign max crop to scaler crop
    const Size &pixelArraySize = properties.get(properties::PixelArraySize).value_or(Size{});
    Rectangle maxScalerCrop = Rectangle{ pixelArraySize };
    controls[&controls::ScalerCrop] = ControlInfo(maxScalerCrop, maxScalerCrop, maxScalerCrop);

    std::vector<ControlValue> sceneModes{
        static_cast<uint8_t>(controls::SceneModeDisabled),
    };

    std::vector<ControlValue> supported3AModes;
    str = "control.availableModes";
    auto v3AMode = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    for (auto mode : v3AMode) {
        if (isValueSupported(static_cast<int32_t>(mode), controls::Mode3AValues)) {
            supported3AModes.push_back(static_cast<uint8_t>(mode));
        }
    }

    str = "statistics.info.availableFaceDetectModes";
    auto vFaceMode = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    std::vector<ControlValue> faceDetectModes;
    for (auto mode : vFaceMode) {
        int32_t ctrlValue = 0;
        if (getCtlValue(static_cast<camera_statistics_face_detect_mode_t>(mode), faceDetectModeMap,
                        ARRAY_SIZE(faceDetectModeMap), &ctrlValue)) {
            if (ctrlValue != controls::FaceDetectModeOff) {
                sceneModes.push_back(static_cast<uint8_t>(controls::SceneModeFacePriority));
                // must support use scene mode when SceneModeFacePriority is supported
                supported3AModes.push_back(static_cast<uint8_t>(controls::Mode3AUseSceneMode));
            }
            faceDetectModes.push_back(static_cast<uint8_t>(ctrlValue));
        }
    }
    if (!faceDetectModes.empty())
        controls[&controls::FaceDetectMode] = ControlInfo(faceDetectModes);

    controls[&controls::SceneMode] = ControlInfo(sceneModes);
    if (!supported3AModes.empty()) {
        controls[&controls::Mode3A] = ControlInfo(supported3AModes);
    }
}

void ParameterConverter::initProperties(int cameraId, ControlList& properties) {
    std::string str = "sensor.info.pixelArraySize";
    auto pixel = PlatformData::getInt32StaticMetadata(cameraId, str);
    if (pixel.size() == 2) {
        Size pixelArraySize(pixel[0], pixel[1]);
        Rectangle cropMax(0, 0, pixel[0], pixel[1]);
        properties.set(properties::PixelArraySize, pixelArraySize);
        properties.set(properties::ScalerCropMaximum, cropMax);
    }

    str = "sensor.info.physicalSize";
    auto phySize = PlatformData::getFloatStaticMetadata(cameraId, str);
    if (phySize.size() == 2 && pixel.size() == 2) {
        Size unitCellSize(phySize[0] * 1e6f / pixel[0], phySize[1] * 1e6f / pixel[1]);  // nm
        properties.set(properties::UnitCellSize, unitCellSize);
    }

    str = "sensor.info.activeArraySize";
    auto active = PlatformData::getInt32StaticMetadata(cameraId, str);
    if (active.size() == 4) {
        Rectangle activeArea(active[0], active[1], active[2], active[3]);
        properties.set(properties::PixelArrayActiveAreas, { activeArea });
    }

    str = "sensor.orientation";
    int32_t rotation = 0;
    auto orientation = icamera::PlatformData::getInt32StaticMetadata(cameraId, str);
    if (orientation.size() == 1) rotation = orientation[0];
    properties.set(properties::Rotation, rotation);

    str = "lens.facing";
    int32_t value = properties::CameraLocationBack;
    auto facing = PlatformData::getByteStaticMetadata(cameraId, str);
    if (active.size() == 1 && active[0] == LENS_FACING_FRONT)
        value = properties::CameraLocationFront;
    properties.set(properties::Location, value);

    std::string sensorName = PlatformData::getSensorName(cameraId);
    if (sensorName.c_str()) {
        properties.set(properties::Model, sensorName);
    }

    str = "sensor.info.colorFilterArrangement";
    auto vCFA = icamera::PlatformData::getByteStaticMetadata(cameraId, str);
    if (vCFA.size() == 1) {
        properties.set(properties::draft::ColorFilterArrangement, static_cast<int32_t>(vCFA[0]));
    }
}

void ParameterConverter::convertEdgeControls(const ControlList& controls,
                                             icamera::DataContext* context) {
    if (!context) return;

    /* When in still capture mode , the edgeMode default value should be HQ. In other case,
       the edgeMode default value should be FAST. The default value corresponds to
       EDGE_MODE_LEVEL_2.
       In addition, we use the same level for OFF and ZSL.
    */
    uint8_t requestEdgeMode = controls.get(controls::EdgeMode).value_or(controls::EdgeModeFast);
    icamera::camera_edge_mode_t edgeMode = icamera::EDGE_MODE_LEVEL_2;
    if ((requestEdgeMode == controls::EdgeModeOff) ||
        (requestEdgeMode == controls::EdgeModeZeroShutterLag)) {
        edgeMode = icamera::EDGE_MODE_LEVEL_4;
    } else if (requestEdgeMode == controls::EdgeModeFast) {  // todo, only in still capture mode
        edgeMode = icamera::EDGE_MODE_LEVEL_3;
    } else if (requestEdgeMode == controls::EdgeModeHighQuality) {  // todo, only in video mode
        edgeMode = icamera::EDGE_MODE_LEVEL_1;
    }

    context->mIspParams.edgeMode = edgeMode;
}
void ParameterConverter::convertNRControls(const ControlList& controls,
                                           icamera::DataContext* context) {
    if (!context) return;

    uint8_t mode = controls.get(controls::draft::NoiseReductionMode)
                                .value_or(controls::draft::NoiseReductionModeOff);
    /* When is still capture, the nrMode default value should be HQ. In other case,
       the nrMode default value should be FAST. The default value corresponds to
       NR_MODE_LEVEL_2.
       In addition, we use the same level for OFF and ZSL.
    */
    icamera::camera_nr_mode_t nrMode = icamera::NR_MODE_LEVEL_2;
    if ((mode == controls::draft::NoiseReductionModeOff) ||
        (mode == controls::draft::NoiseReductionModeZSL)) {
        nrMode = icamera::NR_MODE_LEVEL_4;
    } else if (mode == controls::draft::NoiseReductionModeFast &&
        context->mAiqParams.frameUsage == icamera::FRAME_USAGE_STILL) {
        nrMode = icamera::NR_MODE_LEVEL_3;
    } else if (mode == controls::draft::NoiseReductionModeHighQuality &&
        context->mAiqParams.frameUsage != icamera::FRAME_USAGE_STILL) {
        nrMode = icamera::NR_MODE_LEVEL_1;
    }

    context->mIspParams.nrMode = nrMode;
}

void ParameterConverter::convertTonemapControls(const ControlList& controls,
                                                icamera::DataContext* context) {
    if (!context) return;

    uint8_t mode = controls.get(controls::TonemapMode).value_or(controls::ToneMapModeContrastCurve);
    camera_tonemap_mode_t tonemapMode;
    int ret = getHalValue(mode, tonemapModesTable, ARRAY_SIZE(tonemapModesTable), &tonemapMode);
    if (ret == icamera::OK) {
        context->mAiqParams.tonemapMode = tonemapMode;
    }

    if (context->mAiqParams.tonemapMode != TONEMAP_MODE_CONTRAST_CURVE) return;

    icamera::camera_tonemap_curves_t curves;
    const auto& redCurve = controls.get(controls::TonemapCurveRed);
    if (redCurve) {
        curves.rSize = redCurve->size();
        curves.rCurve = redCurve->data();
    }
    const auto& greenCurve = controls.get(controls::TonemapCurveGreen);
    if (greenCurve) {
        curves.gSize = greenCurve->size();
        curves.gCurve = greenCurve->data();
    }
    const auto& blueCurve = controls.get(controls::TonemapCurveBlue);
    if (blueCurve) {
        curves.bSize = blueCurve->size();
        curves.bCurve = blueCurve->data();
    }

    if (curves.rSize > 0 && curves.gSize > 0 && curves.bSize > 0) {
        if (context->mAiqParams.tonemapMode == icamera::TONEMAP_MODE_CONTRAST_CURVE) {
            if (curves.rSize > DEFAULT_TONEMAP_CURVE_POINT_NUM ||
                curves.gSize > DEFAULT_TONEMAP_CURVE_POINT_NUM ||
                curves.bSize > DEFAULT_TONEMAP_CURVE_POINT_NUM) {
                LOG(IPU7, Warning) << "user curve size is too big, use default size";
            }

            int curveSize = sizeof(float) * DEFAULT_TONEMAP_CURVE_POINT_NUM;
            MEMCPY_S(&context->mAiqParams.tonemapCurveMem[0], curveSize,
                     curves.rCurve, sizeof(float) * curves.rSize);
            MEMCPY_S(&context->mAiqParams.tonemapCurveMem[DEFAULT_TONEMAP_CURVE_POINT_NUM],
                     curveSize, curves.gCurve, sizeof(float) * curves.gSize);
            MEMCPY_S(&context->mAiqParams.tonemapCurveMem[DEFAULT_TONEMAP_CURVE_POINT_NUM * 2],
                     curveSize, curves.bCurve, sizeof(float) * curves.bSize);
            context->mAiqParams.tonemapCurves.rSize
                = std::min(DEFAULT_TONEMAP_CURVE_POINT_NUM, curves.rSize);
            context->mAiqParams.tonemapCurves.gSize
                = std::min(DEFAULT_TONEMAP_CURVE_POINT_NUM, curves.gSize);
            context->mAiqParams.tonemapCurves.bSize
                = std::min(DEFAULT_TONEMAP_CURVE_POINT_NUM, curves.bSize);
        } else {
            context->mAiqParams.tonemapCurves.rSize = 0;
            context->mAiqParams.tonemapCurves.gSize = 0;
            context->mAiqParams.tonemapCurves.bSize = 0;
        }
    }
}

void ParameterConverter::controls2DataContext(int cameraId, const ControlList& controls,
                                              DataContext* context) {
    if (!context) return;

    auto metadata = PlatformData::getStaticMetadata(cameraId);

    dumpControls(controls);

    uint8_t controlMode = controls.get(controls::Mode3A).value_or(controls::Mode3AAuto);
    // AE
    auto aeEnable = controls.get(controls::AeEnable).value_or(true);
    auto aeMode = controls.get(controls::AeMode).value_or(controls::AeModeOn);
    context->mAiqParams.aeForceLock = controls.get(controls::AeLocked).value_or(false);
    context->mAiqParams.aeMode =
        !aeEnable || controlMode == controls::Mode3AOff || aeMode == controls::AeModeOff
            ? AE_MODE_MANUAL
            : AE_MODE_AUTO;

    const auto& ev = controls.get(controls::ExposureValue);
    if (ev && metadata->mEvStep.size() == 2 && metadata->mEvStep[1]) {
        float step = static_cast<float>(metadata->mEvStep[0]) / metadata->mEvStep[1];
        context->mAiqParams.evShift = *ev;
        context->mAiqParams.evSetting = *ev / step;
    }

    const auto& antibandingMode = controls.get(controls::AeAntiBandingMode);
    if (antibandingMode) {
        if (!getHalValue(*antibandingMode, antibandingModeMap, ARRAY_SIZE(antibandingModeMap),
                         &context->mAiqParams.antibandingMode)) {
            context->mAiqParams.antibandingMode = ANTIBANDING_MODE_OFF;
        }
    }

    if (context->mAiqParams.aeMode == AE_MODE_MANUAL) {
        const auto& et = controls.get(controls::ExposureTime);
        if (et) {
            context->mAiqParams.manualExpTimeUs = *et;
        }

        const auto& ag = controls.get(controls::AnalogueGain);
        if (ag) {
            /* libcamera use AnalogueGain to set the sensitivity, and DigitalGain is not used
            ** so we set AnalogueGain value to ISO
            */
            context->mAiqParams.manualIso = *ag;
        }
    }

    // controls::AeMeteringMode
    // controls::AeConstraintMode
    // controls::AeExposureMode
    // controls::draft::AePrecaptureTrigger);

    // AWB
    // controls::AwbEnable not set by libcamera
    // controls::AwbLocked
    // controls::ColourGains
    // controls::ColourTemperature
    // controls::draft::ColorCorrectionAberrationMode

    const auto awbMode = controls.get(controls::AwbMode).value_or(controls::AwbAuto);
    if (controlMode == controls::Mode3AOff) {
        context->mAiqParams.awbMode = AWB_MODE_MANUAL_COLOR_TRANSFORM;
    } else if (!getHalValue(awbMode, awbModeMap, ARRAY_SIZE(awbModeMap),
                            &context->mAiqParams.awbMode)) {
        context->mAiqParams.awbMode = AWB_MODE_AUTO;
    }

    context->mAiqParams.awbForceLock = controls.get(controls::AwbLocked).value_or(false);

    const float ccMatrixDefault[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    const auto& ccMatrix = controls.get(controls::ColourCorrectionMatrix).value_or(ccMatrixDefault);
    if (ccMatrix.size() == 9) {
        for (size_t i = 0; i < 9; i++) {
            context->mAiqParams.manualColorMatrix.color_transform[i / 3][i % 3] = ccMatrix[i];
        }
    }

    const float gainDefaultValues[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const auto& ccGains = controls.get(controls::ColorCorrectionGains).value_or(gainDefaultValues);
    if (ccGains.size() == 4) {
        for (size_t i = 0; i < 4; i++) {
            context->mAiqParams.manualColorGains.color_gains_rggb[i] = ccGains[i];
        }
    }
    // AF
    const auto afMode = controls.get(controls::AfMode).value_or(controls::AfModeAuto);
    if (controlMode == controls::Mode3AOff) {
        context->mAiqParams.afMode = AF_MODE_OFF;
    } else if (!getHalValue(afMode, afModeMap, ARRAY_SIZE(afModeMap),
                            &context->mAiqParams.afMode)) {
        context->mAiqParams.afMode = AF_MODE_AUTO;
    }

    const auto& afWindows = controls.get(controls::AfWindows);
    if (afWindows) {
        context->mAiqParams.afRegions.clear();
        for (auto const& win : *afWindows) {
            if (win.isNull()) continue;
            camera_window_t w;
            w.left = win.x;
            w.top = win.y;
            w.right = win.x + win.width;
            w.bottom = win.y + win.height;
            w.weight = 1;
            context->mAiqParams.afRegions.push_back(w);
        }
    }

    const auto& afTrigger = controls.get(controls::AfTrigger);
    if (afTrigger) {
        context->mAiqParams.afTrigger =
            (*afTrigger == controls::AfTriggerStart) ? AF_TRIGGER_START : AF_TRIGGER_CANCEL;
    }

    // controls::AfRange
    // controls::AfSpeed
    // controls::AfMetering
    // controls::AfPause

    // controls::FocusFoM

    // Advanced
    // controls::Brightness
    // controls::Contrast
    // controls::Lux
    // controls::Saturation
    // controls::Sharpness

    convertNRControls(controls, context);

    // Sensor
    // controls::SensorBlackLevels
    // controls::FrameDuration
    const auto& frameDurationLimits = controls.get(controls::FrameDurationLimits);
    if (frameDurationLimits) {
        context->mAiqParams.aeFpsRange.min = 1'000'000 / frameDurationLimits->data()[1];
        context->mAiqParams.aeFpsRange.max = 1'000'000 / frameDurationLimits->data()[0];
    }

    const auto& testM = controls.get(controls::draft::TestPatternMode);
    if (testM) {
        camera_test_pattern_mode_t testMode = TEST_PATTERN_OFF;
        if (getHalValue(*testM, testPatternMap, ARRAY_SIZE(testPatternMap), &testMode)) {
            context->mAiqParams.testPatternMode = testMode;
        }
    }

    // Lens
    if (afMode == AF_MODE_OFF) {
        const auto& lensPos = controls.get(controls::LensPosition);
        if (lensPos) {
            context->mAiqParams.focusDistance = *lensPos;
        }
    }

    const auto& sM = controls.get(controls::draft::LensShadingMapMode);
    if (sM) {
        context->mAiqParams.lensShadingMapMode =
            (*sM == controls::draft::LensShadingMapModeOn) ? LENS_SHADING_MAP_MODE_ON
                                                           : LENS_SHADING_MAP_MODE_OFF;
    }

    // Others
    // controls::ScalerCrop
    const auto& scalerCrop = controls.get<Rectangle>(controls::ScalerCrop).value_or(Rectangle());
    context->zoomRegion = {scalerCrop.x, scalerCrop.y, static_cast<int32_t>(scalerCrop.width),
                           static_cast<int32_t>(scalerCrop.height), context->zoomRegion.ratio};

    // controls::FaceDetectMode
    auto faceMode = controls.get(controls::FaceDetectMode);
    if (faceMode) {
        camera_statistics_face_detect_mode_t fdMode = CAMERA_STATISTICS_FACE_DETECT_MODE_OFF;
        getHalValue(*faceMode, faceDetectModeMap, ARRAY_SIZE(faceDetectModeMap), &fdMode);
        context->mFaceDetectMode = static_cast<uint8_t>(fdMode);
    }

    convertEdgeControls(controls, context);

    convertTonemapControls(controls, context);

    context->mAiqParams.dump();
}

void ParameterConverter::convertFaceParameters(const FaceDetectionResult* faceResult,
                                               const DataContext* context, ControlList& controls) {
    if (faceResult != nullptr &&
        context->mFaceDetectMode ==
            static_cast<uint8_t>(CAMERA_STATISTICS_FACE_DETECT_MODE_SIMPLE)) {
        std::vector<Rectangle> rectangles;
        std::vector<uint8_t> scores;
        for (int i = 0; i < faceResult->ccaFaceState.num_faces; i++) {
            Rectangle rect(
                faceResult->faceRect[i * RECT_SIZE + 0], faceResult->faceRect[i * RECT_SIZE + 1],
                faceResult->faceRect[i * RECT_SIZE + 2], faceResult->faceRect[i * RECT_SIZE + 3]);
            rectangles.push_back(rect);
            scores.push_back(faceResult->faceScores[i]);
        }

        controls.set(controls::FaceDetectFaceRectangles, rectangles);
        controls.set(controls::FaceDetectFaceScores, scores);
    }
}

void ParameterConverter::convertColorCorrectionParameter(const icamera::AiqResult* aiqResult,
                                                         ControlList& controls) {
    std::array<float, 9> correctionMatrix;
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            correctionMatrix[i * 3 + j] = aiqResult->mPaResults.color_conversion_matrix[i][j];
        }
    }
    controls.set(controls::ColourCorrectionMatrix, correctionMatrix);

    std::array<float, 4> correctionGains;
    correctionGains[0] = aiqResult->mPaResults.color_gains.r;
    correctionGains[1] = aiqResult->mPaResults.color_gains.gr;
    correctionGains[2] = aiqResult->mPaResults.color_gains.gb;
    correctionGains[3] = aiqResult->mPaResults.color_gains.b;
    controls.set(controls::ColorCorrectionGains, correctionGains);
}

void ParameterConverter::dataContext2Controls(int cameraId, const DataContext* context,
                                              const FaceDetectionResult* faceResult,
                                              const AiqResult* aiqResult, ControlList& controls) {
    if (!(aiqResult && context)) return;

    // AE
    controls.set(controls::ExposureTime,
                 aiqResult->mAeResults.exposures[0].exposure[0].exposure_time_us);
    // set to ANDROID_SENSOR_SENSITIVITY
    controls.set(controls::AnalogueGain,
                 aiqResult->mAeResults.exposures[0].exposure[0].iso);
    controls.set(controls::DigitalGain,
                 aiqResult->mAeResults.exposures[0].exposure[0].digital_gain);

    controls.set(controls::AeAntiBandingMode,
                 static_cast<int32_t>(context->mAiqParams.antibandingMode));
    // controls::AeFlickerDetected

    convertColorCorrectionParameter(aiqResult, controls);

    // controls::ColourTemperature

    int32_t awbMode = controls::AwbAuto;
    getCtlValue(context->mAiqParams.awbMode, awbModeMap, ARRAY_SIZE(awbModeMap), &awbMode);
    controls.set(controls::AwbMode, awbMode);

    // controls::AfPauseState

    // lens
    controls.set(controls::LensFocusDistance, context->mAiqParams.focusDistance);

    // Sensor
    controls.set(controls::FrameDuration, aiqResult->mFrameDuration * 1000); // us -> ns
    // controls::SensorTimestamp: done in shutterReady()
    controls.set(controls::draft::SensorRollingShutterSkew, aiqResult->mFrameDuration);

    Rectangle crop = {context->zoomRegion.left, context->zoomRegion.top,
                      static_cast<uint32_t>(context->zoomRegion.right - context->zoomRegion.left),
                      static_cast<uint32_t>(context->zoomRegion.bottom - context->zoomRegion.top)};
    controls.set(controls::ScalerCrop, crop);

    int32_t testPatternMode = controls::draft::TestPatternModeOff;
    getCtlValue(context->mAiqParams.testPatternMode, testPatternMap, ARRAY_SIZE(testPatternMap),
                &testPatternMode);
    controls.set(controls::draft::TestPatternMode, testPatternMode);

    convertFaceParameters(faceResult, context, controls);
}

void ParameterConverter::dumpControls(const ControlList& controls) {
    const ControlIdMap* idM = controls.idMap();
    const ControlInfoMap* infoM = controls.infoMap();

    if (idM) dumpControlIdMap(*idM);
    if (infoM) dumpControlInfoMap(*infoM);

    LOG(IPU7, Debug) << "count: " << controls.size();
    for (const auto &ctrl : controls) {
        const ControlValue& val = controls.get(ctrl.first);
        LOG(IPU7, Debug) << "    " << ctrl.first << ": " << val.toString();
    }
}

void ParameterConverter::dumpControlInfoMap(const ControlInfoMap& controls) {
    LOG(IPU7, Debug) << "count: " << controls.size();
    for (auto const& ctl : controls) {
        const ControlId* id = ctl.first;
        const ControlInfo& info = ctl.second;

        const ControlValue& defV = info.def();
        LOG(IPU7, Debug) << "    " << id->id() << ": " << id->name() << ": " << info.toString()
                         << ", def " << defV.toString();
        const std::vector<ControlValue>& vals = info.values();
        for (auto const& val : vals) {
            LOG(IPU7, Debug) << "        val: " << val.toString();
        }
    }
}

void ParameterConverter::dumpControlIdMap(const ControlIdMap& ids) {
    LOG(IPU7, Debug) << "count " << ids.size();
    for (auto const& id : ids) {
        unsigned int index = id.first;
        const ControlId* val = id.second;
        const char* type = val->type() == ControlTypeBool ? "bool"
                         : val->type() == ControlTypeByte ? "byte"
                         : val->type() == ControlTypeInteger32 ? "int32"
                         : val->type() == ControlTypeInteger64 ? "int64"
                         : val->type() == ControlTypeFloat ? "float"
                         : val->type() == ControlTypeString ? "string"
                         : val->type() == ControlTypeRectangle ? "rect"
                         : val->type() == ControlTypeSize ? "size"
                         : "none";
        LOG(IPU7, Debug) << "    " << index << ": " << val->id() << ": " << val->name() << ", type "
                         << type;
    }
}

} /* namespace libcamera */
