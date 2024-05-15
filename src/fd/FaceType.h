/*
 * Copyright (C) 2019-2022 Intel Corporation.
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

#include "IntelCCATypes.h"

namespace icamera {

#define RECT_SIZE 4
#define LM_SIZE 6

#define MAX_FACES_DETECTABLE 10
#define MAX_FACE_FRAME_WIDTH 1920
#define MAX_FACE_FRAME_HEIGHT 1280

#define MAX_FACE_FRAME_SIZE_ASYNC (MAX_FACE_FRAME_WIDTH * MAX_FACE_FRAME_HEIGHT)  // only using Y

/* Face Detection results */
typedef struct {
    cca::cca_face_state ccaFaceState;
    int faceIds[MAX_FACES_DETECTABLE];
    int faceLandmarks[LM_SIZE * MAX_FACES_DETECTABLE];
    int faceRect[RECT_SIZE * MAX_FACES_DETECTABLE];
    uint8_t faceScores[MAX_FACES_DETECTABLE];
    int64_t sequence;
} FaceDetectionResult;

}  // namespace icamera
