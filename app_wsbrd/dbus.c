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
#include <arpa/inet.h>
#include <systemd/sd-bus.h>
#include "app_wsbrd/tun.h"
#include "common/named_values.h"
#include "common/utils.h"
#include "common/log.h"
#include "stack/ws_bbr_api.h"
#include "common/log_legacy.h"

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
#include "rcp_api.h"
#include "wsbr.h"
#include "tun.h"

#include "dbus.h"

#define TRACE_GROUP "dbus"

static int dbus_set_slot_algorithm(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    int ret;
    uint8_t mode;

    ret = sd_bus_message_read(m, "y", &mode);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);

    if (mode == 0)
        rcp_set_tx_allowance_level(WS_TX_AND_RX_SLOT, WS_TX_AND_RX_SLOT);
    else if (mode == 1)
        rcp_set_tx_allowance_level(WS_TX_SLOT, WS_TX_SLOT);
    else
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_set_mode_switch(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int ret;
    uint8_t *eui64;
    size_t eui64_len;
    int phy_mode_id;

    tr_warn("---------dbus_set_mode_switch()");

    ret = sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);

    ret = sd_bus_message_read_basic(m, 'i', &phy_mode_id);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);

    if (eui64_len == 0)
        eui64 = NULL;
    else if (eui64_len != 8)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    if (phy_mode_id > 0)
        ret = ws_bbr_set_mode_switch(ctxt->rcp_if_id, 1, phy_mode_id, eui64); // mode switch enabled
    else if (phy_mode_id == -1)
        ret = ws_bbr_set_mode_switch(ctxt->rcp_if_id, -1, 0, eui64); // mode switch disabled
    else if (phy_mode_id == 0)
        ret = ws_bbr_set_mode_switch(ctxt->rcp_if_id, 0, 0, eui64); // mode switch back to default

    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_start_fan10(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;

    /* stop BBR and close network interface first */
    ws_bbr_stop(ctxt->rcp_if_id);
    arm_nwk_interface_down(ctxt->rcp_if_id); 

    /* restart BBR after stop */
    tr_warn("-----restart FAN10: %p", ctxt);
    wsbr_restart(ctxt); 

    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_stop_fan10(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;

    tr_warn("-----userdata:%p", ctxt);
    tr_warn("-----ctxt->rcp_if_id:%d", ctxt->rcp_if_id);

    ws_bbr_stop(ctxt->rcp_if_id);

    tr_warn("-----network interface down: %d", ctxt->rcp_if_id);
    arm_nwk_interface_down(ctxt->rcp_if_id); 

    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_join_multicast_group(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    struct net_if *cur = protocol_stack_interface_info_get_by_id(ctxt->rcp_if_id);
    const uint8_t *ipv6;
    size_t len;
    int ret;

    if (!cur)
        return sd_bus_error_set_errno(ret_error, EFAULT);

    ret = sd_bus_message_read_array(m, 'y', (const void **)&ipv6, &len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    if (len != 16)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ret = wsbr_tun_join_mcast_group(ctxt->sock_mcast, ctxt->config.tun_dev, ipv6);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, errno);
    addr_add_group(cur, ipv6);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_leave_multicast_group(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    struct net_if *cur = protocol_stack_interface_info_get_by_id(ctxt->rcp_if_id);
    const uint8_t *ipv6;
    size_t len;
    int ret;

    if (!cur)
        return sd_bus_error_set_errno(ret_error, EFAULT);

    ret = sd_bus_message_read_array(m, 'y', (const void **)&ipv6, &len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    if (len != 16)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    wsbr_tun_leave_mcast_group(ctxt->sock_mcast, ctxt->config.tun_dev, ipv6);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, errno);
    addr_remove_group(cur, ipv6);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

void dbus_emit_keys_change(struct wsbr_ctxt *ctxt)
{
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Gtks", NULL);
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Gaks", NULL);
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Lgtks", NULL);
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Lgaks", NULL);
}

static int dbus_get_transient_keys(sd_bus_message *reply, void *userdata,
                                   sd_bus_error *ret_error, bool is_lfn)
{
    int interface_id = *(int *)userdata;
    sec_prot_gtk_keys_t *gtks = ws_pae_controller_get_transient_keys(interface_id, is_lfn);
    const int key_cnt = is_lfn ? LGTK_NUM : GTK_NUM;
    int ret;

    if (!gtks)
        return sd_bus_error_set_errno(ret_error, EBADR);
    ret = sd_bus_message_open_container(reply, 'a', "ay");
    WARN_ON(ret < 0, "%s", strerror(-ret));
    for (int i = 0; i < key_cnt; i++) {
        ret = sd_bus_message_append_array(reply, 'y', gtks->gtk[i].key, ARRAY_SIZE(gtks->gtk[i].key));
        WARN_ON(ret < 0, "%s", strerror(-ret));
    }
    ret = sd_bus_message_close_container(reply);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

static int dbus_get_gtks(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_transient_keys(reply, userdata, ret_error, false);
}

static int dbus_get_lgtks(sd_bus *bus, const char *path, const char *interface,
                          const char *property, sd_bus_message *reply,
                          void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_transient_keys(reply, userdata, ret_error, true);
}

static int dbus_get_aes_keys(sd_bus_message *reply, void *userdata,
                             sd_bus_error *ret_error, bool is_lfn)
{
    int interface_id = *(int *)userdata;
    struct net_if *interface_ptr = protocol_stack_interface_info_get_by_id(interface_id);
    sec_prot_gtk_keys_t *gtks = ws_pae_controller_get_transient_keys(interface_id, is_lfn);
    const int key_cnt = is_lfn ? LGTK_NUM : GTK_NUM;
    uint8_t gak[16];
    int ret;

    if (!gtks || !interface_ptr || !interface_ptr->ws_info.cfg)
        return sd_bus_error_set_errno(ret_error, EBADR);
    ret = sd_bus_message_open_container(reply, 'a', "ay");
    WARN_ON(ret < 0, "%s", strerror(-ret));
    for (int i = 0; i < key_cnt; i++) {
        // GAK is SHA256 of network name concatened with GTK
        ws_pae_controller_gak_from_gtk(gak, gtks->gtk[i].key, interface_ptr->ws_info.cfg->gen.network_name);
        ret = sd_bus_message_append_array(reply, 'y', gak, ARRAY_SIZE(gak));
        WARN_ON(ret < 0, "%s", strerror(-ret));
    }
    ret = sd_bus_message_close_container(reply);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

static int dbus_get_gaks(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_aes_keys(reply, userdata, ret_error, false);
}

static int dbus_get_lgaks(sd_bus *bus, const char *path, const char *interface,
                          const char *property, sd_bus_message *reply,
                          void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_aes_keys(reply, userdata, ret_error, true);
}

static int dbus_revoke_pairwise_keys(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    size_t eui64_len;
    uint8_t *eui64;
    int ret;

    ret = sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    if (eui64_len != 8)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    ret = ws_bbr_node_keys_remove(ctxt->rcp_if_id, eui64);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_revoke_group_keys(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    uint8_t *gtk, *lgtk;
    size_t len;
    int ret;

    ret = sd_bus_message_read_array(m, 'y', (const void **)&gtk, &len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    if (!len)
        gtk = NULL;
    else if (len != GTK_LEN)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    ret = sd_bus_message_read_array(m, 'y', (const void **)&lgtk, &len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    if (!len)
        lgtk = NULL;
    else if (len != GTK_LEN)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ret = ws_bbr_node_access_revoke_start(ctxt->rcp_if_id, false, gtk);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    ret = ws_bbr_node_access_revoke_start(ctxt->rcp_if_id, true, lgtk);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_install_group_key(sd_bus_message *m, void *userdata,
                                  sd_bus_error *ret_error, bool is_lgtk)
{
    struct wsbr_ctxt *ctxt = userdata;
    const uint8_t *gtk;
    size_t len;
    int ret;

    ret = sd_bus_message_read_array(m, 'y', (const void **)&gtk, &len);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    if (len != GTK_LEN)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ws_pae_auth_gtk_install(ctxt->rcp_if_id, gtk, is_lgtk);

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_install_gtk(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    return dbus_install_group_key(m, userdata, ret_error, false);
}

static int dbus_install_lgtk(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    return dbus_install_group_key(m, userdata, ret_error, true);
}

void dbus_emit_nodes_change(struct wsbr_ctxt *ctxt)
{
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Nodes", NULL);
}

static int dbus_message_open_info(sd_bus_message *m, const char *property,
                                  const char *name, const char *type)
{
    int ret;

    ret = sd_bus_message_open_container(m, 'e', "sv");
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    ret = sd_bus_message_append(m, "s", name);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    ret = sd_bus_message_open_container(m, 'v', type);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    return ret;
}

static int dbus_message_close_info(sd_bus_message *m, const char *property)
{
    int ret;

    ret = sd_bus_message_close_container(m);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    ret = sd_bus_message_close_container(m);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    return ret;
}

struct neighbor_info {
    int rssi;
    int rsl;
    int rsl_adv;
};

static int dbus_message_append_node(
    sd_bus_message *m,
    const char *property,
    const uint8_t self[8],
    const uint8_t parent[8],
    const uint8_t ipv6[][16],
    bool is_br,
    supp_entry_t *supp,
    struct neighbor_info *neighbor)
{
    int ret, val;

    ret = sd_bus_message_open_container(m, 'r', "aya{sv}");
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    ret = sd_bus_message_append_array(m, 'y', self, 8);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    ret = sd_bus_message_open_container(m, 'a', "{sv}");
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    {
        if (is_br) {
            dbus_message_open_info(m, property, "is_border_router", "b");
            ret = sd_bus_message_append(m, "b", true);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
            dbus_message_close_info(m, property);
            // TODO: deprecate is_border_router
            dbus_message_open_info(m, property, "node_role", "y");
            ret = sd_bus_message_append(m, "y", WS_NR_ROLE_BR);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
            dbus_message_close_info(m, property);
        } else if (supp) {
            dbus_message_open_info(m, property, "is_authenticated", "b");
            val = true;
            ret = sd_bus_message_append(m, "b", val);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
            dbus_message_close_info(m, property);
            if (ws_common_is_valid_nr(supp->sec_keys.node_role)) {
                dbus_message_open_info(m, property, "node_role", "y");
                ret = sd_bus_message_append(m, "y", supp->sec_keys.node_role);
                WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
                dbus_message_close_info(m, property);
            }
        }
        if (parent) {
            dbus_message_open_info(m, property, "parent", "ay");
            ret = sd_bus_message_append_array(m, 'y', parent, 8);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
            dbus_message_close_info(m, property);
        }
        if (neighbor) {
            dbus_message_open_info(m, property, "is_neighbor", "b");
            ret = sd_bus_message_append(m, "b", true);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
            dbus_message_close_info(m, property);

            dbus_message_open_info(m, property, "rssi", "i");
            ret = sd_bus_message_append_basic(m, 'i', &neighbor->rssi);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
            dbus_message_close_info(m, property);
            if (neighbor->rsl != INT_MIN) {
                dbus_message_open_info(m, property, "rsl", "i");
                ret = sd_bus_message_append_basic(m, 'i', &neighbor->rsl);
                WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
                dbus_message_close_info(m, property);
            }
            if (neighbor->rsl_adv != INT_MIN) {
                dbus_message_open_info(m, property, "rsl_adv", "i");
                ret = sd_bus_message_append_basic(m, 'i', &neighbor->rsl_adv);
                WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
                dbus_message_close_info(m, property);
            }
        }
        dbus_message_open_info(m, property, "ipv6", "aay");
        ret = sd_bus_message_open_container(m, 'a', "ay");
        WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
        for (; memcmp(*ipv6, ADDR_UNSPECIFIED, 16); ipv6++) {
            ret = sd_bus_message_append_array(m, 'y', *ipv6, 16);
            WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
        }
        ret = sd_bus_message_close_container(m);
        WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
        dbus_message_close_info(m, property);
    }
    ret = sd_bus_message_close_container(m);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    ret = sd_bus_message_close_container(m);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    return ret;
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

int dbus_get_nodes(sd_bus *bus, const char *path, const char *interface,
                       const char *property, sd_bus_message *reply,
                       void *userdata, sd_bus_error *ret_error)
{
    struct neighbor_info *neighbor_info_ptr;
    struct neighbor_info neighbor_info;
    struct wsbr_ctxt *ctxt = userdata;
    uint8_t node_ipv6[3][16] = { 0 };
    bbr_route_info_t table[4096];
    uint8_t *parent, *ucast_addr;
    int len_pae, len_rpl, ret, j;
    uint8_t eui64_pae[4096][8];
    bbr_information_t br_info;
    supp_entry_t *supp;
    uint8_t ipv6[16];

    ret = ws_bbr_info_get(ctxt->rcp_if_id, &br_info);
    if (ret)
        return sd_bus_error_set_errno(ret_error, EAGAIN);
    len_pae = ws_pae_auth_supp_list(ctxt->rcp_if_id, eui64_pae, sizeof(eui64_pae));
    len_rpl = ws_bbr_routing_table_get(ctxt->rcp_if_id, table, ARRAY_SIZE(table));
    if (len_rpl < 0)
        return sd_bus_error_set_errno(ret_error, EAGAIN);

    ret = sd_bus_message_open_container(reply, 'a', "(aya{sv})");
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    tun_addr_get_link_local(ctxt->config.tun_dev, node_ipv6[0]);
    tun_addr_get_global_unicast(ctxt->config.tun_dev, node_ipv6[1]);
    dbus_message_append_node(reply, property, ctxt->rcp.eui64, NULL,
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
        dbus_message_append_node(reply, property, eui64_pae[i], parent, node_ipv6,
                                 false, supp, neighbor_info_ptr);
        if (supp)
            free(supp);
    }
    ret = sd_bus_message_close_container(reply);
    WARN_ON(ret < 0, "d %s: %s", property, strerror(-ret));
    return 0;
}

int dbus_get_hw_address(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    uint8_t *hw_addr = userdata;
    int ret;

    ret = sd_bus_message_append_array(reply, 'y', hw_addr, 8);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int dbus_get_ws_pan_id(sd_bus *bus, const char *path, const char *interface,
                       const char *property, sd_bus_message *reply,
                       void *userdata, sd_bus_error *ret_error)
{
    struct net_if *net_if = protocol_stack_interface_info_get_by_id(*(int *)userdata);
    int ret;

    if (!net_if)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    ret = sd_bus_message_append(reply, "q", net_if->ws_info.network_pan_id);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    return 0;
}

int wsbrd_get_ws_domain(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    int *domain = userdata;
    int ret;

    ret = sd_bus_message_append(reply, "s", val_to_str(*domain, valid_ws_domains, "[unknown]"));
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int wsbrd_get_ws_size(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    int *size = userdata;
    int ret;

    ret = sd_bus_message_append(reply, "s", val_to_str(*size, valid_ws_size, NULL));
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int dbus_get_string(sd_bus *bus, const char *path, const char *interface,
               const char *property, sd_bus_message *reply,
               void *userdata, sd_bus_error *ret_error)
{
    char *val = userdata;
    int ret;

    ret = sd_bus_message_append(reply, "s", val);
    WARN_ON(ret < 0, "%s: %s", property, strerror(-ret));
    return 0;
}


/***** new functions */
static int dbus_get_network_state(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    int ret;

    ////////tr_warn("---- Network state:");
    uint8_t address_buf[128];
    int address_count = 0;

    ret = sd_bus_message_open_container(reply, 'a', "ay");
    WARN_ON(ret < 0, "%s", strerror(-ret));
    if (arm_net_address_list_get(interface_id, 128, address_buf, &address_count) == 0) {
        uint8_t *t_buf = address_buf;
        for (int i = 0; i < address_count; ++i) {
            ////////tr_info("address%d: %s",i,tr_ipv6(t_buf));
            ret = sd_bus_message_append_array(reply, 'y', t_buf, 16);
            WARN_ON(ret < 0, "%s", strerror(-ret));
            t_buf += 16;
        }
    }
    ret = sd_bus_message_close_container(reply);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    return 0;
}


int dbus_set_network_name(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    char *nwkname;
    int ret;

    ret = sd_bus_message_read_basic(m, 's', (void **)&nwkname);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    tr_info("Set wisun network_name: %s", nwkname);
    ws_management_network_name_set(interface_id, nwkname);
    strncpy(ctxt->config.ws_name, nwkname, 32);

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_set_phy_configs(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    int ret;

    uint8_t regulatory_domain, operating_class, operating_mode;

    ret = sd_bus_message_read(m, "y", &regulatory_domain);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "y", &operating_class);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "y", &operating_mode);
    WARN_ON(ret < 0, "%s", strerror(-ret));


    /* stop BBR and close network interface first */
    ws_bbr_stop(ctxt->rcp_if_id);
    arm_nwk_interface_down(ctxt->rcp_if_id); 

    tr_warn("-----Expected Domain: %d\tClass: %d\tMode: %d", regulatory_domain, operating_class, operating_mode);
    ctxt->config.ws_domain = regulatory_domain;
    ctxt->config.ws_class  = operating_class;
    ctxt->config.ws_mode   = operating_mode;
    ret = ws_management_regulatory_domain_set(interface_id, ctxt->config.ws_domain,
                                              ctxt->config.ws_class, ctxt->config.ws_mode,
                                              ctxt->config.ws_phy_mode_id, ctxt->config.ws_chan_plan_id);
    WARN_ON(ret);

    /* restart BBR after stop */
    tr_warn("-----restart FAN10: %p", ctxt);
    wsbr_restart(ctxt);     

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_set_timing_params(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    int ret;

    uint8_t trickle_k;
    uint16_t trickle_imin, trickle_imax, pan_timeout;

    ret = sd_bus_message_read(m, "q", &trickle_imin);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "q", &trickle_imax);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "y", &trickle_k);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "q", &pan_timeout);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    ws_management_timing_parameters_set(interface_id, trickle_imin, trickle_imax, trickle_k, pan_timeout);
    //////tr_warn("disc_trickle_imin: 0x%04x, disc_trickle_imax: 0x%04x, disc_trickle_k: 0x%02x, pan_timeout: 0x%04x\n", trickle_imin, trickle_imax, trickle_k, pan_timeout);

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_get_timing_param(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    uint16_t disc_trickle_imin;
    uint16_t disc_trickle_imax;  
    uint8_t disc_trickle_k;
    uint16_t pan_timeout;
    uint16_t timing_params[4];
    int ret;

    ws_management_timing_parameters_get(interface_id, &disc_trickle_imin, &disc_trickle_imax, &disc_trickle_k, &pan_timeout);
    //////tr_warn("disc_trickle_imin: 0x%04x, disc_trickle_imax: 0x%04x, disc_trickle_k: 0x%02x, pan_timeout: 0x%04x\n", disc_trickle_imin, disc_trickle_imax, disc_trickle_k, pan_timeout);
    timing_params[0] = disc_trickle_imin;
    timing_params[1] = disc_trickle_imax;
    timing_params[2] = (uint16_t)disc_trickle_k;
    timing_params[3] = pan_timeout;

    ret = sd_bus_message_append_array(reply, 'q', &timing_params, 8);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int dbus_get_fhss_channel_mask(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    uint32_t channel_mask[8];
    int ret;

    ws_management_channel_mask_get(interface_id, (uint8_t *)channel_mask);
    //////tr_warn("fhss channal mask: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", channel_mask[0], channel_mask[1], channel_mask[2], channel_mask[3], channel_mask[4], channel_mask[5], channel_mask[6], channel_mask[7]);

    ret = sd_bus_message_append_array(reply, 'u', &channel_mask, 32);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int dbus_get_fhss_timing_configure(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int interface_id = ctxt->rcp_if_id;
    uint8_t  uc_channel_function;
    uint8_t  bc_channel_function;
    uint16_t fixed_channel;
    uint8_t  uc_dwell_interval;
    uint8_t  bc_dwell_interval;
    uint32_t broadcast_interval;
    uint32_t fhss_timing_configure[5];
    int ret;

    ws_management_fhss_unicast_channel_function_get(interface_id, &uc_channel_function, 
                                                    &fixed_channel, &uc_dwell_interval);
    ws_management_fhss_broadcast_channel_function_get(interface_id, &bc_channel_function, 
                                &fixed_channel, &bc_dwell_interval, &broadcast_interval);

    //////tr_warn("fhss timing configure: uc_dwell_interval %d ms, bc_interval %d ms, bc_dwell_interval %d ms\n", 
    //////                            uc_dwell_interval, broadcast_interval, bc_dwell_interval);
    fhss_timing_configure[0] = (uint32_t)uc_dwell_interval;
    fhss_timing_configure[1] = broadcast_interval;
    fhss_timing_configure[2] = (uint32_t)bc_dwell_interval;
    fhss_timing_configure[3] = (uint32_t)uc_channel_function;
    fhss_timing_configure[4] = (uint32_t)bc_channel_function;

    ret = sd_bus_message_append_array(reply, 'u', &fhss_timing_configure, 20);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int dbus_get_gtk_active_key_index(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    struct net_if *interface_ptr = protocol_stack_interface_info_get_by_id(ctxt->rcp_if_id);
    int ret;

    int8_t gtk_active_index = ws_pae_controller_gtk_active_index_get(interface_ptr);
    WARN_ON(gtk_active_index < 0, "%s", strerror(-gtk_active_index));

    ret = sd_bus_message_append(reply, "y", (uint8_t)gtk_active_index);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    return 0;

}

int dbus_get_wisun_cfg_settings(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    struct net_if *cur = protocol_stack_interface_info_get_by_id(ctxt->rcp_if_id);


    uint16_t configuration_u16[51];
    ws_cfg_t ws_cfg_val;
    int ret;
    ws_cfg_settings_get(&ws_cfg_val);
    //// INFO("general configuration:\n network_size: %d  network_name: %s rpl_parent_candidate_max: 0x%04x rpl_selected_parent_max: 0x%04x\n", ws_cfg_val.gen.network_size, ws_cfg_val.gen.network_name, ws_cfg_val.gen.rpl_parent_candidate_max, ws_cfg_val.gen.rpl_selected_parent_max);
    //// INFO("phy configuration:\n regulatory_domain: 0x%02x  operating_class: 0x%02x  operating_mode: 0x%02x phy_mode_id: 0x%02x channel_plan_id: 0x%02x\n", ws_cfg_val.phy.regulatory_domain, ws_cfg_val.phy.operating_class, ws_cfg_val.phy.operating_mode, ws_cfg_val.phy.phy_mode_id, ws_cfg_val.phy.channel_plan_id);
    //// INFO("timing configuration:\n disc_trickle_imin: %d seconds disc_trickle_imax: %d seconds disc_trickle_k: 0x%02x pan_timeout: %d seconds temp_link_min_timeout: %d temp_eapol_min_timeout: %d seconds\n", ws_cfg_val.timing.disc_trickle_imin, ws_cfg_val.timing.disc_trickle_imax, ws_cfg_val.timing.disc_trickle_k, ws_cfg_val.timing.pan_timeout, ws_cfg_val.timing.temp_link_min_timeout, ws_cfg_val.timing.temp_eapol_min_timeout);
    //// INFO("bbr configuration:\n dio_interval_min: %d ms dio_interval_doublings: %d dio_redundancy_constant: %d dag_max_rank_increase: %d min_hop_rank_increase: %d seconds rpl_default_lifetime: %d seconds\n", ws_cfg_val.bbr.dio_interval_min, ws_cfg_val.bbr.dio_interval_doublings, ws_cfg_val.bbr.dio_redundancy_constant, ws_cfg_val.bbr.dag_max_rank_increase, ws_cfg_val.bbr.min_hop_rank_increase, ws_cfg_val.bbr.rpl_default_lifetime);
    //// INFO("fhss configuration:\n fhss_uc_dwell_interval: %d ms fhss_bc_dwell_interval: %d ms fhss_bc_interval: %d ms fhss_uc_channel_function: %d fhss_uc_fixed_channel: %d fhss_bc_fixed_channel: %d  fhss_channel_mask: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", ws_cfg_val.fhss.fhss_uc_dwell_interval, ws_cfg_val.fhss.fhss_bc_dwell_interval, ws_cfg_val.fhss.fhss_bc_interval, ws_cfg_val.fhss.fhss_uc_channel_function, ws_cfg_val.fhss.fhss_uc_fixed_channel, ws_cfg_val.fhss.fhss_bc_fixed_channel, ws_cfg_val.fhss.fhss_channel_mask[0], ws_cfg_val.fhss.fhss_channel_mask[1], ws_cfg_val.fhss.fhss_channel_mask[2], ws_cfg_val.fhss.fhss_channel_mask[3], ws_cfg_val.fhss.fhss_channel_mask[4], ws_cfg_val.fhss.fhss_channel_mask[5], ws_cfg_val.fhss.fhss_channel_mask[6], ws_cfg_val.fhss.fhss_channel_mask[7]);
    //// INFO("mpl configuration:\n mpl_trickle_imin: %d seconds  mpl_trickle_imax: %d seconds  mpl_trickle_k: %d mpl_trickle_timer_exp: %d seed_set_entry_lifetime: %d seconds\n", ws_cfg_val.mpl.mpl_trickle_imin, ws_cfg_val.mpl.mpl_trickle_imax, ws_cfg_val.mpl.mpl_trickle_k, ws_cfg_val.mpl.mpl_trickle_timer_exp, ws_cfg_val.mpl.seed_set_entry_lifetime);
    //// INFO("sec_timer configuration:\n gtk_expire_offset: %d minutes  pmk_lifetime: %d minutes  ptk_lifetime: %d minutes gtk_new_act_time: %d minutes\n", ws_cfg_val.sec_timer.gtk_expire_offset, ws_cfg_val.sec_timer.pmk_lifetime, ws_cfg_val.sec_timer.ptk_lifetime, ws_cfg_val.sec_timer.gtk_new_act_time);
    //// INFO("minutes gtk_new_install_req:\n %d percent of GTK lifetime\n", ws_cfg_val.sec_timer.gtk_new_install_req);
    configuration_u16[0]   = ws_cfg_val.gen.network_size;
    configuration_u16[1]   = cur->ws_info.network_pan_id;
    configuration_u16[2]   = ws_cfg_val.gen.rpl_parent_candidate_max;
    configuration_u16[3]   = ws_cfg_val.gen.rpl_selected_parent_max;
 
    configuration_u16[4]   = ws_cfg_val.phy.regulatory_domain;
    configuration_u16[5]   = ws_cfg_val.phy.operating_class;
    configuration_u16[6]   = ws_cfg_val.phy.operating_mode;
    configuration_u16[7]   = ws_cfg_val.phy.phy_mode_id;
    configuration_u16[8]   = ws_cfg_val.phy.channel_plan_id;
 
    configuration_u16[9]   = ws_cfg_val.timing.disc_trickle_imin;
    configuration_u16[10]  = ws_cfg_val.timing.disc_trickle_imax;
    configuration_u16[11]  = ws_cfg_val.timing.disc_trickle_k;
    configuration_u16[12]  = ws_cfg_val.timing.pan_timeout;
    configuration_u16[13]  = ws_cfg_val.timing.temp_link_min_timeout;
    configuration_u16[14]  = ws_cfg_val.timing.temp_eapol_min_timeout;
 
    configuration_u16[15]  = ws_cfg_val.bbr.dio_interval_min;
    configuration_u16[16]  = ws_cfg_val.bbr.dio_interval_doublings;
    configuration_u16[17]  = ws_cfg_val.bbr.dio_redundancy_constant;
    configuration_u16[18]  = ws_cfg_val.bbr.dag_max_rank_increase;
    configuration_u16[19]  = ws_cfg_val.bbr.min_hop_rank_increase;
    configuration_u16[20]  = (ws_cfg_val.bbr.rpl_default_lifetime & 0x0000ffff);
    configuration_u16[21]  = ((ws_cfg_val.bbr.rpl_default_lifetime>>16) & 0x0000ffff);

    configuration_u16[22]  = ws_cfg_val.fhss.fhss_uc_dwell_interval;
    configuration_u16[23]  = ws_cfg_val.fhss.fhss_bc_dwell_interval;
    configuration_u16[24]  = (ws_cfg_val.fhss.fhss_bc_interval & 0x0000ffff);
    configuration_u16[25]  = ((ws_cfg_val.fhss.fhss_bc_interval>>16) & 0x0000ffff);
    configuration_u16[26]  = ws_cfg_val.fhss.fhss_uc_channel_function;
    configuration_u16[27]  = ws_cfg_val.fhss.fhss_uc_fixed_channel;
    configuration_u16[28]  = ws_cfg_val.fhss.fhss_bc_fixed_channel;
    configuration_u16[29]  = ws_cfg_val.fhss.fhss_channel_mask[0];
    configuration_u16[30]  = ws_cfg_val.fhss.fhss_channel_mask[1];
    configuration_u16[31]  = ws_cfg_val.fhss.fhss_channel_mask[2];
    configuration_u16[32]  = ws_cfg_val.fhss.fhss_channel_mask[3];
    configuration_u16[33]  = ws_cfg_val.fhss.fhss_channel_mask[4];
    configuration_u16[34]  = ws_cfg_val.fhss.fhss_channel_mask[5];
    configuration_u16[35]  = ws_cfg_val.fhss.fhss_channel_mask[6];
    configuration_u16[36]  = ws_cfg_val.fhss.fhss_channel_mask[7];

    configuration_u16[37]  = ws_cfg_val.mpl.mpl_trickle_imin;
    configuration_u16[38]  = ws_cfg_val.mpl.mpl_trickle_imax;
    configuration_u16[39]  = ws_cfg_val.mpl.mpl_trickle_k;
    configuration_u16[40]  = ws_cfg_val.mpl.mpl_trickle_timer_exp;
    configuration_u16[41]  = ws_cfg_val.mpl.seed_set_entry_lifetime;

    configuration_u16[42]  = (ws_cfg_val.sec_timer.gtk_expire_offset & 0x0000ffff);
    configuration_u16[43]  = ((ws_cfg_val.sec_timer.gtk_expire_offset>>16) & 0x0000ffff);
    configuration_u16[44]  = (ws_cfg_val.sec_timer.pmk_lifetime & 0x0000ffff);
    configuration_u16[45]  = ((ws_cfg_val.sec_timer.pmk_lifetime>>16) & 0x0000ffff);
    configuration_u16[46]  = (ws_cfg_val.sec_timer.ptk_lifetime & 0x0000ffff);
    configuration_u16[47]  = ((ws_cfg_val.sec_timer.ptk_lifetime>>16) & 0x0000ffff);
    configuration_u16[48]  = ws_cfg_val.sec_timer.gtk_new_act_time;
    configuration_u16[49]  = ws_cfg_val.sec_timer.ffn_revocat_lifetime_reduct;
    configuration_u16[50]  = ws_cfg_val.sec_timer.gtk_new_install_req;

    ret = sd_bus_message_append_array(reply, 'q', &configuration_u16, 51*2);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    return 0;
}

int dbus_set_fhss_ch_mask_f4b(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int ret;

    uint32_t Channel_mask[8];
    /* get current channel mask settings from stack, than we modify it and set back */
    ws_management_channel_mask_get(ctxt->rcp_if_id, (uint8_t *)Channel_mask);

    ret = sd_bus_message_read(m, "u", &Channel_mask[0]);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &Channel_mask[1]);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &Channel_mask[2]);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &Channel_mask[3]);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    /* get current channel mask settings from stack, than we modify it and set back */
    ws_management_channel_mask_set(ctxt->rcp_if_id, (uint8_t *)Channel_mask);

    return 0;
}

int dbus_set_fhss_ch_mask_l4b(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int ret;

    uint32_t Channel_mask[8];
    /* get current channel mask settings from stack, than we modify it and set back */
    ws_management_channel_mask_get(ctxt->rcp_if_id, (uint8_t *)Channel_mask);

    ret = sd_bus_message_read(m, "u", &Channel_mask[4]);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &Channel_mask[5]);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &Channel_mask[6]);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &Channel_mask[7]);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    /* get current channel mask settings from stack, than we modify it and set back */
    ws_management_channel_mask_set(ctxt->rcp_if_id, (uint8_t *)Channel_mask);

    return 0;
}

int dbus_set_fhss_timing_configure(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int ret;

    uint8_t  uc_dwell_interval;
    uint32_t broadcast_interval;
    uint8_t  bc_dwell_interval;

    ret = sd_bus_message_read(m, "y", &uc_dwell_interval);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &broadcast_interval);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "y", &bc_dwell_interval);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    ws_management_fhss_timing_configure_set(ctxt->rcp_if_id, uc_dwell_interval, broadcast_interval, bc_dwell_interval);

    return 0;
}

int dbus_set_fhss_uc_function(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int ret;

    uint8_t  channel_function;
    uint16_t fixed_channel;
    uint8_t  dwell_interval;

    ret = sd_bus_message_read(m, "y", &channel_function);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "q", &fixed_channel);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "y", &dwell_interval);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    ws_management_fhss_unicast_channel_function_configure(ctxt->rcp_if_id, channel_function, fixed_channel, dwell_interval);

    return 0;
}

int dbus_set_fhss_bc_function(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    int ret;

    uint8_t  channel_function;
    uint16_t fixed_channel;
    uint8_t  dwell_interval;
    uint32_t broadcast_interval;

    ret = sd_bus_message_read(m, "y", &channel_function);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "q", &fixed_channel);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "y", &dwell_interval);
    WARN_ON(ret < 0, "%s", strerror(-ret));
    ret = sd_bus_message_read(m, "u", &broadcast_interval);
    WARN_ON(ret < 0, "%s", strerror(-ret));

    ws_management_fhss_broadcast_channel_function_configure(ctxt->rcp_if_id, channel_function, fixed_channel, dwell_interval, broadcast_interval);

    return 0;
}

static const sd_bus_vtable dbus_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("startFan10", "ayi", NULL,
                      dbus_start_fan10, 0),
        SD_BUS_METHOD("stopFan10", "ayi", NULL,
                      dbus_stop_fan10, 0),
        SD_BUS_METHOD("JoinMulticastGroup", "ay", NULL,
                      dbus_join_multicast_group, 0),
        SD_BUS_METHOD("LeaveMulticastGroup", "ay", NULL,
                      dbus_leave_multicast_group, 0),
        SD_BUS_METHOD("SetModeSwitch", "ayi", NULL,
                      dbus_set_mode_switch, 0),
        SD_BUS_METHOD("SetSlotAlgorithm", "y", NULL,
                      dbus_set_slot_algorithm, 0),
        SD_BUS_METHOD("RevokePairwiseKeys", "ay", NULL,
                      dbus_revoke_pairwise_keys, 0),
        SD_BUS_METHOD("RevokeGroupKeys", "ayay", NULL,
                      dbus_revoke_group_keys, 0),
        SD_BUS_METHOD("InstallGtk", "ay", NULL,
                      dbus_install_gtk, 0),
        SD_BUS_METHOD("InstallLgtk", "ay", NULL,
                      dbus_install_lgtk, 0),
        SD_BUS_PROPERTY("Gtks", "aay", dbus_get_gtks,
                        offsetof(struct wsbr_ctxt, rcp_if_id),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Gaks", "aay", dbus_get_gaks,
                        offsetof(struct wsbr_ctxt, rcp_if_id),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Lgtks", "aay", dbus_get_lgtks,
                        offsetof(struct wsbr_ctxt, rcp_if_id),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Lgaks", "aay", dbus_get_lgaks,
                        offsetof(struct wsbr_ctxt, rcp_if_id),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Nodes", "a(aya{sv})", dbus_get_nodes, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
        SD_BUS_PROPERTY("HwAddress", "ay", dbus_get_hw_address,
                        offsetof(struct wsbr_ctxt, rcp.eui64),
                        0),
        SD_BUS_PROPERTY("WisunNetworkName", "s", dbus_get_string,
                        offsetof(struct wsbr_ctxt, config.ws_name),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunSize", "s", wsbrd_get_ws_size,
                        offsetof(struct wsbr_ctxt, config.ws_size),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunDomain", "s", wsbrd_get_ws_domain,
                        offsetof(struct wsbr_ctxt, config.ws_domain),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunMode", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_mode),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunClass", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_class),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunPhyModeId", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_phy_mode_id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunChanPlanId", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_chan_plan_id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunPanId", "q", dbus_get_ws_pan_id,
                        offsetof(struct wsbr_ctxt, rcp_if_id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunFanVersion", "y", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_fan_version),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        /******* now command */
        SD_BUS_PROPERTY("NetworkState", "aay", dbus_get_network_state, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_METHOD("SetNetworkName", "s", NULL,
                        dbus_set_network_name, 0),
        SD_BUS_METHOD("SetPhyConfigs", "yyy", NULL,
                        dbus_set_phy_configs, 0),
        SD_BUS_PROPERTY("GetTimingParam", "aq", dbus_get_timing_param, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_METHOD("SetTimingParams", "qqyq", NULL,
                        dbus_set_timing_params, 0),
        SD_BUS_PROPERTY("getFhssChannelMask", "au", dbus_get_fhss_channel_mask, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("getFhssTimingConfigure", "au", dbus_get_fhss_timing_configure, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("getGtkActiveKeyIndex", "y", dbus_get_gtk_active_key_index, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("getWisunCfgSettings", "aq", dbus_get_wisun_cfg_settings, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_METHOD("setFhssChMaskF4b", "uuuu", NULL,
                        dbus_set_fhss_ch_mask_f4b, 0),
        SD_BUS_METHOD("setFhssChMaskL4b", "uuuu", NULL,
                        dbus_set_fhss_ch_mask_l4b, 0),
        SD_BUS_METHOD("setFhssTimingConfig", "yuy", NULL,
                        dbus_set_fhss_timing_configure, 0),
        SD_BUS_METHOD("setFhssUCFuntion", "yqy", NULL,
                        dbus_set_fhss_uc_function, 0),
        SD_BUS_METHOD("setFhssBCFuntion", "yqyu", NULL,
                        dbus_set_fhss_bc_function, 0),
        SD_BUS_VTABLE_END
};

void dbus_register(struct wsbr_ctxt *ctxt)
{
    int ret;
    char mode = 'A';
    const char *env_var;
    const char *dbus_scope = "undefined";

    env_var = getenv("DBUS_STARTER_BUS_TYPE");
    if (env_var && !strcmp(env_var, "system"))
        mode = 'S';
    if (env_var && !strcmp(env_var, "user"))
        mode = 'U';
    if (env_var && !strcmp(env_var, "session"))
        mode = 'U';
    if (mode == 'U' || mode == 'A')
        ret = sd_bus_default_user(&ctxt->dbus);
    if (mode == 'S' || (mode == 'A' && ret < 0))
        ret = sd_bus_default_system(&ctxt->dbus);
    if (ret < 0) {
        WARN("DBus not available: %s", strerror(-ret));
        return;
    }

    ret = sd_bus_add_object_vtable(ctxt->dbus, NULL, "/com/silabs/Wisun/BorderRouter",
                                   "com.silabs.Wisun.BorderRouter",
                                   dbus_vtable, ctxt);
    if (ret < 0) {
        WARN("%s: %s", __func__, strerror(-ret));
        return;
    }

    ret = sd_bus_request_name(ctxt->dbus, "com.silabs.Wisun.BorderRouter",
                              SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_REPLACE_EXISTING);
    if (ret < 0) {
        WARN("%s: %s", __func__, strerror(-ret));
        return;
    }

    sd_bus_get_scope(ctxt->dbus, &dbus_scope);
    INFO("Successfully registered to %s DBus", dbus_scope);
}

int dbus_process(struct wsbr_ctxt *ctxt)
{
    BUG_ON(!ctxt->dbus);
    sd_bus_process(ctxt->dbus, NULL);
    return 0;
}

int dbus_get_fd(struct wsbr_ctxt *ctxt)
{
    if (ctxt->dbus)
        return sd_bus_get_fd(ctxt->dbus);
    else
        return -1;
}
