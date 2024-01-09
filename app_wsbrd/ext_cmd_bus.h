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
#ifndef EXT_CMD_BUS_H
#define EXT_CMD_BUS_H
#include <stdint.h>
#include <sys/uio.h>

struct extcmd {
    int  (*device_tx)(struct os_ctxt *ctxt, const void *buf, unsigned int len);
    int  (*device_rx)(struct os_ctxt *ctxt, void *buf, unsigned int len);
    void (*on_crc_error)(struct os_ctxt *ctxt, uint16_t crc, uint32_t frame_len, uint8_t header, uint8_t irq_err_counter);

};

// Only used by the fuzzer
struct ext_rx_cmds {
    uint32_t cmd;
    uint32_t prop;
    void (*fn)(struct wsbr_ctxt *ctxt, uint32_t prop, struct iobuf_read *buf);
};

void ext_cmd_tx(struct wsbr_ctxt *ctxt, struct iobuf_write *buf);
void ext_cmd_rx(struct wsbr_ctxt *ctxt);


#endif //EXT_CMD_BUS_H
