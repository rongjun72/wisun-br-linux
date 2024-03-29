/*
 * Copyright (c) 2016-2021, Pelion and affiliates.
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
#include <stdlib.h>
#include "common/endian.h"
#include "common/log_legacy.h"
#include "stack/mac/mac_api.h"
#include "stack/mac/fhss_api.h"
#include "stack/mac/sw_mac.h"

#include "mac/ieee802154/sw_mac_internal.h"
#include "mac/ieee802154/mac_mlme.h"
#include "mac/ieee802154/mac_defines.h"
#include "mac/ieee802154/mac_mcps_sap.h"
#include "mac/ieee802154/mac_pd_sap.h"
#include "mac/ieee802154/mac_security_mib.h"
#include "mac/rf_driver_storage.h"
#include "mac_fhss_callbacks.h"
#include "os_timer.h"

#define TRACE_GROUP "swm"

//TODO: create linked list of created MACs

typedef struct mac_internal {
    mac_api_t *mac_api;
    arm_device_driver_list_s *dev_driver;
    //Move define inside MAC (now in protocol_abstract.h)
    struct protocol_interface_rf_mac_setup *setup;
    uint8_t device_table_size;
    uint8_t key_description_size;
    //linked list link
} mac_internal_t;

static mac_internal_t mac_store; //Hack only at this point, later put into linked list

static int8_t ns_sw_mac_initialize(mac_api_t *api, mcps_data_confirm *mcps_data_conf_cb,
                                   mcps_data_indication *mcps_data_ind_cb, mcps_purge_confirm *purge_conf_cb,
                                   mlme_confirm *mlme_conf_callback, mlme_indication *mlme_ind_callback, int8_t parent_id);
static int8_t ns_sw_mac_api_enable_mcps_ext(mac_api_t *api, mcps_data_indication_ext *data_ind_cb, mcps_data_confirm_ext *data_cnf_cb, mcps_ack_data_req_ext *ack_data_req_cb);
static int8_t ns_sw_mac_api_enable_edfe_ext(mac_api_t *api, mcps_edfe_handler *edfe_ind_cb);

static int8_t mlme_req(const mac_api_t *api, mlme_primitive_e id, const void *data);
static void mcps_req(const mac_api_t *api, const mcps_data_req_t *data);
static void mcps_req_ext(const mac_api_t *api, const mcps_data_req_t *data, const mcps_data_req_ie_list_t *ie_ext, const channel_list_t *asynch_channel_list,  mac_data_priority_e priority, uint8_t phy_mode_id);
static uint8_t purge_req(const mac_api_t *api, const mcps_purge_t *data);
static int8_t macext_mac64_address_set(const mac_api_t *api, const uint8_t *mac64);
static int8_t macext_mac64_address_get(const mac_api_t *api, mac_extended_address_type_e type, uint8_t *mac64_buf);

static int8_t sw_mac_net_phy_rx(const uint8_t *data_ptr, uint16_t data_len, uint8_t link_quality, int8_t dbm, int8_t driver_id);
static int8_t sw_mac_net_phy_tx_done(int8_t driver_id, uint8_t tx_handle, phy_link_tx_status_e status, uint8_t cca_retry, uint8_t tx_retry);
static int8_t sw_mac_storage_description_sizes_get(const mac_api_t *api, mac_description_storage_size_t *buffer);


static int8_t sw_mac_storage_description_sizes_get(const mac_api_t *api, mac_description_storage_size_t *buffer)
{
    if (!api || !buffer || api != mac_store.mac_api) {
        return -1;
    }

    buffer->device_description_table_size = mac_store.setup->device_description_table_size;
    buffer->key_description_table_size = mac_store.setup->key_description_table_size;
    buffer->key_lookup_size = mac_store.setup->key_lookup_list_size;
    buffer->key_usage_size = mac_store.setup->key_usage_list_size;
    return 0;
}

mac_api_t *ns_sw_mac_create(int8_t rf_driver_id, mac_description_storage_size_t *storage_sizes)
{
    //TODO: Refactor this away, Drivers should be stored in MAC layer in future
    if (!storage_sizes || !storage_sizes->device_description_table_size || !storage_sizes->key_description_table_size || !storage_sizes->key_lookup_size || !storage_sizes->key_usage_size) {
        return NULL;
    }

    arm_device_driver_list_s *driver = arm_net_phy_driver_pointer(rf_driver_id);

    if (!driver || !driver->phy_driver) {
        return NULL;
    }
    mac_api_t *this = malloc(sizeof(mac_api_t));
    if (!this) {
        return NULL;
    }
    memset(this, 0, sizeof(mac_api_t));
    this->parent_id = -1;
    mac_store.dev_driver = driver;

    // Set default MTU size to 127 unless it is too much for PHY driver
    if (driver->phy_driver->phy_MTU > MAC_IEEE_802_15_4_MAX_PHY_PACKET_SIZE) {
        this->mtu = MAC_IEEE_802_15_4_MAX_PHY_PACKET_SIZE;
    } else {
        this->mtu = driver->phy_driver->phy_MTU;
    }

    mac_store.setup = mac_mlme_data_base_allocate(mac_store.dev_driver->phy_driver->PHY_MAC, mac_store.dev_driver, storage_sizes, this->mtu);

    if (!mac_store.setup) {
        free(this);
        return NULL;
    }

    // Set MAC mode to PHY driver
    mac_store.setup->current_mac_mode = IEEE_802_15_4_2011;
    if (mac_store.setup->dev_driver->phy_driver->extension) {
        mac_store.setup->dev_driver->phy_driver->extension(PHY_EXTENSION_SET_802_15_4_MODE, (uint8_t *) &mac_store.setup->current_mac_mode);
    }
    tr_debug("Set MAC mode to %s, MTU size: %u", "IEEE 802.15.4-2011", mac_store.setup->phy_mtu_size);

    arm_net_phy_init(driver->phy_driver, &sw_mac_net_phy_rx, &sw_mac_net_phy_tx_done);

    this->mac_initialize = &ns_sw_mac_initialize;
    this->mac_mcps_extension_enable = &ns_sw_mac_api_enable_mcps_ext;
    this->mac_mcps_edfe_enable = &ns_sw_mac_api_enable_edfe_ext;
    this->mlme_req = &mlme_req;
    this->mcps_data_req = &mcps_req;
    this->mcps_data_req_ext = &mcps_req_ext;
    this->mcps_purge_req = &purge_req;
    this->mac64_get = &macext_mac64_address_get;
    this->mac64_set = &macext_mac64_address_set;
    this->mac_storage_sizes_get = &sw_mac_storage_description_sizes_get;

    mac_store.mac_api = this;
    return this;
}

int8_t ns_sw_mac_enable_frame_counter_per_key(struct mac_api *mac_api_s, bool enable_feature)
{
    if (!mac_api_s || mac_api_s != mac_store.mac_api) {
        return -1;
    }
    return mac_sec_mib_frame_counter_per_key_set(mac_store.setup, enable_feature);
}

int ns_sw_mac_fhss_register(mac_api_t *mac_api, fhss_api_t *fhss_api)
{
    if (!mac_api || !fhss_api) {
        return -1;
    }
    // Get a pointer to MAC setup structure
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_mac_api(mac_api);
    if (!mac_setup) {
        return -2;
    }

    if (!mac_setup->rf_csma_extension_supported) {
        return -2;
    }

    // Assign FHSS API
    mac_setup->fhss_api = fhss_api;
    // Pass MAC functions to FHSS
    fhss_callback_t callbacks;
    callbacks.read_tx_queue_size = &mac_read_tx_queue_sizes;
    callbacks.read_datarate = &mac_read_phy_datarate;
    callbacks.read_timestamp = &mac_read_phy_timestamp;
    callbacks.read_mac_address = &mac_read_64bit_mac_address;
    callbacks.change_channel = &mac_set_channel;
    callbacks.tx_poll = &mac_poll_tx_queue;
    callbacks.broadcast_notify = &mac_broadcast_notification;
    callbacks.read_coord_mac_address = &mac_get_coordinator_mac_address;
    mac_setup->fhss_api->init_callbacks(mac_setup->fhss_api, &callbacks);
    return 0;
}

int ns_sw_mac_fhss_unregister(mac_api_t *mac_api)
{
    if (!mac_api) {
        return -1;
    }
    // Get a pointer to MAC setup structure
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_mac_api(mac_api);
    if (!mac_setup) {
        return -1;
    }
    mac_setup->fhss_api = NULL;
    return 0;
}

struct fhss_api *ns_sw_mac_get_fhss_api(struct mac_api *mac_api)
{
    if (!mac_api) {
        return NULL;
    }
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_mac_api(mac_api);
    if (!mac_setup) {
        return NULL;
    }
    return mac_setup->fhss_api;
}

int ns_sw_mac_statistics_start(struct mac_api *mac_api, struct mac_statistics *mac_statistics)
{
    if (!mac_api || !mac_statistics) {
        return -1;
    }
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_mac_api(mac_api);
    if (!mac_setup) {
        return -1;
    }
    mac_setup->mac_statistics = mac_statistics;
    return 0;
}

static int8_t ns_sw_mac_initialize(mac_api_t *api, mcps_data_confirm *mcps_data_conf_cb,
                                   mcps_data_indication *mcps_data_ind_cb, mcps_purge_confirm *mcps_purge_conf_cb,
                                   mlme_confirm *mlme_conf_callback, mlme_indication *mlme_ind_callback, int8_t parent_id)
{
    //TODO: Find from linked list instead
    if (api != mac_store.mac_api) {
        return -1;
    }
    mac_api_t *cur = mac_store.mac_api;
    //TODO: Check validity of callbacks
    cur->data_conf_cb = mcps_data_conf_cb;
    cur->data_ind_cb = mcps_data_ind_cb;
    cur->purge_conf_cb = mcps_purge_conf_cb;
    cur->mlme_conf_cb = mlme_conf_callback;
    cur->mlme_ind_cb = mlme_ind_callback;
    cur->parent_id = parent_id;

    mac_store.setup->mac_interface_id = parent_id;

    return 0;
}

static int8_t ns_sw_mac_api_enable_mcps_ext(mac_api_t *api, mcps_data_indication_ext *data_ind_cb, mcps_data_confirm_ext *data_cnf_cb, mcps_ack_data_req_ext *ack_data_req_cb)
{
    //TODO: Find from linked list instead
    if (api != mac_store.mac_api) {
        return -1;
    }

    mac_api_t *cur = mac_store.mac_api;

    if (!mac_store.setup->rf_csma_extension_supported) {
        return -1;
    }

    cur->data_conf_ext_cb = data_cnf_cb;
    cur->data_ind_ext_cb = data_ind_cb;
    cur->enhanced_ack_data_req_cb = ack_data_req_cb;
    if (data_cnf_cb && data_ind_cb && ack_data_req_cb) {
        arm_device_driver_list_s *dev_driver = mac_store.setup->dev_driver;
        free(mac_store.setup->dev_driver_tx_buffer.enhanced_ack_buf);

        uint16_t total_length;
        if (ENHANCED_ACK_MAX_LENGTH > mac_store.setup->phy_mtu_size) {
            total_length = mac_store.setup->phy_mtu_size;
        } else {
            total_length = ENHANCED_ACK_MAX_LENGTH;
        }

        total_length += (dev_driver->phy_driver->phy_header_length + dev_driver->phy_driver->phy_tail_length);
        mac_store.setup->dev_driver_tx_buffer.enhanced_ack_buf = malloc(total_length);
        if (!mac_store.setup->dev_driver_tx_buffer.enhanced_ack_buf) {
            return -2;
        }

        mac_store.setup->mac_extension_enabled = true;
    } else {
        mac_store.setup->mac_extension_enabled = false;
    }
    return 0;
}

static int8_t ns_sw_mac_api_enable_edfe_ext(mac_api_t *api, mcps_edfe_handler *edfe_ind_cb)
{
    //TODO: Find from linked list instead
    if (api != mac_store.mac_api) {
        return -1;
    }

    mac_api_t *cur = mac_store.mac_api;

    if (!mac_store.setup->mac_extension_enabled) {
        return -1;
    }
    cur->edfe_ind_cb = edfe_ind_cb;
    if (edfe_ind_cb) {
        free(mac_store.setup->mac_edfe_info);
        mac_store.setup->mac_edfe_info = malloc(sizeof(mac_mcps_edfe_frame_info_t));
        if (!mac_store.setup->mac_edfe_info) {
            return -2;
        }
        mac_store.setup->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
        mac_store.setup->mac_edfe_enabled = true;
    } else {
        mac_store.setup->mac_edfe_enabled = false;
    }
    return 0;
}

mac_api_t *get_sw_mac_api(protocol_interface_rf_mac_setup_s *setup)
{
    if (!mac_store.mac_api || mac_store.mac_api->parent_id == -1 || mac_store.setup != setup) {
        return NULL;
    }
    return mac_store.mac_api;

}

protocol_interface_rf_mac_setup_s *get_sw_mac_ptr_by_mac_api(mac_api_t *api)
{
    if (mac_store.mac_api == api) {
        return mac_store.setup;
    }
    return NULL;
}

protocol_interface_rf_mac_setup_s *get_sw_mac_ptr_by_fhss_api(const fhss_api_t *api)
{
    if (mac_store.setup->fhss_api == api) {
        return mac_store.setup;
    }
    return NULL;
}

protocol_interface_rf_mac_setup_s *get_sw_mac_ptr_by_timer(int id, arm_nwk_timer_id_e type)
{

    if (!mac_store.mac_api || mac_store.mac_api->parent_id == -1) {
        return NULL;
    }
    protocol_interface_rf_mac_setup_s *rf_ptr = mac_store.setup;

    switch (type) {
        case ARM_NWK_MLME_TIMER:
            if (rf_ptr->mlme_timer_id == id) {
                return rf_ptr;
            }
            break;
        case ARM_NWK_MAC_TIMER:
            if (rf_ptr->mac_timer_id == id) {
                return rf_ptr;
            }
            break;
        case ARM_NWK_CCA_TIMER:
            if (rf_ptr->cca_timer_id == id) {
                return rf_ptr;
            }
            break;
        case ARM_NWK_BC_TIMER:
            if (rf_ptr->bc_timer_id == id) {
                return rf_ptr;
            }
            break;
        case ARM_MCPS_TIMER:
            if (rf_ptr->mac_mcps_timer == id) {
                return rf_ptr;
            }
            break;
        default: /* ARM_NWK_IFS_TIMER */
            if (rf_ptr->ifs_timer_id == id) {
                return rf_ptr;
            }
            break;
    }

    return NULL;
}

protocol_interface_rf_mac_setup_s *get_sw_mac_ptr_by_driver_id(int8_t id)
{
    if (mac_store.mac_api && mac_store.setup) {
        if (mac_store.setup->dev_driver && mac_store.setup->dev_driver->id == id) {
            return mac_store.setup;
        }
    }
    return NULL;
}

int8_t mlme_req(const mac_api_t *api, mlme_primitive_e id, const void *data)
{
    int8_t res = 0;

    if (mac_store.mac_api != api) {
        return -1;
    }
    //TODO: cast & handle
    switch (id) {
        case MLME_ASSOCIATE: {
            break;
        }
        case MLME_DISASSOCIATE: {
            break;
        }
        case MLME_GET: {
            mlme_get_conf_t get_confirm;
            const mlme_get_t *dat = (const mlme_get_t *)data;
            memset(&get_confirm, 0, sizeof(mlme_get_conf_t));
            get_confirm.attr = dat->attr;
            get_confirm.attr_index = dat->attr_index;

            if (mac_mlme_get_req(mac_store.setup, &get_confirm) == 0) {
                //call configured confirmation
                api->mlme_conf_cb(api, MLME_GET, &get_confirm);
            }

            break;
        }
        case MLME_GTS: {
            //Unsupported
            break;
        }
        case MLME_RESET: {
            const mlme_reset_t *dat = (const mlme_reset_t *)data;
            mac_mlme_reset(mac_store.setup, dat);
            break;
        }
        case MLME_RX_ENABLE: {
            break;
        }
        case MLME_SCAN: {
            break;
        }
        case MLME_SET: {
            const mlme_set_t *dat = (const mlme_set_t *)data;
            res = mac_mlme_set_req(mac_store.setup, dat);
            if (!res) {
                if (mac_store.dev_driver->mlme_observer_cb) {
                    mac_store.dev_driver->mlme_observer_cb(dat);
                }
            }
            break;
        }
        case MLME_START: {
            const mlme_start_t *dat = (mlme_start_t *)data;
            //TODO: Populate linked list when present
            mac_mlme_start_req(dat, mac_store.setup);
            break;
        }
        case MLME_SYNC: {
            break;
        }
        case MLME_POLL: {
            const mlme_poll_t *dat = (mlme_poll_t *)data;
            mac_mlme_poll_req(mac_store.setup, dat);
            break;
        }
        default:
            break;
    }
    return res;
}

static void mcps_req(const mac_api_t *api, const mcps_data_req_t *data)
{
    //TODO: Populate linked list when present
    if (mac_store.mac_api == api) {
        /* Call direct new API but without IE extensions */
        mcps_data_req_ie_list_t ie_list;
        memset(&ie_list, 0, sizeof(mcps_data_req_ie_list_t));
        mcps_sap_data_req_handler_ext(mac_store.setup, data, &ie_list, NULL, MAC_DATA_NORMAL_PRIORITY, 0);
    }
}

static void mcps_req_ext(const mac_api_t *api, const mcps_data_req_t *data, const mcps_data_req_ie_list_t *ie_ext, const channel_list_t *asynch_channel_list, mac_data_priority_e priority, uint8_t phy_mode_id)
{
//TODO: Populate linked list when present
    if (mac_store.mac_api == api) {
        mcps_sap_data_req_handler_ext(mac_store.setup, data, ie_ext, asynch_channel_list, priority, phy_mode_id);
    }
}


static uint8_t purge_req(const mac_api_t *api, const mcps_purge_t *data)
{
    if (mac_store.mac_api == api) {
        return mcps_sap_purge_reg_handler(mac_store.setup, data);
    }
    return MLME_INVALID_HANDLE;
}

static int8_t macext_mac64_address_set(const mac_api_t *api, const uint8_t *mac64)
{
    if (!mac64 || !api || mac_store.mac_api != api) {
        return -1;
    }

    mac_extended_mac_set(mac_store.setup, mac64);
    return 0;

}


static int8_t macext_mac64_address_get(const mac_api_t *api, mac_extended_address_type_e type, uint8_t *mac64_buf)
{
    if (!mac64_buf || !api || mac_store.mac_api != api) {
        return -1;
    }
    uint8_t *ptr;
    switch (type) {
        case MAC_EXTENDED_READ_ONLY:
            ptr = mac_store.setup->dev_driver->phy_driver->PHY_MAC;
            break;
        case MAC_EXTENDED_DYNAMIC:
            ptr = mac_store.setup->mac64;
            break;
        default:
            return -1;
    }
    memcpy(mac64_buf, ptr, 8);
    return 0;
}

//void mlme_assoc_resp(const mac_api_t* api, mlme_associate_resp_t *data)
//{
//    //TODO:
//}

static int8_t sw_mac_net_phy_rx(const uint8_t *data_ptr, uint16_t data_len, uint8_t link_quality, int8_t dbm, int8_t driver_id)
{
    arm_phy_sap_msg_t phy_msg;
    arm_device_driver_list_s *driver = arm_net_phy_driver_pointer(driver_id);
    if (!driver || driver != mac_store.dev_driver) {
        return -1;
    }

    //TODO: event might need mac_api if we ever decide to support more than 1 ethernet MAC!

    phy_msg.message.generic_data_ind.data_len = data_len;
    phy_msg.message.generic_data_ind.data_ptr = data_ptr;
    phy_msg.message.generic_data_ind.dbm = dbm;
    phy_msg.message.generic_data_ind.link_quality = link_quality;

    phy_msg.id = MAC15_4_PD_SAP_DATA_IND;
    mac_pd_sap_data_cb(driver->phy_sap_identifier, &phy_msg);

    return 0;
}

static int8_t sw_mac_net_phy_tx_done(int8_t driver_id, uint8_t tx_handle, phy_link_tx_status_e status, uint8_t cca_retry, uint8_t tx_retry)
{
    arm_phy_sap_msg_t phy_msg;
    (void)tx_handle;
    arm_device_driver_list_s *driver = arm_net_phy_driver_pointer(driver_id);
    if (!driver) {
        return -1;
    }
    phy_msg.id = MAC15_4_PD_SAP_DATA_TX_CONFIRM;
    phy_msg.message.mac15_4_pd_sap_confirm.status = status;
    phy_msg.message.mac15_4_pd_sap_confirm.cca_retry = cca_retry;
    phy_msg.message.mac15_4_pd_sap_confirm.tx_retry = tx_retry;

    return mac_pd_sap_data_cb(driver->phy_sap_identifier, &phy_msg);
}

void sw_mac_stats_update(protocol_interface_rf_mac_setup_s *setup, mac_stats_type_t type, uint32_t update_val)
{
    if (setup->mac_statistics) {
        switch (type) {
            case STAT_MAC_TX_QUEUE:
                setup->mac_statistics->mac_tx_queue_size = update_val;
                if (setup->mac_statistics->mac_tx_queue_size > setup->mac_statistics->mac_tx_queue_peak) {
                    setup->mac_statistics->mac_tx_queue_peak = setup->mac_statistics->mac_tx_queue_size;
                }
                break;
            case STAT_MAC_RX_COUNT:
                setup->mac_statistics->mac_rx_count++;
                setup->mac_statistics->mac_rx_bytes += update_val;
                break;
            case STAT_MAC_TX_COUNT:
                setup->mac_statistics->mac_tx_count++;
                setup->mac_statistics->mac_tx_bytes += update_val;
                break;
            case STAT_MAC_BC_RX_COUNT:
                setup->mac_statistics->mac_bc_rx_count++;
                break;
            case STAT_MAC_BC_TX_COUNT:
                setup->mac_statistics->mac_bc_tx_count++;
                break;
            case STAT_MAC_BEA_RX_COUNT:
                setup->mac_statistics->mac_beacon_rx_count++;
                break;
            case STAT_MAC_BEA_TX_COUNT:
                setup->mac_statistics->mac_beacon_tx_count++;
                break;
            case STAT_MAC_RX_DROP:
                setup->mac_statistics->mac_rx_drop_count++;
                break;
            case STAT_MAC_TX_FAIL:
                setup->mac_statistics->mac_tx_failed_count++;
                break;
            case STAT_MAC_TX_RETRY:
                setup->mac_statistics->mac_retry_count += update_val;
                break;
            case STAT_MAC_TX_CCA_ATT:
                setup->mac_statistics->mac_cca_attempts_count += update_val;
                break;
            case STAT_MAC_TX_CCA_FAIL:
                setup->mac_statistics->mac_failed_cca_count++;
                break;
            case STAT_MAC_TX_LATENCY:
                if (update_val > setup->mac_statistics->mac_tx_latency_max) {
                    setup->mac_statistics->mac_tx_latency_max = update_val;
                }
                break;
        }
    }
}

int ns_sw_mac_phy_statistics_start(struct mac_api *mac_api, phy_rf_statistics_t *phy_statistics)
{
    if (!mac_api || !phy_statistics) {
        return -1;
    }
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_mac_api(mac_api);
    if (!mac_setup) {
        return -1;
    }
    mac_setup->dev_driver->phy_driver->phy_rf_statistics = phy_statistics;
    return 0;
}

uint32_t ns_sw_mac_read_current_timestamp(struct mac_api *mac_api)
{
    if (!mac_api) {
        return 0;
    }

    // Get a pointer to MAC setup structure
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_mac_api(mac_api);
    if (!mac_setup) {
        return 0;
    }
    uint32_t time_stamp_buffer;
    mac_setup->dev_driver->phy_driver->extension(PHY_EXTENSION_GET_TIMESTAMP, (uint8_t *)&time_stamp_buffer);
    return time_stamp_buffer;
}
