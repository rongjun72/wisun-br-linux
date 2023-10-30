/*
 * Copyright (c) 2018-2021, Pelion and affiliates.
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "common/log_legacy.h"
#include "common/ns_list.h"
#include "stack/mac/fhss_config.h"
#include "stack/ws_management_api.h"

#include "nwk_interface/protocol.h"
#include "6lowpan/ws/ws_common.h"
#include "6lowpan/ws/ws_bootstrap.h"
#include "6lowpan/ws/ws_cfg_settings.h"

#include "stack/ns_address.h"
#include "stack/source/legacy/net_socket.h"
#include "stack/source/common_protocols/icmpv6.h"

#define TRACE_GROUP "wsmg"


int ws_management_node_init(
    int8_t interface_id,
    uint8_t regulatory_domain,
    char *network_name_ptr)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);

    if (interface_id >= 0 && !cur) {
        return -1;
    }

    if (!network_name_ptr) {
        return -2;
    }

    ws_phy_cfg_t phy_cfg;
    if (ws_cfg_phy_get(&phy_cfg) < 0) {
        return -3;
    }

    phy_cfg.regulatory_domain = regulatory_domain;

    if (ws_cfg_phy_set(cur, &phy_cfg, 0) < 0) {
        return -4;
    }

    ws_gen_cfg_t gen_cfg;
    if (ws_cfg_gen_get(&gen_cfg) < 0) {
        return -3;
    }

    strncpy(gen_cfg.network_name, network_name_ptr, 32);

    if (ws_cfg_gen_set(cur, &gen_cfg, 0) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_network_name_set(
    int8_t interface_id,
    char *network_name_ptr)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!network_name_ptr) {
        return -2;
    }

    ws_gen_cfg_t cfg;
    if (ws_cfg_gen_get(&cfg) < 0) {
        return -3;
    }

    strncpy(cfg.network_name, network_name_ptr, 32);

    if (ws_cfg_gen_set(cur, &cfg, 0) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_network_name_get(
    int8_t interface_id,
    char *network_name_ptr)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!network_name_ptr) {
        return -2;
    }

    ws_gen_cfg_t cfg;
    if (ws_cfg_gen_get(&cfg) < 0) {
        return -3;
    }

    memcpy(network_name_ptr, cfg.network_name, 32);

    return 0;
}

int ws_management_network_name_validate(
    int8_t interface_id,
    char *network_name_ptr)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!network_name_ptr) {
        return -2;
    }

    ws_gen_cfg_t cfg;
    if (ws_cfg_gen_get(&cfg) < 0) {
        return -3;
    }

    strncpy(cfg.network_name, network_name_ptr, 32);

    if (ws_cfg_gen_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_domain_configuration_set(
    int8_t interface_id,
    uint8_t regulatory_domain,
    uint8_t phy_mode_id,
    uint8_t channel_plan_id)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_phy_cfg_t cfg;
    ws_phy_cfg_t cfg_default;
    if (ws_cfg_phy_get(&cfg) < 0) {
        return -3;
    }

    if (ws_cfg_phy_default_set(&cfg_default) < 0) {
        return -3;
    }

    if (regulatory_domain == 255) {
        cfg.regulatory_domain = cfg_default.regulatory_domain;
    } else if (regulatory_domain != 0) {
        cfg.regulatory_domain = regulatory_domain;
    }

    if (phy_mode_id == 255) {
        cfg.phy_mode_id = cfg_default.phy_mode_id;
    } else if (phy_mode_id != 0) {
        cfg.phy_mode_id = phy_mode_id;
    }

    if (channel_plan_id == 255) {
        cfg.channel_plan_id = cfg_default.channel_plan_id;
    } else if (channel_plan_id != 0) {
        cfg.channel_plan_id = channel_plan_id;
    }

    if (ws_cfg_phy_set(cur, &cfg, 0) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_domain_configuration_get(
    int8_t interface_id,
    uint8_t *regulatory_domain,
    uint8_t *phy_mode_id,
    uint8_t *channel_plan_id)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_phy_cfg_t cfg;
    if (ws_cfg_phy_get(&cfg) < 0) {
        return -2;
    }

    if (regulatory_domain) {
        *regulatory_domain = cfg.regulatory_domain;
    }
    if (phy_mode_id) {
        *phy_mode_id = cfg.phy_mode_id;
    }
    if (channel_plan_id) {
        *channel_plan_id = cfg.channel_plan_id;
    }

    return 0;
}

int ws_management_domain_configuration_validate(
    int8_t interface_id,
    uint8_t regulatory_domain,
    uint8_t phy_mode_id,
    uint8_t channel_plan_id)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_phy_cfg_t cfg;
    if (ws_cfg_phy_get(&cfg) < 0) {
        return -3;
    }

    cfg.regulatory_domain = regulatory_domain;
    cfg.phy_mode_id = phy_mode_id;
    cfg.channel_plan_id = channel_plan_id;

    if (ws_cfg_phy_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_regulatory_domain_set(
    int8_t interface_id,
    uint8_t regulatory_domain,
    uint8_t operating_class,
    uint8_t operating_mode,
    uint8_t phy_mode_id,
    uint8_t channel_plan_id)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_phy_cfg_t cfg;
    if (ws_cfg_phy_get(&cfg) < 0) {
        return -3;
    }

    cfg.regulatory_domain = regulatory_domain;
    cfg.operating_mode = operating_mode;
    cfg.operating_class = operating_class;
    cfg.phy_mode_id = phy_mode_id;
    cfg.channel_plan_id = channel_plan_id;

    if (ws_cfg_phy_set(cur, &cfg, 0) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_regulatory_domain_get(
    int8_t interface_id,
    uint8_t *regulatory_domain,
    uint8_t *operating_class,
    uint8_t *operating_mode)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!regulatory_domain || !operating_class || !operating_mode) {
        return -2;
    }

    ws_phy_cfg_t cfg;
    if (ws_cfg_phy_get(&cfg) < 0) {
        return -3;
    }

    *regulatory_domain = cfg.regulatory_domain;
    *operating_class = cfg.operating_class;
    *operating_mode = cfg.operating_mode;

    return 0;
}

int ws_management_regulatory_domain_validate(
    int8_t interface_id,
    uint8_t regulatory_domain,
    uint8_t operating_class,
    uint8_t operating_mode)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_phy_cfg_t cfg;
    if (ws_cfg_phy_get(&cfg) < 0) {
        return -3;
    }

    cfg.regulatory_domain = regulatory_domain;
    cfg.operating_class = operating_class;
    cfg.operating_mode = operating_mode;

    if (ws_cfg_phy_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_network_size_set(
    int8_t interface_id,
    uint8_t network_size)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_gen_cfg_t cfg;
    if (ws_cfg_network_size_get(&cfg) < 0) {
        return -3;
    }

    cfg.network_size = network_size;

    if (ws_cfg_network_size_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

int ws_management_network_size_get(
    int8_t interface_id,
    uint8_t *network_size)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!network_size) {
        return -2;
    }

    ws_gen_cfg_t cfg;
    if (ws_cfg_network_size_get(&cfg) < 0) {
        return -3;
    }

    *network_size = cfg.network_size;

    return 0;
}

int ws_management_network_size_validate(
    int8_t interface_id,
    uint8_t network_size)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_gen_cfg_t cfg;
    if (ws_cfg_network_size_get(&cfg) < 0) {
        return -3;
    }

    cfg.network_size = network_size;

    if (ws_cfg_network_size_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_channel_mask_set(
    int8_t interface_id,
    uint8_t channel_mask[32])
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    ws_fhss_cfg_t cfg_default;
    if (ws_cfg_fhss_default_set(&cfg_default) < 0) {
        return -2;
    }

    if (channel_mask) {
        memcpy(cfg.fhss_channel_mask, channel_mask, 32);
    } else {
        // Use the default
        memcpy(cfg.fhss_channel_mask, cfg_default.fhss_channel_mask, 32);
    }


    if (ws_cfg_fhss_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

int ws_management_channel_mask_get(
    int8_t interface_id,
    uint8_t *channel_mask)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!channel_mask) {
        return -2;
    }

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    memcpy(channel_mask, cfg.fhss_channel_mask, 32);

    return 0;
}

int ws_management_channel_mask_validate(
    int8_t interface_id,
    uint32_t channel_mask[8])
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    memcpy(cfg.fhss_channel_mask, channel_mask, sizeof(uint32_t) * 8);

    if (ws_cfg_fhss_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_channel_plan_set(
    int8_t interface_id,
    uint8_t uc_channel_function,
    uint8_t bc_channel_function,
    uint32_t ch0_freq, // Stack can not modify this
    uint8_t channel_spacing,// Stack can not modify this
    uint8_t number_of_channels)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur)
        return -1;
    cur->ws_info.hopping_schedule.channel_plan = 1;
    cur->ws_info.hopping_schedule.uc_channel_function = uc_channel_function;
    cur->ws_info.hopping_schedule.bc_channel_function = bc_channel_function;
    cur->ws_info.hopping_schedule.ch0_freq = ch0_freq;
    cur->ws_info.hopping_schedule.channel_spacing = channel_spacing;
    cur->ws_info.hopping_schedule.number_of_channels = number_of_channels;

    // TODO update fields to llc
    return 0;
}

int ws_management_fhss_timing_configure(
    int8_t interface_id,
    uint8_t fhss_uc_dwell_interval,
    uint32_t fhss_broadcast_interval,
    uint8_t fhss_bc_dwell_interval)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    ws_fhss_cfg_t cfg_default;
    if (ws_cfg_fhss_default_set(&cfg_default) < 0) {
        return -2;
    }

    if (fhss_uc_dwell_interval == 0) {
        cfg.fhss_uc_dwell_interval = cfg_default.fhss_uc_dwell_interval;
    } else {
        cfg.fhss_uc_dwell_interval = fhss_uc_dwell_interval;
    }

    if (fhss_broadcast_interval > 0xffffff) {
        cfg.fhss_bc_interval = cfg_default.fhss_bc_interval;
    } else if (fhss_broadcast_interval > 0) {
        cfg.fhss_bc_interval = fhss_broadcast_interval;
    }

    if (fhss_bc_dwell_interval == 0) {
        cfg.fhss_bc_dwell_interval = cfg_default.fhss_bc_dwell_interval;
    } else {
        cfg.fhss_bc_dwell_interval = fhss_bc_dwell_interval;
    }

    if (ws_cfg_fhss_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

int ws_management_fhss_unicast_channel_function_configure(
    int8_t interface_id,
    uint8_t channel_function,
    uint16_t fixed_channel,
    uint8_t dwell_interval)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    ws_fhss_cfg_t cfg_default;
    if (ws_cfg_fhss_default_set(&cfg_default) < 0) {
        return -2;
    }

    if (dwell_interval == 0) {
        cfg.fhss_uc_dwell_interval = cfg_default.fhss_uc_dwell_interval;
    } else {
        cfg.fhss_uc_dwell_interval = dwell_interval;
    }
    if (channel_function < 0xff) {
        cfg.fhss_uc_channel_function = channel_function;
    } else {
        cfg.fhss_uc_channel_function = cfg_default.fhss_uc_channel_function;
    }
    tr_info("----UC dewell time set: %dms", cfg.fhss_uc_dwell_interval); 

    if (fixed_channel < 0xffff) {
        cfg.fhss_uc_fixed_channel = fixed_channel;
    } else {
        cfg.fhss_uc_fixed_channel = cfg_default.fhss_uc_fixed_channel;
    }

    if (ws_cfg_fhss_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

int ws_management_fhss_unicast_channel_function_get(
    int8_t interface_id,
    uint8_t *channel_function,
    uint16_t *fixed_channel,
    uint8_t *dwell_interval)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!channel_function || !fixed_channel || !dwell_interval) {
        return -2;
    }

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    *dwell_interval = cfg.fhss_uc_dwell_interval;
    *channel_function = cfg.fhss_uc_channel_function;
    *fixed_channel = cfg.fhss_uc_fixed_channel;

    return 0;
}

int ws_management_fhss_unicast_channel_function_validate(
    int8_t interface_id,
    uint8_t channel_function,
    uint16_t fixed_channel,
    uint8_t dwell_interval)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    cfg.fhss_uc_dwell_interval = dwell_interval;
    cfg.fhss_uc_channel_function = channel_function;
    cfg.fhss_uc_fixed_channel = fixed_channel;

    if (ws_cfg_fhss_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_fhss_broadcast_channel_function_configure(
    int8_t interface_id,
    uint8_t channel_function,
    uint16_t fixed_channel,
    uint8_t dwell_interval,
    uint32_t broadcast_interval)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }
    ws_fhss_cfg_t cfg_default;
    if (ws_cfg_fhss_default_set(&cfg_default) < 0) {
        return -2;
    }

    if (dwell_interval == 0) {
        cfg.fhss_bc_dwell_interval = cfg_default.fhss_bc_dwell_interval;
    } else {
        cfg.fhss_bc_dwell_interval = dwell_interval;
    }

    if (broadcast_interval > 0xffffff) {
        cfg.fhss_bc_interval = cfg_default.fhss_bc_interval;
    } else if (broadcast_interval > 0) {
        cfg.fhss_bc_interval = broadcast_interval;
    }

    if (channel_function != 0xff) {
        cfg.fhss_bc_channel_function = channel_function;
    } else {
        cfg.fhss_bc_channel_function = cfg_default.fhss_bc_channel_function;
    }

    if (fixed_channel != 0xffff) {
        cfg.fhss_bc_fixed_channel = fixed_channel;
    } else {
        cfg.fhss_bc_fixed_channel = cfg_default.fhss_bc_fixed_channel;
    }

    if (ws_cfg_fhss_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

int ws_management_fhss_broadcast_channel_function_get(
    int8_t interface_id,
    uint8_t *channel_function,
    uint16_t *fixed_channel,
    uint8_t *dwell_interval,
    uint32_t *broadcast_interval)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!channel_function || !fixed_channel || !dwell_interval) {
        return -2;
    }

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    *dwell_interval = cfg.fhss_bc_dwell_interval;
    *broadcast_interval = cfg.fhss_bc_interval;
    *channel_function = cfg.fhss_bc_channel_function;
    *fixed_channel = cfg.fhss_bc_fixed_channel;

    return 0;
}

int ws_management_fhss_broadcast_channel_function_validate(
    int8_t interface_id,
    uint8_t channel_function,
    uint16_t fixed_channel,
    uint8_t dwell_interval,
    uint32_t broadcast_interval)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    cfg.fhss_bc_dwell_interval = dwell_interval;
    cfg.fhss_bc_interval = broadcast_interval;
    cfg.fhss_bc_channel_function = channel_function;
    cfg.fhss_bc_fixed_channel = fixed_channel;

    if (ws_cfg_fhss_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_management_fhss_lfn_configure(int8_t if_id,
                                     uint24_t lfn_bc_interval,
                                     uint8_t lfn_bc_sync_period)
{
    struct net_if *net_if = protocol_stack_interface_info_get_by_id(if_id);
    ws_fhss_cfg_t cfg_default;
    ws_fhss_cfg_t cfg;

    if (!net_if)
        return -1;
    if (ws_cfg_fhss_get(&cfg) < 0)
        return -2;
    if (ws_cfg_fhss_default_set(&cfg_default) < 0)
        return -2;
    cfg.lfn_bc_interval    = lfn_bc_interval    ? : cfg_default.lfn_bc_interval;
    cfg.lfn_bc_sync_period = lfn_bc_sync_period ? : cfg_default.lfn_bc_sync_period;
    if (ws_cfg_fhss_set(net_if, &cfg, 0) < 0)
        return -3;
    return 0;
}

int ws_management_timing_parameters_set(
    int8_t interface_id,
    uint16_t disc_trickle_imin,
    uint16_t disc_trickle_imax,
    uint8_t disc_trickle_k,
    uint16_t pan_timeout)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_timing_cfg_t cfg;
    if (ws_cfg_timing_get(&cfg) < 0) {
        return -2;
    }

    ws_timing_cfg_t cfg_default;
    if (ws_cfg_timing_default_set(&cfg_default) < 0) {
        return -2;
    }

    if (disc_trickle_imin > 0) {
        cfg.disc_trickle_imin = disc_trickle_imin;
    } else {
        cfg.disc_trickle_imin = cfg_default.disc_trickle_imin;
    }

    if (disc_trickle_imax > 0) {
        cfg.disc_trickle_imax = disc_trickle_imax;
    } else {
        cfg.disc_trickle_imax = cfg_default.disc_trickle_imax;;
    }

    if (disc_trickle_k > 0) {
        cfg.disc_trickle_k = disc_trickle_k;
    } else {
        cfg.disc_trickle_k = cfg_default.disc_trickle_k;;
    }

    if (pan_timeout > 0) {
        cfg.pan_timeout = pan_timeout;
    } else {
        cfg.pan_timeout = cfg_default.pan_timeout;;
    }

    if (ws_cfg_timing_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

int ws_management_timing_parameters_get(
    int8_t interface_id,
    uint16_t *disc_trickle_imin,
    uint16_t *disc_trickle_imax,
    uint8_t *disc_trickle_k,
    uint16_t *pan_timeout)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;
    if (!disc_trickle_imin || !disc_trickle_imax || !disc_trickle_k || !pan_timeout) {
        return -2;
    }

    ws_timing_cfg_t cfg;
    if (ws_cfg_timing_get(&cfg) < 0) {
        return -2;
    }

    *disc_trickle_imin = cfg.disc_trickle_imin;
    *disc_trickle_imax = cfg.disc_trickle_imax;
    *disc_trickle_k = cfg.disc_trickle_k;
    *pan_timeout = cfg.pan_timeout;

    return 0;
}

int ws_management_timing_parameters_validate(
    int8_t interface_id,
    uint16_t disc_trickle_imin,
    uint16_t disc_trickle_imax,
    uint8_t disc_trickle_k,
    uint16_t pan_timeout)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur)
        return -1;

    ws_timing_cfg_t cfg;
    if (ws_cfg_timing_get(&cfg) < 0) {
        return -2;
    }

    cfg.disc_trickle_imin = disc_trickle_imin;
    cfg.disc_trickle_imax = disc_trickle_imax;
    cfg.disc_trickle_k = disc_trickle_k;
    cfg.pan_timeout = pan_timeout;

    if (ws_cfg_timing_validate(&cfg) < 0) {
        return -4;
    }

    return 0;
}

int ws_stack_info_get(int8_t interface_id, ws_stack_info_t *info_ptr)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !info_ptr)
        return -1;
    return ws_bootstrap_stack_info_get(cur, info_ptr);
}

int ws_neighbor_info_get(
    int8_t interface_id,
    ws_neighbour_info_t *neighbor_ptr,
    uint16_t count)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur)
        return -1;
    return ws_bootstrap_neighbor_info_get(cur, neighbor_ptr, count);
}



/* new added api by jurong */
int ws_management_fhss_timing_configure_set(
    int8_t interface_id,
    uint8_t fhss_uc_dwell_interval,
    uint32_t fhss_broadcast_interval,
    uint8_t fhss_bc_dwell_interval)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (interface_id >= 0 && !cur) {
        return -1;
    }

    ws_fhss_cfg_t cfg;
    if (ws_cfg_fhss_get(&cfg) < 0) {
        return -2;
    }

    ws_fhss_cfg_t cfg_default;
    if (ws_cfg_fhss_default_set(&cfg_default) < 0) {
        return -2;
    }

    if (fhss_uc_dwell_interval == 0) {
        cfg.fhss_uc_dwell_interval = cfg_default.fhss_uc_dwell_interval;
    } else {
        cfg.fhss_uc_dwell_interval = fhss_uc_dwell_interval;
    }

    if (fhss_broadcast_interval > 0xffffff) {  
        cfg.fhss_bc_interval = cfg_default.fhss_bc_interval;
    } else if (fhss_broadcast_interval > 0) {
        cfg.fhss_bc_interval = fhss_broadcast_interval;
    }

    if (fhss_bc_dwell_interval == 0) {
        cfg.fhss_bc_dwell_interval = cfg_default.fhss_bc_dwell_interval;
    } else {
        cfg.fhss_bc_dwell_interval = fhss_bc_dwell_interval;
    }

    if (ws_cfg_fhss_set(cur, &cfg, 0) < 0) {
        return -3;
    }

    return 0;
}

//#include <ifaddrs.h>
//#include <string.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <sys/ioctl.h>
//#include <net/if.h>
//#include <linux/if.h>
//#include <linux/if_tun.h>
#include <netinet/icmp6.h>

int8_t g_local_udp_sid;
uint8_t g_test_buf[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                          0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};

// multi cast addr: ff15::810a:64d1;
uint8_t multi_cast_addr[16] = {0xff,0x15,0x00,0x00,0x00,0x00,0x00,0x00,
                               0x00,0x00,0x00,0x00,0x81,0x0a,0x64,0xd1};

int16_t multicast_hops = 20;

const uint8_t udp_body_uint[26] = {0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,
                                   0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a};

uint8_t udp_payload[1280] = {0};
uint16_t udp_body_uint_repeat_times = 1;
uint8_t udp_tail[10] = {0};

ns_address_t g_dst_udp_address;

//int recv_udp_msg(void* ptr)
//{
//    ERROR("recv udp socket msg");
//    return 0;
//}

int ws_managemnt_create_udp_socket(uint16_t port_num)
{
    struct sockaddr_in6 sockaddr = {
        .sin6_family = AF_INET6,
        .sin6_addr = IN6ADDR_ANY_INIT,
        .sin6_port = port_num,
    };
    int ret;

    //init g_dst_udp_address
    memset(&g_dst_udp_address, 0, sizeof(ns_address_t));
    g_dst_udp_address.type = ADDRESS_IPV6;

    g_local_udp_sid = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    ERROR_ON(g_local_udp_sid < 0, "%s: socket: %m", __func__);
    ret = bind(g_local_udp_sid, (struct sockaddr *) &sockaddr, sizeof(sockaddr));
    ERROR_ON(ret < 0, "%s: bind: %m", __func__);

    ret = setsockopt(g_local_udp_sid, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &multicast_hops, sizeof(multicast_hops));
    WARN_ON(ret < 0, "ipv6 multicast hops \"%s\": %m", __func__);
    
    struct ipv6_mreq mreq;
    mreq.ipv6mr_interface = 0;
    memcpy(&mreq.ipv6mr_multiaddr, multi_cast_addr, 16);

    ret = setsockopt(g_local_udp_sid, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof mreq);
    WARN_ON(ret < 0, "ipv6 join group \"%s\": %m", __func__);

    return 0;
}

int ws_managemnt_set_dst_udp_port(uint16_t dst_port)
{
    g_dst_udp_address.identifier = dst_port;

    return 0;
}

int ws_managemnt_set_udp_tail(const uint8_t *udp_tail_ptr)
{
    memcpy(udp_tail, udp_tail_ptr, 10);
    return 0;
}

int ws_managemnt_udp_set_repeat_times(uint16_t repeat_times)
{
    udp_body_uint_repeat_times = repeat_times;
    return 0;
}


int ws_managemnt_udp_sent_to(const uint8_t *dst_addr)
{
    uint8_t *ptr_payload = &udp_payload[0];
    int ret;

    for (uint8_t i = 0; i < udp_body_uint_repeat_times; i++) {
        memcpy((ptr_payload+4+(26*i)), udp_body_uint, 26);
    }

    memcpy((ptr_payload+(26*udp_body_uint_repeat_times)), udp_tail, 10);

    memcpy(g_dst_udp_address.address, dst_addr, 16);

    ret = sendto(g_local_udp_sid, ptr_payload, (26*udp_body_uint_repeat_times)+10, 0, (struct sockaddr *)&g_dst_udp_address, sizeof(struct sockaddr_in6));
    WARN_ON(ret < 0, "UDP packet sent to \"%s\": %m", __func__);

    return 0;
}

int ws_managemnt_set_multicast_addr(const uint8_t *multicast_addr)
{
    int ret;
    struct ipv6_mreq mreq;
    mreq.ipv6mr_interface = 0;
    memcpy(&mreq.ipv6mr_multiaddr, multi_cast_addr, 16);

    ret = setsockopt(g_local_udp_sid, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof mreq);
    WARN_ON(ret < 0, "set multicast address \"%s\": %m", __func__);

    return 0;
}

int ws_managemnt_set_edfe_mode(uint8_t *enable)
{
    ////int ret;

    // can not find option: SOCKET_EDFE_MODE
    //// ret = setsockopt(g_local_udp_sid, IPPROTO_IPV6, SOCKET_EDFE_MODE, (const void *)enable, 1);

    return 0;
}

int ws_test_gtk_time_settings_set(
    int8_t interface_id, 
    uint8_t revocat_lifetime_reduct, 
    uint8_t new_activation_time, 
    uint8_t new_install_req, 
    uint32_t max_mismatch)
{
    (void) revocat_lifetime_reduct;
    (void) max_mismatch;
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur) 
        return -1;

    ws_sec_timer_cfg_t cfg;
    if (ws_cfg_sec_timer_get(&cfg) < 0) {
        return -2;
    }

    if (new_activation_time > 0) {
        cfg.gtk_new_act_time = new_activation_time;
    }
    if (new_install_req > 0) {
        cfg.gtk_new_install_req = new_install_req;
    }

    if (ws_cfg_sec_timer_set(cur, &cfg, 0x00) < 0) {
        return -3;
    }

    return 0;
}


int ws_managemnt_icmpv6_set_id(uint16_t id)
{
    cmt_set_icmpv6_id(id);
    return 0;
}

int ws_managemnt_icmpv6_set_seqnum(uint16_t seqnum)
{
    cmt_set_icmpv6_seqnum(seqnum);
    return 0;
}

int ws_managemnt_icmpv6_set_tail(uint8_t* tail)
{
    cmt_set_icmpv6_tail(tail);
    return 0;
}

int ws_managemnt_icmpv6_set_repeat_times(uint16_t repeat_times)
{
    cmt_set_icmpv6_repeat_times(repeat_times);
    return 0;
}

int ws_managemnt_icmpv6_build_echo_req(int8_t interface_id,
    uint8_t *dst_addr)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);

    if (interface_id >= 0 && (!cur)) {
        return -1;
    }

    cmt_icmpv6_echo_req(cur, dst_addr);

    return 0;
}
