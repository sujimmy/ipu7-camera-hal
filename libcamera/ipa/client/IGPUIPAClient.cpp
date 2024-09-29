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

#include "IGPUIPAClient.h"

#include <libcamera/base/log.h>
#include <libcamera/base/shared_fd.h>
#include <libcamera/base/unique_fd.h>
#include <libcamera/internal/ipa_manager.h>

#include <string>
#include <vector>

#include "IGPUHeader.h"
namespace libcamera {

LOG_DEFINE_CATEGORY(GPUClient)
IGPUIPAClient* IGPUIPAClient::sIPAClient = nullptr;
std::mutex IGPUIPAClient::sLock;

IGPUIPAClient* IGPUIPAClient::getInstance() {
    std::lock_guard<std::mutex> locker(sLock);

    if (!sIPAClient) {
        LOG(GPUClient, Error) << "IPAClient is nullptr";
    }

    return sIPAClient;
}

IGPUIPAClient* IGPUIPAClient::createInstance(PipelineHandler* handler) {
    LOG(GPUClient, Debug) << "IGPUIPAClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    if (sIPAClient) {
        LOG(GPUClient, Warning) << "IPAClient isn't nullptr";
        return sIPAClient;
    }

    sIPAClient = new IGPUIPAClient(handler);

    return sIPAClient;
}

void IGPUIPAClient::removeInstance() {
    LOG(GPUClient, Debug) << "IGPUIPAClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    delete sIPAClient;
    sIPAClient = nullptr;
}

IGPUIPAClient::IGPUIPAClient(PipelineHandler* handler)
        : mPipelineHandler(handler),
          mValidated(false),
          mIPAFine(false) {
    LOG(GPUClient, Debug) << "IGPUIPAClient";

    mIpaProxy = std::make_unique<IpaProxy>();
    mIpaProxy->moveToThread(this);

    start();
}

IGPUIPAClient::~IGPUIPAClient() {
    LOG(GPUClient, Debug) << "~IGPUIPAClient";
    mIpaProxy = nullptr;
    if (isRunning()) {
        exit();
        wait();
    }
}

bool IGPUIPAClient::isIPAFine() {
    if (!mValidated) {
        mIPAFine = init() == 0 ? true : false;
        mValidated = true;
    }

    return mIPAFine;
}

void IGPUIPAClient::run() {
    LOG(GPUClient, Debug) << "IPA Proxy thread started";
    exec();
}

int IGPUIPAClient::init() {
    return mIpaProxy->invokeMethod(&IpaProxy::init, ConnectionTypeBlocking, mPipelineHandler);
}

int IGPUIPAClient::IpaProxy::init(PipelineHandler* handler) {
    LOG(GPUClient, Debug) << " IpaProxy " << __func__;
    mIpa = IPAManager::createIPA<ipa::igpu::IPAProxyGPU>(
        handler, IGPU_IPA_VERSION, IGPU_IPA_VERSION, true, IPCPipeUnixSocket::kGpuPath);
    int ret = mIpa->init(IC2_LIB_PATH);
    if (ret) {
        mIpa = nullptr;
        LOG(GPUClient, Warning) << " create IPA failed " << ret;
        return ret;
    }
    mIpa->start();
    return ret;
}

IGPUIPAClient::IpaProxy::~IpaProxy() {
    if (mIpa) mIpa->stop();
    mIpa = nullptr;
}
}  // namespace libcamera
