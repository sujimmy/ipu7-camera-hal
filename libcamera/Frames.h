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

#include "ParamDataType.h"

namespace libcamera {

class FrameBuffer;
class Request;

struct Info {
    unsigned int id;
    Request* request;

    std::set<int> outBuffers;
    std::set<int> inBuffer;
    bool metadataReady;
    bool shutterReady;

    Info() {
        id = 0;
        request = nullptr;

        metadataReady = false;
        shutterReady = false;
    }
};

class IPU7Frames {
 public:
    IPU7Frames();
    ~IPU7Frames();

    void clear();

    Info* create(Request* request);
    void remove(Info* info);

    void processRequest(Info* info);
    bool getBuffer(Info* info, const icamera::stream_t& halStream, FrameBuffer* frameBuffer,
                   icamera::camera_buffer_t* buf);
    void returnRequest(Info* info);

    void updateShutterInfo(unsigned int frameNumber, uint64_t timestamp);
    void upateMetadataInfo(unsigned int frameNumber, int64_t sequence);

 private:
    mutable Mutex mMutex;

    static const uint8_t kMaxProcessingRequest = 10;
    Info mRequestBuffers[kMaxProcessingRequest];
    std::queue<Info*> mAvailableRequestBuffers;

    std::map<unsigned int, Info*> mProcessingRequests;
};

class IPU7Results : public icamera::camera_callback_ops_t {
 public:
    IPU7Results(IPU7Frames* frames);
    ~IPU7Results();

    Signal<unsigned int, int64_t> mMetadataAvailable;
    Signal<unsigned int> mBufferAvailable;

 private:
    static void notifyCallback(const icamera::camera_callback_ops_t* cb,
                               const icamera::camera_msg_data_t& data);

    void handleEvent(const icamera::camera_msg_data_t& data);

    void shutterDone(unsigned int frameNumber, uint64_t timestamp);
    void metadataDone(unsigned int frameNumber, int64_t sequence);
    void bufferDone(unsigned int streamId);

    IPU7Frames* mIPU7Frames;
};

} /* namespace libcamera */
