/*
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */
#ifndef RCP_FW_UPDATE_H
#define RCP_FW_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "os_types.h"

typedef enum {
    CONTINUE            = 1,
    FWUPD_ACCEPT        = 2,
    FWUPD_REFUSE        = 3,
    GET_BLK             = 4,
    RECV_OK             = 5,  
    RECV_BAD_CRC        = 6,
    IMAGE_VERIFY_OK     = 7,
    IMAGE_VERIFY_FAIL   = 8  
} ota_host_cmd_t;

typedef struct rcp_fwupt_attr {
    uint8_t  force_upt;
    uint8_t  firmware_major_ver;
    uint8_t  firmware_minor_ver;
    uint8_t  firmware_patch_ver;
    uint32_t firmware_len;
    uint16_t block_size;
    uint16_t block_num;
    uint32_t firmware_crc32;
} rcp_fwupd_attr_t;

void rcp_send_fwupd_request(rcp_fwupd_attr_t* params);
void *rcp_firmware_update_thread(void *arg);
void *node_firmware_ota_thread(void *arg);

#endif

