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
    CONTINUE      = 1,
    START_ACK     = 2,
    GET_BLK       = 3,
    RECV_OK       = 4  
} ota_host_cmd_t;

typedef struct rcp_fwupt_attr {
    uint8_t  force_upt;
    uint32_t fw_len;
    uint16_t block_size;
    uint16_t block_num;
} rcp_fwupd_attr_t;

void rcp_firmware_update_start(rcp_fwupd_attr_t* params);
void *rcp_firmware_update_thread(void *arg);

#endif

