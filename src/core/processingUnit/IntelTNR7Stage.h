/*
 * Copyright (C) 2020-2025 Intel Corporation
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

#include <vector>

#include "BufferQueue.h"
#include "CameraBuffer.h"
#include "PlatformData.h"
#include "TNRCommon.h"
#include "modules/algowrapper/IntelICBM.h"
#include "IntelCCATypes.h"
#include "src/icbm/ICBMTypes.h"

namespace icamera {

class IntelTNR7Stage {
 public:
    static IntelTNR7Stage* createIntelTNR(int cameraId);
    ~IntelTNR7Stage();
    int init(int width, int height);
    Tnr7Param* allocTnr7ParamBuf();
    int runTnrFrame(void* inBufAddr, void* outBufAddr, uint32_t inBufSize, uint32_t outBufSize,
                    Tnr7Param* tnrParam, int fd = -1);
    void* allocCamBuf(uint32_t bufSize, int id);
    void freeAllBufs();
    // tnr extra frame count depend on AE gain
    int getTnrExtraFrameCount(int64_t seq);

 private:
    explicit IntelTNR7Stage(int cameraId);
    int mCameraId;
    const char* LIBFS_PATH = "/usr/share/cros-camera/";
    std::unique_ptr<IntelICBM> mIntelICBM;
    int mWidth;
    int mHeight;
    tnr7us_trigger_info_t mStillTnrTriggerInfo;

 private:
    int getStillTnrTriggerInfo(TuningMode mode = TUNING_MODE_VIDEO);
    int getTotalGain(int64_t seq, float* totalGain);

 private:
    DISALLOW_COPY_AND_ASSIGN(IntelTNR7Stage);
};
}  // namespace icamera
