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
#include <mutex>
#include <string>
#include <memory>

namespace libcamera {

class IAlgoClient {
 public:
    virtual void sendRequest(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId) = 0;

    IAlgoClient() {}
    virtual ~IAlgoClient() {}
};

class IPAClientWorker {
 public:
    IPAClientWorker(IAlgoClient* client, const char* name);
    ~IPAClientWorker();

    int sendRequest(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId);
    void setIPCRet(uint32_t cmd, int ret);
    void signal();

 private:
    std::cv_status wait(uint32_t cmd);

    IAlgoClient* mIAlgoClient;
    std::string mName;

    /* each cmd must return in 5s */
    const static uint64_t kWaitTimeout = 5;  // 5s
    std::mutex mWaitLock;
    std::condition_variable mWaitCallDone;

    /* first: cmd id, second: ipc returned status */
    std::map<uint32_t, int> mSendingCmdMap;
};

/* first: cmd id, second: IPAClientWorker instance */
typedef std::map<int, std::shared_ptr<IPAClientWorker>> IPAClientWorkerMap;
/* first: pair(cameraId, tuningMode), second: IPAClientWorkerMap */
typedef std::map<std::pair<int, int>, IPAClientWorkerMap> IPAClientWorkerMaps;

} /* namespace libcamera */
