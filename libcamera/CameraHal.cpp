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

#define LOG_TAG Camera3HAL

#include "CameraHal.h"

#include <libcamera/control_ids.h>

#include "Errors.h"
#include "CameraLog.h"
#include "PlatformData.h"
#include "MediaControl.h"
#include "CameraContext.h"
#include "ParameterConverter.h"

namespace libcamera {

using namespace icamera;

void CameraHal::setup() {
    LOGI("%s: ", __func__);
    icamera::Log::setDebugLevel();
    icamera::CameraDump::setDumpLevel();
}

void CameraHal::tearDown() {
    LOGI("%s: ", __func__);
    icamera::PlatformData::releaseInstance();
}

CameraHal::CameraHal(int32_t cameraId)    : mCameraId(cameraId), mHalStatus(HAL_UNKNOWN) {
    LOG2("%s: camera %d", __func__, mCameraId);

    icamera::PlatformData::init();
    icamera::MediaControl* mc = icamera::MediaControl::getInstance();
    CheckAndLogError(!mc, VOID_VALUE, "MediaControl init failed");
    mc->resetAllLinks();

    icamera::CameraContext::getInstance(mCameraId);

    mCameraDevice = new icamera::CameraDevice(mCameraId);
    int ret = mCameraDevice->init();
    if (ret != icamera::OK) {
        LOGE("@%s, CameraDevice init failed, ret:%d", __func__, ret);
        mCameraDevice->deinit();
        return;
    }
    mCamera3AMetadata = new Camera3AMetadata(mCameraId);

    mHalStatus = HAL_INIT;
}

CameraHal::~CameraHal() {
    if (mCameraDevice) {
        mCameraDevice->deinit();
        delete mCameraDevice;
    }

    if (mCamera3AMetadata) delete mCamera3AMetadata;

    icamera::CameraContext::releaseInstance(mCameraId);
}

bool CameraHal::validate(const icamera::stream_t& stream) {
    return PlatformData::isSupportedStream(mCameraId, stream);
}

std::vector<SizeRange> CameraHal::availableStreamSizes(const PixelFormat &pixelFormat) const {
    auto staticMetadata = PlatformData::getStaticMetadata(mCameraId);
    std::vector<SizeRange> sizes;
    if (staticMetadata && staticMetadata->mConfigsArray.size() > 0) {
        // currently only NV12 format output is supported, SGRBG10 not supported in config file
        int streamFormat =
            pixelFormat == formats::SGRBG10 ? V4L2_PIX_FMT_SGBRG10 : V4L2_PIX_FMT_NV12;
        for (auto& stream : staticMetadata->mConfigsArray) {
            if (stream.format == streamFormat) {
                // size is sorted in descending order in config file
                sizes.emplace_back(Size(stream.width, stream.height));
            }
        }
    }

    return sizes;
}

int CameraHal::configure(icamera::stream_config_t *streamList) {
    return mCameraDevice->configure(streamList);
}

void CameraHal::callbackRegister(const icamera::camera_callback_ops_t* callback) {
    mCameraDevice->callbackRegister(callback);
}

int CameraHal::start() {
    return mCameraDevice->start();
}

int CameraHal::stop() {
    return mCameraDevice->stop();
}

static const std::map<controls::draft::TestPatternModeEnum,
                      icamera::camera_test_pattern_mode_t> testPatternMap = {
    {controls::draft::TestPatternModeOff, icamera::TEST_PATTERN_OFF},
    {controls::draft::TestPatternModeSolidColor, icamera::SOLID_COLOR},
    {controls::draft::TestPatternModeColorBars, icamera::COLOR_BARS},
    {controls::draft::TestPatternModeColorBarsFadeToGray, icamera::COLOR_BARS_FADE_TO_GRAY},
    {controls::draft::TestPatternModePn9, icamera::PN9},
    {controls::draft::TestPatternModeCustom1, icamera::TEST_PATTERN_CUSTOM1},
};

void CameraHal::processControls(Request* request, bool isStill) {
    if (!request) return;

    auto cameraContext = icamera::CameraContext::getInstance(mCameraId);
    icamera::DataContext* dataContext = cameraContext->acquireDataContext();

    cameraContext->updateDataContextMapByFn(request->sequence(), dataContext);

    dataContext->mAiqParams.makernoteMode = icamera::MAKERNOTE_MODE_OFF;
    if (isStill)
        dataContext->mAiqParams.frameUsage = icamera::FRAME_USAGE_STILL;
    else
        dataContext->mAiqParams.frameUsage = icamera::FRAME_USAGE_PREVIEW;

    ParameterConverter::controls2DataContext(mCameraId, request->controls(), dataContext);
}

void CameraHal::updateMetadataResult(int64_t sequence, const ControlList& controls,
                                     ControlList& metadata) {
    auto cameraContext = icamera::CameraContext::getInstance(mCameraId);
    auto dataContext = cameraContext->getDataContextBySeq(sequence);
    auto resultStorage = cameraContext->getAiqResultStorage();
    const AiqResult* aiqResult = resultStorage->getAiqResult(sequence);
    const auto faceResult = resultStorage->getFaceResult();
    mCamera3AMetadata->process3Astate(aiqResult, dataContext, controls, metadata);

    ParameterConverter::dataContext2Controls(mCameraId, dataContext, faceResult, aiqResult,
                                             metadata);
}

int CameraHal::qbuf(icamera::camera_buffer_t** ubuffer, int bufferNum) {
    return mCameraDevice->qbuf(ubuffer, bufferNum);
}

int CameraHal::dqbuf(int streamId, icamera::camera_buffer_t** ubuffer) {
    return mCameraDevice->dqbuf(streamId, ubuffer);
}

} /* namespace libcamera */
