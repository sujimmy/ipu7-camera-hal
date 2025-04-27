/*
 * Copyright (C) 2022-2023 Intel Corporation.
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

typedef enum _TerminalType
{
    TERMINAL_TYPE_LOAD,
    TERMINAL_TYPE_CONNECT,
    TERMINAL_TYPE_N
} TerminalType;

typedef enum _PacBufferType
{
    PAC_BUFFER_TYPE_PARAM_IN,
    PAC_BUFFER_TYPE_PARAM_OUT,
    PAC_BUFFER_TYPE_PROGRAM,
    PAC_BUFFER_TYPE_SPATIAL_IN,
    PAC_BUFFER_TYPE_SPATIAL_OUT,
    PAC_BUFFER_TYPE_FRAME_IN,
    PAC_BUFFER_TYPE_FRAME_OUT,
    PAC_BUFFER_TYPE_SYS_FRAG_SEQUENCER,
    PAC_BUFFER_TYPE_SR_FRAME_IN,
    PAC_BUFFER_TYPE_SR_FRAG_SEQUENCER,
    PAC_BUFFER_TYPE_NONE,
} PacBufferType;

typedef enum _TerminalBufferType
{
    TERMINAL_BUFFER_TYPE_DATA,
    TERMINAL_BUFFER_TYPE_METADATA,
    TERMINAL_BUFFER_TYPE_N
} TerminalBufferType;

typedef enum _TerminalDirection
{
    TERMINAL_DIR_IN,
    TERMINAL_DIR_OUT,
    TERMINAL_DIR_IN_OUT,
    TERMINAL_DIR_N
} TerminalDirection;

typedef struct _TerminalDescriptor
{
    int                 TerminalId;
    TerminalType        TerminalType;
    const char         *TerminalName;
    PacBufferType       PacBufferType;
    TerminalBufferType  TerminalBufferType;
    TerminalDirection   TerminalDirection;
    int                 TerminalLinkedKernel;
} TerminalDescriptor;

enum SW_ISYSTerminalID
{
    SW_ISYS_TERMINAL_CONNECT_INPUT,
    SW_ISYS_TERMINAL_CONNECT_OUTPUT,
    SW_ISYS_TERMINAL_CONNECT_INPUT_PDAF,
    SW_ISYS_TERMINAL_CONNECT_OUTPUT_PDAF,
    SW_ISYS_TERMINAL_CONNECT_INPUT_DOL_LONG,
    SW_ISYS_TERMINAL_CONNECT_OUTPUT_DOL_LONG,
};

enum LBFFTerminalID
{
    LBFF_TERMINAL_LOAD_ALGO_CACHED,
    LBFF_TERMINAL_LOAD_ALGO_FRAG_SEQ,
    LBFF_TERMINAL_LOAD_SYSTEM,
    LBFF_TERMINAL_CONNECT_MAIN_DATA_INPUT,
    LBFF_TERMINAL_CONNECT_LSC_INPUT,
    LBFF_TERMINAL_CONNECT_PDAF_DATA_INPUT,
    LBFF_TERMINAL_CONNECT_AE_OUTPUT,
    LBFF_TERMINAL_CONNECT_AF_STD_OUTPUT,
    LBFF_TERMINAL_CONNECT_AWB_STD_OUTPUT,
    LBFF_TERMINAL_CONNECT_AWB_SAT_OUTPUT,
    LBFF_TERMINAL_CONNECT_PDAF_OUTPUT,
    LBFF_TERMINAL_CONNECT_IR_OUTPUT,
    LBFF_TERMINAL_CONNECT_BURST_ISP_OUTPUT,
    LBFF_TERMINAL_CONNECT_ME_OUTPUT,
    LBFF_TERMINAL_CONNECT_PS_OUTPUT,
    LBFF_TERMINAL_CONNECT_GMV_INPUT,
    LBFF_TERMINAL_CONNECT_AWB_SVE_OUTPUT,
    LBFF_TERMINAL_CONNECT_AF_SVE_OUTPUT,
    LBFF_TERMINAL_CONNECT_GMV_MATCH_OUTPUT,
    LBFF_TERMINAL_CONNECT_GMV_FEATURE_OUTPUT,
};

enum BBPSTerminalID
{
    BBPS_TERMINAL_LOAD_ALGO_CACHED,
    BBPS_TERMINAL_LOAD_ALGO_FRAG_SEQ,
    BBPS_TERMINAL_LOAD_SYSTEM,
    BBPS_TERMINAL_LOAD_SR_FRAME_IN,
    BBPS_TERMINAL_LOAD_SR_FRAG_SEQ,
    BBPS_TERMINAL_CONNECT_SLIM_TNR_BC_YUV4NM1_IFD,
    BBPS_TERMINAL_CONNECT_SLIM_TNR_BC_RS4NM1_IFD,
    BBPS_TERMINAL_CONNECT_TNR_BC_YUV4N_IFD,
    BBPS_TERMINAL_CONNECT_TNR_BC_RS4N_ODR,
    BBPS_TERMINAL_CONNECT_SLIM_SPATIAL_YUVN_IFD,
    BBPS_TERMINAL_CONNECT_SLIM_TNR_BLEND_YUVNM1_IFD,
    BBPS_TERMINAL_CONNECT_TNR_BLEND_RS4N_IFD,
    BBPS_TERMINAL_CONNECT_TNR_BLEND_YUVN_ODR,
    BBPS_TERMINAL_CONNECT_TNR_SCALE_YUV4N_ODR,
    BBPS_TERMINAL_CONNECT_OFS_MP_YUVN_ODR,
    BBPS_TERMINAL_CONNECT_OFS_DP_YUVN_ODR,
    BBPS_TERMINAL_CONNECT_OFS_PP_YUVN_ODR,
};

enum SW_DOLTerminalID
{
    SW_DOL_TERMINAL_CONNECT_DOL_LONG,
    SW_DOL_TERMINAL_CONNECT_DOL_SHORT,
    SW_DOL_TERMINAL_CONNECT_OUTPUT,
};

enum SW_GDCTerminalID
{
    SW_GDC_TERMINAL_CONNECT_INPUT,
    SW_GDC_TERMINAL_CONNECT_OUTPUT,
};

enum SW_GTMTerminalID
{
    SW_GTM_TERMINAL_CONNECT_INPUT,
    SW_GTM_TERMINAL_CONNECT_OUTPUT,
};

extern TerminalDescriptor SW_ISYSTerminalDesc[];
extern TerminalDescriptor LBFFTerminalDesc[];
extern TerminalDescriptor BBPSTerminalDesc[];
extern TerminalDescriptor SW_DOLTerminalDesc[];
extern TerminalDescriptor SW_GDCTerminalDesc[];
extern TerminalDescriptor SW_GTMTerminalDesc[];

extern int CountOfSW_ISYSTerminalDesc;
extern int CountOfLBFFTerminalDesc;
extern int CountOfBBPSTerminalDesc;
extern int CountOfSW_DOLTerminalDesc;
extern int CountOfSW_GDCTerminalDesc;
extern int CountOfSW_GTMTerminalDesc;
