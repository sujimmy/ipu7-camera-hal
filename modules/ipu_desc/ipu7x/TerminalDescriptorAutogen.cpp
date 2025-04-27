/*
 * Copyright (C) 2023 Intel Corporation.
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

#include "TerminalDescriptorAutogen.h"

TerminalDescriptor SW_ISYSTerminalDesc[] =
{
    {
        SW_ISYS_TERMINAL_CONNECT_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_INPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        11470, // is_odr_a
    },
    {
        SW_ISYS_TERMINAL_CONNECT_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        11470, // is_odr_a
    },
    {
        SW_ISYS_TERMINAL_CONNECT_INPUT_PDAF,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_INPUT_PDAF",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        55449, // is_odr_b
    },
    {
        SW_ISYS_TERMINAL_CONNECT_OUTPUT_PDAF,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OUTPUT_PDAF",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        55449, // is_odr_b
    },
    {
        SW_ISYS_TERMINAL_CONNECT_INPUT_DOL_LONG,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_INPUT_DOL_LONG",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        50407, // is_odr_c
    },
    {
        SW_ISYS_TERMINAL_CONNECT_OUTPUT_DOL_LONG,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OUTPUT_DOL_LONG",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        50407, // is_odr_c
    },
};

TerminalDescriptor LBFFTerminalDesc[] =
{
    {
        LBFF_TERMINAL_LOAD_ALGO_CACHED,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_ALGO_CACHED",
        PAC_BUFFER_TYPE_PARAM_IN,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        0,
    },
    {
        LBFF_TERMINAL_LOAD_ALGO_FRAG_SEQ,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_ALGO_FRAG_SEQ",
        PAC_BUFFER_TYPE_PROGRAM,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        0,
    },
    {
        LBFF_TERMINAL_LOAD_SYSTEM,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_SYSTEM",
        PAC_BUFFER_TYPE_SYS_FRAG_SEQUENCER,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        0,
    },
    {
        LBFF_TERMINAL_CONNECT_MAIN_DATA_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_MAIN_DATA_INPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        52164, // ifd_pipe_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_LSC_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_LSC_INPUT",
        PAC_BUFFER_TYPE_SPATIAL_IN,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        6070, // ifd_lsc_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_PDAF_DATA_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_PDAF_DATA_INPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        3971, // ifd_pdaf_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_AE_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_AE_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        50677, // odr_ae_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_AF_STD_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_AF_STD_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        6500, // odr_af_std_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_AWB_STD_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_AWB_STD_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        20731, // odr_awb_std_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_AWB_SAT_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_AWB_SAT_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        54176, // odr_awb_sat_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_PDAF_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_PDAF_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        31724, // odr_pdaf_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_IR_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_IR_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        3371, // odr_ir_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_BURST_ISP_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_BURST_ISP_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        57981, // odr_burst_isp_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_ME_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_ME_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        55391, // odr_output_me_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_PS_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_PS_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        40915, // odr_output_ps_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_GMV_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_GMV_INPUT",
        PAC_BUFFER_TYPE_SPATIAL_IN,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        41864, // ifd_gmv_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_AWB_SVE_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_AWB_SVE_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        2452, // odr_awb_sve_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_AF_SVE_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_AF_SVE_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        0, // No graphs found
    },
    {
        LBFF_TERMINAL_CONNECT_GMV_MATCH_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_GMV_MATCH_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        13820, // odr_gmv_match_1_1
    },
    {
        LBFF_TERMINAL_CONNECT_GMV_FEATURE_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_GMV_FEATURE_OUTPUT",
        PAC_BUFFER_TYPE_SPATIAL_OUT,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        8985, // odr_gmv_feature_1_1
    },
};

TerminalDescriptor BBPSTerminalDesc[] =
{
    {
        BBPS_TERMINAL_LOAD_ALGO_CACHED,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_ALGO_CACHED",
        PAC_BUFFER_TYPE_PARAM_IN,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        0,
    },
    {
        BBPS_TERMINAL_LOAD_ALGO_FRAG_SEQ,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_ALGO_FRAG_SEQ",
        PAC_BUFFER_TYPE_PROGRAM,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        0,
    },
    {
        BBPS_TERMINAL_LOAD_SYSTEM,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_SYSTEM",
        PAC_BUFFER_TYPE_SYS_FRAG_SEQUENCER,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        0,
    },
    {
        BBPS_TERMINAL_LOAD_SR_FRAME_IN,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_SR_FRAME_IN",
        PAC_BUFFER_TYPE_SR_FRAME_IN,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN_OUT,
        0,
    },
    {
        BBPS_TERMINAL_LOAD_SR_FRAG_SEQ,
        TERMINAL_TYPE_LOAD,
        "TERMINAL_LOAD_SR_FRAG_SEQ",
        PAC_BUFFER_TYPE_SR_FRAG_SEQUENCER,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN_OUT,
        0,
    },
    {
        BBPS_TERMINAL_CONNECT_SLIM_TNR_BC_YUV4NM1_IFD,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_SLIM_TNR_BC_YUV4NM1_IFD",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        11500, // slim_tnr_sp_bc_bifd_yuv4nm1_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_SLIM_TNR_BC_RS4NM1_IFD,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_SLIM_TNR_BC_RS4NM1_IFD",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        33179, // slim_tnr_sp_bc_bifd_rs4nm1_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_TNR_BC_YUV4N_IFD,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_TNR_BC_YUV4N_IFD",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        6326, // tnr_sp_bc_bifd_yuv4n_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_TNR_BC_RS4N_ODR,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_TNR_BC_RS4N_ODR",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_OUT,
        48743, // tnr_sp_bc_bodr_rs4n_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_SLIM_SPATIAL_YUVN_IFD,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_SLIM_SPATIAL_YUVN_IFD",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        44984, // slim_tnr_spatial_bifd_yuvn_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_SLIM_TNR_BLEND_YUVNM1_IFD,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_SLIM_TNR_BLEND_YUVNM1_IFD",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        27830, // slim_tnr_fp_blend_bifd_yuvnm1_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_TNR_BLEND_RS4N_IFD,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_TNR_BLEND_RS4N_IFD",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_METADATA,
        TERMINAL_DIR_IN,
        44199, // tnr_fp_blend_bifd_rs4n_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_TNR_BLEND_YUVN_ODR,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_TNR_BLEND_YUVN_ODR",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        39844, // tnr_fp_bodr_yuvn_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_TNR_SCALE_YUV4N_ODR,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_TNR_SCALE_YUV4N_ODR",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        57148, // tnr_scale_fp_bodr_yuv4n_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_OFS_MP_YUVN_ODR,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OFS_MP_YUVN_ODR",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        7175, // ofs_mp_bodr_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_OFS_DP_YUVN_ODR,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OFS_DP_YUVN_ODR",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        30277, // ofs_dp_bodr_regs_1_1
    },
    {
        BBPS_TERMINAL_CONNECT_OFS_PP_YUVN_ODR,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OFS_PP_YUVN_ODR",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        31882, // ofs_pp_bodr_regs_1_1
    },
};

TerminalDescriptor SW_DOLTerminalDesc[] =
{
    {
        SW_DOL_TERMINAL_CONNECT_DOL_LONG,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_DOL_LONG",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        6265, // dol_lite_1_0
    },
    {
        SW_DOL_TERMINAL_CONNECT_DOL_SHORT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_DOL_SHORT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        6265, // dol_lite_1_0
    },
    {
        SW_DOL_TERMINAL_CONNECT_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        6265, // dol_lite_1_0
    },
};

TerminalDescriptor SW_GDCTerminalDesc[] =
{
    {
        SW_GDC_TERMINAL_CONNECT_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_INPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        2565, // gdc7_1
    },
    {
        SW_GDC_TERMINAL_CONNECT_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        2565, // gdc7_1
    },
};

TerminalDescriptor SW_GTMTerminalDesc[] =
{
    {
        SW_GTM_TERMINAL_CONNECT_INPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_INPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_IN,
        40423, // tm_app
    },
    {
        SW_GTM_TERMINAL_CONNECT_OUTPUT,
        TERMINAL_TYPE_CONNECT,
        "TERMINAL_CONNECT_OUTPUT",
        PAC_BUFFER_TYPE_NONE,
        TERMINAL_BUFFER_TYPE_DATA,
        TERMINAL_DIR_OUT,
        40423, // tm_app
    },
};

int CountOfSW_ISYSTerminalDesc = sizeof(SW_ISYSTerminalDesc) / sizeof(SW_ISYSTerminalDesc[0]);
int CountOfLBFFTerminalDesc = sizeof(LBFFTerminalDesc) / sizeof(LBFFTerminalDesc[0]);
int CountOfBBPSTerminalDesc = sizeof(BBPSTerminalDesc) / sizeof(BBPSTerminalDesc[0]);
int CountOfSW_DOLTerminalDesc = sizeof(SW_DOLTerminalDesc) / sizeof(SW_DOLTerminalDesc[0]);
int CountOfSW_GDCTerminalDesc = sizeof(SW_GDCTerminalDesc) / sizeof(SW_GDCTerminalDesc[0]);
int CountOfSW_GTMTerminalDesc = sizeof(SW_GTMTerminalDesc) / sizeof(SW_GTMTerminalDesc[0]);
