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

#include <stdarg.h>

#include <libcamera/base/log.h>

#include "IntelCCA.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU)

namespace ipa::ipu7 {

// CCA Debug
const char* CCA_DEBUG = "cameraDebug";
// Keep same value as the log level defined in CameraLog
const int CAMERA_DEBUG_LOG_CCA = 0x100;
const int CAMERA_DEBUG_LOG_INFO = 0x10;

enum CCA_LOG {
    ERR,
    INFO,
    DEBUG,
};

static int ccaDebugLevel = 0;

__attribute__((__format__(__printf__, 2, 0))) static void printLog(int level, const char* fmt,
                                                                   va_list ap) {
    char message[256] = {};
    vsnprintf(message, sizeof(message), fmt, ap);

    if (level == CCA_LOG::ERR) {
        LOG(IPAIPU, Error) << message;
    } else if (level == CCA_LOG::INFO) {
        LOG(IPAIPU, Info) << message;
    } else if (level == CCA_LOG::DEBUG) {
        LOG(IPAIPU, Debug) << message;
    }
}

__attribute__((__format__(__printf__, 1, 0))) void ccaPrintError(const char* fmt, va_list ap) {
    printLog(CCA_LOG::ERR, fmt, ap);
}

__attribute__((__format__(__printf__, 1, 0))) void ccaPrintInfo(const char* fmt, va_list ap) {
    if (ccaDebugLevel & CAMERA_DEBUG_LOG_INFO) {
        printLog(CCA_LOG::INFO, fmt, ap);
    }
}

__attribute__((__format__(__printf__, 1, 0))) void ccaPrintDebug(const char* fmt, va_list ap) {
    if (ccaDebugLevel & CAMERA_DEBUG_LOG_CCA) {
        printLog(CCA_LOG::DEBUG, fmt, ap);
    }
}

void initCcaDebug() {
    char* debug = getenv(CCA_DEBUG);
    if (debug) {
        ccaDebugLevel = strtoul(debug, nullptr, 0);
    }

    ia_env env = {&ccaPrintDebug, &ccaPrintError, &ccaPrintInfo, &ccaPrintInfo};
    ia_log_init(&env);
}

void deinitCcaDebug() {
    ia_log_deinit();
}

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
