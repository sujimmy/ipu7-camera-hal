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

#include "IntelAlgoClient.h"

#include <string>
#include <vector>

#include <libcamera/base/log.h>
#include <libcamera/base/shared_fd.h>
#include <libcamera/base/unique_fd.h>

#include <libcamera/internal/ipa_manager.h>
#include <libcamera/ipa/ipu7_ipa_interface.h>
#include <libcamera/ipa/ipu7_ipa_proxy.h>

#include "libcamera/internal/framebuffer.h"

#include "IPAHeader.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPU7)

IntelAlgoClient* IntelAlgoClient::sIntelAlgoClient = nullptr;
std::mutex IntelAlgoClient::sLock;

IntelAlgoClient* IntelAlgoClient::getInstance() {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    if (!sIntelAlgoClient) {
        LOG(IPU7, Error) << "sIntelAlgoClient is nullptr";
    }

    return sIntelAlgoClient;
}

IntelAlgoClient* IntelAlgoClient::createInstance(PipelineHandler* handler) {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    if (sIntelAlgoClient) {
        LOG(IPU7, Warning) << "sIntelAlgoClient isn't nullptr";
        return sIntelAlgoClient;
    }

    sIntelAlgoClient = new IntelAlgoClient(handler);

    return sIntelAlgoClient;
}

void IntelAlgoClient::removeInstance() {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    delete sIntelAlgoClient;
    sIntelAlgoClient = nullptr;
}

IntelAlgoClient::SyncMessage::SyncMessage(IntelAlgoClient* client) {
    mClient = client;
}

IntelAlgoClient::SyncMessage::~SyncMessage() {
}

void IntelAlgoClient::SyncMessage::exit() {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    mClient->exitIPA();
}

void IntelAlgoClient::SyncMessage::init(uint32_t bufferId) {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    mClient->init(bufferId);
}

void IntelAlgoClient::SyncMessage::mapBuffers(const std::vector<IPABuffer>& buffers) {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    mClient->mapBuffers(buffers);
}

void IntelAlgoClient::SyncMessage::unmapBuffers(const std::vector<unsigned int>& ids) {
    LOG(IPU7, Debug) << "IntelAlgoClient " << __func__;
    mClient->unmapBuffers(ids);
}

IntelAlgoClient::IntelAlgoClient(PipelineHandler* handler)
    : mPipelineHandler(handler),
      mValidated(false),
      mIPAFine(false) {
    LOG(IPU7, Debug) << "IntelAlgoClient";

    std::string filename("validateIPA");
    mMemValidIPA.filename = filename + std::to_string(reinterpret_cast<uintptr_t>(this));
    mMemValidIPA.memAddr = nullptr;
    mMemValidIPA.handle = 0;

    mSyncMessage = std::make_unique<SyncMessage>(this);
    mSyncMessage->moveToThread(this);

    start();
    LOG(IPU7, Debug) << "IntelAlgoClient started";
}

IntelAlgoClient::~IntelAlgoClient() {
    LOG(IPU7, Debug) << "~IntelAlgoClient";

    if (mMemValidIPA.memAddr) {
        freeShmMem(mMemValidIPA.filename, mMemValidIPA.memAddr, mMemValidIPA.handle);
    }

    mSyncMessage->invokeMethod(&SyncMessage::exit, ConnectionTypeBlocking);

    wait();

    LOG(IPU7, Debug) << "IntelAlgoClient exited";
}

void IntelAlgoClient::init(uint32_t bufferId) {
    mIpa->init(bufferId);
}

void IntelAlgoClient::exitIPA() {
    mIpa = nullptr;

    exit();
}

bool IntelAlgoClient::isIPAFine() {
    if (!mValidated) {
        validate();
        mValidated = true;
    }

    return mIPAFine;
}

void IntelAlgoClient::validate() {
    int size = 1024;
    if (!mMemValidIPA.memAddr) {
        bool alloc = allocShmMem(mMemValidIPA.filename, size, &mMemValidIPA.memAddr,
                                 mMemValidIPA.handle);
        if (!alloc) return;

    }

    memset(mMemValidIPA.memAddr, 0, size);
    uint8_t* addr = static_cast<uint8_t*>(mMemValidIPA.memAddr);
    *addr = IPC_MATCHING_KEY;

    mSyncMessage->invokeMethod(&SyncMessage::init, ConnectionTypeBlocking, mMemValidIPA.handle);

    if (*addr == IPC_MATCHED_KEY) {
        mIPAFine = true;
        LOG(IPU7, Debug) << "IPC matched key is " << *addr;
    }
}

void IntelAlgoClient::run() {
    LOG(IPU7, Debug) << "Load IPA Proxy in IntelAlgoClient";

    mIpa = IPAManager::createIPA<ipa::ipu7::IPAProxyIPU7>(mPipelineHandler, 1, 1);
    mIpa->notifyCallback.connect(this, &IntelAlgoClient::notifyCallback);

    exec();
}

bool IntelAlgoClient::allocShmMem(const std::string& name, int size, void** addr,
                                  uint32_t& handle) {
    auto buffer = mIPAMemory.allocateBuffer(name, size, addr);
    if (!buffer) {
        LOG(IPU7, Error) << " failed to allocate shm" << __func__;
        return false;
    }

    std::vector<IPABuffer> ipaBuffer;
    ipaBuffer.emplace_back(buffer->cookie(), buffer->planes());

    mSyncMessage->invokeMethod(&SyncMessage::mapBuffers, ConnectionTypeBlocking, ipaBuffer);

    handle = buffer->cookie();

    MutexLocker locker(mMapMutex);
    mShmMap[*addr] = handle;
    mFrameBufferMap[*addr] = buffer;

    LOG(IPU7, Warning) << "found buffer " << handle;
    return true;
}

void IntelAlgoClient::freeShmMem(const std::string& name, void* addr, uint32_t handle) {
    MutexLocker locker(mMapMutex);
    if (mFrameBufferMap.find(addr) != mFrameBufferMap.end()) {
        auto& buffer = mFrameBufferMap[addr];

        std::vector<uint32_t> ids;
        ids.push_back(handle);

        mSyncMessage->invokeMethod(&SyncMessage::unmapBuffers, ConnectionTypeBlocking, ids);

        mIPAMemory.freeBuffer(name, buffer, addr);

        return;
    }

    LOG(IPU7, Warning) << "no found buffer " << handle;
}

uint32_t IntelAlgoClient::getShmMemHandle(void* addr) {
    MutexLocker locker(mMapMutex);
    if (mShmMap.find(addr) != mShmMap.end()) {
        return mShmMap[addr];
    }

    return 0;
}

int IntelAlgoClient::initCca(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    initClientWorkerMap(cameraId, tuningMode, mIPAClientWorkerMaps);

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_INIT, bufferId,
                             mIPAClientWorkerMaps);
}

int IntelAlgoClient::reinitAic(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_REINIT_AIC, bufferId,
                             mIPAClientWorkerMaps);
}

void IntelAlgoClient::deinitCca(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_DEINIT, bufferId,
                      mIPAClientWorkerMaps);
}

int IntelAlgoClient::setStats(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_SET_STATS,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::runAec(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_RUN_AEC,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::runAiq(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_RUN_AIQ,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::updateTuning(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_UPDATE_TUNING,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::getCmc(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_CMC,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::getMkn(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_MKN,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::getAiqd(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_AIQD,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::configAic(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_CONFIG_AIC,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::runAic(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_RUN_AIC,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::updateConfigurationResolutions(int cameraId, int tuningMode,
                                                    uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_UPDATE_CONFIG_RES,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::registerAicBuf(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_REGISTER_AIC_BUFFER,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::getAicBuf(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_AIC_BUFFER,
                             bufferId, mIPAClientWorkerMaps);
}

int IntelAlgoClient::decodeStats(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPU7, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                     << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_DECODE_STATS,
                             bufferId, mIPAClientWorkerMaps);
}

void IntelAlgoClient::initClientWorkerMap(int cameraId, int tuningMode,
                                          IPAClientWorkerMaps& clientWorkerMaps) {
    auto key = std::make_pair(cameraId, tuningMode);

    if (clientWorkerMaps.find(key) == clientWorkerMaps.end()) {
        std::map<std::pair<int, int>, const char*> groupMap = {
            {{ ipa::ipu7::IPC_CCA_GROUP_START, ipa::ipu7::IPC_CCA_GROUP_END }, "cca"},
            {{ ipa::ipu7::IPC_CCA_PAC_GROUP_START, ipa::ipu7::IPC_CCA_PAC_GROUP_END }, "pac"}
        };

        IPAClientWorkerMap map;

        for (auto iter : groupMap) {
            auto worker = std::make_shared<IPAClientWorker>(this, iter.second);
            for (int i = iter.first.first + 1; i < iter.first.second; i++) {
                map[i] = worker;
            }
        }

        clientWorkerMaps[key] = std::move(map);
    }
}

int IntelAlgoClient::sendCmdWithWorker(int cameraId, int tuningMode, uint32_t cmd,
                                       uint32_t bufferId,
                                       IPAClientWorkerMaps& clientWorkerMaps) {
    auto key = std::make_pair(cameraId, tuningMode);

    if (clientWorkerMaps.find(key) != clientWorkerMaps.end()) {
        auto& map = clientWorkerMaps[key];
        if (map.find(cmd) != map.end()) {
            int ret = map[cmd]->sendRequest(cameraId, tuningMode, cmd, bufferId);
            if (ret != 0) {
                LOG(IPU7, Error) << "cameraId " << cameraId << " tuningMode " << tuningMode
                                 << " cmd " << cmd;
            }
            return ret;
        }
    }

    LOG(IPU7, Warning) << " " << __func__ << " cameraId " << cameraId << " tuningMode "
                       << tuningMode << " cmd" << cmd << " bufferId " << bufferId;
    return -1;
}

void IntelAlgoClient::sendRequest(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId) {
    ipa::ipu7::IPACmdInfo cmdInfo = { cameraId, tuningMode, cmd, bufferId };

    MutexLocker locker(mIpaLock);
    mIpa->requestASync(cmdInfo);
}

void IntelAlgoClient::notifyCallback(const ipa::ipu7::IPACmdInfo& cmdInfo, int ret) {
    LOG(IPU7, Debug) << "notify callback cameraId " << cmdInfo.cameraId << " tuningMode "
                     << cmdInfo.tuningMode << " cmd " << cmdInfo.cmd << " bufferId "
                     << cmdInfo.bufferId << " ret " << ret;
    auto key = std::make_pair(cmdInfo.cameraId, cmdInfo.tuningMode);

    if (mIPAClientWorkerMaps.find(key) != mIPAClientWorkerMaps.end()) {
        auto& map = mIPAClientWorkerMaps[key];
        if (map.find(cmdInfo.cmd) != map.end()) {
            map[cmdInfo.cmd]->setIPCRet(cmdInfo.cmd, ret);
            map[cmdInfo.cmd]->signal();
        }
    }
}

} /* namespace icamera */
