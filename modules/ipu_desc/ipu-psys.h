/****************************************************************************
 * Copyright (C) 2022-2023 Intel Corporation.
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _UAPI_IPU_PSYS_H
#define _UAPI_IPU_PSYS_H
#include <stdint.h>

struct ipu_psys_capability {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t version;
  uint8_t driver[20];
  uint8_t dev_model[32];
  uint32_t reserved[17];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
enum ipu_psys_event_error {
  IPU_PSYS_EVT_ERROR_NONE = 0U,
  IPU_PSYS_EVT_ERROR_INTERNAL = 1U,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  IPU_PSYS_EVT_ERROR_FRAME = 2U,
  IPU_PSYS_EVT_ERROR_FORCE_CLOSED = 3U,
  IPU_PSYS_EVT_ERROR_MAX
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ipu_psys_event {
  uint8_t graph_id;
  uint8_t node_ctx_id;
  uint8_t frame_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t error;
  int32_t reserved[2];
} __attribute__((packed));
struct ipu_psys_buffer {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint64_t len;
  union {
    int fd;
    void *userptr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
    uint64_t reserved;
  } base;
  uint32_t data_offset;
  uint32_t bytes_used;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t flags;
  uint32_t reserved[2];
} __attribute__((packed));
#define MAX_GRAPH_NODES 5U
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MAX_GRAPH_NODE_PROFILES 1U
#define MAX_GRAPH_LINKS 10U
#define MAX_GRAPH_TERMINALS 32U
struct node_profile {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t teb[2];
  uint32_t deb[4];
  uint32_t rbm[4];
  uint32_t reb[4];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct node_ternimal {
  uint8_t term_id;
  uint32_t buf_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
struct graph_node {
  uint8_t node_rsrc_id;
  uint8_t node_ctx_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint8_t num_terms;
  struct node_profile profiles[MAX_GRAPH_NODE_PROFILES];
  struct node_ternimal terminals[MAX_GRAPH_TERMINALS];
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct graph_link_ep {
  uint8_t node_ctx_id;
  uint8_t term_id;
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPU_PSYS_FOREIGN_KEY_NONE UINT16_MAX
#define IPU_PSYS_LINK_PBK_ID_NONE UINT8_MAX
#define IPU_PSYS_LINK_PBK_SLOT_ID_NONE UINT8_MAX
#define IPU_PSYS_LINK_STREAMING_MODE_SOFF 0U
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct graph_link {
  struct graph_link_ep ep_src;
  struct graph_link_ep ep_dst;
  uint16_t foreign_key;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint8_t streaming_mode;
  uint8_t pbk_id;
  uint8_t pbk_slot_id;
  uint8_t delayed_link;
} __attribute__((packed));
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ipu_psys_graph_info {
  uint8_t graph_id;
  uint8_t num_nodes;
  struct graph_node *nodes;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct graph_link links[MAX_GRAPH_LINKS];
} __attribute__((packed));
struct ipu_psys_term_buffers {
  uint8_t term_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  struct ipu_psys_buffer term_buf;
} __attribute__((packed));
struct ipu_psys_task_request {
  uint8_t graph_id;
  uint8_t node_ctx_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint8_t frame_id;
  uint32_t payload_reuse_bm[2];
  uint8_t term_buf_count;
  struct ipu_psys_term_buffers *task_buffers;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed));
#define IPU_BUFFER_FLAG_INPUT (1 << 0)
#define IPU_BUFFER_FLAG_OUTPUT (1 << 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPU_BUFFER_FLAG_MAPPED (1 << 2)
#define IPU_BUFFER_FLAG_NO_FLUSH (1U << 3)
#define IPU_BUFFER_FLAG_DMA_HANDLE (1U << 4)
#define IPU_BUFFER_FLAG_USERPTR (1U << 5)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPU_IOC_QUERYCAP _IOR('A', 1, struct ipu_psys_capability)
#define IPU_IOC_MAPBUF _IOWR('A', 2, int)
#define IPU_IOC_UNMAPBUF _IOWR('A', 3, int)
#define IPU_IOC_GETBUF _IOWR('A', 4, struct ipu_psys_buffer)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPU_IOC_PUTBUF _IOWR('A', 5, struct ipu_psys_buffer)
#define IPU_IOC_DQEVENT _IOWR('A', 6, struct ipu_psys_event)
#define IPU_IOC_GRAPH_OPEN _IOWR('A', 7, struct ipu_psys_graph_info)
#define IPU_IOC_TASK_REQUEST _IOWR('A', 8, struct ipu_psys_task_request)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define IPU_IOC_GRAPH_CLOSE _IOWR('A', 9, int)
#endif
