/*
 * Copyright (c) 2014-2019, Pelion and affiliates.
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

#include <stdint.h>
#include <string.h>
#include "stack/net_rpl.h"

#include "core/ns_address_internal.h"
#include "nwk_interface/protocol.h"
#include "6lowpan/bootstraps/protocol_6lowpan.h"
#include "rpl/rpl_protocol.h"
#include "rpl/rpl_control.h"
#include "rpl/rpl_data.h"

/**
 * \file net_rpl.c
 * \brief Router and Border Router RPL API.
 *
 * Rough backwards compatibility being roughly preserved with old RPL system
 */

int8_t arm_nwk_6lowpan_rpl_dodag_init(int8_t interface_id, const uint8_t *dodag_id, const dodag_config_t *config, uint8_t instance_id, uint8_t flags)
{
    (void)interface_id;

#ifdef HAVE_RPL_ROOT
    if (protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }
    rpl_dodag_conf_t new_conf;
    new_conf.default_lifetime = config->LIFE_IN_SECONDS;
    new_conf.lifetime_unit = config->LIFETIME_UNIT;
    new_conf.dag_max_rank_increase = config->DAG_MAX_RANK_INC;
    new_conf.min_hop_rank_increase = config->DAG_MIN_HOP_RANK_INC;
    new_conf.dio_interval_min = config->DAG_DIO_INT_MIN;
    new_conf.dio_interval_doublings = config->DAG_DIO_INT_DOUB;
    new_conf.dio_redundancy_constant = config->DAG_DIO_REDU;
    new_conf.objective_code_point = config->DAG_OCP;
    new_conf.authentication = config->DAG_SEC_PCS & 8;
    new_conf.path_control_size = config->DAG_SEC_PCS & 7;
    rpl_data_init_root();
    protocol_6lowpan_rpl_root_dodag = rpl_control_create_dodag_root(protocol_6lowpan_rpl_domain, instance_id, dodag_id, &new_conf, new_conf.min_hop_rank_increase, flags);
    if (!protocol_6lowpan_rpl_root_dodag) {
        return -2;
    }

    /* Something of a hack - if we're root of a non-storing DODAG, disable hard
     * memory limits, and relax soft memory limit. Soft memory limit doesn't
     * really matter if we're just a border router - we won't have anything
     * freeable to keep under it anyway - but if there's another DODAG we may
     * find ourselves struggling to join it if RPL memory is full of our own
     * root storage.
     */
    if ((flags & RPL_MODE_MASK) == RPL_MODE_NON_STORING) {
        rpl_control_set_memory_limits(64 * 1024, 0);
    }
    return 0;
#else // !HAVE_RPL_ROOT
    (void)dodag_id;
    (void)config;
    (void)instance_id;
    (void)flags;

    return -1;
#endif
}

int8_t arm_nwk_6lowpan_rpl_memory_limit_set(size_t soft_limit, size_t hard_limit)
{
    if (hard_limit != 0 && soft_limit > hard_limit) {
        return -1;
    }

    rpl_control_set_memory_limits(soft_limit, hard_limit);
    return 0;
}

int8_t arm_nwk_6lowpan_rpl_dodag_remove(int8_t interface_id)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !cur->rpl_domain) {
        return -1;
    }
    if (!protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }
    rpl_control_delete_dodag_root(cur->rpl_domain, protocol_6lowpan_rpl_root_dodag);
    return -1;
}

int8_t arm_nwk_6lowpan_rpl_dodag_start(int8_t interface_id)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !cur->rpl_domain) {
        return -1;
    }
    rpl_control_force_leaf(cur->rpl_domain, false);
    return 0;
}

int8_t arm_nwk_6lowpan_rpl_dodag_route_update(int8_t interface_id, uint8_t *route_ptr, uint8_t prefix_len, uint8_t flags, uint32_t lifetime)
{
    (void)interface_id;

    if (prefix_len && !route_ptr) {
        return -2;
    }
    if (!protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }
    rpl_control_update_dodag_route(protocol_6lowpan_rpl_root_dodag, route_ptr, prefix_len, flags, lifetime, false);
    return 0;
}

int8_t arm_nwk_6lowpan_rpl_dodag_prefix_update(int8_t interface_id, uint8_t *prefix_ptr, uint8_t prefix_len, uint8_t flags, uint32_t lifetime)
{
    (void)interface_id;

    if (prefix_len && !prefix_ptr) {
        return -2;
    }
    if (!protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }
    rpl_control_update_dodag_prefix(protocol_6lowpan_rpl_root_dodag, prefix_ptr, prefix_len, flags, lifetime, lifetime, false);
    return 0;
}


int8_t arm_nwk_6lowpan_rpl_dodag_poison(int8_t interface_id)
{
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !cur->rpl_domain) {
        return -1;
    }
    rpl_control_poison(cur->rpl_domain, 3 /* arbitrary poison count */);
    /* Make sure we send no more adverts after poison */
    rpl_control_force_leaf(cur->rpl_domain, true);
    return 0;
}

int8_t arm_nwk_6lowpan_rpl_dodag_dao_trig(int8_t interface_id)
{
    /* New code version - specifying interface ID makes no sense - fudge to let it increase main RPL root */
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !cur->rpl_domain || cur->rpl_domain != protocol_6lowpan_rpl_domain || !protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }

    rpl_control_increment_dtsn(protocol_6lowpan_rpl_root_dodag);

    return 0;
}

int8_t arm_nwk_6lowpan_rpl_dodag_version_increment(int8_t interface_id)
{
    /* New code version - specifying interface ID makes no sense - fudge to let it increase main RPL root */
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !cur->rpl_domain || cur->rpl_domain != protocol_6lowpan_rpl_domain || !protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }

    rpl_control_increment_dodag_version(protocol_6lowpan_rpl_root_dodag);

    return 0;
}

int8_t arm_nwk_6lowpan_rpl_dodag_pref_set(int8_t interface_id, uint8_t preference)
{
    /* New code version - specifying interface ID makes no sense - fudge to let it increase main RPL root */
    struct net_if *cur;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur || !cur->rpl_domain || cur->rpl_domain != protocol_6lowpan_rpl_domain || !protocol_6lowpan_rpl_root_dodag) {
        return -1;
    }

    if (preference > RPL_DODAG_PREF_MASK) {
        return -2;
    }

    rpl_control_set_dodag_pref(protocol_6lowpan_rpl_root_dodag, preference);

    return 0;
}
