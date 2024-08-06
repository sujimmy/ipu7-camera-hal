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

#include <libcamera/base/mutex.h>
#include <libcamera/base/object.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/thread.h>
#include <libcamera/ipa/ipu7_igpu_ipa_interface.h>
#include <libcamera/ipa/ipu7_igpu_ipa_proxy.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace libcamera {

class PipelineHandler;

class IGPUIPAClient : public Thread {
 public:
    static IGPUIPAClient* getInstance();
    static IGPUIPAClient* createInstance(PipelineHandler* handler);
    static void removeInstance();

    IGPUIPAClient(PipelineHandler* handler);
    virtual ~IGPUIPAClient();
    bool isIPAFine();
    int init();

    /* Create a specified class and thread to access socket, all socket API should
    ** be called in the same thread
    **/
    class IpaProxy : public Object {
     public:
        IpaProxy() {}
        ~IpaProxy();

        int init(PipelineHandler* handler);

     private:
        const std::string IC2_LIB_PATH = "/usr/lib64/";
        std::unique_ptr<ipa::igpu::IPAProxyGPU> mIpa;
    };

 private:
    void run() override;

 private:
    PipelineHandler* mPipelineHandler;
    std::unique_ptr<IpaProxy> mIpaProxy;
    bool mValidated;
    bool mIPAFine;
    static IGPUIPAClient* sIPAClient;
    static std::mutex sLock;
};

} /* namespace libcamera */
