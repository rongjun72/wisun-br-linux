/*
 * Copyright (c) 2022 Silicon Laboratories Inc. (www.silabs.com)
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
#include <unistd.h>

#include "app_wsbrd/wsbr.h"
#include "app_wsbrd/wsbr_mac.h"
#include "common/bus_uart.h"
#include "common/crc.h"
#include "common/log.h"
#include "common/spinel_buffer.h"
#include "common/spinel_defs.h"
#include "wsbrd_fuzz.h"
#include "interfaces.h"
#include "capture.h"

void fuzz_capture(struct fuzz_ctxt *ctxt, const void *data, size_t size)
{
    int ret;
    int fd;

    if (ctxt->capture_init_enabled && !(g_ctxt.rcp_init_state & RCP_INIT_DONE))
        fd = ctxt->capture_init_fd;
    else
        fd = ctxt->capture_fd;
    ret = write(fd, data, size);
    FATAL_ON(ret < 0, 2, "write: %m");
    FATAL_ON(ret < size, 2, "write: Short write");
}

static void fuzz_capture_spinel(struct fuzz_ctxt *ctxt, struct spinel_buffer *buf)
{
    uint16_t crc = crc16(buf->frame, buf->cnt);
    uint8_t *frame = malloc(buf->cnt * 2 + 3);
    size_t frame_len;

    frame_len = uart_encode_hdlc(frame, buf->frame, buf->cnt, crc);
    fuzz_capture(ctxt, frame, frame_len);
    free(frame);
}

void fuzz_capture_timers(struct fuzz_ctxt *ctxt)
{
    struct spinel_buffer *buf;

    if (!ctxt->timer_counter)
        return;

    buf = ALLOC_STACK_SPINEL_BUF(9);
    spinel_push_u8(buf, wsbr_get_spinel_hdr(&g_ctxt));
    spinel_push_uint(buf, SPINEL_CMD_REPLAY_TIMERS);
    spinel_push_u16(buf, ctxt->timer_counter);
    fuzz_capture_spinel(ctxt, buf);
    ctxt->timer_counter = 0;
}

void fuzz_capture_interface(struct fuzz_ctxt *ctxt, uint8_t interface,
                            const uint8_t src_addr[16], uint16_t src_port,
                            const void *data, size_t size)
{
    struct spinel_buffer *buf;

    buf = ALLOC_STACK_SPINEL_BUF(1 + 3 + 1 + 16 + 2 + 2 + size);
    spinel_push_u8(buf, wsbr_get_spinel_hdr(&g_ctxt));
    spinel_push_uint(buf, SPINEL_CMD_REPLAY_INTERFACE);
    spinel_push_u8(buf, interface);
    spinel_push_fixed_u8_array(buf, src_addr, 16);
    spinel_push_u16(buf, src_port);
    spinel_push_data(buf, data, size);
    fuzz_capture_spinel(ctxt, buf);
}
