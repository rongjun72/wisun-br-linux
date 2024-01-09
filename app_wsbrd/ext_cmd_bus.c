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
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <systemd/sd-bus.h>
#include "app_wsbrd/tun.h"
#include "common/named_values.h"
#include "common/utils.h"
#include "common/log.h"
#include "stack/ws_bbr_api.h"
#include "common/log_legacy.h"
#include "common/rcp_fw_update.h"
#include "common/iobuf.h"
#include "common/spinel_defs.h"
#include "common/spinel_buffer.h"

#include "stack/source/6lowpan/ws/ws_common.h"
#include "stack/source/6lowpan/ws/ws_pae_controller.h"
#include "stack/source/6lowpan/ws/ws_pae_key_storage.h"
#include "stack/source/6lowpan/ws/ws_pae_lib.h"
#include "stack/source/6lowpan/ws/ws_pae_auth.h"
#include "stack/source/6lowpan/ws/ws_cfg_settings.h"
#include "stack/source/6lowpan/ws/ws_bootstrap.h"
#include "stack/source/6lowpan/ws/ws_llc.h"
#include "stack/source/nwk_interface/protocol.h"
#include "stack/source/security/protocols/sec_prot_keys.h"
#include "stack/source/common_protocols/icmpv6.h"
#include "stack/stack/ws_management_api.h"

#include "commandline_values.h"
#include "wsbr.h"
#include "tun.h"

#include "ext_cmd_bus.h" 

#define TRACE_GROUP "extc"

void ext_cmd_tx(struct wsbr_ctxt *ctxt, struct iobuf_write *buf)
{
    spinel_trace_tx(buf);
    ctxt->extcmd.device_tx(ctxt->ext_cmd_ctxt, buf->data, buf->len);
}

static void rcp_rx_no_op(struct wsbr_ctxt *ctxt, uint32_t prop, struct iobuf_read *buf)
{
}

// Some debug tools (fuzzers) may deflect this struct. So keep it public.
struct ext_rx_cmds ext_cmds[] = {
    { SPINEL_CMD_NOOP,             (uint32_t)-1,                         rcp_rx_no_op },
    { SPINEL_CMD_PROP_IS,          SPINEL_PROP_EXTCMD_NOP,               rcp_rx_no_op },
    { (uint32_t)-1,                (uint32_t)-1,                         NULL },
};

void ext_cmd_rx(struct wsbr_ctxt *ctxt)
{
    static uint8_t rx_buf[4096];
    struct iobuf_read buf = {
        .data = rx_buf,
    };
    uint32_t cmd, prop;
    int i;

    buf.data_size = ctxt->extcmd.device_rx(ctxt->ext_cmd_ctxt, rx_buf, sizeof(rx_buf));
    if (!buf.data_size)
        return;
    spinel_trace_rx(&buf);
    spinel_pop_u8(&buf); /* packet header */
    cmd = spinel_pop_uint(&buf);
    if (cmd != SPINEL_CMD_PROP_IS) {
        prop = (uint32_t)-1;
    } else {
        prop = spinel_pop_uint(&buf);
    }
    for (i = 0; ext_cmds[i].cmd != (uint32_t)-1; i++)
        if (ext_cmds[i].cmd == cmd && ext_cmds[i].prop == prop)
            return ext_cmds[i].fn(ctxt, prop, &buf);
    ERROR("%s: command %04x/%04x not implemented", __func__, cmd, prop);
}
