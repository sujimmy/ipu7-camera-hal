/*
 * Copyright (C) 2020-2024 Intel Corporation
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

#define LOG_TAG IntelTNR7Stage

#include "src/core/processingUnit/IntelTNR7Stage.h"

#include <string>

#include "CameraContext.h"
#include "3a/AiqResultStorage.h"
#include "iutils/CameraLog.h"
#include "iutils/Utils.h"

namespace icamera {

IntelTNR7Stage* IntelTNR7Stage::createIntelTNR(int cameraId) {
    if (!PlatformData::isGpuTnrEnabled(cameraId)) {
        return nullptr;
    }
    return new IntelTNR7Stage(cameraId);
}

IntelTNR7Stage::IntelTNR7Stage(int cameraId) : mCameraId(cameraId) {
    LOG1("<id%d> %s, Construct", cameraId, __func__);
}

IntelTNR7Stage::~IntelTNR7Stage() {
    if (mIntelICBM) {
        freeAllBufs();
        icamera::ICBMReqInfo reqInfo;
        reqInfo.cameraId = mCameraId;
        reqInfo.sessionType = icamera::ICBMFeatureType::LEVEL0_TNR;
        mIntelICBM->shutdown(reqInfo);
        mIntelICBM = nullptr;
    }
    LOG1("<id%d> %s, Destroy", mCameraId, __func__);
}

int IntelTNR7Stage::init(int width, int height) {
    LOG1("<id%d> %s  %dx%d", mCameraId, __func__, width, height);
    getStillTnrTriggerInfo();

    icamera::ICBMInitInfo initParam = {.cameraId = mCameraId,
                                       .sessionType = icamera::ICBMFeatureType::LEVEL0_TNR,
                                       .libPathHandle = -1,
                                       .libPath = LIBFS_PATH};

    mWidth = width;
    mHeight = height;
    mIntelICBM = std::make_unique<IntelICBM>();

    int ret = mIntelICBM->setup(&initParam);
    if (ret != OK) {
        LOGE("%s, Failed to setup IntelICBM", __func__);
        mIntelICBM = nullptr;
    }

    return ret;
}

int IntelTNR7Stage::runTnrFrame(void* inBufAddr, void* outBufAddr, uint32_t inBufSize,
                                uint32_t outBufSize, Tnr7Param* tnrParam, int fd) {
    CheckAndLogError(mIntelICBM == nullptr, NO_INIT, "%s: No ICBM", __func__);
    ImageInfo input = {};
    input.width = mWidth;
    input.height = mHeight;
    input.size = inBufSize;
    input.stride = mWidth;
    input.bufAddr = inBufAddr;

    ImageInfo output = {};
    output.width = mWidth;
    output.height = mHeight;
    output.size = outBufSize;
    output.stride = mWidth;
    output.bufAddr = outBufAddr;

    ICBMReqInfo reqInfo;
    reqInfo.cameraId = mCameraId;
    reqInfo.sessionType = icamera::ICBMFeatureType::LEVEL0_TNR;
    reqInfo.reqType = icamera::ICBMFeatureType::LEVEL0_TNR;
    reqInfo.inII = input;
    reqInfo.outII = output;
    reqInfo.paramAddr = tnrParam;
    reqInfo.outFd = fd;

    return mIntelICBM->processFrame(reqInfo);
}

void* IntelTNR7Stage::allocCamBuf(uint32_t bufSize, int id) {
    CheckAndLogError(mIntelICBM == nullptr, nullptr, "%s: No ICBM", __func__);
    return mIntelICBM->allocBuffer(bufSize, id);
}

void IntelTNR7Stage::freeAllBufs() {
    CheckAndLogError(mIntelICBM == nullptr, VOID_VALUE, "%s: No ICBM", __func__);
    mIntelICBM->freeAllBufs();
}

Tnr7Param* IntelTNR7Stage::allocTnr7ParamBuf() {
    CheckAndLogError(mIntelICBM == nullptr, nullptr, "%s: No ICBM", __func__);
    return reinterpret_cast<Tnr7Param*>(mIntelICBM->allocBuffer(sizeof(Tnr7Param), 0xFF));
}

int IntelTNR7Stage::getStillTnrTriggerInfo(TuningMode mode) {
    IntelCca* intelCca = IntelCca::getInstance(mCameraId, mode);
    CheckAndLogError(!intelCca, UNKNOWN_ERROR, "cca is nullptr, mode:%d", mode);
    cca::cca_cmc cmc;
    ia_err ret = intelCca->getCMC(&cmc);
    CheckAndLogError(ret != ia_err_none, BAD_VALUE, "Get cmc data failed");
    mStillTnrTriggerInfo = cmc.tnr7us_trigger_info;
    LOG1("%s still tnr trigger gain num: %d threshold: %f", __func__,
         mStillTnrTriggerInfo.num_gains, mStillTnrTriggerInfo.tnr7us_threshold_gain);
    for (int i = 0; i < mStillTnrTriggerInfo.num_gains; i++) {
        LOG1("%s threshold: %f, tnr frame count: %d", __func__,
             mStillTnrTriggerInfo.trigger_infos[i].gain,
             mStillTnrTriggerInfo.trigger_infos[i].frame_count);
    }
    return OK;
}

int IntelTNR7Stage::getTotalGain(int64_t seq, float* totalGain) {
    auto cameraContext = CameraContext::getInstance(mCameraId);
    AiqResultStorage* resultStorage = cameraContext->getAiqResultStorage();
    const AiqResult* aiqResults = resultStorage->getAiqResult(seq);

    if (aiqResults == nullptr) {
        LOGW("No result for sequence %ld! use the latest instead", seq);
        aiqResults = resultStorage->getAiqResult(-1);
        CheckAndLogError((aiqResults == nullptr), INVALID_OPERATION,
                         "Cannot find available aiq result.");
    }
    *totalGain = (aiqResults->mAeResults.exposures[0].exposure->analog_gain *
                  aiqResults->mAeResults.exposures[0].exposure->digital_gain);
    LOG2("%s totalGain: %f", __func__, *totalGain);
    return OK;
}

int IntelTNR7Stage::getTnrExtraFrameCount(int64_t seq) {
    if (!mStillTnrTriggerInfo.num_gains) {
        return 0;
    }

    float totalGain = 0.0f;
    int ret = getTotalGain(seq, &totalGain);
    CheckAndLogError(ret, 0, "Failed to get total gain");

    int index = 0;
    for (int i = 1; i < mStillTnrTriggerInfo.num_gains; i++) {
        if (fabs(mStillTnrTriggerInfo.trigger_infos[i].gain - totalGain) <
            fabs(mStillTnrTriggerInfo.trigger_infos[i - 1].gain - totalGain)) {
            index = i;
        }
    }
    /* the frame_count is total tnr7 frame count, already run 1 frame */
    LOG2("%s total gain %f with tnr frame count %d", __func__, totalGain,
         mStillTnrTriggerInfo.trigger_infos[index].frame_count);
    return mStillTnrTriggerInfo.trigger_infos[index].frame_count - 1;
}
}  // namespace icamera
