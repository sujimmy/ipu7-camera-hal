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

#define IPC_MATCHING_KEY 0x56  // the value is randomly chosen
#define IPC_MATCHED_KEY 0x47  // the value is randomly chosen

// each group shared one thread and one waiting condition
enum IPC_CMD {
    // worked in IPC_CCA group
    IPC_CCA_GROUP_START,
    IPC_CCA_INIT,
    IPC_CCA_SET_STATS,
    IPC_CCA_RUN_AEC,
    IPC_CCA_RUN_AIQ,
    IPC_CCA_GET_CMC,

    IPC_CCA_GET_MKN,
    IPC_CCA_GET_AIQD,
    IPC_CCA_UPDATE_TUNING,
    IPC_CCA_DEINIT,
    IPC_CCA_GROUP_END,

    IPC_CCA_PAC_GROUP_START,
    IPC_CCA_REINIT_AIC,
    IPC_CCA_CONFIG_AIC,
    IPC_CCA_REGISTER_AIC_BUFFER,
    IPC_CCA_GET_AIC_BUFFER,
    IPC_CCA_UPDATE_CONFIG_RES,
    IPC_CCA_RUN_AIC,
    IPC_CCA_DECODE_STATS,
    IPC_CCA_PAC_GROUP_END,
};

struct cmd_event {
    uint32_t cmd;
    uint8_t* data;
    int size;
};

/**
 * \brief The IPU7 IPA callback interface
 *
 */
class IIPAServerCallback {
 public:
    IIPAServerCallback() {}
    virtual ~IIPAServerCallback() {}

    virtual void notifyCallback(int cameraId, int tuningMode, uint32_t cmd, int ret) = 0;
    virtual void* getBuffer(uint32_t bufferId) = 0;
};

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
