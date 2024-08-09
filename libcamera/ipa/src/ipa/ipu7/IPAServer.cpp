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

#include "IPAServer.h"

#include <libcamera/base/log.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU)

namespace ipa::ipu7 {

IPAServer::IPAServer(IIPAIPUCallback* callback)
    : mIIPAIPUCallback(callback) {
    LOG(IPAIPU, Debug) << "IPAServer " << __func__;
}

IPAServer::~IPAServer() {
    LOG(IPAIPU, Debug) << "IPAServer " << __func__;
}

int IPAServer::init(uint8_t* data) {
    if (!data) return -1;

    LOG(IPAIPU, Debug) << "IPAServer " << __func__ << " data " << *data;

    if (*data == IPC_MATCHING_KEY) {
        *data = IPC_MATCHED_KEY;
    }

    return 0;
}

int IPAServer::sendRequest(int cameraId, int tuningMode, uint32_t cmd, const Span<uint8_t>& mem) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode "
                       << tuningMode << " data " << mem.data();

    auto key = std::make_pair(cameraId, tuningMode);

    MutexLocker locker(mWorkersMapLock);

    if (cmd > IPC_CCA_GROUP_START && cmd < IPC_CCA_PAC_GROUP_END) {
        if (mCcaWorkers.find(key) == mCcaWorkers.end()) {
            mCcaWorkers[key] = std::make_unique<CcaWorker>(cameraId, tuningMode, this);
        }

        return mCcaWorkers[key]->sendRequest(cmd, mem);
    }

    return 0;
}

void IPAServer::returnRequestReady(int cameraId, int tuningMode, uint32_t cmd, int ret) {
    LOG(IPAIPU, Debug) << "CameraId " << cameraId << " tuningMode " << tuningMode
                       << " cmd " << cmd << " ret " << ret;

    MutexLocker locker(mIpaLock);
    mIIPAIPUCallback->returnRequestReady(cameraId, tuningMode, cmd, ret);
}

void* IPAServer::getBuffer(uint32_t bufferId) {
    return mIIPAIPUCallback->getBuffer(bufferId);
}

} /* namespace ipa:ipu7 */
} /* namespace libcamera */
