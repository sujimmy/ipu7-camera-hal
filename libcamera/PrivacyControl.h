/*
 * Copyright (C) 2024 Intel Corporation
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

#include <libcamera/base/mutex.h>
#include <libcamera/base/thread.h>
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <sys/time.h>

#include <map>
#include <memory>
#include <queue>
#include <vector>

#include "Errors.h"
#include "HwPrivacyControl.h"
#include "ParamDataType.h"
#include "Utils.h"

namespace libcamera {

using namespace icamera;

/**
 * \class PrivacyControl
 *
 * This class is used to handle the request in privacy mode
 */
class PrivacyControl : public Thread {
 public:
    PrivacyControl(int cameraId);
    virtual ~PrivacyControl();
    int configure(stream_config_t* streamList) { return OK; }
    void callbackRegister(const camera_callback_ops_t* callback) { mCallbackOps = callback; }
    int start();
    int stop();
    int qbuf(camera_buffer_t** ubuffer, int bufferNum);
    int dqbuf(int streamId, camera_buffer_t** ubuffer);
    bool getPrivacyMode();
    void updateMetadataResult(ControlList& metadata);

 private:
    virtual void run() override;

 private:
    static const uint32_t kMaxStreamNum = 6;
    struct CaptureRequest {
        CaptureRequest() : mBufferNum(0) { CLEAR(mBuffer); }

        int mBufferNum;
        camera_buffer_t* mBuffer[kMaxStreamNum];
    };

    const int mCameraId;
    uint64_t mLastTimestamp;
    std::unique_ptr<HwPrivacyControl> mHwPrivacyControl;

    // lock for capture request and thread
    Mutex mLock;
    bool mThreadRunning;

    ConditionVariable mRequestCondition;
    ConditionVariable mResultCondition[kMaxStreamNum];
    std::queue<std::shared_ptr<CaptureRequest>> mCaptureRequest;
    // buffer result queue for each stream id
    std::map<int, std::queue<camera_buffer_t*>> mStreamQueueMap;

    const camera_callback_ops_t* mCallbackOps;
};
}  // namespace libcamera
