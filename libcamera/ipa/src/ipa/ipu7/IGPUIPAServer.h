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
#include <libcamera/base/signal.h>
#include <libcamera/base/span.h>
#include <libcamera/base/thread.h>

#include <map>
#include <memory>
#include <utility>

#include "subway_autogen.h"
namespace libcamera {

namespace ipa::igpu {

struct IC2ApiHandle {
#define _DEF_IC2_FUNC(ret, name, ...)      \
    typedef ret (*pFn##name)(__VA_ARGS__); \
    pFn##name name

    _DEF_IC2_FUNC(void, query_version, int* major, int* minor, int* patch);
    _DEF_IC2_FUNC(void, set_loglevel, iaic_log_level leve);
};
/**
 * \brief The GPU IPA Algo Sever implementation
 *
 */
class IGPUIPAServer {
 public:
    IGPUIPAServer();
    virtual ~IGPUIPAServer();

    int init(const std::string &libPath);

 private:
    bool loadIC2Library(const std::string &libPath);

 private:
    const std::string IC2_LIB_NAME = "libintelic.so";
    void* mIC2Handle;
    std::shared_ptr<IC2ApiHandle> mIC2Api;
};

} /* namespace ipa::igpu */
} /* namespace libcamera */
