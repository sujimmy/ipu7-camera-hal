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

#include <map>
#include <queue>
#include <string>
#include <memory>

#include <libcamera/base/span.h>
#include <libcamera/base/mutex.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/thread.h>

#include "IPAHeader.h"

namespace libcamera {

namespace ipa::ipu7 {

class IAlgoWorker {
 public:
    virtual void handleEvent(const cmd_event& event) = 0;

    IAlgoWorker() {}
    virtual ~IAlgoWorker() {}
};

class IPAServerThread : public Thread {
 public:
    IPAServerThread(IAlgoWorker* worker, const char* name);
    ~IPAServerThread();

    void sendRequest(uint32_t cmd, const Span<uint8_t>& mem);

    enum State {
        Stopped,
        Running,
    };

 protected:
    void run() override;

 private:
    IAlgoWorker* mIAlgoWorker;
    std::string mName;

    mutable Mutex mMutex;
    ConditionVariable mEventCondition;
    std::queue<cmd_event> mEventQueue;

    State mState LIBCAMERA_TSA_GUARDED_BY(mMutex);
};

/* first: cmd id, second: IPAServerThread instance */
typedef std::map<int, std::shared_ptr<IPAServerThread>> IPAServerThreadMap;

#define INIT_SERVER_THREAD_MAP(start, end, mServerThreadMap, name)  \
    {  \
        std::shared_ptr<IPAServerThread> worker = std::make_shared<IPAServerThread>(this, name);  \
        for (int i = start; i < end; i++) {  \
            mServerThreadMap[i] = worker;  \
        }  \
    }  \

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
