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