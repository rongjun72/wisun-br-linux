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

struct neighbor_info {
    int rssi;
    int rsl;
    int rsl_adv;
};

/* spinel push a fixed 20 length char string */
void spinel_push_string(struct iobuf_write *buf, const char *val)
{
    char attr_tag[20] = {0};
    memcpy(attr_tag, val, 20);
    spinel_push_str(buf, attr_tag);
}

static void ext_message_append_node(
    struct iobuf_write *buf,
    const uint8_t self[8],
    const uint8_t parent[8],
    const uint8_t ipv6[3][16],
    bool is_br,
    supp_entry_t *supp,
    struct neighbor_info *neighbor)
{
    
    spinel_push_string(buf, "node eui64");
    spinel_push_fixed_u8_array(buf, self, 8);
    
    if (is_br) {
        spinel_push_string(buf, "isBorderRouter");
    } else if (supp) {
        spinel_push_string(buf, "isAuthenticated");
        if (ws_common_is_valid_nr(supp->sec_keys.node_role)) {
            spinel_push_string(buf, "nodeRole");
            spinel_push_u8(buf, supp->sec_keys.node_role);
        }
    }
    if (parent) {
        spinel_push_string(buf, "parent eui64");
        spinel_push_fixed_u8_array(buf, parent, 8);
    }
    if (neighbor) {
        spinel_push_string(buf, "neighbor rssi");
        spinel_push_uint(buf, neighbor->rssi);
        
        if (neighbor->rsl != INT_MIN) {
            spinel_push_string(buf, "neighbor rsl");
            spinel_push_uint(buf, neighbor->rsl);
        }
        if (neighbor->rsl_adv != INT_MIN) {
            spinel_push_string(buf, "neighbor rsl_adv");
            spinel_push_uint(buf, neighbor->rsl_adv);
        }
    }
    for (uint8_t index = 0; memcmp(*ipv6, ADDR_UNSPECIFIED, 16); ipv6++, index++) {
        if (index ==0)
            spinel_push_string(buf, "LinkLocal ipv6");
        else
            spinel_push_string(buf, "global ipv6");

        spinel_push_fixed_u8_array(buf, (uint8_t *)ipv6, 16);
    }
}

static uint8_t *dhcp_eui64_to_ipv6(struct wsbr_ctxt *ctxt, const uint8_t eui64[8])
{
    for (int i = 0; i < ctxt->dhcp_leases_len; i++)
        if (!memcmp(eui64, ctxt->dhcp_leases[i].eui64, 8))
            return ctxt->dhcp_leases[i].ipv6;
    return NULL;
}

static uint8_t *dhcp_ipv6_to_eui64(struct wsbr_ctxt *ctxt, const uint8_t ipv6[16])
{
    for (int i = 0; i < ctxt->dhcp_leases_len; i++)
        if (!memcmp(ipv6, ctxt->dhcp_leases[i].ipv6, 16))
            return ctxt->dhcp_leases[i].eui64;
    return NULL;
}

static bool dbus_get_neighbor_info(struct wsbr_ctxt *ctxt, struct neighbor_info *info, const uint8_t eui64[8])
{
    struct net_if *net_if = protocol_stack_interface_info_get_by_id(ctxt->rcp_if_id);
    ws_neighbor_class_entry_t *neighbor_ws = NULL;
    ws_neighbor_temp_class_t *neighbor_ws_tmp;
    llc_neighbour_req_t neighbor_llc;

    neighbor_ws_tmp = ws_llc_get_eapol_temp_entry(net_if, eui64);
    if (!neighbor_ws_tmp)
        neighbor_ws_tmp = ws_llc_get_multicast_temp_entry(net_if, eui64);
    if (neighbor_ws_tmp) {
        neighbor_ws = &neighbor_ws_tmp->neigh_info_list;
        neighbor_ws->rssi = neighbor_ws_tmp->signal_dbm;
    }
    if (!neighbor_ws) {
        if (ws_bootstrap_neighbor_get(net_if, eui64, &neighbor_llc))
            neighbor_ws = neighbor_llc.ws_neighbor;
        else
            return false;
    }
    info->rssi = neighbor_ws->rssi;
    info->rsl = neighbor_ws->rsl_in == RSL_UNITITIALIZED
              ? INT_MIN
              : -174 + ws_neighbor_class_rsl_in_get(neighbor_ws);
    info->rsl_adv = neighbor_ws->rsl_in == RSL_UNITITIALIZED
                  ? INT_MIN
                  : -174 + ws_neighbor_class_rsl_out_get(neighbor_ws);
    return true;
}

static void exp_get_wisun_nodes(struct wsbr_ctxt *ctxt, uint32_t prop, struct iobuf_read *buf)
{
    struct neighbor_info *neighbor_info_ptr;
    struct neighbor_info neighbor_info;
    uint8_t node_ipv6[3][16] = { 0 };
    bbr_route_info_t table[4096];
    uint8_t *parent, *ucast_addr;
    int len_pae, len_rpl, ret, j;
    uint8_t eui64_pae[4096][8];
    bbr_information_t br_info;
    supp_entry_t *supp;
    uint8_t ipv6[16];
    struct iobuf_write tx_buf = { };

    ret = ws_bbr_info_get(ctxt->rcp_if_id, &br_info);
    BUG_ON(ret < 0, "%d: %s", prop, strerror(-ret));
    len_pae = ws_pae_auth_supp_list(ctxt->rcp_if_id, eui64_pae, sizeof(eui64_pae));
    len_rpl = ws_bbr_routing_table_get(ctxt->rcp_if_id, table, ARRAY_SIZE(table));
    BUG_ON(len_rpl < 0, "%d: %s", prop, strerror(-len_rpl));

    tun_addr_get_link_local(ctxt->config.tun_dev, node_ipv6[0]);
    tun_addr_get_global_unicast(ctxt->config.tun_dev, node_ipv6[1]);

    WARN("-------send wisun nodes info through spinel");
    spinel_push_hdr_is_prop(&tx_buf, SPINEL_PROP_EXT_WisunNodes);
    ext_message_append_node(&tx_buf, ctxt->rcp.eui64, NULL,
                             node_ipv6, true, false, NULL);

    for (int i = 0; i < len_pae; i++) {
        memcpy(node_ipv6[0], ADDR_LINK_LOCAL_PREFIX, 8);
        memcpy(node_ipv6[0] + 8, eui64_pae[i], 8);
        memcpy(node_ipv6[1], ADDR_UNSPECIFIED, 16);
        parent = NULL;
        ucast_addr = dhcp_eui64_to_ipv6(ctxt, eui64_pae[i]);
        if (ucast_addr) {
            memcpy(node_ipv6[1], ucast_addr, 16);
            for (j = 0; j < len_rpl; j++)
                if (!memcmp(table[j].target, node_ipv6[1] + 8, 8))
                    break;
            if (j != len_rpl) {
                memcpy(ipv6, br_info.prefix, 8);
                memcpy(ipv6 + 8, table[j].parent, 8);
                parent = dhcp_ipv6_to_eui64(ctxt, ipv6);
                WARN_ON(!parent, "RPL parent not in DHCP leases (%s)", tr_ipv6(ipv6));
            }
        }
        if (dbus_get_neighbor_info(ctxt, &neighbor_info, eui64_pae[i]))
            neighbor_info_ptr = &neighbor_info;
        else
            neighbor_info_ptr = NULL;
        if (ws_pae_key_storage_supp_exists(eui64_pae[i]))
            supp = ws_pae_key_storage_supp_read(NULL, eui64_pae[i], NULL, NULL, NULL);
        else
            supp = NULL;

        ext_message_append_node(&tx_buf, eui64_pae[i], parent, node_ipv6,
                                 false, supp, neighbor_info_ptr);
        if (supp)
            free(supp);
    }
    ext_cmd_tx(ctxt, &tx_buf);
    iobuf_free(&tx_buf);
}

// Some debug tools (fuzzers) may deflect this struct. So keep it public.
struct ext_rx_cmds ext_cmds[] = {
    { SPINEL_CMD_NOOP,             (uint32_t)-1,                         ext_rx_no_op },
    { SPINEL_CMD_PROP_GET,         SPINEL_PROP_EXT_WisunStatus,          exp_get_wisun_status },
    { SPINEL_CMD_PROP_SET,         SPINEL_PROP_EXT_WisunNetworkName,     exp_set_wisun_network_name },
    { SPINEL_CMD_PROP_GET,         SPINEL_PROP_EXT_WisunNodes,           exp_get_wisun_nodes },
    { (uint32_t)-1,                (uint32_t)-1,                         NULL },
};

void ext_cmd_rx(struct wsbr_ctxt *ctxt)
{
    static uint8_t rx_buf[4096];
    struct iobuf_read buf = {
        .data = rx_buf,
    };
    uint32_t cmd, prop;

    buf.data_size = ctxt->extcmd.device_rx(ctxt->ext_cmd_ctxt, rx_buf, sizeof(rx_buf));
    if (!buf.data_size)
        return;
    spinel_trace_rx(&buf);
    spinel_pop_u8(&buf); /* packet header */
    cmd = spinel_pop_uint(&buf);
    WARN("--------cmd : %s", spinel_cmd_str(cmd));
    if (cmd < SPINEL_CMD_PROP_GET || cmd > SPINEL_CMD_PROP_REMOVED) {
        prop = (uint32_t)-1;
    } else {
        prop = spinel_pop_uint(&buf);
    }
    WARN("-------prop : %s", spinel_prop_str(prop));
    for (int i = 0; ext_cmds[i].cmd != (uint32_t)-1; i++)
        if (ext_cmds[i].cmd == cmd && ext_cmds[i].prop == prop)
            return ext_cmds[i].fn(ctxt, prop, &buf);
    ERROR("%s: command %04x/%04x not implemented", __func__, cmd, prop);
}
