/*
 * Copyright (C) 2022 Intel Corporation
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
#include "Errors.h"
#include "PostProcessorCore.h"
#include "iutils/Utils.h"
#include "iutils/Errors.h"

namespace icamera {
/**
 * \class SwPostProcessUnit
 *
 * A wrapper based on PostProcessorCore for handling post-processing sequence,
 * there are two main purposes of this class.
 * 1. Provide the wrapper to implement post-processing feature.
 * 2. Parsing the processing type and formulate the processing sequence
 */
class SwPostProcessUnit {
 public:
    SwPostProcessUnit(int cameraId);
    virtual ~SwPostProcessUnit();

    status_t configure(const stream_t& srcStream, const stream_t& dstStream);
    int getPostProcessType() { return mPostProcessType; }
    int getMemoryType();
    bool isBypassed(int64_t sequence);
    status_t doPostProcessing(const std::shared_ptr<CameraBuffer>& inBuf,
                              std::shared_ptr<CameraBuffer> outBuf);

 private:
    DISALLOW_COPY_AND_ASSIGN(SwPostProcessUnit);

 private:
    int mCameraId;
    int mPostProcessType;
    std::unique_ptr<PostProcessorCore> mPostProcessorCore;
};

}  // namespace icamera
