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

#include "memory"
#include <map>

#include "IntelCCA.h"
#include "IPCCca.h"
#include "IPAHeader.h"
#include "IPAServerThread.h"

using namespace icamera;

namespace libcamera {

namespace ipa::ipu7 {

/**
 * \brief The IPU7 IPA CCA implementation
 *
 */
class CcaWorker : public IAlgoWorker {
 public:
    CcaWorker(int cameraId, int tuningMode, IIPAServerCallback* callback);
    virtual ~CcaWorker();

    int sendRequest(uint32_t cmd, const Span<uint8_t>& mem);
    void handleEvent(const cmd_event& event) override;

 private:
    int init(uint8_t* pData);
    int reinitAic(uint8_t* pData);
    int setStats(uint8_t* pData);
    int runAEC(uint8_t* pData);
    int runAIQ(uint8_t* pData);
    int configAIC(uint8_t* pData, int dataSize);
    int registerAicBuf(uint8_t* pData);
    int getAicBuf(uint8_t* pData);
    int updateConfigurationResolutions(uint8_t* pData, int dataSize);
    int runAIC(uint8_t* pData);
    int getCMC(uint8_t* pData);
    int getMKN(uint8_t* pData);
    int getAiqd(uint8_t* pData);
    int updateTuning(uint8_t* pData);
    int deinit(uint8_t* pData);
    int decodeStats(uint8_t* pData);

    ia_err getTerminalBuf(intel_cca_aic_control_data* params);

    int mCameraId;
    int mTuningMode;

    IIPAServerCallback* mIPACallback;
    IPAServerThreadMap mIPAServerThreadMap;

    std::unique_ptr<cca::IntelCCA> mCca;
    IPCCca mIpcCca;
    std::map<void*, void*> mServerToClientPayloadMap;
};

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
