/*
 * Copyright (C) 2021-2022 Intel Corporation
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
#ifdef ENABLE_SANDBOXING
#include "modules/sandboxing/client/IntelFaceDetectionClient.h"
#else
#include "modules/algowrapper/IntelFaceDetection.h"
#endif

#include "FaceDetection.h"
#include "modules/sandboxing/IPCIntelFD.h"

namespace icamera {

class FaceDetectionPVL : public FaceDetection {
 public:
    FaceDetectionPVL(int cameraId, int width, int height);
    virtual ~FaceDetectionPVL();

    virtual void runFaceDetection(const std::shared_ptr<CameraBuffer>& camBuffer);

 private:
    DISALLOW_COPY_AND_ASSIGN(FaceDetectionPVL);

    int initFaceDetection();
    void deinitFaceDetection();
    void updateFaceResult(const FaceDetectionPVLResult& result, int64_t sequence);

 private:
    std::unique_ptr<IntelFaceDetection> mFace;
};

}  // namespace icamera
