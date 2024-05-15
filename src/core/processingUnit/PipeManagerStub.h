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

#include <map>
#include <set>
#include <unordered_map>

#include "IPipeManager.h"

namespace icamera {

class PipeManagerStub : public IPipeManager, public icamera::Thread {
 public:
    PipeManagerStub(int cameraId, PipeManagerCallback* callback);
    virtual ~PipeManagerStub();
    virtual int configure(const std::map<uuid, stream_t>& inputInfo,
                          const std::map<uuid, stream_t>& outputInfo, ConfigMode configMode,
                          TuningMode tuningMode,
                          const std::map<uuid, stream_t>* yuvInputInfo = nullptr);
    virtual int start();
    virtual int stop();
    virtual void addTask(PipeTaskData taskParam);
    virtual TuningMode getTuningMode(int64_t sequence) { return mTuningMode; }
    virtual int prepareIpuParams(IspSettings* settings, int64_t sequence = 0,
                                 int streamId = VIDEO_STREAM_ID);

    virtual int onBufferDone(uuid port, const std::shared_ptr<CameraBuffer>& buffer);
    virtual void onMetadataReady(int64_t sequence);

 private:
    int mCameraId;
    std::map<uuid, stream_t> mInputFrameInfo;
    std::map<uuid, stream_t> mOutputFrameInfo;
    ConfigMode mConfigMode;
    TuningMode mTuningMode;
    uuid mDefaultMainInputPort;
    std::map<uuid, std::vector<int32_t>> mOutputPortToStreamIds;
    Mutex mTaskLock;
    std::vector<TaskInfo> mOngoingTasks;
    std::condition_variable mTaskReadyCondition;

    Mutex mOngoingPalMapLock;
    // first is sequence id, second is a set of stream id
    std::map<int64_t, std::set<int32_t>> mOngoingPalMap;
    PipeManagerCallback* mPipeManagerCallback;
#ifdef IPU7_SIMULATION
    // create blank stats buffer to trigger 3A
    std::shared_ptr<CameraBuffer> mStatsBuffer;
#endif
    std::shared_ptr<CameraBuffer> mIntermBuffer;
    /* for Pnp test, will only fill the first 30 frames with real data */
    static const int kStartingFrameCount = 30;

 private:
    // use a thread to simulate pipeLine process frames
    virtual bool threadLoop();
    int processTask(const PipeTaskData& task);
    int queueBuffers();
};
}  // namespace icamera
