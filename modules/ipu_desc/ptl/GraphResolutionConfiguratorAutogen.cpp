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

#include "GraphResolutionConfiguratorAutogen.h"

uint32_t GraphResolutionConfiguratorHelper::getRunKernelUuid(GraphResolutionConfiguratorKernelRole role)
{
    switch (role)
    {
        case GraphResolutionConfiguratorKernelRole::UpScaler:    return 25569; // upscaler_1_0
        case GraphResolutionConfiguratorKernelRole::DownScaler:  return 40299; // b2i_ds_1_1
        case GraphResolutionConfiguratorKernelRole::FinalCropper:  return 42330; // lbff_crop_espa_1_3
    }

    return 0;
}

uint32_t GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(HwSink hwSink)
{
    switch (hwSink)
    {
        case HwSink::ImageMpSink:   return 18789; // ofs_mp_bodr_regs_1_3
        case HwSink::ImageDpSink:   return 27847; // ofs_dp_bodr_regs_1_3
        case HwSink::ProcessedMainSink:   return 2565; // gdc7_1
        default: break;
    }

    return 0;
}

StaticGraphStatus GraphResolutionConfiguratorHelper::getRunKernelUuidForResHistoryUpdate(std::vector<uint32_t>& kernelUuids)
{
    // Must take only one from each resolution history index, since in static graph they all share the same
    // resolution history instance
    kernelUuids.push_back(6907);  // slim_tnr_spatial_bifd_yuvn_regs_1_3
    kernelUuids.push_back(27847);  // ofs_dp_bodr_regs_1_3
    kernelUuids.push_back(25579);  // slim_tnr_sp_bc_bifd_yuv4nm1_regs_1_3
    kernelUuids.push_back(48987);  // tnr7_ims_1_1
    kernelUuids.push_back(42936);  // tnr_fp_blend_bifd_rs4n_regs_1_3
    kernelUuids.push_back(32696);  // tnr7_blend_1_0
    kernelUuids.push_back(20865);  // tnr_scale_fp_bodr_yuv4n_regs_1_3
    kernelUuids.push_back(48078);  // slim_tnr_sp_bc_bifd_rs4nm1_regs_1_3
    kernelUuids.push_back(57803);  // tnr_sp_bc_bifd_yuv4n_regs_1_3
    kernelUuids.push_back(26536);  // slim_tnr_fp_blend_bifd_yuvnm1_regs_1_3
    return StaticGraphStatus::SG_OK;
}

