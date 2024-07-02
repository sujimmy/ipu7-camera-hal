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

#pragma once

#include <libcamera/controls.h>
#include "CameraContext.h"
#include "AiqResult.h"
#include "FaceType.h"

namespace libcamera {

class ParameterConverter {
 public:
    static void initializeCapabilities(int cameraId, const ControlList& properties,
                                       ControlInfoMap::Map& controls);
    static void initProperties(int cameraId, ControlList& properties);

    static void controls2DataContext(int cameraId, const ControlList& controls,
                                     icamera::DataContext* context);
    static void dataContext2Controls(int cameraId, const icamera::DataContext* context,
                                     const icamera::FaceDetectionResult* faceResult,
                                     const icamera::AiqResult* aiqResult, ControlList& controls);

    static void dumpControls(const ControlList& controls);
    static void dumpControlInfoMap(const ControlInfoMap& controls);
    static void dumpControlIdMap(const ControlIdMap& ids);

 private:
    static void fillLensStaticMetadata(int cameraId, ControlInfoMap::Map& controls);
    static void convertEdgeControls(const ControlList& controls, icamera::DataContext* context);
    static void convertTonemapControls(const ControlList& controls, icamera::DataContext* context);
    static void convertNRControls(const ControlList& controls, icamera::DataContext* context);
    static void convertFaceParameters(const icamera::FaceDetectionResult* faceResult,
                                      const icamera::DataContext* context, ControlList& controls);
    static void convertColorCorrectionParameter(const icamera::AiqResult* aiqResult,
                                                ControlList& controls);
};

} /* namespace libcamera */
