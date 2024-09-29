/*
 * Copyright (C) 2019-2024 Intel Corporation
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

#define LOG_TAG FaceDetection
#include "src/fd/FaceDetection.h"

#include "AiqUtils.h"
#include "PlatformData.h"
#include "iutils/CameraLog.h"

namespace icamera {
#define FPS_FD_COUNT 60  // the face detection interval to print fps

FaceDetection::FaceDetection(int cameraId, int width, int height, int memoryType)
        : mCameraId(cameraId),
          mInitialized(false),
          mWidth(width),
          mHeight(height),
          mMaxFaceNum(PlatformData::getMaxFaceDetectionNumber(cameraId)),
          mMemoryType(memoryType),
          mLastFaceNum(-1),
          mDefaultInterval(PlatformData::faceEngineRunningInterval(cameraId)),
          mNoFaceInterval(PlatformData::faceEngineRunningIntervalNoFace(cameraId)),
          mRunInterval(PlatformData::faceEngineRunningInterval(cameraId)) {
    LOG1("<id%d> default interval:%d, no face interval:%d", cameraId,
         mDefaultInterval, mNoFaceInterval);
    initRatioInfo(&mRatioInfo);
}

FaceDetection::~FaceDetection() {
    LOG1("<id%d> @%s", mCameraId, __func__);
}

void FaceDetection::initRatioInfo(struct RatioInfo* ratioInfo) {
    CLEAR(*ratioInfo);
    // construct android coordinate based on active pixel array
    camera_coordinate_system_t activePixelArray = PlatformData::getActivePixelArray(mCameraId);

    int verticalCrop = 0;
    int horizontalCrop = 0;
    bool imageRotationChanged = false;
    int activeHeight = activePixelArray.bottom - activePixelArray.top;
    int activeWidth = activePixelArray.right - activePixelArray.left;

    // do extra conversion if the image ratio is not the same ratio with the android coordinate.
    if (mHeight * activeWidth != mWidth * activeHeight) {
        imageRotationChanged = true;
        int gap = (mWidth * activeHeight / activeWidth) - mHeight;

        if (gap > 0) {
            // vertical crop pixel
            verticalCrop = gap;
        } else if (gap < 0) {
            // horizontal crop pixel
            horizontalCrop = mHeight * activeWidth / activeHeight - mWidth;
        }
    }
    LOG2("%s, face info(%dx%d), active info(%dx%d), crop info(v: %d, h: %d), ratio changed: %d",
         __func__, mHeight, mWidth, activeWidth, activeHeight, verticalCrop, horizontalCrop,
         imageRotationChanged);

    *ratioInfo = {{0, 0, activeWidth, activeHeight}, verticalCrop, horizontalCrop,
                  imageRotationChanged};
}

bool FaceDetection::needRunFace(int64_t sequence) {
    if (!mInitialized) return false;

    int lastFaceNum = getFaceNum();

    /*
     * FD runs 1 frame every mRunInterval frames.
     * And the default value of mRunInterval is mDefaultInterval
     */
    if (sequence % mRunInterval == 0) {
        LOG2("%s, Running face detection for sequence: %ld faceNum %d",
             __func__, sequence, lastFaceNum);
        return true;
    }

    /*
     * When face doesn't be detected during mFDRunIntervalNoFace's frame,
     * we may change FD running's interval frames.
     */
    if (mNoFaceInterval > mDefaultInterval) {
        if (lastFaceNum == 0) {
            if (sequence % mNoFaceInterval == 0) {
                // Change the interval if there isn't face detected for mNoFaceInterval frames
                // (mNoFaceInterval / mDefaultInterval) times.
                mRunInterval = mNoFaceInterval;
            }
        } else if (mRunInterval != mDefaultInterval) {
            // Recovery to default normal interval
            mRunInterval = mDefaultInterval;
        }
    }

    return false;
}

int FaceDetection::getFaceNum() {
    AutoMutex l(mFaceResultLock);
    return mLastFaceNum;
}

void FaceDetection::printfFDRunRate() {
    if (!Log::isLogTagEnabled(ST_FPS)) return;

    static int runCount = 0;
    static timeval lastTime = {};
    if (runCount == 0) {
        gettimeofday(&lastTime, nullptr);
    }

    if (++runCount % FPS_FD_COUNT != 0) return;

    struct timeval curTime;
    gettimeofday(&curTime, nullptr);
    int duration = static_cast<int>(curTime.tv_usec - lastTime.tv_usec +
                                    ((curTime.tv_sec - lastTime.tv_sec) * 1000000));
    float curFps = static_cast<float>(1000000) / static_cast<float>(duration / FPS_FD_COUNT);
    LOG2(ST_FPS, "@%s, face detection fps: %02f", __func__, curFps);
    lastTime = curTime;
}

void FaceDetection::convertFaceCoordinate(camera_coordinate_system_t& sysCoord, int* left, int* top,
                                    int* right, int* bottom) {
    int verticalCrop = mRatioInfo.verticalCrop;
    int horizontalCrop = mRatioInfo.horizontalCrop;
    bool imageRotationChanged = mRatioInfo.imageRotationChanged;
    camera_coordinate_t srcCoord = {0, 0};
    camera_coordinate_t destCoord = {0, 0};
    const camera_coordinate_system_t fillFrameCoord = {0, 0, mWidth + horizontalCrop,
                                                       mHeight + verticalCrop};

    if (imageRotationChanged) {
        camera_coordinate_t pointCoord = {0, 0};
        pointCoord.x = *left + (horizontalCrop / 2);
        pointCoord.y = *top + (verticalCrop / 2);
        destCoord = AiqUtils::convertCoordinateSystem(fillFrameCoord, sysCoord, pointCoord);
        *left = destCoord.x;  // rect.left
        *top = destCoord.y;   // rect.top
        pointCoord.x = *right + (horizontalCrop / 2);
        pointCoord.y = *bottom + (verticalCrop / 2);
        destCoord = AiqUtils::convertCoordinateSystem(fillFrameCoord, sysCoord, pointCoord);
        *right = destCoord.x;   // rect.right
        *bottom = destCoord.y;  // rect.bottom
    } else {
        srcCoord = {*left, *top};
        destCoord = AiqUtils::convertCoordinateSystem(fillFrameCoord, sysCoord, srcCoord);
        *left = destCoord.x;  // rect.left
        *top = destCoord.y;   // rect.top
        srcCoord = {*right, *bottom};
        destCoord = AiqUtils::convertCoordinateSystem(fillFrameCoord, sysCoord, srcCoord);
        *right = destCoord.x;   // rect.right
        *bottom = destCoord.y;  // rect.bottom
    }
}
}  // namespace icamera
