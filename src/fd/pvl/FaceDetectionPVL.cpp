/*
 * Copyright (C) 2021-2025 Intel Corporation
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

#define LOG_TAG FaceDetectionPVL
#include "src/fd/pvl/FaceDetectionPVL.h"

#include "AiqResultStorage.h"
#include "CameraContext.h"
#include "iutils/CameraLog.h"
#include "iutils/Errors.h"
#include "iutils/Utils.h"

namespace icamera {

using namespace std;

FaceDetectionPVL::FaceDetectionPVL(int cameraId, int width, int height)
        : FaceDetection(cameraId, width, height),
          mFace(nullptr) {
    int ret = initFaceDetection();
    CheckAndLogError(ret != OK, VOID_VALUE, "failed to init face detection, ret %d", ret);
}

FaceDetectionPVL::~FaceDetectionPVL() {
    LOG1("<id%d> @%s", mCameraId, __func__);
    deinitFaceDetection();
}

void FaceDetectionPVL::deinitFaceDetection() {
    if (mFace) {
        FaceDetectionDeinitParams params;
        params.cameraId = mCameraId;
        mFace->deinit(&params, sizeof(FaceDetectionDeinitParams));
    }
    mInitialized = false;
}

int FaceDetectionPVL::initFaceDetection() {
    if (mMaxFaceNum > MAX_FACES_DETECTABLE || mWidth > MAX_FACE_FRAME_WIDTH ||
        mHeight > MAX_FACE_FRAME_HEIGHT) {
        LOGW("%s, face number or frame size is too big. Don't run face", __func__,
             mMaxFaceNum, mWidth, mHeight);
        return OK;
    }

    /* init IntelFaceDetection */
    FaceDetectionInitParams params;
    params.max_face_num = mMaxFaceNum;
    params.cameraId = mCameraId;
    mFace = std::unique_ptr<IntelFaceDetection>(new IntelFaceDetection());
    int ret = mFace->init(&params, sizeof(FaceDetectionInitParams));
    CheckAndLogError(ret != OK, ret, "mFace init failed, ret %d", ret);

    mInitialized = true;
    return OK;
}

void FaceDetectionPVL::runFaceDetection(const shared_ptr<CameraBuffer>& camBuffer) {
    CheckAndLogError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __func__);
    CheckAndLogError(!camBuffer, VOID_VALUE, "@%s, camBuffer buffer is nullptr", __func__);

    uint64_t sequence = camBuffer->getSequence();
    LOG2("<seq:%ld>@%s", sequence, __func__);
    FaceDetectionRunParams* params = mFace->getRunBuffer();
    CheckAndLogError(!params, VOID_VALUE, "Failed to get buffer for running face");

    params->size = camBuffer->getBufferSize();
    params->width = mWidth;
    params->height = mHeight;
    params->rotation = 0;  // Doesn't support dynamic rotation
    params->format = pvl_image_format_nv12;
    params->stride = camBuffer->getStride();
    params->bufferHandle = -1;
    params->cameraId = mCameraId;

    nsecs_t startTime = CameraUtils::systemTime();
    int ret = mFace->run(params, sizeof(FaceDetectionRunParams), camBuffer->getBufferAddr());
    CheckAndLogError(ret != OK, VOID_VALUE, "%s, Failed to run face for sequence: %ld",
                     __func__, sequence);

    printfFDRunRate();
    LOG2("@%s: ret:%d, mFace runs %ums", __func__, ret,
         (unsigned)((CameraUtils::systemTime() - startTime) / 1000000));

    updateFaceResult(params->results, sequence);
}

void FaceDetectionPVL::updateFaceResult(const FaceDetectionPVLResult& result, int64_t sequence) {
    camera_coordinate_system_t sysCoord = {IA_COORDINATE_LEFT, IA_COORDINATE_TOP,
                                           IA_COORDINATE_RIGHT, IA_COORDINATE_BOTTOM};

    auto cameraContext = CameraContext::getInstance(mCameraId);
    auto aiqResultStorage = cameraContext->getAiqResultStorage();

    FaceDetectionResult* buf = aiqResultStorage->acquireFaceResult();
    CLEAR(*buf);
    buf->ccaFaceState.updated = true;
    buf->ccaFaceState.is_video_conf = true;
    buf->ccaFaceState.num_faces = CLIP(result.faceNum, mMaxFaceNum, 0);

    LOG2("@<seq%ld>%s, face number: %d", sequence, __func__, buf->ccaFaceState.num_faces);
    for (int i = 0; i < result.faceNum; i++) {
        buf->ccaFaceState.faces[i].face_area.left = result.faceResults[i].rect.left;
        buf->ccaFaceState.faces[i].face_area.top = result.faceResults[i].rect.top;
        buf->ccaFaceState.faces[i].face_area.right = result.faceResults[i].rect.right;
        buf->ccaFaceState.faces[i].face_area.bottom = result.faceResults[i].rect.bottom;
        convertFaceCoordinate(sysCoord, &buf->ccaFaceState.faces[i].face_area.left,
                              &buf->ccaFaceState.faces[i].face_area.top,
                              &buf->ccaFaceState.faces[i].face_area.right,
                              &buf->ccaFaceState.faces[i].face_area.bottom);
        buf->ccaFaceState.faces[i].rip_angle = result.faceResults[i].rip_angle;
        buf->ccaFaceState.faces[i].rop_angle = result.faceResults[i].rop_angle;
        buf->ccaFaceState.faces[i].tracking_id = result.faceResults[i].tracking_id;
        buf->ccaFaceState.faces[i].confidence = result.faceResults[i].confidence;
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

        buf->faceIds[i] = result.faceResults[i].tracking_id;
        buf->faceScores[i] = result.faceResults[i].confidence;
        buf->faceRect[i * 4] = result.faceResults[i].rect.left;
        buf->faceRect[i * 4 + 1] = result.faceResults[i].rect.top;
        buf->faceRect[i * 4 + 2] = result.faceResults[i].rect.right;
        buf->faceRect[i * 4 + 3] = result.faceResults[i].rect.bottom;
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
