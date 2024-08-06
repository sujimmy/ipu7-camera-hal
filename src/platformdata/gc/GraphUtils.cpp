/*
 * Copyright (C) 2018-2023 Intel Corporation
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
#define LOG_TAG GraphUtils

#include "GraphUtils.h"

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "CBLayoutUtils.h"

namespace icamera {

#define GET_FOURCC_FMT(a, b, c, d) \
    ((uint32_t)(d) | ((uint32_t)(c) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

using namespace CBLayoutUtils;

int32_t GraphUtils::createStageId(uint8_t resourceId, uint8_t contextId) {
    // ipu stage id start with 0x10, resourceId uses bit7:4, contextId used bit3:0
    return (IPU_STAGE_ID_BASE + ((resourceId << 4) & 0xF0) + (contextId & 0xF));
}

uint8_t GraphUtils::getResourceId(int32_t stageId) {
    return static_cast<uint8_t>(((stageId - IPU_STAGE_ID_BASE) & 0xF0) >> 4);
}

uint8_t GraphUtils::getContextId(int32_t stageId) {
    return static_cast<uint8_t>((stageId - IPU_STAGE_ID_BASE) & 0xF);
}

const char* GraphUtils::getStageName(int32_t stageId, int32_t streamId) {
    uint8_t ipuResourceId = getResourceId(stageId);

    if (ipuResourceId == NODE_RESOURCE_ID_BBPS)
        return (streamId == STILL_STREAM_ID) ? "bbps1" : "bbps";

    if (ipuResourceId == NODE_RESOURCE_ID_LBFF)
        return (streamId == STILL_STREAM_ID) ? "lbff1" : "lbff";

    return (stageId == SW_POST_STAGE_ID_BASE) ? "post_0"
          : (stageId == SW_POST_STAGE_ID_BASE + 1) ? "post_1"
          : (stageId == SW_POST_STAGE_ID_BASE + 2) ? "post_2"
          : "UNKNOWN";
}

int32_t GraphUtils::getFourccFmt(uint8_t resourceId, int32_t terminalId, int32_t bpp) {
    // Hard code, if no format info in static graph
    // Only for frame terminal
    if (resourceId == NODE_RESOURCE_ID_LBFF) {
        // LB input
        if (terminalId == LBFF_TERMINAL_CONNECT_MAIN_DATA_INPUT ||
            terminalId == LBFF_TERMINAL_CONNECT_LSC_INPUT)
            return (bpp == 10) ? GET_FOURCC_FMT('G', 'R', '1', '0')
                                : GET_FOURCC_FMT('G', 'R', '0', '8');
        // LB output
        if (terminalId == LBFF_TERMINAL_CONNECT_ME_OUTPUT ||
            terminalId == LBFF_TERMINAL_CONNECT_PS_OUTPUT)
            return (bpp == 8) ? GET_FOURCC_FMT('V', '4', '2', '0') : 0;
    }

    if (resourceId == NODE_RESOURCE_ID_BBPS) {
        // BB input
        if (terminalId == BBPS_TERMINAL_CONNECT_TNR_BC_YUV4N_IFD ||
            terminalId == BBPS_TERMINAL_CONNECT_SLIM_SPATIAL_YUVN_IFD)
            return (bpp == 8) ? GET_FOURCC_FMT('V', '4', '2', '0') : 0;
        // BB output
        if (terminalId == BBPS_TERMINAL_CONNECT_OFS_MP_YUVN_ODR ||
            terminalId == BBPS_TERMINAL_CONNECT_OFS_DP_YUVN_ODR)
            return (bpp == 10) ? GET_FOURCC_FMT('P', '0', '1', '0')
                                : GET_FOURCC_FMT('N', 'V', '1', '2');
    }

    LOGW("%s: no fourcc for resourceId %d, term %d", __func__, resourceId, terminalId);
    return 0;
}

void GraphUtils::dumpConnections(const std::vector<IGraphType::PipelineConnection>& connections) {
    if (!Log::isLogTagEnabled(GET_FILE_SHIFT(GraphUtils))) return;

    LOG3("Graph connections:");
    for (auto& conn : connections) {
        LOG3(
            "Format settings: enabled === %d ===, terminalIdx %x, width %d, height %d, fourcc %s, "
            "bpl %d, bpp %d",
            conn.portFormatSettings.enabled, conn.portFormatSettings.terminalId,
            conn.portFormatSettings.width, conn.portFormatSettings.height,
            CameraUtils::fourcc2String(conn.portFormatSettings.fourcc).c_str(),
            conn.portFormatSettings.bpl, conn.portFormatSettings.bpp);

        LOG3(
            "Connection config: sourceStage %d(%x), sourceTerminal %d(%x), sourceIteration %d, "
            "sinkStage %d(%x), sinkTerminal %d(%x), sinkIteration %d, connectionType %d",
            GET_STAGE_ID(conn.connectionConfig.mSourceStage),
            conn.connectionConfig.mSourceStage,
            conn.connectionConfig.mSourceTerminal - conn.connectionConfig.mSourceStage - 1,
            conn.connectionConfig.mSourceTerminal, conn.connectionConfig.mSourceIteration,
            GET_STAGE_ID(conn.connectionConfig.mSinkStage),
            conn.connectionConfig.mSinkStage,
            conn.connectionConfig.mSinkTerminal - conn.connectionConfig.mSinkStage - 1,
            conn.connectionConfig.mSinkTerminal, conn.connectionConfig.mSinkIteration,
            conn.connectionConfig.mConnectionType);

        LOG3("Edge port: %d", conn.hasEdgePort);
    }

    return;
}

void GraphUtils::dumpKernelInfo(const ia_isp_bxt_program_group& programGroup) {
    if (!Log::isLogTagEnabled(GET_FILE_SHIFT(GraphUtils))) return;

    LOG3("Kernel info: count %d, opMode %d", programGroup.kernel_count,
         programGroup.operation_mode);

    for (unsigned int i = 0; i < programGroup.kernel_count; i++) {
        const ia_isp_bxt_run_kernels_t& curRunKernel = programGroup.run_kernels[i];

        LOG3("uid %d, streamId: %d, enabled %d", curRunKernel.kernel_uuid, curRunKernel.stream_id,
             curRunKernel.enable);

        if (programGroup.run_kernels[i].resolution_info) {
            LOG3(
                "Resolution: inputWidth %d, inputHeight %d, inputCrop %d %d %d %d,"
                "outputWidth %d, outputHeight %d, outputCrop %d %d %d %d,",
                curRunKernel.resolution_info->input_width,
                curRunKernel.resolution_info->input_height,
                curRunKernel.resolution_info->input_crop.left,
                curRunKernel.resolution_info->input_crop.top,
                curRunKernel.resolution_info->input_crop.right,
                curRunKernel.resolution_info->input_crop.bottom,
                curRunKernel.resolution_info->output_width,
                curRunKernel.resolution_info->output_height,
                curRunKernel.resolution_info->output_crop.left,
                curRunKernel.resolution_info->output_crop.top,
                curRunKernel.resolution_info->output_crop.right,
                curRunKernel.resolution_info->output_crop.bottom);
        }

        if (programGroup.run_kernels[i].resolution_history) {
            LOG3(
                "Resolution history: inputWidth %d, inputHeight %d, inputCrop %d %d %d %d,"
                "outputWidth %d, outputHeight %d, outputCrop %d %d %d %d,",
                curRunKernel.resolution_history->input_width,
                curRunKernel.resolution_history->input_height,
                curRunKernel.resolution_history->input_crop.left,
                curRunKernel.resolution_history->input_crop.top,
                curRunKernel.resolution_history->input_crop.right,
                curRunKernel.resolution_history->input_crop.bottom,
                curRunKernel.resolution_history->output_width,
                curRunKernel.resolution_history->output_height,
                curRunKernel.resolution_history->output_crop.left,
                curRunKernel.resolution_history->output_crop.top,
                curRunKernel.resolution_history->output_crop.right,
                curRunKernel.resolution_history->output_crop.bottom);
        }
        LOG3("metadata %d %d %d %d, bppInfo: %d %d, outputCount %d", curRunKernel.metadata[0],
             curRunKernel.metadata[1], curRunKernel.metadata[2], curRunKernel.metadata[3],
             curRunKernel.bpp_info.input_bpp, curRunKernel.bpp_info.output_bpp,
             curRunKernel.output_count);
    }

    return;
}
}  // namespace icamera
