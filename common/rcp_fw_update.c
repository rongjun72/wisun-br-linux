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
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/file.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>

#include "crc.h"
#include "log.h"
#include "common/bus_uart.h"
#include "utils.h"
#include "os_types.h"
#include "common/iobuf.h"
#include "common/spinel_defs.h"
#include "common/spinel_buffer.h"
#include "app_wsbrd/rcp_api.h"
#include "rcp_fw_update.h"
#include "app_wsbrd/wsbr.h"

void rcp_send_fwupd_request(rcp_fwupd_attr_t* params)
{
    struct wsbr_ctxt *ctxt = &g_ctxt;
    struct iobuf_write buf = { };

    spinel_push_u8(&buf, rcp_get_spinel_hdr());
    spinel_push_uint(&buf, SPINEL_CMD_BOOTLOADER_UPDATE);
    spinel_push_u8(&buf, params->force_upt);
    spinel_push_u8(&buf, params->firmware_major_ver);
    spinel_push_u8(&buf, params->firmware_minor_ver);
    spinel_push_u8(&buf, params->firmware_patch_ver);
    spinel_push_u32(&buf, params->firmware_len);
    spinel_push_u16(&buf, params->block_size);
    spinel_push_u16(&buf, params->block_num);
    spinel_push_u32(&buf, params->firmware_crc32);
    rcp_tx(ctxt, &buf);
    iobuf_free(&buf);
}

void *rcp_firmware_update_thread(void *arg)
{
    int  ret;
    int  bin_size = 0;
    char filename[PATH_MAX];
    rcp_fwupd_attr_t params = {
        .force_upt = 1,
    };

    WARN("-----------------------------------------------");
    /* automatically detach current thread.
     * resouce will be released after thread end */
    ret = pthread_detach(pthread_self());
    if (ret) {
       BUG("pthread_detach error: %m");
       return NULL;
    }

    struct wsbr_ctxt *ctxt = (struct wsbr_ctxt *)arg;

    //WARN("-ctxt->filename = %s", ctxt->fw_upt_filename);
    snprintf(filename, sizeof(filename), "%s", ctxt->fw_upt_filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        WARN("fopen: %s: %m", ctxt->fw_upt_filename);
        pthread_exit(NULL);
    }

    fseek(fp, 0, SEEK_END);
    bin_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    /* search firmware version in the bin file */
    /* There is constant build info structure start with a string "Build_#*&^12", 
     * search for it to know the location of following build date/time(15+10 byte)
     * and RCP firmware version */
    int header_size = 8;
    uint8_t match_array[] = {0x42, 0x75, 0x69, 0x6C, 0x64, 0x5F, 0x23, 0x2A, 0x26, 0x5E, 0x31, 0x32, 0x00, 0x00, 0x00};
    uint8_t *fw_update_array = malloc(header_size + bin_size);
    uint8_t *rcp_bin_array = fw_update_array + header_size; //malloc(bin_size);
    ret = fread(rcp_bin_array, sizeof(uint8_t), bin_size, fp);
    ERROR_ON(ret != bin_size, "Failed to read bin file...");
    WARN("read bin file stream %d bytes", ret);

    uint32_t firmware_length = (bin_size<<8);
    uint32_t crc_init = 0x00000000;
    uint32_t firmware_crc32 = block_crc32(crc_init, rcp_bin_array, bin_size);
    WARN("--firmware_length = %d\t firmware_crc32 = 0x%08x", firmware_length>>8, firmware_crc32);
    fw_update_array[0] = 0;
    // bug here !!!!!!!!!!!!
    fw_update_array[1] = ((0+bin_size) & 0xff0000)>>16;
    fw_update_array[2] = ((0+bin_size) & 0x00ff00)>>8;
    fw_update_array[3] = ((0+bin_size) & 0x0000ff)>>0;
    fw_update_array[4] = (firmware_crc32 & 0xff000000)>>24;
    fw_update_array[5] = (firmware_crc32 & 0x00ff0000)>>16;
    fw_update_array[6] = (firmware_crc32 & 0x0000ff00)>>8;
    fw_update_array[7] = (firmware_crc32 & 0x000000ff)>>0;
    WARN("header[8]: %d\t 0x%02x%02x%02x%02x", fw_update_array[1]*256*256+fw_update_array[2]*256+fw_update_array[3],
                                    fw_update_array[4],fw_update_array[5],fw_update_array[6],fw_update_array[7]);

    uint8_t  match_count = 0;
    uint32_t match_location = 0;
    uint32_t search_num= bin_size+1-sizeof(match_array);
    /* search for the match location, which is a string tag, and after that
     * we can find the RCP firmware build version infomation */
    for (uint32_t idx=0; idx<search_num; idx++) {
        //WARN("rcp_bin_array[%d]= 0x%02x", idx, rcp_bin_array[idx]);
        uint16_t code_distance = 0;
        for (uint8_t iidx=0; iidx<sizeof(match_array); iidx++) {
            code_distance += abs((int16_t)rcp_bin_array[idx+iidx]-(int16_t)match_array[iidx]);
        }
        
        if (code_distance==0) {
            match_count++;
            match_location = idx;
            //WARN("match at loc : %d", idx);
        }
    }
    
    if (match_count == 0) {
        WARN("No firmware build version segment found in bin file...");
        WARN("Exit the firmware update thread...");
        free(fw_update_array);
        fclose(fp);
        pthread_exit(NULL);
    } else if (match_count == 1) {
        /* from the match location: 15byte tag, 15byte build date, 10byte build time, then the version infomation */
        params.firmware_major_ver = rcp_bin_array[match_location+15+15+10];
        params.firmware_minor_ver = rcp_bin_array[match_location+15+15+10+1];
        params.firmware_patch_ver = rcp_bin_array[match_location+15+15+10+2];
        params.firmware_len       = bin_size;
        params.block_size         = 1024;
        uint16_t temp_blocks      = (header_size + bin_size)/params.block_size;
        params.block_num          = ((header_size + bin_size)%params.block_size==0)? temp_blocks : temp_blocks+1;
        params.firmware_crc32     = firmware_crc32;
        WARN("firmware_version is: %d.%d.%d", params.firmware_major_ver, params.firmware_minor_ver, 
                                    params.firmware_patch_ver);
        WARN("Firmware image will be send to RCP in %d blocks(%d-byte each)", params.block_num, params.block_size);
    } else {
        WARN("Found multiple(%d) build info tag, this invalid case...", match_count);
    }
    fclose(fp);
    WARN("-successfully opened bin file, its size = %d", bin_size);




    

    rcp_send_fwupd_request(&params);
    ret = sem_wait(&ctxt->os_ctxt->fwupd_reply_semid);
    FATAL_ON(ret < 0, 2, "wait semaphore: %m");
    if (ctxt->os_ctxt->fwupd_reply_cmd == FWUPD_ACCEPT) {
        WARN("-receive RCP update ACCEPT, start the process...");
    } else if (ctxt->os_ctxt->fwupd_reply_cmd == FWUPD_REFUSE) {
        WARN("-RCP refused firmware update, give up this time");
        free(fw_update_array);
        pthread_exit(NULL);
    } else {
        ERROR("-RCP replied unexpected reponse, give up firmware update");
        free(fw_update_array);
        pthread_exit(NULL);
    }

    WARN("-Wait a moment.....");
    usleep(5000);

    uint16_t retry_cnt = 0;
    uint16_t block_idx = 0;
    while (block_idx<params.block_num) {
        //WARN("enter loop....");
        uint8_t *pblock = &fw_update_array[block_idx*params.block_size];
        uint16_t block_size = params.block_size;
        /* for the last block, block size equals to (header_size + bin_size)%params.block_size */ 
        if (block_idx==params.block_num-1) {
            if ((header_size + bin_size)%params.block_size!=0) 
                block_size = (header_size + bin_size)%params.block_size;
        }
        WARN("-Send blcok[%d] - block size : %d", block_idx, block_size);
        uint16_t crc = crc16(pblock, block_size);
        rcp_firmware_send_block(block_idx, block_size, pblock, crc);

        /* wait for semaphore released when receive ACK form RCP */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        ret = sem_timedwait(&ctxt->os_ctxt->fwupd_reply_semid, &ts);
        //ret = sem_wait(&ctxt->os_ctxt->fwupd_reply_semid);
        if (ret == 0) {
            if (ctxt->os_ctxt->fwupd_reply_cmd == RECV_BAD_CRC) {
                WARN("--receive RCP block RECV_BAD_CRC...");
                continue;
            }
            if (ctxt->os_ctxt->fwupd_reply_cmd == FWUPD_REFUSE) {
                WARN("--receive RCP block FWUPD_REFUSE...");
                break;
            }
            if (ctxt->os_ctxt->fwupd_reply_cmd == RECV_OK) {
                WARN("--receive RCP block RECV_OK...");
                if (block_idx == params.block_num-1) {
                    WARN("--All bin file blocks are send to RCP with ACK...");
                    /* wait for maximum 2 second for RCP confirm firmware update complete */
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += 2;
                    ret = sem_timedwait(&ctxt->os_ctxt->fwupd_reply_semid, &ts);
                    if (ret == 0) {
                        if (ctxt->os_ctxt->fwupd_reply_cmd == IMAGE_VERIFY_OK) {
                            WARN("Received RCP confirmation, firmware update complete");
                        } else {
                            WARN("Received RCP confirmation, IMAGE not VERIFied");
                        }
                    } else if ( ret == -1) {
                        WARN("Can not get RCP confirmation, but all blocks send OK");
                    } else {
                        WARN("unknown error ");
                    }
                }
            }
            FATAL_ON(ret < 0, 2, "wait semaphore: %m");
            block_idx++;
        } else if (ret == -1 && errno == ETIMEDOUT) {
            WARN("wait for RCP response timeout, re-transmitt this block...");
            if ((retry_cnt++)>100) {
                WARN("retry too many times, terminate RCP firmware update process...");
                /* send a all zero block, telling RCP to terminate firmware update */
                rcp_firmware_send_block(0, 0, pblock, 0);
                free(fw_update_array);
                pthread_exit(NULL);
            }
            continue;
        } else {
            WARN("unknown error ");
        }
    }

    /* end up this thread, release all resouce */
    free(fw_update_array);
    pthread_exit(NULL);
}


/***************************************************************************************/
// below code should be moved to ota specific file after debug
/***************************************************************************************/
static uint16_t cal_checksum(struct block_header* ptr)
{
    uint32_t sum = 0;
    uint16_t carry_val = 0;
    
    uint16_t image_crc[2] = {0};
    
    image_crc[0] = (ptr->image_crc >> 16);
    image_crc[1] = ptr->image_crc & 0x0000ffff;
    
    //sum = ptr->control_cmd + ptr->ver_num + ptr->pkt_crc + ptr->image_crc + ptr->block_seq_num + ptr->block_total_num + ptr->pkt_len;
    sum = ptr->control_cmd + ptr->ver_num + ptr->pkt_crc + image_crc[0] + image_crc[1] + ptr->block_seq_num + ptr->block_total_num + ptr->pkt_len;
    
    carry_val = sum >> 16;
    
    while(carry_val > 0)
    {
        sum &= 0x0000ffff;
        sum += carry_val;
        carry_val = sum >> 16;
    }
    
    return (uint16_t)~sum;
}

void recv_ota_msg(node_ota_attr_t *node_ota_attr)
{
    INFO("recv udp socket msg.\n");
    
    memset(node_ota_attr->tmp_buffer, 0x00, sizeof(node_ota_attr->tmp_buffer));
    /* get received content and source address, thread will blocked here untill received from socket */
    socklen_t src_addr_len = sizeof(struct sockaddr_in6);
    int len = recvfrom(node_ota_attr->ota_sid, (void *)node_ota_attr->tmp_buffer, sizeof(node_ota_attr->tmp_buffer), 
                        0, (struct sockaddr *) &node_ota_attr->ota_src_addr, &src_addr_len);
    
    if (len > 0){
        INFO("socket received len is %d.\n",len);

        /* clear fwupgrade header, then copy received header */
        memset((char *)&node_ota_attr->recv_fwupgrade_hdr, 0x00, node_ota_attr->block_hdr_size);
        memcpy((char *)&node_ota_attr->recv_fwupgrade_hdr, node_ota_attr->tmp_buffer, node_ota_attr->block_hdr_size);
        
        /* move forward only if we got correct checksum calculation */
        if(node_ota_attr->recv_fwupgrade_hdr.checksum == cal_checksum(&node_ota_attr->recv_fwupgrade_hdr)){
            INFO("hdr checksum is ok.");
            
            /* switch according to received control command */
            switch(node_ota_attr->recv_fwupgrade_hdr.control_cmd){
                case GET_BLOCK:{
                    uint16_t block_size = 0x0000;
                    
                    /* compare received ver_num and sequence number, version should match 
                     * and seq_num should be less than total_block_num */
                    if((node_ota_attr->recv_fwupgrade_hdr.ver_num != node_ota_attr->upgrading_ver_num) && 
                        (node_ota_attr->recv_fwupgrade_hdr.block_seq_num >= node_ota_attr->upgrading_total_block_num)){
                        ERROR("ver num not match or sequence number exceed.");
                        break;
                    }
                    
                    if(node_ota_attr->recv_fwupgrade_hdr.block_seq_num == (node_ota_attr->upgrading_total_block_num-1)){
                        block_size = node_ota_attr->upgrading_last_block_size;
                    }else{
                        block_size = node_ota_attr->upgrading_block_size;
                    }
    
                    uint32_t read_addr = (node_ota_attr->recv_fwupgrade_hdr.block_seq_num*node_ota_attr->upgrading_block_size);
                    
                    memset(node_ota_attr->ota_buffer, 0x00, sizeof(node_ota_attr->ota_buffer));
                    memcpy(&node_ota_attr->ota_buffer[node_ota_attr->block_hdr_size], &node_ota_attr->ota_upgrade_array[read_addr], block_size);
                    
                    /* clear and copy received header */
                    memset((char *)&node_ota_attr->tran_fwupgrade_hdr, 0x00, node_ota_attr->block_hdr_size);
                    node_ota_attr->tran_fwupgrade_hdr.control_cmd   = PUT_BLOCK;
                    node_ota_attr->tran_fwupgrade_hdr.block_seq_num = node_ota_attr->recv_fwupgrade_hdr.block_seq_num;
                    node_ota_attr->tran_fwupgrade_hdr.ver_num = node_ota_attr->upgrading_ver_num;
                    node_ota_attr->tran_fwupgrade_hdr.pkt_len = block_size + node_ota_attr->block_hdr_size;
                    node_ota_attr->tran_fwupgrade_hdr.pkt_crc = crc16(&node_ota_attr->ota_buffer[node_ota_attr->block_hdr_size], block_size);
                    node_ota_attr->tran_fwupgrade_hdr.checksum = cal_checksum(&node_ota_attr->tran_fwupgrade_hdr);
                                    
                    
                    memcpy(&node_ota_attr->ota_buffer[0], (char *)&node_ota_attr->tran_fwupgrade_hdr, node_ota_attr->block_hdr_size);
                    
                    /* send back block to source address */
                    sendto(node_ota_attr->ota_sid, &node_ota_attr->ota_buffer[0], (node_ota_attr->block_hdr_size+block_size), 0, 
                                    (struct sockaddr *) &node_ota_attr->ota_src_addr, sizeof(struct sockaddr_in6));                                
                    break;
                }
                
                default:
                    break;
            }
        }else{
            ERROR("pkt checksum error !!!");
        }
    }

}

static int init_ota_socket(struct wsbr_ctxt *ctxt, node_ota_attr_t *node_ota_attr)
{
    int  ret;

    node_ota_attr->ota_port_num = 0x1234;
    node_ota_attr->ota_multicast_hops = 20;
    node_ota_attr->recv_image_flg = true;
    node_ota_attr->ota_dst_addr.sin6_family = AF_INET6;
    node_ota_attr->ota_dst_addr.sin6_port = htons(node_ota_attr->ota_port_num);
    memcpy((void*)&node_ota_attr->ota_dst_addr.sin6_addr, ctxt->node_ota_address, 16);

    node_ota_attr->block_seq = 0;
    node_ota_attr->image_size = 0;
    node_ota_attr->upgrading_flg = 0;
    node_ota_attr->upgrading_ver_num = 0x0616;
    node_ota_attr->upgrading_image_crc = 0;
    node_ota_attr->upgrading_total_block_num = 0;
    node_ota_attr->upgrading_block_size = 0x400;
    node_ota_attr->upgrading_total_byte = 0;
    node_ota_attr->upgrading_last_block_size = 0;
    node_ota_attr->upgrading_total_pkt_size = 0;
    node_ota_attr->block_hdr_size = 0;

    node_ota_attr->ota_sid = socket(node_ota_attr->ota_dst_addr.sin6_family, SOCK_DGRAM, IPPROTO_UDP);
    ERROR_ON(node_ota_attr->ota_sid < 0, "%s: socket: %m", __func__);
    ret = bind(node_ota_attr->ota_sid, (struct sockaddr *) &node_ota_attr->ota_dst_addr, sizeof(struct sockaddr_in6));
    ERROR_ON(ret < 0, "%s: bind: %m", __func__);

    ret = setsockopt(node_ota_attr->ota_sid, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *)&node_ota_attr->ota_multicast_hops, sizeof(node_ota_attr->ota_multicast_hops));
    WARN_ON(ret < 0, "ipv6 multicast hops \"%s\": %m", __func__);
    
    return ret;
}

void fragment_info_init(uint32_t image_size, uint16_t block_unit, node_ota_attr_t *node_ota_attr)
{
    uint16_t remainder = image_size % block_unit;
    uint16_t quotient  = image_size / block_unit;
    
    if (remainder == 0) {
        node_ota_attr->upgrading_total_block_num = quotient;
        node_ota_attr->upgrading_last_block_size = block_unit;
    } else {
        node_ota_attr->upgrading_total_block_num = quotient + 1;
        node_ota_attr->upgrading_last_block_size = remainder;
    }
}

uint8_t *read_ota_file(struct wsbr_ctxt *ctxt, node_ota_attr_t *node_ota_attr)
{
    int ret;
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s", ctxt->node_ota_filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    node_ota_attr->image_size = ftell(fp);
    uint8_t *ota_upgrade_array = malloc(node_ota_attr->image_size);
    ERROR_ON(ota_upgrade_array == NULL, "Failed to malloc...");
    ret = fread(ota_upgrade_array, sizeof(uint8_t), node_ota_attr->image_size, fp);
    ERROR_ON(ret != node_ota_attr->image_size, "Failed to read bin file...");
    WARN("read bin file stream %d bytes", ret);

    uint32_t image_crc = 0x00000000;
    image_crc|= ota_upgrade_array[4];
    image_crc<<=8; image_crc|= ota_upgrade_array[5];
    image_crc<<=8; image_crc|= ota_upgrade_array[6];
    image_crc<<=8; image_crc|= ota_upgrade_array[7];
    node_ota_attr->upgrading_image_crc = image_crc;

    return ota_upgrade_array;
}

void send_ota_data(struct wsbr_ctxt *ctxt, node_ota_attr_t *node_ota_attr, uint16_t block_seq, uint16_t len)
{
    struct block_header* hdr = &node_ota_attr->tran_fwupgrade_hdr;
    
    if(block_seq == 0){
        hdr->control_cmd = OTA_START;
    }else if(block_seq == (node_ota_attr->upgrading_total_block_num-1)){
        hdr->control_cmd = OTA_END;
    }else{
        hdr->control_cmd = OTA_INTERNAL;
    }
    
    hdr->block_seq_num = block_seq;
    hdr->pkt_len       = len + sizeof(struct block_header);
    hdr->checksum      = cal_checksum(hdr);
    
    memcpy(node_ota_attr->ota_buffer, (char*)hdr, sizeof(struct block_header));
    //memcpy((void*)&node_ota_attr->ota_dst_addr.sin6_addr, ctxt->node_ota_address, 16);

    sendto(node_ota_attr->ota_sid, node_ota_attr->ota_buffer, (len+sizeof(struct block_header)), 0, 
                    (struct sockaddr *) &node_ota_attr->ota_dst_addr, sizeof(struct sockaddr_in6));                                
}

void init_fwupgrade_hdr(node_ota_attr_t *node_ota_attr)
{
    memset((char*)&node_ota_attr->tran_fwupgrade_hdr, 0x00, sizeof(struct block_header));
    node_ota_attr->tran_fwupgrade_hdr.ver_num         = node_ota_attr->upgrading_ver_num;
    node_ota_attr->tran_fwupgrade_hdr.image_crc       = node_ota_attr->upgrading_image_crc;
    node_ota_attr->tran_fwupgrade_hdr.block_total_num = node_ota_attr->upgrading_total_block_num;
}

void ota_upgrade_start(struct wsbr_ctxt *ctxt, node_ota_attr_t *node_ota_attr)
{
    node_ota_attr->block_hdr_size = sizeof(struct block_header);

    // image size got when read ota file 
    //node_ota_attr->image_size = get_firmware_length();

    // 从外部flash获取固件版本号
//    upgrading_ver_num = get_firmware_version();
//    ota_info.update_soft_vevsion[0] = upgrading_ver_num >> 8;
//    ota_info.update_soft_vevsion[1] = upgrading_ver_num & 0x00ff;

    fragment_info_init(node_ota_attr->image_size, node_ota_attr->upgrading_block_size, node_ota_attr);  // 因为有8个字节的头,所以整个image字节数要+8

    // image CRC32 got when read ota file

    init_fwupgrade_hdr(node_ota_attr);

    for(node_ota_attr->block_seq = 0; node_ota_attr->block_seq < node_ota_attr->upgrading_total_block_num; node_ota_attr->block_seq++)
    {
        uint32_t read_addr = node_ota_attr->block_seq*node_ota_attr->upgrading_block_size;
        memset(node_ota_attr->ota_buffer, 0x00, sizeof(node_ota_attr->ota_buffer));

        if(node_ota_attr->block_seq == (node_ota_attr->upgrading_total_block_num - 1)){
            memcpy(&node_ota_attr->ota_buffer[node_ota_attr->block_hdr_size], 
                   &node_ota_attr->ota_upgrade_array[read_addr], node_ota_attr->upgrading_last_block_size);
            send_ota_data(ctxt, node_ota_attr, node_ota_attr->block_seq, node_ota_attr->upgrading_last_block_size);
            /* wait for ota response from node */
            recv_ota_msg(node_ota_attr);
        }
        else{
            memcpy(&node_ota_attr->ota_buffer[node_ota_attr->block_hdr_size], 
               &node_ota_attr->ota_upgrade_array[read_addr], node_ota_attr->upgrading_block_size);
            send_ota_data(ctxt, node_ota_attr, node_ota_attr->block_seq, node_ota_attr->upgrading_block_size);
            /* wait for ota response from node */
            recv_ota_msg(node_ota_attr);
        }
    }
}

void *node_firmware_ota_thread(void *arg)
{
    int  ret;

    WARN("-----------------------------------------------");
    /* automatically detach current thread.
     * resouce will be released after thread end */
    ret = pthread_detach(pthread_self());
    if (ret) {
       BUG("pthread_detach error: %m");
       return NULL;
    }

    struct wsbr_ctxt *ctxt = (struct wsbr_ctxt *)arg;
    node_ota_attr_t *node_ota_attr = malloc(sizeof(node_ota_attr_t));
    memset((void*)node_ota_attr, 0, sizeof(node_ota_attr_t));


    init_ota_socket(ctxt, node_ota_attr);
    uint8_t *ota_upgrade_array = read_ota_file(ctxt, node_ota_attr);
    node_ota_attr->ota_upgrade_array = ota_upgrade_array;
    




    free(ota_upgrade_array);
    free(node_ota_attr);
    pthread_exit(NULL);
}
