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
#include <string.h>
#include <stdlib.h>
#include "os_types.h"
#include <netinet/icmp6.h>

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

#define ota_buffer_size     (1050)
#define SOCKET_RECV_TIMEOUT 5
#define OTA_SOCKET_PORT     0x1234
#define OTA_MULTCAST_HOPS   20

typedef enum {
    OTA_START     = 0,
    OTA_INTERNAL  = 1,
    OTA_END       = 2,
    GET_BLOCK     = 3,
    PUT_BLOCK     = 4   
} ota_ctl_cmd_t;

typedef enum {
    SOCKET_OPEN_ERR         = -10,
    SOCKET_BIND_ERR         = -11,
    SOCKET_SET_MHOPS_ERR    = -12,
    SOCKET_RECV_TIMEO_ERR   = -13,
} ota_error_t;


typedef struct block_header {
    uint16_t control_cmd;              
	uint16_t ver_num;
	uint16_t pkt_crc;
    uint32_t image_crc;
    uint16_t block_seq_num;
	uint16_t block_total_num;
    uint16_t pkt_len;
    uint16_t checksum;
} __attribute__((packed)) block_header_t;

typedef struct node_ota_attr {
    uint16_t ota_port_num;
    int32_t ota_multicast_hops;
    struct sockaddr_in6 ota_dst_addr;
    struct sockaddr_in6 ota_src_addr;
    uint8_t recv_image_flg;
    int ota_sid;

    uint8_t ota_upgrade_flg;// = 1;                        
    uint16_t block_seq;//       = 0x00;                                              
    uint32_t image_size;//      = 0x00;   
    uint8_t  upgrading_flg;//             = 0x00;
    uint16_t upgrading_ver_num;//         = 0x0616;
    uint32_t upgrading_image_crc;//       = 0x00000000;
    uint16_t upgrading_total_block_num;// = 0x0000;
    uint16_t upgrading_block_size;//      = 0x0400;    // block size is 1024
    uint32_t upgrading_total_byte;//      = 0x00000000;
    uint16_t upgrading_last_block_size;// = 0x0000;
    uint32_t upgrading_total_pkt_size;//  = 0x00000000;    
    uint8_t  block_hdr_size;//  = 0x00;

    block_header_t tran_fwupgrade_hdr;
    block_header_t recv_fwupgrade_hdr;
    uint8_t tmp_buffer[ota_buffer_size];
    uint8_t ota_buffer[ota_buffer_size];
    uint8_t *ota_upgrade_array;
} node_ota_attr_t;


#endif

