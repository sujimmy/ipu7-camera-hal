/*
 * Copyright (C) 2021-2024 Intel Corporation
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

#define LOG_TAG FaceSSD
#include "src/fd/facessd/FaceSSD.h"

#include <vector>

#include "AiqResultStorage.h"
#include "CameraContext.h"
#include "iutils/CameraLog.h"
#include "iutils/Errors.h"
#include "iutils/Utils.h"

namespace icamera {

using namespace std;

FaceSSD::FaceSSD(int cameraId, int width, int height)
        : FaceDetection(cameraId, width, height, V4L2_MEMORY_DMABUF),
          mFaceDetector(nullptr) {
    int ret = initFaceDetection();
    CheckAndLogError(ret != OK, VOID_VALUE, "failed to init face detection, ret %d", ret);
}

FaceSSD::~FaceSSD() {
    LOG1("<id%d> @%s", mCameraId, __func__);
}

int FaceSSD::initFaceDetection() {
    if (mMaxFaceNum > MAX_FACES_DETECTABLE || mWidth > MAX_FACE_FRAME_WIDTH ||
        mHeight > MAX_FACE_FRAME_HEIGHT) {
        LOGW("%s, face number or frame size is too big. Don't run face", __func__,
             mMaxFaceNum, mWidth, mHeight);
        return OK;
    }

    mFaceDetector = cros::FaceDetector::Create();
    CheckAndLogError(!mFaceDetector, NO_INIT, "Failed to create Face SSD instance %s", __func__);

    mInitialized = true;
    return OK;
}

void FaceSSD::runFaceDetection(const shared_ptr<CameraBuffer>& camBuffer) {
    LOG2("@%s", __func__);
    CheckAndLogError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __func__);
    CheckAndLogError(!camBuffer, VOID_VALUE, "@%s, ccBuf buffer is nullptr", __func__);

    camera_buffer_t* ubuffer = camBuffer->getUserBuffer();
    auto handle = reinterpret_cast<buffer_handle_t>(static_cast<uintptr_t>(ubuffer->privateHandle));
    CheckAndLogError(!handle, VOID_VALUE, "Face SSD only supports gbm handle buffer", __func__);

    int64_t sequence = camBuffer->getSequence();
    nsecs_t startTime = CameraUtils::systemTime();
    std::vector<human_sensing::CrosFace> faces;
    cros::FaceDetectResult ret = mFaceDetector->Detect(handle, &faces);
    CheckAndLogError(ret != cros::FaceDetectResult::kDetectOk, VOID_VALUE,
                     "%s, Failed to run face for sequence: %ld", __func__, sequence);

    printfFDRunRate();
    LOG2("<seq%u>%s: ret:%d, it takes need %ums", camBuffer->getSequence(), __func__, ret,
         (unsigned)((CameraUtils::systemTime() - startTime) / 1000000));

    updateFaceResult(faces, sequence);
}

void FaceSSD::updateFaceResult(const std::vector<human_sensing::CrosFace>& result,
                               int64_t sequence) {
    camera_coordinate_system_t sysCoord = {IA_COORDINATE_LEFT, IA_COORDINATE_TOP,
                                           IA_COORDINATE_RIGHT, IA_COORDINATE_BOTTOM};
    auto cameraContext = CameraContext::getInstance(mCameraId);
    auto aiqResultStorage = cameraContext->getAiqResultStorage();

    FaceDetectionResult* buf = aiqResultStorage->acquireFaceResult();
    CLEAR(*buf);

    buf->ccaFaceState.updated = true;
    buf->ccaFaceState.is_video_conf = true;
    buf->ccaFaceState.num_faces = CLIP(result.size(), mMaxFaceNum, 0);

    LOG2("@<seq%ld>%s, face number: %d", sequence, __func__, buf->ccaFaceState.num_faces);
    for (int i = 0; i < buf->ccaFaceState.num_faces; i++) {
        buf->ccaFaceState.faces[i].face_area.left =
            static_cast<int>(result[i].bounding_box.x1);  // rect.left;
        buf->ccaFaceState.faces[i].face_area.top =
            static_cast<int>(result[i].bounding_box.y1);  // rect.top
        buf->ccaFaceState.faces[i].face_area.right =
            static_cast<int>(result[i].bounding_box.x2);  // rect.right
        buf->ccaFaceState.faces[i].face_area.bottom =
            static_cast<int>(result[i].bounding_box.y2);  // rect.bottom
        convertFaceCoordinate(sysCoord, &buf->ccaFaceState.faces[i].face_area.left,
                              &buf->ccaFaceState.faces[i].face_area.top,
                              &buf->ccaFaceState.faces[i].face_area.right,
                              &buf->ccaFaceState.faces[i].face_area.bottom);

        buf->ccaFaceState.faces[i].rip_angle = 0;
        buf->ccaFaceState.faces[i].rop_angle = 0;
        buf->ccaFaceState.faces[i].tracking_id = i;
        buf->ccaFaceState.faces[i].confidence = result[i].confidence;
        buf->ccaFaceState.faces[i].person_id = -1;
        buf->ccaFaceState.faces[i].similarity = 0;
        buf->ccaFaceState.faces[i].best_ratio = 0;
        buf->ccaFaceState.faces[i].face_condition = 0;

        buf->ccaFaceState.faces[i].smile_state = 0;
        buf->ccaFaceState.faces[i].smile_score = 0;
        buf->ccaFaceState.faces[i].mouth.x = 0;
        buf->ccaFaceState.faces[i].mouth.y = 0;
        buf->ccaFaceState.faces[i].eye_validity = 0;
        LOG2("@%s, face info for 3A, id:%d, left:%d, top:%d, right:%d, bottom:%d", __func__, i,
             buf->ccaFaceState.faces[i].face_area.left, buf->ccaFaceState.faces[i].face_area.top,
             buf->ccaFaceState.faces[i].face_area.right,
             buf->ccaFaceState.faces[i].face_area.bottom);

        buf->faceIds[i] = i;
        buf->faceScores[i] = static_cast<int>(result[i].confidence * 100);
        buf->faceRect[i * 4] = static_cast<int>(result[i].bounding_box.x1);  // rect.left;
        buf->faceRect[i * 4 + 1] = static_cast<int>(result[i].bounding_box.y1);  // rect.top
        buf->faceRect[i * 4 + 2] = static_cast<int>(result[i].bounding_box.x2);  // rect.right
        buf->faceRect[i * 4 + 3] = static_cast<int>(result[i].bounding_box.y2);  // rect.bottom
        convertFaceCoordinate(mRatioInfo.sysCoord, &buf->faceRect[i * 4],
                              &buf->faceRect[i * 4 + 1], &buf->faceRect[i * 4 + 2],
                              &buf->faceRect[i * 4 + 3]);
        LOG2("@%s, face info for app, id:%d, left:%d, top:%d, right:%d, bottom:%d", __func__, i,
             buf->faceRect[i * 4], buf->faceRect[i * 4 + 1], buf->faceRect[i * 4 + 2],
             buf->faceRect[i * 4 + 3]);
    }
    AutoMutex l(mFaceResultLock);
    mLastFaceNum = buf->ccaFaceState.num_faces;

    aiqResultStorage->updateFaceResult(sequence);
}
}  // namespace icamera
