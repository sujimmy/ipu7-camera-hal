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

#include "IPCIntelCca.h"

namespace icamera {

// macro for memcpy
#ifndef MEMCPY_S
#define MEMCPY_S(dest, dmax, src, smax) \
    memcpy((dest), (src), std::min((size_t)(dmax), (size_t)(smax)))
#endif

void IPCIntelCca::flattenTerminalConfig(ipc_cca_aic_terminal_config& terminalConfig,
                                        const cca::cca_aic_terminal_config& termConfig) {
    terminalConfig.cb_num = termConfig.cb_num;
    for (uint32_t cb = 0; cb < termConfig.cb_num; cb++) {
        terminalConfig.cb_terminal_buf[cb].group_id = termConfig.cb_terminal_buf[cb].group_id;
        terminalConfig.cb_terminal_buf[cb].num_terminal =
            termConfig.cb_terminal_buf[cb].num_terminal;
        for (uint32_t terminal = 0; terminal < termConfig.cb_terminal_buf[cb].num_terminal;
             terminal++) {
            terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].terminal_index =
                termConfig.cb_terminal_buf[cb].terminal_buf[terminal].terminal_index;
            terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].buf_size =
                termConfig.cb_terminal_buf[cb].terminal_buf[terminal].buf_size;
            terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].fragment_index =
                termConfig.cb_terminal_buf[cb].terminal_buf[terminal].fragment_index;

            if (termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload &&
                (termConfig.cb_terminal_buf[cb].terminal_buf[terminal].buf_size > 0)) {
                terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload =
                    *(termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload);
            }
        }
    }
}

void IPCIntelCca::unFlattenTerminalConfig(ipc_cca_aic_terminal_config& terminalConfig,
                                          cca::cca_aic_terminal_config& termConfig) {
    termConfig.cb_num = terminalConfig.cb_num;
    for (uint32_t cb = 0; cb < terminalConfig.cb_num; cb++) {
        termConfig.cb_terminal_buf[cb].group_id = terminalConfig.cb_terminal_buf[cb].group_id;
        termConfig.cb_terminal_buf[cb].num_terminal =
            terminalConfig.cb_terminal_buf[cb].num_terminal;
        for (uint32_t terminal = 0; terminal < terminalConfig.cb_terminal_buf[cb].num_terminal;
             terminal++) {
            termConfig.cb_terminal_buf[cb].terminal_buf[terminal].terminal_index =
                terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].terminal_index;
            termConfig.cb_terminal_buf[cb].terminal_buf[terminal].buf_size =
                terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].buf_size;
            termConfig.cb_terminal_buf[cb].terminal_buf[terminal].fragment_index =
                terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].fragment_index;

            if (termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload) {
                *(termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload) =
                    terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload;
            } else {
                termConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload =
                    &(terminalConfig.cb_terminal_buf[cb].terminal_buf[terminal].payload);
            }
        }
    }
}

void IPCIntelCca::flattenAicConfig(const cca::cca_aic_config& aicCfg,
                                   ipc_cca_aic_config* ipcCfg) {
    ipcCfg->cbNum = aicCfg.cb_num;
    for (uint32_t cb = 0; cb < aicCfg.cb_num; cb++) {
        const cca::cca_cb_config& aicCb = aicCfg.cb_config[cb];
        cca::cca_cb_config& ipcCb = ipcCfg->cbConfig[cb];

        ipcCb.group_id = aicCb.group_id;
        ipcCb.fragment_count = aicCb.fragment_count;

        ipcCb.kernel_group = &ipcCfg->data[cb].kernelGroup;
        ipcCb.kernel_group->kernelCount = aicCb.kernel_group->kernelCount;
        ipcCb.kernel_group->operationMode = aicCb.kernel_group->operationMode;
        ipcCb.kernel_group->streamId = aicCb.kernel_group->streamId;
        ipcCb.kernel_group->kernelList = ipcCfg->data[cb].kernelList;

        int systemApiDataOffset = 0;
        for (uint32_t kernel = 0; kernel < aicCfg.cb_config[cb].kernel_group->kernelCount;
             kernel++) {
            const aic::ia_pac_kernel_info& aicKL = aicCb.kernel_group->kernelList[kernel];
            aic::ia_pac_kernel_info& ipcKL = ipcCb.kernel_group->kernelList[kernel];

            ipcKL.fragments_defined = aicKL.fragments_defined;
            // TODO: fix desc ptr error in StaticGraph
            ipcKL.fragment_descs = nullptr;
            /*
            if (aicKL.fragment_descs) {
                ipcKL.fragment_descs = ipcCfg->data[cb].fragmentDescs[kernel];
                *ipcKL.fragment_descs = *aicKL.fragment_descs;
            } else {
                ipcKL.fragment_descs = nullptr;
            }
            */

            const ia_isp_bxt_run_kernels& aicK = aicKL.run_kernel;
            ia_isp_bxt_run_kernels& ipcK = ipcKL.run_kernel;
            ipcK = aicK;
            if (aicK.resolution_info) {
                ipcK.resolution_info = &ipcCfg->data[cb].resolutionInfo[kernel];
                *ipcK.resolution_info = *aicK.resolution_info;
            } else {
                ipcK.resolution_info = nullptr;
            }
            if (aicK.resolution_history) {
                ipcK.resolution_history = &ipcCfg->data[cb].resolutionHistory[kernel];
                *ipcK.resolution_history = *aicK.resolution_history;
            } else {
                ipcK.resolution_history = nullptr;
            }

            if (aicK.system_api.data) {
                ipcK.system_api.size = aicK.system_api.size;
                ipcK.system_api.data = &ipcCfg->data[cb].systemApiData[systemApiDataOffset];
                MEMCPY_S(ipcK.system_api.data, ipcK.system_api.size,
                         aicK.system_api.data, aicK.system_api.size);
                systemApiDataOffset += ipcK.system_api.size;
            } else {
                ipcK.system_api.size = 0;
                ipcK.system_api.data = nullptr;
            }
        }
    }
}

bool IPCIntelCca::clientFlattenConfigAic(void* pData, uint32_t size,
                                         const cca::cca_aic_config& aicConf,
                                         const cca::cca_aic_kernel_offset& kernelOffset,
                                         cca::cca_aic_terminal_config& termConfig, int32_t aicId,
                                         const int32_t* statsBufToTermIds) {
    if (!pData || size < sizeof(intel_cca_aic_control_data)) return false;

    intel_cca_aic_control_data* aicControl = static_cast<intel_cca_aic_control_data*>(pData);

    aicControl->aicId = aicId;
    flattenAicConfig(aicConf, &aicControl->config);

    // kernel offset flatten
    ipc_cca_aic_kernel_offset& ipcKernelOffset = aicControl->kernelOffset;
    ipcKernelOffset.cb_num = kernelOffset.cb_num;
    for (uint32_t cb = 0; cb < kernelOffset.cb_num; cb++) {
        ipcKernelOffset.cb_kernel_offset[cb].group_id = kernelOffset.cb_kernel_offset[cb].group_id;
        ipcKernelOffset.cb_kernel_offset[cb].num_kernels =
            kernelOffset.cb_kernel_offset[cb].num_kernels;
        for (uint32_t kernel = 0; kernel < kernelOffset.cb_kernel_offset[cb].num_kernels;
             kernel++) {
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].num_offsets =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].num_offsets;
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].uuid =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].uuid;
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_type =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_type;
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_index =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_index;
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].fragment =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].fragment;
            // offsets&sizes store the offset value in share memory
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].offsets =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].offsets -
                ipcKernelOffset.offsetBuffer;
            ipcKernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].sizes =
                kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].sizes -
                ipcKernelOffset.offsetBuffer;
        }
    }

    flattenTerminalConfig(aicControl->termConfig, termConfig);
    aicControl->isKeyResChanged = false;
    if (statsBufToTermIds)
        MEMCPY_S(aicControl->statsBufToTermIds, sizeof(aicControl->statsBufToTermIds),
                 statsBufToTermIds, sizeof(aicControl->statsBufToTermIds));
    else
        aicControl->statsBufToTermIds[0] = -1;

    return true;
}

bool IPCIntelCca::clientFlattenUpdateCfgRes(void* pData, uint32_t size,
                                            const cca::cca_aic_config& aicConf, int32_t aicId,
                                            bool isKeyResChanged) {
    if (!pData || size < sizeof(intel_cca_aic_control_data)) return false;

    intel_cca_aic_control_data* aicControl = static_cast<intel_cca_aic_control_data*>(pData);

    aicControl->aicId = aicId;
    aicControl->config.cbNum = aicConf.cb_num;
    flattenAicConfig(aicConf, &aicControl->config);
    aicControl->isKeyResChanged = isKeyResChanged;

    return true;
}

void IPCIntelCca::unflattenAicConfig(ipc_cca_aic_config& ipcCfg,
                                     cca::cca_aic_config& aicCfg) {
    aicCfg.cb_num = ipcCfg.cbNum;
    MEMCPY_S(&aicCfg.cb_config, sizeof(aicCfg.cb_config), &ipcCfg.cbConfig,
             sizeof(ipcCfg.cbConfig));

    // Update pointers
    for (uint32_t cb = 0; cb < ipcCfg.cbNum; cb++) {
        cca::cca_cb_config& aicCb = aicCfg.cb_config[cb];

        aicCb.kernel_group = &ipcCfg.data[cb].kernelGroup;
        aicCb.kernel_group->kernelList = ipcCfg.data[cb].kernelList;

        int systemApiDataOffset = 0;
        for (uint32_t kernel = 0; kernel < aicCb.kernel_group->kernelCount; kernel++) {
            aic::ia_pac_kernel_info& aicKL = aicCb.kernel_group->kernelList[kernel];

            if (aicKL.fragment_descs) {
                aicKL.fragment_descs = &ipcCfg.data[cb].fragmentDescs[kernel];
            }

            ia_isp_bxt_run_kernels& aicK = aicKL.run_kernel;
            if (aicK.resolution_info) {
                aicK.resolution_info = &ipcCfg.data[cb].resolutionInfo[kernel];
            }
            if (aicK.resolution_history) {
                aicK.resolution_history = &ipcCfg.data[cb].resolutionHistory[kernel];
            }

            if (aicK.system_api.data) {
                aicK.system_api.data = &ipcCfg.data[cb].systemApiData[systemApiDataOffset];
                systemApiDataOffset += aicK.system_api.size;
            }
        }
    }
}

bool IPCIntelCca::serverUnflattenConfigAic(void* pData, uint32_t size, cca::cca_aic_config& aicConf,
                                           cca::cca_aic_kernel_offset& kernelOffset,
                                           cca::cca_aic_terminal_config& termConfig,
                                           int32_t& aicId, int32_t*& statsBufToTermIds) {
    if (!pData || size < sizeof(intel_cca_aic_control_data)) return false;

    intel_cca_aic_control_data* aicControl = static_cast<intel_cca_aic_control_data*>(pData);

    aicId = aicControl->aicId;
    unflattenAicConfig(aicControl->config, aicConf);

    // unflatten offset
    kernelOffset.cb_num = aicControl->kernelOffset.cb_num;
    for (uint32_t cb = 0; cb < kernelOffset.cb_num; cb++) {
        kernelOffset.cb_kernel_offset[cb].group_id =
            aicControl->kernelOffset.cb_kernel_offset[cb].group_id;
        kernelOffset.cb_kernel_offset[cb].num_kernels =
            aicControl->kernelOffset.cb_kernel_offset[cb].num_kernels;
        for (uint32_t kernel = 0; kernel < kernelOffset.cb_kernel_offset[cb].num_kernels;
             kernel++) {
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].num_offsets =
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].num_offsets;
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].uuid =
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].uuid;
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_type =
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_type;
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_index =
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].terminal_index;
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].fragment =
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].fragment;
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].offsets =
                aicControl->kernelOffset.offsetBuffer +
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].offsets;
            kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].sizes =
                aicControl->kernelOffset.offsetBuffer +
                aicControl->kernelOffset.cb_kernel_offset[cb].kernels_offset[kernel].sizes;
        }
    }

    unFlattenTerminalConfig(aicControl->termConfig, termConfig);
    statsBufToTermIds = (aicControl->statsBufToTermIds[0] >= 0) ? aicControl->statsBufToTermIds
                                                                : nullptr;
    return true;
}

bool IPCIntelCca::serverUnflattenUpdateCfgRes(void* pData, uint32_t size,
                                              cca::cca_aic_config& aicConf, int32_t& aicId,
                                              bool& isKeyResChanged) {
    if (!pData || size < sizeof(intel_cca_aic_control_data)) return false;

    intel_cca_aic_control_data* aicControl = static_cast<intel_cca_aic_control_data*>(pData);

    aicId = aicControl->aicId;
    unflattenAicConfig(aicControl->config, aicConf);
    isKeyResChanged = aicControl->isKeyResChanged;

    return true;
}

} /* namespace icamera */
