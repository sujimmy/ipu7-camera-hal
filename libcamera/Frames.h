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

#include <set>
#include <map>
#include <queue>

#include <libcamera/base/mutex.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/thread.h>

#include "ZslCapture.h"
#include "CameraTypes.h"
#include "ParamDataType.h"

namespace libcamera {

class FrameBuffer;
class Request;

struct Info {
    unsigned int id;
    Request* request;

    std::map<int, FrameBuffer*> outBuffers;
    std::map<int, FrameBuffer*> inBuffer;
    bool metadataReady;
    bool shutterReady;
    bool isStill;

    icamera::camera_buffer_t halBuffer[MAX_STREAM_NUMBER];

    Info() {
        id = 0;
        request = nullptr;

        metadataReady = false;
        shutterReady = false;
        isStill = false;

        memset(halBuffer, 0, sizeof(halBuffer));
    }
};

class IPUFrames {
 public:
    explicit IPUFrames(bool zslEnable);
    ~IPUFrames();

    void clear();

    Info* create(Request* request);
    Info* find(unsigned int frameNumber);

    bool getBuffer(Info* info, const icamera::stream_t& halStream, FrameBuffer* frameBuffer,
                   icamera::camera_buffer_t* buf);
    void recycle(Info* info);

    void shutterReady(unsigned int frameNumber, uint64_t timestamp);
    void metadataReady(unsigned int frameNumber, int64_t sequence, const ControlList& metadata);
    void bufferReady(unsigned int frameNumber, unsigned int streamId);

    Info* requestComplete(unsigned int frameNumber);

 private:
    ZslCapture* mZslCapture;

    mutable Mutex mMutex;

    static const uint8_t kMaxProcessingRequest = 10;
    Info mRequestBuffers[kMaxProcessingRequest];
    std::queue<Info*> mAvailableRequestBuffers;

    std::map<unsigned int, Info*> mProcessingRequests;
};

} /* namespace libcamera */
