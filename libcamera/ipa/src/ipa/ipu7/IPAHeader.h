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

namespace libcamera {

namespace ipa::ipu7 {

#define IPU7_IPA_VERSION 1
#define IPC_MATCHING_KEY 0x56  // the value is randomly chosen
#define IPC_MATCHED_KEY 0x47  // the value is randomly chosen

struct cmd_event {
    uint32_t cmd;
    uint8_t* data;
    int size;
};

/**
 * \brief The IPU IPA callback interface
 *
 */
class IIPAServerCallback {
 public:
    IIPAServerCallback() {}
    virtual ~IIPAServerCallback() {}

    virtual void returnRequestReady(int cameraId, int tuningMode, uint32_t cmd, int ret) = 0;
    virtual void* getBuffer(uint32_t bufferId) = 0;
};

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
