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

#include "IPAClient.h"

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

LOG_DEFINE_CATEGORY(IPAIPU)

IPAClient* IPAClient::sIPAClient = nullptr;
std::mutex IPAClient::sLock;

IPAClient* IPAClient::getInstance() {
    std::lock_guard<std::mutex> locker(sLock);

    if (!sIPAClient) {
        LOG(IPAIPU, Error) << "sIPAClient is nullptr";
    }

    return sIPAClient;
}

IPAClient* IPAClient::createInstance(PipelineHandler* handler) {
    LOG(IPAIPU, Debug) << "IPAClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    if (sIPAClient) {
        LOG(IPAIPU, Warning) << "sIPAClient isn't nullptr";
        return sIPAClient;
    }

    sIPAClient = new IPAClient(handler);

    return sIPAClient;
}

void IPAClient::removeInstance() {
    LOG(IPAIPU, Debug) << "IPAClient " << __func__;
    std::lock_guard<std::mutex> locker(sLock);

    delete sIPAClient;
    sIPAClient = nullptr;
}

IPAClient::SyncMessage::SyncMessage(IPAClient* client) {
    mClient = client;
}

IPAClient::SyncMessage::~SyncMessage() {
}

void IPAClient::SyncMessage::exit() {
    LOG(IPAIPU, Debug) << "IPAClient " << __func__;
    mClient->exitIPA();
}

void IPAClient::SyncMessage::init(uint32_t bufferId) {
    LOG(IPAIPU, Debug) << "IPAClient " << __func__;
    mClient->init(bufferId);
}

void IPAClient::SyncMessage::mapBuffers(const std::vector<IPABuffer>& buffers) {
    LOG(IPAIPU, Debug) << "IPAClient " << __func__;
    mClient->mapBuffers(buffers);
}

void IPAClient::SyncMessage::unmapBuffers(const std::vector<unsigned int>& ids) {
    LOG(IPAIPU, Debug) << "IPAClient " << __func__;
    mClient->unmapBuffers(ids);
}

IPAClient::IPAClient(PipelineHandler* handler)
    : mPipelineHandler(handler),
      mValidated(false),
      mIPAFine(false) {
    LOG(IPAIPU, Debug) << "IPAClient";

    std::string filename("validateIPA");
    mMemValidIPA.filename = filename + std::to_string(reinterpret_cast<uintptr_t>(this));
    mMemValidIPA.memAddr = nullptr;
    mMemValidIPA.handle = 0;

    mSyncMessage = std::make_unique<SyncMessage>(this);
    mSyncMessage->moveToThread(this);

    start();
    LOG(IPAIPU, Debug) << "IPAClient started";
}

IPAClient::~IPAClient() {
    LOG(IPAIPU, Debug) << "~IPAClient";

    if (mMemValidIPA.memAddr) {
        freeShmMem(mMemValidIPA.filename, mMemValidIPA.memAddr, mMemValidIPA.handle);
    }

    mSyncMessage->invokeMethod(&SyncMessage::exit, ConnectionTypeBlocking);

    wait();

    LOG(IPAIPU, Debug) << "IPAClient exited";
}

void IPAClient::init(uint32_t bufferId) {
    mIpa->init(bufferId);
    mIpa->start();
}

void IPAClient::exitIPA() {
    mIpa->stop();
    mIpa = nullptr;

    exit();
}

bool IPAClient::isIPAFine() {
    if (!mValidated) {
        validate();
        mValidated = true;
    }

    return mIPAFine;
}

void IPAClient::validate() {
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
        LOG(IPAIPU, Debug) << "IPC matched key is " << *addr;
    }
}

void IPAClient::run() {
    LOG(IPAIPU, Debug) << "Load IPA Proxy in IPAClient";

#ifdef HAVE_CHROME_OS
    mIpa = IPAManager::createIPA<ipa::ipu7::IPAProxyIPU7>(
        mPipelineHandler, IPU7_IPA_VERSION, IPU7_IPA_VERSION, true, IPCPipeUnixSocket::kCpuPath);
#else
    mIpa = IPAManager::createIPA<ipa::ipu7::IPAProxyIPU7>(mPipelineHandler, IPU7_IPA_VERSION,
                                                          IPU7_IPA_VERSION);
#endif
    mIpa->requestReady.connect(this, &IPAClient::returnRequestReady);

    exec();
}

bool IPAClient::allocShmMem(const std::string& name, int size, void** addr,
                                  uint32_t& handle) {
    auto buffer = mIPAMemory.allocateBuffer(name, size, addr);
    if (!buffer) {
        LOG(IPAIPU, Error) << " failed to allocate shm" << __func__;
        return false;
    }

    std::vector<IPABuffer> ipaBuffer;
    ipaBuffer.emplace_back(buffer->cookie(), buffer->planes());

    mSyncMessage->invokeMethod(&SyncMessage::mapBuffers, ConnectionTypeBlocking, ipaBuffer);

    handle = buffer->cookie();

    MutexLocker locker(mMapMutex);
    mShmMap[*addr] = handle;
    mFrameBufferMap[*addr] = buffer;

    return true;
}

void IPAClient::freeShmMem(const std::string& name, void* addr, uint32_t handle) {
    MutexLocker locker(mMapMutex);
    if (mFrameBufferMap.find(addr) != mFrameBufferMap.end()) {
        auto& buffer = mFrameBufferMap[addr];

        std::vector<uint32_t> ids;
        ids.push_back(handle);

        mSyncMessage->invokeMethod(&SyncMessage::unmapBuffers, ConnectionTypeBlocking, ids);

        mIPAMemory.freeBuffer(name, buffer, addr);
        mFrameBufferMap.erase(addr);

        return;
    }

    LOG(IPAIPU, Warning) << "no found buffer " << handle;
}

uint32_t IPAClient::getShmMemHandle(void* addr) {
    MutexLocker locker(mMapMutex);
    if (mShmMap.find(addr) != mShmMap.end()) {
        return mShmMap[addr];
    }

    return 0;
}

int IPAClient::initCca(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    initClientWorkerMap(cameraId, tuningMode, mIPAClientWorkerMaps);

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_INIT, bufferId,
                             mIPAClientWorkerMaps);
}

int IPAClient::reinitAic(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_REINIT_AIC, bufferId,
                             mIPAClientWorkerMaps);
}

void IPAClient::deinitCca(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_DEINIT, bufferId,
                      mIPAClientWorkerMaps);
}

int IPAClient::setStats(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_SET_STATS,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::runAec(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_RUN_AEC,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::runAiq(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_RUN_AIQ,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::updateTuning(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_UPDATE_TUNING,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::getCmc(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_CMC,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::getMkn(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_MKN,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::getAiqd(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_AIQD,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::configAic(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_CONFIG_AIC,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::runAic(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_RUN_AIC,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::updateConfigurationResolutions(int cameraId, int tuningMode,
                                                    uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_UPDATE_CONFIG_RES,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::registerAicBuf(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_REGISTER_AIC_BUFFER,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::getAicBuf(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_GET_AIC_BUFFER,
                             bufferId, mIPAClientWorkerMaps);
}

int IPAClient::decodeStats(int cameraId, int tuningMode, uint32_t bufferId) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cameraId " << cameraId << " tuningMode " << tuningMode
                       << " bufferId " << bufferId;

    return sendCmdWithWorker(cameraId, tuningMode, ipa::ipu7::IPC_CCA_DECODE_STATS,
                             bufferId, mIPAClientWorkerMaps);
}

void IPAClient::initClientWorkerMap(int cameraId, int tuningMode,
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

int IPAClient::sendCmdWithWorker(int cameraId, int tuningMode, uint32_t cmd,
                                       uint32_t bufferId,
                                       IPAClientWorkerMaps& clientWorkerMaps) {
    auto key = std::make_pair(cameraId, tuningMode);

    if (clientWorkerMaps.find(key) != clientWorkerMaps.end()) {
        auto& map = clientWorkerMaps[key];
        if (map.find(cmd) != map.end()) {
            int ret = map[cmd]->sendRequest(cameraId, tuningMode, cmd, bufferId);
            if (ret != 0) {
                LOG(IPAIPU, Error) << "cameraId " << cameraId << " tuningMode " << tuningMode
                                   << " cmd " << cmd;
            }
            return ret;
        }
    }

    LOG(IPAIPU, Warning) << " " << __func__ << " cameraId " << cameraId << " tuningMode "
                         << tuningMode << " cmd" << cmd << " bufferId " << bufferId;
    return -1;
}

void IPAClient::sendRequest(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId) {
    ipa::ipu7::IPACmdInfo cmdInfo = { cameraId, tuningMode, cmd, bufferId };

    MutexLocker locker(mIpaLock);
    mIpa->sendRequest(cmdInfo);
}

void IPAClient::returnRequestReady(const ipa::ipu7::IPACmdInfo& cmdInfo, int ret) {
    LOG(IPAIPU, Debug) << "notify callback cameraId " << cmdInfo.cameraId << " tuningMode "
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

} /* namespace libcamera */
