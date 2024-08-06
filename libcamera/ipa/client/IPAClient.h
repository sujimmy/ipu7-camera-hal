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
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

#include <libcamera/base/mutex.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/thread.h>

#include <libcamera/ipa/ipu7_ipa_proxy.h>

#include "IPAMemory.h"
#include "IPAClientWorker.h"

namespace libcamera {

class PipelineHandler;

class IPAClient : public IAlgoClient, public Thread {
 public:
    static IPAClient* getInstance();
    static IPAClient* createInstance(PipelineHandler* handler);
    static void removeInstance();

    IPAClient(PipelineHandler* handler);
    virtual ~IPAClient();

    void init(uint32_t bufferId);
    void exitIPA();
    bool isIPAFine();

    bool allocShmMem(const std::string& name, int size, void** addr, uint32_t& handle);
    void freeShmMem(const std::string& name, void* addr, uint32_t handle);
    uint32_t getShmMemHandle(void* addr);

    int initCca(int cameraId, int tuningMode, uint32_t bufferId);
    int reinitAic(int cameraId, int tuningMode, uint32_t bufferId);
    void deinitCca(int cameraId, int tuningMode, uint32_t bufferId);
    int setStats(int cameraId, int tuningMode, uint32_t bufferId);
    int runAec(int cameraId, int tuningMode, uint32_t bufferId);
    int runAiq(int cameraId, int tuningMode, uint32_t bufferId);
    int updateTuning(int cameraId, int tuningMode, uint32_t bufferId);
    int getCmc(int cameraId, int tuningMode, uint32_t bufferId);
    int getMkn(int cameraId, int tuningMode, uint32_t bufferId);
    int getAiqd(int cameraId, int tuningMode, uint32_t bufferId);

    int configAic(int cameraId, int tuningMode, uint32_t bufferId);
    int runAic(int cameraId, int tuningMode, uint32_t bufferId);
    int updateConfigurationResolutions(int cameraId, int tuningMode, uint32_t bufferId);
    int registerAicBuf(int cameraId, int tuningMode, uint32_t bufferId);
    int getAicBuf(int cameraId, int tuningMode, uint32_t bufferId);
    int decodeStats(int cameraId, int tuningMode, uint32_t bufferId);

    void sendRequest(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId) override;

    void mapBuffers(const std::vector<IPABuffer>& buffers) {
        mIpa->mapBuffers(buffers);
    }
    void unmapBuffers(const std::vector<unsigned int>& ids) {
        mIpa->unmapBuffers(ids);
    }

    class SyncMessage : public Object {
     public:
        SyncMessage(IPAClient* client);
        ~SyncMessage();

        void init(uint32_t bufferId);
        void exit();

        void mapBuffers(const std::vector<IPABuffer>& buffers);
        void unmapBuffers(const std::vector<unsigned int>& ids);

     private:
        IPAClient* mClient;
    };

 protected:
    void run() override;

 private:
    void validate();

    void returnRequestReady(const ipa::ipu7::IPACmdInfo& cmdInfo, int ret);
    void initClientWorkerMap(int cameraId, int tuningMode, IPAClientWorkerMaps& clientWorkerMaps);
    int sendCmdWithWorker(int cameraId, int tuningMode, uint32_t cmd, uint32_t bufferId,
                          IPAClientWorkerMaps& clientWorkerMaps);

    PipelineHandler* mPipelineHandler;
    std::unique_ptr<ipa::ipu7::IPAProxyIPU7> mIpa;
    IPAMemory mIPAMemory;

    std::unique_ptr<SyncMessage> mSyncMessage;

    bool mValidated;
    bool mIPAFine;
    struct {
        std::string filename;
        void* memAddr;
        uint32_t handle;
    } mMemValidIPA;

    IPAClientWorkerMaps mIPAClientWorkerMaps;

    /* All async cmds run synchronously and are protected by mIpaLock */
    mutable Mutex mIpaLock;

    mutable Mutex mMapMutex;
    std::unordered_map<void*, uint32_t> mShmMap;
    std::unordered_map<void*, std::shared_ptr<FrameBuffer>> mFrameBufferMap;

    static IPAClient* sIPAClient;
    static std::mutex sLock;
};

} /* namespace libcamera */
