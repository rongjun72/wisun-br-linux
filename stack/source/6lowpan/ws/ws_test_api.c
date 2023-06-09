/*
 * Copyright (c) 2018-2020, Pelion and affiliates.
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
#include <stdlib.h>
#include "common/rand.h"
#include "common/log_legacy.h"
#include "stack-services/ns_list.h"
#include "stack-services/common_functions.h"
#include "stack/ws_test_api.h"
#include "stack/ws_management_api.h"
#include "stack/mac/fhss_config.h"
#include "stack/mac/mac_api.h"

#include "6lowpan/mac/mac_helper.h"
#include "nwk_interface/protocol.h"
#include "6lowpan/mac/mac_helper.h"
#include "6lowpan/ws/ws_config.h"
#include "6lowpan/ws/ws_common.h"
#include "6lowpan/ws/ws_bbr_api_internal.h"
#include "6lowpan/ws/ws_pae_controller.h"
#include "6lowpan/ws/ws_cfg_settings.h"
#include "6lowpan/ws/ws_bootstrap.h"

#define TRACE_GROUP "wste"

int ws_test_version_set(int8_t interface_id, uint8_t version)
{
    struct net_if *cur = protocol_stack_interface_info_get_by_id(interface_id);

    test_pan_version = version;
    if (cur) {
        if (!ws_info(cur)) {
            return -1;
        }
        cur->ws_info->version = version;
        if (ws_version_1_0(cur)) {
            cur->ws_info->pan_information.version = WS_FAN_VERSION_1_0;
        } else if (ws_version_1_1(cur)) {
            cur->ws_info->pan_information.version = WS_FAN_VERSION_1_1;
        }
    }
    return 0;
}

int ws_test_max_child_count_set(int8_t interface_id, uint16_t child_count)
{
    test_max_child_count_override = child_count;
    return 0;
}

int ws_test_gtk_set(int8_t interface_id, uint8_t *gtk[4])
{
    return ws_pae_controller_gtk_update(interface_id, gtk);
}

int ws_test_lgtk_set(int8_t interface_id, uint8_t *lgtk[3])
{
    return ws_pae_controller_lgtk_update(interface_id, lgtk);
}

int ws_test_active_key_set(int8_t interface_id, uint8_t index)
{
    return ws_pae_controller_active_key_update(interface_id, index);
}

int ws_test_key_lifetime_set(int8_t interface_id, uint32_t gtk_expire_offset, uint32_t lgtk_expire_offset, uint32_t pmk_lifetime, uint32_t ptk_lifetime)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !ws_info(cur)) {
        return -1;
    }

    ws_sec_timer_cfg_t cfg;
    if (ws_cfg_sec_timer_get(&cfg) < 0) {
        return -2;
    }

    if (gtk_expire_offset > 0) {
        cfg.gtk_expire_offset = gtk_expire_offset;
    }
    if (lgtk_expire_offset > 0) {
        cfg.lgtk_expire_offset = lgtk_expire_offset;
    }
    if (pmk_lifetime > 0) {
        cfg.pmk_lifetime = pmk_lifetime;
    }
    if (ptk_lifetime > 0) {
        cfg.ptk_lifetime = ptk_lifetime;
    }

    if (ws_cfg_sec_timer_set(cur, &cfg, 0x00) < 0) {
        return -3;
    }

    return 0;
}

int ws_test_gtk_time_settings_set(int8_t interface_id, uint8_t revocat_lifetime_reduct, uint8_t new_activation_time, uint8_t new_install_req, uint32_t max_mismatch)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !ws_info(cur)) {
        return -1;
    }

    ws_sec_timer_cfg_t cfg;
    if (ws_cfg_sec_timer_get(&cfg) < 0) {
        return -2;
    }

    if (revocat_lifetime_reduct > 0) {
        cfg.ffn_revocat_lifetime_reduct = revocat_lifetime_reduct;
    }
    if (new_activation_time > 0) {
        cfg.gtk_new_act_time = new_activation_time;
    }
    if (new_install_req > 0) {
        cfg.gtk_new_install_req = new_install_req;
    }
    if (max_mismatch > 0) {
        cfg.gtk_max_mismatch = max_mismatch;
    }

    if (ws_cfg_sec_timer_set(cur, &cfg, 0x00) < 0) {
        return -3;
    }

    return 0;
}

int ws_test_lgtk_time_settings_set(int8_t interface_id, uint8_t revocat_lifetime_reduct, uint8_t new_activation_time, uint8_t new_install_req, uint32_t max_mismatch)
{
    struct net_if *cur;

    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !ws_info(cur)) {
        return -1;
    }

    ws_sec_timer_cfg_t cfg;
    if (ws_cfg_sec_timer_get(&cfg) < 0) {
        return -2;
    }

    if (revocat_lifetime_reduct > 0) {
        cfg.lfn_revocat_lifetime_reduct = revocat_lifetime_reduct;
    }
    if (new_activation_time > 0) {
        cfg.lgtk_new_act_time = new_activation_time;
    }
    if (new_install_req > 0) {
        cfg.lgtk_new_install_req = new_install_req;
    }
    if (max_mismatch > 0) {
        cfg.lgtk_max_mismatch = max_mismatch;
    }

    if (ws_cfg_sec_timer_set(cur, &cfg, 0x00) < 0) {
        return -3;
    }

    return 0;
}

int ws_test_next_gtk_set(int8_t interface_id, uint8_t *gtk[4])
{
    return ws_pae_controller_next_gtk_update(interface_id, gtk);
}

int ws_test_next_lgtk_set(int8_t interface_id, uint8_t *lgtk[3])
{
    return ws_pae_controller_next_lgtk_update(interface_id, lgtk);
}

int ws_test_neighbour_temporary_lifetime_set(int8_t interface_id, uint32_t temporary_lifetime)
{
    struct net_if *cur = protocol_stack_interface_info_get_by_id(interface_id);

    if (!cur || !ws_info(cur)) {
        return -1;
    }

    ws_cfg_neighbour_temporary_lifetime_set(temporary_lifetime);
    return 0;
}

int ws_test_procedure_trigger(int8_t interface_id, ws_test_proc_e procedure, void *parameters)
{
    struct net_if *cur = NULL;;

    (void) parameters;
    if (interface_id > 0) {
        cur = protocol_stack_interface_info_get_by_id(interface_id);
        if (!cur || !ws_info(cur)) {
            return -1;
        }
    } else {
        cur = protocol_stack_interface_info_get_wisun_mesh();
        if (!cur) {
            if (procedure != PROC_AUTO_ON && procedure != PROC_AUTO_OFF) {
                return -1;
            }
        }
    }

    return ws_bootstrap_test_procedure_trigger(cur, procedure);
}

