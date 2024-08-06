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

#include "IGPUIPAServer.h"
#include <dlfcn.h>
#include <libcamera/base/log.h>

namespace libcamera {

LOG_DECLARE_CATEGORY(IGPUIPA)

namespace ipa::igpu {

#define GET_FUNC_CALL(member, fnName)                                            \
    do {                                                                         \
        mIC2Api->member = (IC2ApiHandle::pFn##member)dlsym(mIC2Handle, #fnName); \
        if (mIC2Api->member == nullptr) {                                        \
            LOG(IGPUIPA, Error) << "LOADING: " #fnName "failed: " << dlerror();   \
            return false;                                                        \
        }                                                                        \
        LOG(IGPUIPA, Debug) << "LOADING: " #fnName " = " << mIC2Api->member;      \
    } while (0)

IGPUIPAServer::IGPUIPAServer() {
    LOG(IGPUIPA, Info) << __func__;
    mIC2Api = std::make_shared<IC2ApiHandle>();
}

IGPUIPAServer::~IGPUIPAServer() {
    LOG(IGPUIPA, Debug) << __func__;
    mIC2Api = nullptr;
    if (mIC2Handle) dlclose(mIC2Handle);
    mIC2Handle = nullptr;
}

int IGPUIPAServer::init(const std::string &libPath) {
    // loadIC2Library should be call only 1 time
    if (mIC2Handle == nullptr && !loadIC2Library(libPath)) return -1;

    return 0;
}

bool IGPUIPAServer::loadIC2Library(const std::string &libPath) {
    std::string fullPath = libPath + IC2_LIB_NAME;
    mIC2Handle = dlopen(fullPath.c_str(), RTLD_NOW);
    if (!mIC2Handle) {
        LOG(IGPUIPA, Error) << "Failed to open library: " << fullPath
                           << " error: " << dlerror();
        return false;
    }

    GET_FUNC_CALL(query_version, iaic_query_version);
    GET_FUNC_CALL(set_loglevel, iaic_set_loglevel);
    return true;
}
}  // namespace ipa::igpu
} /* namespace libcamera */
