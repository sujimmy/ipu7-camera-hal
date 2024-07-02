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

#include <libcamera/request.h>

#include <mutex>
#include <queue>

#include "CameraHal.h"
#include "FileSource.h"
#include "PnpDebugControl.h"
#include "libcamera/internal/pipeline_handler.h"
#include "libcamera/internal/framebuffer.h"

namespace libcamera {

class MockCameraHal : public icamera::Thread, public CameraHal {
 public:
    MockCameraHal(int32_t cameraId, PipelineHandler* pipe);
    virtual ~MockCameraHal();
    virtual int configure(icamera::stream_config_t* streamList);

    virtual int start();
    virtual int stop();
    virtual void callbackRegister(const icamera::camera_callback_ops_t* callback) {}
    virtual void processControls(Request* request, bool isStill = false);
    virtual int qbuf(icamera::camera_buffer_t** ubuffer, int bufferNum) { return 0; }
    virtual int dqbuf(int streamId, icamera::camera_buffer_t** ubuffer) { return 0; }

 private:
    virtual bool threadLoop();
    void completeRequest(Request* request);

 private:
    static const int kMaxOutputBuffers = 6;
    static const int kStartingFrameCount = 100;
    // const uint64_t kMaxDuration = 2000000000;  // 2000ms
    int mFrameIntervalUs;
    icamera::FileSourceFromDir* mFileSource;
    const char* PNP_INJECTION_NAME = "/run/camera/libcamera/";
    std::mutex mLock;
    std::queue<Request *> mPendingRequests;
    PipelineHandler* mPipe;
    uint64_t mTimestamp;
};

} /* namespace libcamera */
