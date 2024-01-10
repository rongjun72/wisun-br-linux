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

static uint8_t ext_get_spinel_hdr()
{
    struct wsbr_ctxt *ctxt = &g_ctxt;
    uint8_t hdr = FIELD_PREP(0xC0, 0x2) | FIELD_PREP(0x30, ctxt->spinel_iid);

    ctxt->spinel_tid = (ctxt->spinel_tid + 1) % 0x10;
    if (!ctxt->spinel_tid)
        ctxt->spinel_tid = 1;
    hdr |= FIELD_PREP(0x0F, ctxt->spinel_tid);
    return hdr;
}

////static void spinel_push_hdr_set_prop(struct iobuf_write *buf, unsigned int prop)
////{
////    spinel_push_u8(buf, ext_get_spinel_hdr());
////    spinel_push_uint(buf, SPINEL_CMD_PROP_SET);
////    spinel_push_uint(buf, prop);
////}
////
////static void spinel_push_hdr_get_prop(struct iobuf_write *buf, unsigned int prop)
////{
////    spinel_push_u8(buf, ext_get_spinel_hdr());
////    spinel_push_uint(buf, SPINEL_CMD_PROP_GET);
////    spinel_push_uint(buf, prop);
////}

static void spinel_push_hdr_is_prop(struct iobuf_write *buf, unsigned int prop)
{
    spinel_push_u8(buf, ext_get_spinel_hdr());
    spinel_push_uint(buf, SPINEL_CMD_PROP_IS);
    spinel_push_uint(buf, prop);
}

void ext_cmd_tx(struct wsbr_ctxt *ctxt, struct iobuf_write *buf)
{
    spinel_trace_tx(buf);
    ctxt->extcmd.device_tx(ctxt->ext_cmd_ctxt, buf->data, buf->len);
}

static void ext_rx_no_op(struct wsbr_ctxt *ctxt, uint32_t prop, struct iobuf_read *buf)
{
}

/* 
 * response to external get_wisun_status command:
 *  network_name,   char*33
 *  fan_version,    uint
 *  domain,         uint
 *  mode,           uint
 *  class,          uint
 *  panid:          uint
 *  gtk[0]:         u8*16
 *  gak[0]:         u8*16
 *  ...
 *  gtk[3]:         u8*16
 *  gak[3]:         u8*16
 */
static void exp_get_wisun_status(struct wsbr_ctxt *ctxt, uint32_t prop, struct iobuf_read *buf)
{
    int interface_id = ctxt->rcp_if_id;
    struct iobuf_write tx_buf = { };
    uint8_t gak[16];
    struct net_if *interface_ptr = protocol_stack_interface_info_get_by_id(interface_id);
    sec_prot_gtk_keys_t *gtks = ws_pae_controller_get_transient_keys(interface_id, false);

    WARN("-------send wisun status data through spinel");
    spinel_push_hdr_is_prop(&tx_buf, SPINEL_PROP_EXT_WisunStatus);
    spinel_push_str( &tx_buf, ctxt->config.ws_name);
    spinel_push_uint(&tx_buf, ctxt->config.ws_fan_version);
    spinel_push_uint(&tx_buf, ctxt->config.ws_domain);
    spinel_push_uint(&tx_buf, ctxt->config.ws_mode);
    spinel_push_uint(&tx_buf, ctxt->config.ws_class);
    spinel_push_uint(&tx_buf, ctxt->config.ws_pan_id);
    spinel_push_uint(&tx_buf, ctxt->config.ws_size);
    for (int i = 0; i < GTK_NUM; i++) {
        spinel_push_fixed_u8_array(&tx_buf, gtks->gtk[i].key, ARRAY_SIZE(gtks->gtk[i].key));
        ws_pae_controller_gak_from_gtk(gak, gtks->gtk[i].key, interface_ptr->ws_info.cfg->gen.network_name);
        spinel_push_fixed_u8_array(&tx_buf, gak, ARRAY_SIZE(gak));
    }

    ext_cmd_tx(ctxt, &tx_buf);
    iobuf_free(&tx_buf);

}

static void exp_set_wisun_network_name(struct wsbr_ctxt *ctxt, uint32_t prop, struct iobuf_read *buf)
{
    int interface_id = ctxt->rcp_if_id;
    char *nwkname = strdup(spinel_pop_str(buf));
    ws_management_network_name_set(interface_id, nwkname);
}

// Some debug tools (fuzzers) may deflect this struct. So keep it public.
struct ext_rx_cmds ext_cmds[] = {
    { SPINEL_CMD_NOOP,             (uint32_t)-1,                         ext_rx_no_op },
    { SPINEL_CMD_PROP_GET,         SPINEL_PROP_EXT_WisunStatus,          exp_get_wisun_status },
    { SPINEL_CMD_PROP_SET,         SPINEL_PROP_EXT_WisunNetworkName,     exp_set_wisun_network_name },
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
    WARN("---------------------EXTERNAL COMMAND UART enter...");

    buf.data_size = ctxt->extcmd.device_rx(ctxt->ext_cmd_ctxt, rx_buf, sizeof(rx_buf));
    if (!buf.data_size)
        return;
    spinel_trace_rx(&buf);
    spinel_pop_u8(&buf); /* packet header */
    cmd = spinel_pop_uint(&buf);
    WARN("--------cmd : 0x%04x", cmd);
    //if (cmd != SPINEL_CMD_PROP_IS) {
    //    prop = (uint32_t)-1;
    //} else {
        prop = spinel_pop_uint(&buf);
    //}
    WARN("--------prop : 0x%04x", prop);
    for (i = 0; ext_cmds[i].cmd != (uint32_t)-1; i++)
        if (ext_cmds[i].cmd == cmd && ext_cmds[i].prop == prop)
            return ext_cmds[i].fn(ctxt, prop, &buf);
    ERROR("%s: command %04x/%04x not implemented", __func__, cmd, prop);
}
