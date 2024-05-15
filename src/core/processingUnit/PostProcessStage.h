/*
 * Copyright (C) 2022 Intel Corporation.
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

#include <map>
#include <memory>
#include <string>

#include "IPipeStage.h"
#include "SwPostProcessUnit.h"

namespace icamera {

class PostProcessStage : public IPipeStage {
 public:
    PostProcessStage(int cameraId, int stageId, const std::string& stageName);
    virtual ~PostProcessStage();

    // BufferQueue
    virtual void setFrameInfo(const std::map<uuid, stream_t>& inputInfo,
                              const std::map<uuid, stream_t>& outputInfo);
    virtual int32_t qbuf(uuid port, const std::shared_ptr<CameraBuffer>& camBuffer);

    virtual int start();
    virtual int stop() { return OK; }

    // ISchedulerNode
    virtual bool process(int64_t triggerId);

    int32_t allocateBuffers();

 private:
    void updateInfoAndSendEvents(const v4l2_buffer_t& inV4l2Buf,
                                 std::shared_ptr<CameraBuffer> outBuffer, int32_t outPort);

    bool fetchRequestBuffer(int64_t sequence, std::shared_ptr<CameraBuffer>& inBuffer);
    void returnBuffers(std::map<uuid, std::shared_ptr<CameraBuffer>>& inBuffers,
                       std::map<uuid, std::shared_ptr<CameraBuffer>>& outBuffers);

 private:
    int32_t mCameraId;
    uuid mInputPort;
    std::map<uuid, std::unique_ptr<SwPostProcessUnit>> mPostProcessors;

    // Collect all buffers for one request. Protected by mBufferQueueLock
    int32_t mOutputBuffersNum;
    std::map<uuid, std::shared_ptr<CameraBuffer>> mPendingOutBuffers;

    // Save internal buffers queued to producers. Protected by mBufferQueueLock
    std::queue<std::shared_ptr<CameraBuffer>> mQueuedInputBuffers;
};

}  // namespace icamera
