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

#include "IPAClientWorker.h"

#include <libcamera/base/log.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(IPU7)

IPAClientWorker::IPAClientWorker(IAlgoClient* client, const char* name)
    : mIAlgoClient(client),
      mName(name) {
    LOG(IPU7, Debug) << " " << __func__ << " name " << mName.c_str();
}

IPAClientWorker::~IPAClientWorker() {
    LOG(IPU7, Debug) << " " << __func__ << " name " << mName.c_str();
}

void IPAClientWorker::setIPCRet(uint32_t cmd, int ret) {
    std::unique_lock<std::mutex> locker(mWaitLock);

    if (mSendingCmdMap.find(cmd) != mSendingCmdMap.end()) {
        mSendingCmdMap[cmd] = ret;
    } else {
        LOG(IPU7, Warning) << " cmd " << cmd << " isn't found ";
    }
}

void IPAClientWorker::signal() {
    std::unique_lock<std::mutex> locker(mWaitLock);
    LOG(IPU7, Debug) << "signal" << " name " << mName.c_str();

    mWaitCallDone.notify_one();
}

std::cv_status IPAClientWorker::wait(uint32_t cmd) {
    std::unique_lock<std::mutex> locker(mWaitLock);
    LOG(IPU7, Debug) << "wait" << " name " << mName.c_str();

    if (mSendingCmdMap[cmd] >= 0) {
        // signal arrived before waiting
        return std::cv_status::no_timeout;
    }

    return mWaitCallDone.wait_for(locker, std::chrono::seconds(kWaitTimeout));
}

int IPAClientWorker::sendRequest(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId) {
    LOG(IPU7, Debug) << "sendRequest cmd " << cmd << " name " << mName.c_str();

    {
        std::unique_lock<std::mutex> locker(mWaitLock);
        mSendingCmdMap[cmd] = -1;
    }

    mIAlgoClient->sendRequest(cameraId, tuningMode, cmd, bufferId);

    std::cv_status status = wait(cmd);
    if (status == std::cv_status::timeout) {
        LOG(IPU7, Warning) << "wait timeout cmd " << cmd;
    }

    int ret = -1;
    {
        std::unique_lock<std::mutex> locker(mWaitLock);
        ret = mSendingCmdMap[cmd];
        mSendingCmdMap.erase(cmd);
    }

    return ret;
}

} /* namespace libcamera */
