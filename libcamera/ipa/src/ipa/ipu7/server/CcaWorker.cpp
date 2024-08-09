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

#include "CcaWorker.h"

#include <libcamera/base/log.h>

#include "IPAHeader.h"
#include "IPCCca.h"

namespace libcamera {

LOG_DECLARE_CATEGORY(IPAIPU)

namespace ipa::ipu7 {

CcaWorker::CcaWorker(int cameraId, int tuningMode, IIPAServerCallback* callback)
        : mCameraId(cameraId),
          mTuningMode(tuningMode),
          mIPACallback(callback),
          mCca(nullptr) {
    LOG(IPAIPU, Debug) << "CcaWorker cameraId " << cameraId << " tuningMode " << tuningMode;

    mCca = std::make_unique<cca::IntelCCA>();
    mServerToClientPayloadMap.clear();

    int start = IPC_CCA_GROUP_START + 1;
    int end = IPC_CCA_GROUP_END;

    INIT_SERVER_THREAD_MAP(start, end, mIPAServerThreadMap, "cca");

    start = IPC_CCA_PAC_GROUP_START + 1;
    end = IPC_CCA_PAC_GROUP_END;

    INIT_SERVER_THREAD_MAP(start, end, mIPAServerThreadMap, "pac");
}

CcaWorker::~CcaWorker() {
    LOG(IPAIPU, Debug) << "~CcaWorker cameraId " << mCameraId << " tuningMode "
                       << mTuningMode;

    mServerToClientPayloadMap.clear();
}

int CcaWorker::sendRequest(uint32_t cmd, const Span<uint8_t>& mem) {
    LOG(IPAIPU, Debug) << " " << __func__ << " cmd " << cmd;

    mIPAServerThreadMap[cmd]->sendRequest(cmd, mem);
    return 0;
}

void CcaWorker::handleEvent(const cmd_event& event) {
    int ret = 0;

    switch (event.cmd) {
        case IPC_CCA_INIT:
            ret = init(event.data);
            break;
        case IPC_CCA_REINIT_AIC:
            ret = reinitAic(event.data);
            break;
        case IPC_CCA_SET_STATS:
            ret = setStats(event.data);
            break;
        case IPC_CCA_RUN_AEC:
            ret = runAEC(event.data);
            break;
        case IPC_CCA_RUN_AIQ:
            ret = runAIQ(event.data);
            break;
        case IPC_CCA_GET_CMC:
            ret = getCMC(event.data);
            break;
        case IPC_CCA_GET_MKN:
            ret = getMKN(event.data);
            break;
        case IPC_CCA_GET_AIQD:
            ret = getAiqd(event.data);
            break;
        case IPC_CCA_UPDATE_TUNING:
            ret = updateTuning(event.data);
            break;
        case IPC_CCA_DEINIT:
            ret = deinit(event.data);
            break;
        case IPC_CCA_CONFIG_AIC:
            ret = configAIC(event.data, event.size);
            break;
        case IPC_CCA_REGISTER_AIC_BUFFER:
            ret = registerAicBuf(event.data);
            break;
        case IPC_CCA_GET_AIC_BUFFER:
            ret = getAicBuf(event.data);
            break;
        case IPC_CCA_UPDATE_CONFIG_RES:
            ret = updateConfigurationResolutions(event.data, event.size);
            break;
        case IPC_CCA_RUN_AIC:
            ret = runAIC(event.data);
            break;
        case IPC_CCA_DECODE_STATS:
            ret = decodeStats(event.data);
            break;
        default:
            LOG(IPAIPU, Warning) << "Unknown cmd " << event.cmd;
            break;
    }

    if (ret != 0) {
        LOG(IPAIPU, Error) << "handleEvent error " << event.cmd;
    }

    mIPACallback->returnRequestReady(mCameraId, mTuningMode, event.cmd, ret);
}

int CcaWorker::init(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_init_data* params = reinterpret_cast<intel_cca_init_data*>(pData);

    ia_err ret = mCca->init(params->inParams);
    LOG(IPAIPU, Debug) << "bitmap: " << params->inParams.bitmap << " version: "
                       << mCca->getVersion();

    return static_cast<int>(ret);
}

int CcaWorker::reinitAic(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_reinit_aic_data* params = reinterpret_cast<intel_cca_reinit_aic_data*>(pData);

    ia_err ret = mCca->reinitAic(params->aicId);

    return static_cast<int>(ret);
}

int CcaWorker::deinit(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    ia_err ret = mCca->deinit();

    return static_cast<int>(ret);
}

int CcaWorker::setStats(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_set_stats_data* params = reinterpret_cast<intel_cca_set_stats_data*>(pData);

    ia_err ret = mCca->setStatsParams(params->inParams);

    return static_cast<int>(ret);
}

int CcaWorker::runAEC(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_run_aec_data* params = reinterpret_cast<intel_cca_run_aec_data*>(pData);

    ia_err ret = mCca->runAEC(params->frameId, params->inParams, &params->results);

    return static_cast<int>(ret);
}

int CcaWorker::runAIQ(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_run_aiq_data* params = reinterpret_cast<intel_cca_run_aiq_data*>(pData);

    ia_err ret = mCca->runAIQ(params->frameId, params->inParams, &params->results);

    return static_cast<int>(ret);
}

int CcaWorker::updateTuning(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_update_tuning_data* params = reinterpret_cast<intel_cca_update_tuning_data*>(pData);

    ia_err ret = mCca->updateTuning(params->lardTags, params->lardParams, params->nvmParams,
                                    params->streamId);

    return static_cast<int>(ret);
}

ia_err CcaWorker::getTerminalBuf(intel_cca_aic_control_data* params) {
    for (uint32_t cb = 0; cb < params->termConfig.cb_num; cb++) {
        for (uint32_t terminal = 0; terminal < params->termConfig.cb_terminal_buf[cb].num_terminal;
             terminal++) {
            int32_t handle =
                params->termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payloadHandle;
            if (handle >= 0) {
                void* bufferAddr = mIPACallback->getBuffer(handle);
                if (!bufferAddr) {
                    LOG(IPAIPU, Error) << "failed to get payloadInfo";
                    return ia_err_argument;
                }
                params->termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payloadServerAddr =
                    bufferAddr;
            }
        }
    }

    return ia_err_none;
}

int CcaWorker::configAIC(uint8_t* pData, int dataSize) {
    if (!pData) return static_cast<int>(ia_err_argument);

    cca::cca_aic_config config = {};
    cca::cca_aic_kernel_offset kernelOffset = {};
    cca::cca_aic_terminal_config termConfig = {};
    int32_t* statsBufToTermIds = nullptr;
    int32_t aicId = 0;

    intel_cca_aic_control_data* params = reinterpret_cast<intel_cca_aic_control_data*>(pData);
    if (params->kernelOffset.offsetHandle >= 0) {
        params->kernelOffset.offsetBuffer =
            static_cast<uint32_t*>(mIPACallback->getBuffer(params->kernelOffset.offsetHandle));
        if (!params->kernelOffset.offsetBuffer) {
            LOG(IPAIPU, Error) << "failed to get offsetBuffer";
            return static_cast<int>(ia_err_argument);
        }
    }

    ia_err ret = getTerminalBuf(params);
    if (ret != ia_err_none) return static_cast<int>(ret);

    bool rt = mIpcCca.serverUnflattenConfigAic(pData, dataSize, config, kernelOffset, termConfig,
                                               aicId, statsBufToTermIds);
    if (!rt) return static_cast<int>(ia_err_internal);

    ret = mCca->configAIC(config, kernelOffset, termConfig, aicId,
                          statsBufToTermIds);
    mIpcCca.flattenTerminalConfig(
        reinterpret_cast<intel_cca_aic_control_data*>(pData)->termConfig, termConfig);

    return static_cast<int>(ret);
}

int CcaWorker::registerAicBuf(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_aic_control_data* params = reinterpret_cast<intel_cca_aic_control_data*>(pData);

    ia_err ret = getTerminalBuf(params);
    if (ret != ia_err_none) return static_cast<int>(ret);

    cca::cca_aic_terminal_config termConfig = {};
    mIpcCca.unFlattenTerminalConfig(params->termConfig, termConfig);

    for (uint32_t cb = 0; cb < params->termConfig.cb_num; cb++) {
        for (uint32_t terminal = 0; terminal < params->termConfig.cb_terminal_buf[cb].num_terminal;
             terminal++) {
            void* serverAddr =
                params->termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payloadServerAddr;
            if (serverAddr) {
                mServerToClientPayloadMap[serverAddr] = params->termConfig.cb_terminal_buf[cb]
                                                            .terminal_buf[terminal]
                                                            .payload.payloadPtr;
                params->termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload.payloadPtr =
                    serverAddr;
            }
        }
    }

    ret = mCca->registerAICBuf(termConfig, params->aicId);

    return static_cast<int>(ret);
}

int CcaWorker::getAicBuf(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_aic_control_data* params = reinterpret_cast<intel_cca_aic_control_data*>(pData);

    ia_err ret = getTerminalBuf(params);
    if (ret != ia_err_none) return static_cast<int>(ret);

    cca::cca_aic_terminal_config termConfig = {};
    mIpcCca.unFlattenTerminalConfig(params->termConfig, termConfig);
    ret = mCca->getAICBuf(termConfig, params->aicId);
    if (ret != ia_err_none) return static_cast<int>(ret);

    for (uint32_t cb = 0; cb < termConfig.cb_num; cb++) {
        for (uint32_t terminal = 0; terminal < termConfig.cb_terminal_buf[cb].num_terminal;
             terminal++) {
            void* serverAddr =
                termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload->payloadPtr;
            if (serverAddr &&
                mServerToClientPayloadMap.find(serverAddr) != mServerToClientPayloadMap.end()) {
                params->termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload.payloadPtr =
                    mServerToClientPayloadMap[serverAddr];
                termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload = nullptr;
            }
        }
    }
    mIpcCca.flattenTerminalConfig(params->termConfig, termConfig);

    return 0;
}

int CcaWorker::updateConfigurationResolutions(uint8_t* pData, int dataSize) {
    if (!pData) return static_cast<int>(ia_err_argument);

    cca::cca_aic_config config = {};
    int32_t aicId = 0;
    bool isKeyResChanged = false;
    bool rt = mIpcCca.serverUnflattenUpdateCfgRes(pData, dataSize, config, aicId, isKeyResChanged);
    if (!rt) return static_cast<int>(ia_err_internal);

    ia_err ret = mCca->updateConfigurationResolutions(config, aicId, isKeyResChanged);

    return static_cast<int>(ret);
}

int CcaWorker::runAIC(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);
    cca::cca_multi_pal_output output = {};

    intel_cca_run_aic_data* params = reinterpret_cast<intel_cca_run_aic_data*>(pData);

    if (params->inParamsHandle >= 0) {
        void* bufferAddr = mIPACallback->getBuffer(params->inParamsHandle);
        if (!bufferAddr) {
            LOG(IPAIPU, Error) << "failed to get inParams";
            return static_cast<int>(ia_err_argument);
        }
        params->inParams = static_cast<cca::cca_pal_input_params*>(bufferAddr);
    }

    ia_err ret = mCca->runAIC(params->frameId, *params->inParams, output, params->bitmap,
                              params->aicId);

    return static_cast<int>(ret);
}

int CcaWorker::getCMC(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_get_cmc_data* params = reinterpret_cast<intel_cca_get_cmc_data*>(pData);

    ia_err ret = mCca->getCMC(params->results);

    LOG(IPAIPU, Debug) << "iso: " << params->results.base_iso << " max_ag: "
                       << params->results.max_ag << " max_dg: " << params->results.max_dg;

    return static_cast<int>(ret);
}

int CcaWorker::getMKN(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_mkn_data* params = reinterpret_cast<intel_cca_mkn_data*>(pData);

    ia_err ret = mCca->getMKN(params->type, *params->results);

    return static_cast<int>(ret);
}

int CcaWorker::getAiqd(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_get_aiqd_data* params = reinterpret_cast<intel_cca_get_aiqd_data*>(pData);

    ia_err ret = mCca->getAiqd(params->results);

    return static_cast<int>(ret);
}

int CcaWorker::decodeStats(uint8_t* pData) {
    if (!pData) return static_cast<int>(ia_err_argument);

    intel_cca_decode_stats_data* params = reinterpret_cast<intel_cca_decode_stats_data*>(pData);
    auto& outStats = params->outStats;

    ia_err ret = mCca->decodeStats(params->groupId, params->sequence, params->aicId);

    if (ret == ia_err_none && outStats.get_rgbs_stats) {
        auto stats = mCca->queryStatsBuf(cca::STATS_BUF_LATEST);
        if (stats) {
            outStats.rgbs_grid[0].grid_width = stats->stats.rgbs_grids[0].grid_width;
            outStats.rgbs_grid[0].grid_height = stats->stats.rgbs_grids[0].grid_height;
            outStats.rgbs_grid[0].shading_correction = stats->stats.shading_corrected;

            unsigned int width = stats->stats.rgbs_grids[0].grid_width;
            unsigned int height = stats->stats.rgbs_grids[0].grid_height;
            for (unsigned int i = 0; i < width * height; i++) {
                outStats.rgbs_blocks[0][i].avg_gr = stats->stats.rgbs_grids[0].avg[i].gr;
                outStats.rgbs_blocks[0][i].avg_r = stats->stats.rgbs_grids[0].avg[i].r;
                outStats.rgbs_blocks[0][i].avg_b = stats->stats.rgbs_grids[0].avg[i].b;
                outStats.rgbs_blocks[0][i].avg_gb = stats->stats.rgbs_grids[0].avg[i].gb;
                outStats.rgbs_blocks[0][i].sat = stats->stats.rgbs_grids[0].sat[i];
            }
        }
    }

    return static_cast<int>(ret);
}

} /* namespace ipa::ipu7 */
} /* namespace libcamera */
