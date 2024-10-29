/*
* INTEL CONFIDENTIAL
* Copyright (c) 2024 Intel Corporation
* All Rights Reserved.
*
* The source code contained or described herein and all documents related to
* the source code ("Material") are owned by Intel Corporation or its
* suppliers or licensors. Title to the Material remains with Intel
* Corporation or its suppliers and licensors. The Material may contain trade
* secrets and proprietary and confidential information of Intel Corporation
* and its suppliers and licensors, and is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may be
* used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*
* Unless otherwise agreed by Intel in writing, you may not remove or alter
* this notice or any other notice embedded in Materials by Intel or Intels
* suppliers or licensors in any way.
*/

#include "Ipu7xGraphResolutionConfiguratorAutogen.h"

uint32_t GraphResolutionConfiguratorHelper::getRunKernelUuid(GraphResolutionConfiguratorKernelRole role)
{
    switch (role)
    {
        case GraphResolutionConfiguratorKernelRole::UpScaler:    return 25569; // upscaler_1_0
        case GraphResolutionConfiguratorKernelRole::DownScaler:  return 20739; // b2i_ds_1_0_1
        case GraphResolutionConfiguratorKernelRole::FinalCropper:  return 36213; // lbff_crop_espa_1_1
    }

    return 0;
}

uint32_t GraphResolutionConfiguratorHelper::getRunKernelUuidOfOutput(HwSink hwSink, int32_t graphId, GraphLink** links)
{
    (void)graphId;
    (void)links;

    switch (hwSink)
    {
        case HwSink::ImageMpSink:    return 7175; // ofs_mp_bodr_regs_1_1
        case HwSink::ImageDpSink:    return 30277; // ofs_dp_bodr_regs_1_1
        case HwSink::ImagePppSink:    return 31882; // ofs_pp_bodr_regs_1_1
        case HwSink::AeOutSink:    return 55073; // aestatistics_2_1
    }

    return 0;
}

StaticGraphStatus GraphResolutionConfiguratorHelper::getRunKernelUuidForResHistoryUpdate(std::vector<uint32_t>& kernelUuids)
{
    // Must take only one from each resolution history index, since in static graph they all share the same
    // resolution history instance
    kernelUuids.push_back(44984);  // slim_tnr_spatial_bifd_yuvn_regs_1_1
    kernelUuids.push_back(30277);  // ofs_dp_bodr_regs_1_1
    kernelUuids.push_back(31882);  // ofs_pp_bodr_regs_1_1
    kernelUuids.push_back(11500);  // slim_tnr_sp_bc_bifd_yuv4nm1_regs_1_1
    kernelUuids.push_back(48987);  // tnr7_ims_1_1
    kernelUuids.push_back(44199);  // tnr_fp_blend_bifd_rs4n_regs_1_1
    kernelUuids.push_back(32696);  // tnr7_blend_1_0
    kernelUuids.push_back(57148);  // tnr_scale_fp_bodr_yuv4n_regs_1_1
    kernelUuids.push_back(33179);  // slim_tnr_sp_bc_bifd_rs4nm1_regs_1_1
    kernelUuids.push_back(6326);  // tnr_sp_bc_bifd_yuv4n_regs_1_1
    kernelUuids.push_back(27830);  // slim_tnr_fp_blend_bifd_yuvnm1_regs_1_1
    kernelUuids.push_back(2565);  // gdc7_1
    kernelUuids.push_back(40423);  // tm_app
    return StaticGraphStatus::SG_OK;
}

