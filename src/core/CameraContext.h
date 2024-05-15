/*
 * Copyright (C) 2022-2023 Intel Corporation.
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

#include <map>
#include <mutex>

#include "Utils.h"
#include "AiqSetting.h"
#include "PlatformData.h"
#include "ParamDataType.h"
#include "EXIFMetaData.h"
#include "AiqResultStorage.h"

namespace icamera {

struct IspParameters {
    camera_edge_mode_t edgeMode;
    camera_nr_mode_t nrMode;
    struct NrLevel {
        bool set;
        camera_nr_level_t nrLevel;
    } nrLevel;
    camera_image_enhancement_t enhancement;

    float digitalZoomRatio;

    IspParameters() {
        CLEAR(*this);

        nrMode = NR_MODE_LEVEL_2;
    }
};

struct JpegParameters {
    double latitude;
    double longitude;
    double altitude;
    char gpsProcessingMethod[MAX_NUM_GPS_PROCESSING_METHOD + 1];
    uint8_t gpsProcessingMethodSize;
    int64_t gpsTimestamp;
    int32_t rotation;
    uint8_t jpegQuality;
    uint8_t thumbQuality;
    camera_resolution_t thumbSize;
    float focalLength;
    float aperture;

    JpegParameters() {
        CLEAR(*this);

        jpegQuality = DEFAULT_JPEG_QUALITY;
        thumbQuality = DEFAULT_JPEG_QUALITY;
    }
};

// ENABLE_EVCP_S
struct EvcpParameters {
    uint8_t eccMode;
    uint8_t bcMode;
    uint8_t ffMode;
    int brWidth;
    int brHeight;
    int brBgFd;

    EvcpParameters() {
        CLEAR(*this);
    }
};
// ENABLE_EVCP_E

class DataContext {
 public:
    int64_t mFrameNumber;
    int64_t mSequence;
    int64_t mCcaId;  // Used for CCA algo

    uint8_t mFaceDetectMode;
    camera_mono_downscale_mode_t monoDsMode;
    camera_deinterlace_mode_t deinterlaceMode;

    camera_crop_region_t cropRegion;
    camera_zoom_region_t zoomRegion;

    struct aiq_parameter_t mAiqParams;
    struct IspParameters mIspParams;
    struct JpegParameters mJpegParams;

// ENABLE_EVCP_S
    struct EvcpParameters mEvcpParams;
// ENABLE_EVCP_E

    DataContext(int cameraId) :
        mFrameNumber(-1),
        mSequence(-1),
        mCcaId(-1),
        mFaceDetectMode(0),
        monoDsMode(MONO_DS_MODE_OFF),
        deinterlaceMode(DEINTERLACE_OFF) {
        cropRegion = {0, 0, 0};
        zoomRegion = {0, 0, 0, 0, 1.0f, icamera::ROTATE_NONE};

        camera_coordinate_system_t activePixelArray = PlatformData::getActivePixelArray(cameraId);
        if ((activePixelArray.right > activePixelArray.left) &&
            (activePixelArray.bottom > activePixelArray.top)) {
            mAiqParams.resolution.width = activePixelArray.right - activePixelArray.left;
            mAiqParams.resolution.height = activePixelArray.bottom - activePixelArray.top;
        }
        const StaticMetadata *staticMetadata = PlatformData::getStaticMetadata(cameraId);
        if (staticMetadata->mEvRange.size() == 2) {
            mAiqParams.evRange = {static_cast<float>(staticMetadata->mEvRange[0]),
                                 static_cast<float>(staticMetadata->mEvRange[1])};
        }
        if (staticMetadata->mEvStep.size() == 2) {
            mAiqParams.evStep = {staticMetadata->mEvStep[0], staticMetadata->mEvStep[1]};
        };

        std::string str = "lens.info.shadingMapSize";
        auto vI = PlatformData::getInt32StaticMetadata(cameraId, str);
        if (vI.size() == 2) {
            mAiqParams.lensShadingMapSize = {vI[0], vI[1]};
        }
        str = "lens.info.minimumFocusDistance";
        auto vF = PlatformData::getFloatStaticMetadata(cameraId, str);
        if (vF.size() == 1) {
            mAiqParams.minFocusDistance = vF[0];
        }
    }
    ~DataContext() {}

    DataContext& operator=(const DataContext& other) {
        mFaceDetectMode = other.mFaceDetectMode;
        monoDsMode = other.monoDsMode;
        deinterlaceMode = other.deinterlaceMode;
        cropRegion = other.cropRegion;
        zoomRegion = other.zoomRegion;
        mAiqParams = other.mAiqParams;
        mIspParams = other.mIspParams;
        mJpegParams = other.mJpegParams;
// ENABLE_EVCP_S
        mEvcpParams = other.mEvcpParams;
// ENABLE_EVCP_E

        return *this;
    }

    void setFrameNumber(int64_t frameNumber) { mFrameNumber = frameNumber; }
    void setSequence(int64_t sequence) { mSequence = sequence; }
    void setCcaId(int64_t ccaId) { mCcaId = ccaId; }
}; /* DataContext */

class CameraContext {
 public:
    explicit CameraContext(int cameraId);
    ~CameraContext();

    static CameraContext* getInstance(int cameraId);
    static void releaseInstance(int cameraId);

    // used to save aiq, face and statistics results
    AiqResultStorage* getAiqResultStorage();

    // only called when parsing request once
    DataContext* acquireDataContext();
    void updateDataContextMapByFn(int64_t frameNumber, DataContext* context);

    // only called when raw reprocessing
    DataContext* getReprocessingDataContextBySeq(int64_t sequence);

    std::shared_ptr<GraphConfig> getGraphConfig(ConfigMode configMode);
    void storeGraphConfig(std::map<ConfigMode, std::shared_ptr<GraphConfig> > gcs) {
        AutoMutex l(mLock);
        mGraphConfigMap = gcs;
    }

    // only called when handling request once
    DataContext* acquireDataContextByFn(int64_t frameNumber);
    void updateDataContextMapBySeq(int64_t sequence, DataContext* context);
    void updateDataContextMapByCcaId(int64_t ccaId, DataContext* context);

    // called runtimely after request has been handled
    const DataContext* getDataContextBySeq(int64_t sequence);
    const DataContext* getDataContextByCcaId(int64_t ccaId);
    bool checkUserRequestBySeq(int64_t sequence);

 private:
    void eraseDataContextMap(const DataContext* context);

 private:
    static std::map<int, CameraContext*> sInstances;
    // Guard for singleton creation
    static std::mutex sLock;
    static const int kContextSize = MAX_SETTING_COUNT;

    int mCameraId;

    int mCurrentIndex;  // range from 0 to MAX_SETTING_COUNT - 1;
    DataContext* mDataContext[kContextSize];

    AiqResultStorage* mAiqResultStorage;

    std::mutex mLock;  // Guard all Maps and public APIs
    std::map<int64_t, DataContext*> mFnToDataContextMap;
    std::map<int64_t, DataContext*> mSeqToDataContextMap;
    std::map<int64_t, DataContext*> mCcaIdToDataContextMap;
    std::map<ConfigMode, std::shared_ptr<GraphConfig> > mGraphConfigMap;
}; /* CameraContext */

} /* namespace icamera */
