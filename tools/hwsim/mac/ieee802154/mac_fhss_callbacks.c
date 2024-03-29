/*
 * Copyright (c) 2016-2018, 2020, Pelion and affiliates.
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
#include "common/endian.h"
#include "stack/mac/mac_api.h"
#include "stack/mac/fhss_api.h"

#include "mac/ieee802154/mac_fhss_callbacks.h"
#include "mac/ieee802154/mac_defines.h"
#include "mac/ieee802154/sw_mac_internal.h"
#include "mac/ieee802154/mac_mlme.h"
#include "mac/ieee802154/mac_mcps_sap.h"
#include "mac/rf_driver_storage.h"

uint16_t mac_read_tx_queue_sizes(const fhss_api_t *fhss_api, bool broadcast_queue)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return 0;
    }
    if (broadcast_queue == true) {
        return mac_setup->broadcast_queue_size;
    }
    return mac_setup->unicast_queue_size;
}

int mac_read_64bit_mac_address(const fhss_api_t *fhss_api, uint8_t *mac_address)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return -1;
    }
    memcpy(mac_address, mac_setup->mac64, 8);
    return 0;
}

uint32_t mac_read_phy_datarate(const fhss_api_t *fhss_api)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return 0;
    }
    uint32_t datarate = 0;
    // When channel page is set, ask data rate directly from PHY driver, otherwise use data rate configured to MAC. Ultimately, use default value instead 0.
    if (mac_setup->mac_channel_list.channel_page != CHANNEL_PAGE_UNDEFINED) {
        datarate = dev_get_phy_datarate(mac_setup->dev_driver->phy_driver, mac_setup->mac_channel_list.channel_page);
    } else if (mac_setup->datarate) {
        datarate = mac_setup->datarate;
    }
    if (!datarate) {
        datarate = 250000;
    }
    return datarate;
}

uint32_t mac_read_phy_timestamp(const fhss_api_t *fhss_api)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return 0;
    }
    uint32_t timestamp;
    mac_setup->dev_driver->phy_driver->extension(PHY_EXTENSION_GET_TIMESTAMP, (uint8_t *)&timestamp);
    return timestamp;
}

int mac_set_channel(const fhss_api_t *fhss_api, uint8_t channel_number)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return -1;
    }

    if (mac_setup->mac_ack_tx_active || (mac_setup->active_pd_data_request && (mac_setup->active_pd_data_request->asynch_request || mac_setup->timer_mac_event == MAC_TIMER_ACK))) {
        return -1;
    }

    //EDFE packet check if active tx or frame change session open for example wait data
    if (mac_setup->mac_edfe_enabled && (mac_setup->mac_edfe_tx_active || mac_setup->mac_edfe_info->state > MAC_EDFE_FRAME_CONNECTING)) {
        return -1;
    }

    return mac_mlme_rf_channel_change(mac_setup, channel_number);
}

int mac_poll_tx_queue(const fhss_api_t *fhss_api)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return -1;
    }
    mcps_sap_trig_tx(mac_setup);
    return 0;
}

int mac_broadcast_notification(const fhss_api_t *fhss_api, uint32_t broadcast_time)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return -1;
    }
    return 0;
}

int mac_get_coordinator_mac_address(const fhss_api_t *fhss_api, uint8_t *mac_address)
{
    protocol_interface_rf_mac_setup_s *mac_setup = get_sw_mac_ptr_by_fhss_api(fhss_api);
    if (!mac_setup) {
        return -1;
    }
    memcpy(mac_address, mac_setup->coord_long_address, 8);
    return 0;
}
