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
#include <cros-camera/camera_face_detection.h>

#include "FaceDetection.h"

namespace icamera {

class FaceSSD : public FaceDetection {
 public:
    FaceSSD(int cameraId, int width, int height);
    virtual ~FaceSSD();

    virtual void runFaceDetection(const std::shared_ptr<CameraBuffer>& camBuffer);

 private:
    DISALLOW_COPY_AND_ASSIGN(FaceSSD);

    int initFaceDetection();
    void updateFaceResult(const std::vector<human_sensing::CrosFace>& result, int64_t sequence);
    void updateFaceResult(const FaceDetectionResult& result, int64_t sequence);

 private:
    std::unique_ptr<cros::FaceDetector> mFaceDetector;
};

}  // namespace icamera
