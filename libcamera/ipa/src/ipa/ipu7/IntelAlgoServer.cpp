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

#include "IntelAlgoServer.h"

#include <libcamera/base/log.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU7)

namespace ipa::ipu7 {

IntelAlgoServer::IntelAlgoServer(IIPAIPU7Callback* callback)
    : mIIPAIPU7Callback(callback) {
    LOG(IPAIPU7, Debug) << "IntelAlgoServer " << __func__;
}

IntelAlgoServer::~IntelAlgoServer() {
    LOG(IPAIPU7, Debug) << "IntelAlgoServer " << __func__;
}

int IntelAlgoServer::init(uint8_t* data) {
    if (!data) return -1;

    LOG(IPAIPU7, Debug) << "IntelAlgoServer " << __func__ << " data " << *data;

    if (*data == IPC_MATCHING_KEY) {
        *data = IPC_MATCHED_KEY;
    }

    return 0;
}

int IntelAlgoServer::sendRequest(int cameraId, int tuningMode, uint32_t cmd,
                                 const Span<uint8_t>& mem) {
    LOG(IPAIPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode "
                        << tuningMode << " data " << mem.data();

    auto key = std::make_pair(cameraId, tuningMode);

    MutexLocker locker(mWorkersMapLock);

    if (cmd > IPC_CCA_GROUP_START && cmd < IPC_CCA_PAC_GROUP_END) {
        if (mIntelCcaWorkers.find(key) == mIntelCcaWorkers.end()) {
            mIntelCcaWorkers[key] = std::make_unique<IntelCcaWorker>(cameraId, tuningMode, this);
        }

        return mIntelCcaWorkers[key]->sendRequest(cmd, mem);
    }

    return 0;
}

void IntelAlgoServer::notifyCallback(int cameraId, int tuningMode, uint32_t cmd, int ret) {
    LOG(IPAIPU7, Debug) << "CameraId " << cameraId << " tuningMode " << tuningMode
                        << " cmd " << cmd << " ret " << ret;

    MutexLocker locker(mIpaLock);
    mIIPAIPU7Callback->notifyIPACallback(cameraId, tuningMode, cmd, ret);
}

void* IntelAlgoServer::getBuffer(uint32_t bufferId) {
    return mIIPAIPU7Callback->getBuffer(bufferId);
}

} /* namespace ipa:ipu7 */
} /* namespace libcamera */
