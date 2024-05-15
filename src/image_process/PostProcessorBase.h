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

#include <memory>

#include "CameraBuffer.h"
#include "EXIFMetaData.h"
#include "IImageProcessor.h"
#include "IJpegEncoder.h"
#include "JpegMaker.h"
#include "iutils/Errors.h"
#include "iutils/Utils.h"

namespace icamera {

class PostProcessorBase {
 public:
    PostProcessorBase(std::string processName);
    virtual ~PostProcessorBase() {}

    std::string getName() { return mName; }

    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf) {
        return OK;
    }
    virtual bool isBypassed(int64_t sequence) { return false; }

 private:
    DISALLOW_COPY_AND_ASSIGN(PostProcessorBase);

 protected:
    std::string mName;
    std::unique_ptr<IImageProcessor> mProcessor;
};

class ScaleProcess : public PostProcessorBase {
 public:
    ScaleProcess();

    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf);
};

class RotateProcess : public PostProcessorBase {
 public:
    RotateProcess(int angle);

    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf);

 private:
    int mAngle;
};

class CropProcess : public PostProcessorBase {
 public:
    CropProcess();

    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf);
};

class ConvertProcess : public PostProcessorBase {
 public:
    ConvertProcess();

    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf);
};

class JpegProcess : public PostProcessorBase {
 public:
    JpegProcess(int cameraId);
    ~JpegProcess(){}

    virtual status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                                      std::shared_ptr<CameraBuffer>& outBuf);

 private:
    void attachJpegBlob(const EncodePackage& package);

    std::shared_ptr<CameraBuffer> cropAndDownscaleThumbnail(
        int thumbWidth, int thumbHeight, const std::shared_ptr<CameraBuffer>& inBuf);
    void fillEncodeInfo(const std::shared_ptr<CameraBuffer>& inBuf,
                        const std::shared_ptr<CameraBuffer>& outBuf,
                        EncodePackage& package);

 private:
    int mCameraId;

    std::shared_ptr<CameraBuffer> mCropBuf;
    std::shared_ptr<CameraBuffer> mScaleBuf;
    std::shared_ptr<CameraBuffer> mThumbOut;

    std::unique_ptr<JpegMaker> mJpegMaker;
    std::unique_ptr<IJpegEncoder> mJpegEncoder;
    std::unique_ptr<unsigned char[]> mExifData;
};

}  // namespace icamera
