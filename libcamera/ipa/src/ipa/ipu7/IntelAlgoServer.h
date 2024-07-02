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

#include <map>
#include <utility>
#include <memory>

#include <libcamera/base/span.h>
#include <libcamera/base/mutex.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/thread.h>

#include "IPAHeader.h"
#include "IntelCcaWorker.h"

namespace libcamera {

namespace ipa::ipu7 {

class IIPAIPU7Callback {
 public:
    IIPAIPU7Callback() {}
    virtual ~IIPAIPU7Callback() {}

    virtual void notifyIPACallback(int cameraId, int tuningMode, uint32_t cmd, int ret) = 0;
    virtual void* getBuffer(uint32_t bufferId) = 0;
};

/**
 * \brief The IPU7 IPA Algo Sever implementation
 *
 */
class IntelAlgoServer : public IIPAServerCallback {
 public:
    IntelAlgoServer(IIPAIPU7Callback* callback);
    ~IntelAlgoServer();

    int init(uint8_t* data);

    int sendRequest(int cameraId, int tuningMode, uint32_t cmd, const Span<uint8_t>& mem);
    void notifyCallback(int cameraId, int tuningMode, uint32_t cmd, int ret) override;
    void* getBuffer(uint32_t bufferId) override;

 private:
    IIPAIPU7Callback* mIIPAIPU7Callback;

    /* All async cmds run synchronously and are protected by mIpaLock */
    mutable Mutex mIpaLock;

    mutable Mutex mWorkersMapLock;
    std::map<std::pair<int, int>, std::unique_ptr<IntelCcaWorker>> mIntelCcaWorkers;
};

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
