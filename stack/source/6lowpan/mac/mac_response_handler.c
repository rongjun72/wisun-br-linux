/*
 * Copyright (c) 2016-2020, Pelion and affiliates.
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
#include "common/log_legacy.h"
#include "stack-services/common_functions.h"
#include "service_libs/mac_neighbor_table/mac_neighbor_table.h"
#include "stack/mac/mlme.h"
#include "stack/mac/mac_mcps.h"

#include "nwk_interface/protocol_abstract.h"
#include "core/ns_address_internal.h"
#include "legacy/ns_socket.h"
#include "6lowpan/lowpan_adaptation_interface.h"
#include "6lowpan/mac/mac_helper.h"
#include "6lowpan/mac/mpx_api.h"

#include "6lowpan/mac/mac_response_handler.h"

#define TRACE_GROUP "MRsH"

static void mac_mlme_device_table_confirmation_handle(struct net_if *info_entry, mlme_get_conf_t *confirmation)
{
    if (confirmation->value_size != sizeof(mlme_device_descriptor_t)) {
        return;
    }

    mlme_device_descriptor_t *description = (mlme_device_descriptor_t *)confirmation->value_pointer;

    tr_debug("Dev stable get confirmation %x", confirmation->status);

    if (confirmation->status == MLME_SUCCESS) {
        //GET ME table by extended mac64 address
        mac_neighbor_table_entry_t *entry = mac_neighbor_table_address_discover(mac_neighbor_info(info_entry), description->ExtAddress, ADDR_802_15_4_LONG);

        if (!entry) {
            return;
        }

        if (entry->mac16 != description->ShortAddress) {
            //Refresh Short ADDRESS
            mlme_set_t set_request;
            description->ShortAddress = entry->mac16;

            //CALL MLME-SET
            set_request.attr = macDeviceTable;
            set_request.attr_index = confirmation->attr_index;
            set_request.value_pointer = description;
            set_request.value_size = confirmation->value_size;
            info_entry->mac_api->mlme_req(info_entry->mac_api, MLME_SET, &set_request);
        }

    }
}

static void mac_mlme_frame_counter_confirmation_handle(struct net_if *info_entry, mlme_get_conf_t *confirmation)
{
    if (confirmation->value_size != 4) {
        return;
    }
    uint32_t *temp_ptr = (uint32_t *)confirmation->value_pointer;
    info_entry->mac_parameters.security_frame_counter = *temp_ptr;
}

static void mac_mlme_cca_threshold_confirmation_handle(struct net_if *info_entry, mlme_get_conf_t *confirmation)
{
    if (confirmation->value_size < 1) {
        return;
    }
    info_entry->mac_parameters.cca_thr_table.number_of_channels = confirmation->value_size;
    info_entry->mac_parameters.cca_thr_table.cca_threshold_table = (int8_t *)confirmation->value_pointer;
}

static void mac_mlme_get_confirmation_handler(struct net_if *info_entry, mlme_get_conf_t *confirmation)
{

    if (!confirmation) {
        return;
    }
    switch (confirmation->attr) {
        case macDeviceTable:
            mac_mlme_device_table_confirmation_handle(info_entry, confirmation);
            break;

        case macFrameCounter:
            mac_mlme_frame_counter_confirmation_handle(info_entry, confirmation);
            break;

        case macCCAThreshold:
            mac_mlme_cca_threshold_confirmation_handle(info_entry, confirmation);
            break;

        default:

            break;

    }
}

void mcps_data_confirm_handler(const mac_api_t *api, const mcps_data_conf_t *data)
{
    struct net_if *info_entry = protocol_stack_interface_info_get_by_id(api->parent_id);
    //TODO: create buffer_t and call correct function
    //Update protocol_status
    lowpan_adaptation_interface_tx_confirm(info_entry, data);
}

void mcps_data_indication_handler(const mac_api_t *api, const mcps_data_ind_t *data_ind)
{
    struct net_if *info_entry = protocol_stack_interface_info_get_by_id(api->parent_id);
    lowpan_adaptation_interface_data_ind(info_entry, data_ind);
}

void mcps_purge_confirm_handler(const mac_api_t *api, mcps_purge_conf_t *data)
{
    (void)api;
    (void)data;
    tr_info("MCPS Data Purge confirm status %u, for handle %u", data->status, data->msduHandle);
}

void mlme_confirm_handler(const mac_api_t *api, mlme_primitive_e id, const void *data)
{
    struct net_if *info_entry = protocol_stack_interface_info_get_by_id(api->parent_id);
    if (!info_entry) {
        return;
    }
    //TODO: create buffer_t and call correct function
    switch (id) {
        case MLME_ASSOCIATE: {
            //Unsupported
            break;
        }
        case MLME_DISASSOCIATE: {
            //Unsupported
            break;
        }
        case MLME_GET: {
            mlme_get_conf_t *dat = (mlme_get_conf_t *)data;
            mac_mlme_get_confirmation_handler(info_entry, dat);
            break;
        }
        case MLME_GTS: {
            //Unsupported
            break;
        }
        case MLME_RESET: {
//            mlme_reset_conf_t *dat = (mlme_reset_conf_t*)data;
            break;
        }
        case MLME_RX_ENABLE: {
            //Unsupported
            break;
        }
        case MLME_SCAN: {
            break;
        }
        case MLME_SET: {
//            mlme_set_conf_t *dat = (mlme_set_conf_t*)data;
            break;
        }
        case MLME_START: {
//            mlme_start_conf_t *dat = (mlme_start_conf_t*)data;
            break;
        }
        case MLME_POLL:
        case MLME_ORPHAN:
        case MLME_COMM_STATUS:
        case MLME_SYNC:
        case MLME_SYNC_LOSS:
        default: {
            tr_error("Invalid state in mlme_confirm_handler(): %d", id);
            break;
        }
    }
}

void mlme_indication_handler(const mac_api_t *api, mlme_primitive_e id, const void *data)
{
    switch (id) {
        case MLME_ASSOCIATE: {
            //Unsupported
            //mlme_associate_ind_t *dat = (mlme_associate_ind_t*)data;
            break;
        }
        case MLME_DISASSOCIATE: {
            //Unsupported
            //mlme_disassociate_ind_t *dat = (mlme_disassociate_ind_t*)data;
            break;
        }
        case MLME_GTS: {
            //Unsupported
            break;
        }
        case MLME_ORPHAN: {
            //Unsupported
            break;
        }
        case MLME_COMM_STATUS: {
            //Unsupported
            // mlme_comm_status_t *dat = (mlme_comm_status_t *)data;
            break;
        }
        case MLME_SYNC_LOSS:
        case MLME_GET:
        case MLME_RESET:
        case MLME_RX_ENABLE:
        case MLME_SCAN:
        case MLME_SET:
        case MLME_START:
        case MLME_SYNC:
        case MLME_POLL:
        default: {
            tr_error("Invalid state in mlme_indication_handler(): %d", id);
            break;
        }
    }
}

