/*
 * Copyright (c) 2018-2019, 2021, Pelion and affiliates.
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
#include "common/log_legacy.h"
#include "stack/ws_management_api.h"

#include "nwk_interface/protocol.h"
#include "6lowpan/ws/ws_common.h"

#include "6lowpan/ws/ws_stats.h"

#define TRACE_GROUP "wsst"


int ws_statistics_start(int8_t interface_id, ws_statistics_t *stats_ptr)
{
    if (!stats_ptr) {
        return -1;
    }
    struct net_if *cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur)
        return -1;
    cur->ws_info.stored_stats_ptr = stats_ptr;
    return 0;
}

int ws_statistics_stop(int8_t interface_id)
{
    struct net_if *cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur)
        return -1;
    cur->ws_info.stored_stats_ptr = NULL;
    return 0;
}

void ws_stats_update(struct net_if *cur, ws_stats_type_e type, uint32_t update_val)
{
    if (!cur || !cur->ws_info.stored_stats_ptr)
        return;
    ws_statistics_t *stored_stats = cur->ws_info.stored_stats_ptr;

    switch (type) {
        case STATS_WS_ASYNCH_TX:
            stored_stats->asynch_tx_count += update_val;
            break;
        case STATS_WS_ASYNCH_RX:
            stored_stats->asynch_rx_count += update_val;
            break;
        case STATS_WS_ASYNCH_TX_PAS:
            stored_stats->sent_PAS += update_val;
            break;
        case STATS_WS_ASYNCH_TX_PA:
            stored_stats->sent_PA += update_val;
            break;
        case STATS_WS_ASYNCH_TX_PCS:
            stored_stats->sent_PCS += update_val;
            break;
        case STATS_WS_ASYNCH_TX_PC:
            stored_stats->sent_PC += update_val;
            break;
        case STATS_WS_ASYNCH_RX_PAS:
            stored_stats->recv_PAS += update_val;
            break;
        case STATS_WS_ASYNCH_RX_PA:
            stored_stats->recv_PA += update_val;
            break;
        case STATS_WS_ASYNCH_RX_PCS:
            stored_stats->recv_PCS += update_val;
            break;
        case STATS_WS_ASYNCH_RX_PC:
            stored_stats->recv_PC += update_val;
            break;
        case STATS_WS_STATE_1:
            stored_stats->join_state_1 += update_val;
            break;
        case STATS_WS_STATE_2:
            stored_stats->join_state_2 += update_val;
            break;
        case STATS_WS_STATE_3:
            stored_stats->join_state_3 += update_val;
            break;
        case STATS_WS_STATE_4:
            stored_stats->join_state_4 += update_val;
            break;
        case STATS_WS_STATE_5:
            stored_stats->join_state_5 += update_val;
            break;
        case STATS_WS_NEIGHBOUR_ADD:
            stored_stats->Neighbour_add += update_val;
            break;
        case STATS_WS_NEIGHBOUR_REMOVE:
            stored_stats->Neighbour_remove += update_val;
            break;
        case STATS_WS_CHILD_ADD:
            stored_stats->Child_add += update_val;
            break;
        case STATS_WS_CHILD_REMOVE:
            stored_stats->child_remove += update_val;
            break;
    }
}
