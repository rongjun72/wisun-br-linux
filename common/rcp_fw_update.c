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
#include <time.h>
#include <limits.h>
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

void *rcp_firmware_update(void *arg)
{
    int ret;
    int size = 0;
    char filename[PATH_MAX];

    struct wsbr_ctxt *ctxt = (struct wsbr_ctxt *)arg;

    WARN("&ctxt->filename = %s", ctxt->fw_upt_filename);
    snprintf(filename, sizeof(filename), "%s", ctxt->fw_upt_filename);
    FILE *fp = fopen(filename, "r");
    FATAL_ON(!fp, 2, "fopen: %m");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fclose(fp);
        WARN("successfully opened bin file, its size = %d", size);
    }

    rcp_firmware_update_start();
    ret = sem_wait(&ctxt->os_ctxt->fwupd_reply_semid);
    FATAL_ON(ret < 0, 2, "poll: %m");
    if (ctxt->os_ctxt->fwupd_reply_cmd != START_ACK)
        return ((void *)0);
    WARN("receive RCP update start ACK...");

    rcp_firmware_send_block();
    //sleep(1);

    ret = sem_wait(&ctxt->os_ctxt->fwupd_reply_semid);
    FATAL_ON(ret < 0, 2, "poll: %m");
    WARN("------fwupd_reply semaphore released");
    WARN("------reply_cmd = %d\t reply_param = %d" ,ctxt->os_ctxt->fwupd_reply_cmd, ctxt->os_ctxt->fwupd_reply_param);

    return((void *)0);
}