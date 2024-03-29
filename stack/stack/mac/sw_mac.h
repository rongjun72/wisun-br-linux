/*
 * Copyright (c) 2016-2020, Pelion and affiliates.
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

/** \file sw_mac.h
 * \brief Software MAC API.
 */
#ifndef NS_SW_MAC_H
#define NS_SW_MAC_H
#include <stdint.h>
#include <stdbool.h>

struct protocol_interface_rf_mac_setup;
struct mac_api;
struct mac_description_storage_size;
struct fhss_api;
struct mac_statistics;
struct phy_rf_statistics;

/**
 * @brief Creates 802.15.4 MAC API instance which will use RF driver given
 * @param rf_driver_id RF driver id. Must be valid
 * @param storage_sizes dynamic mac storage sizes DO NOT set any values to zero !!
 * @return New MAC instance if successful, NULL otherwise
 */
struct mac_api *ns_sw_mac_create(int8_t rf_driver_id, struct mac_description_storage_size *storage_sizes);

/**
 * @brief Registers created FHSS API instance to given software MAC instance.
 * @param mac_api MAC instance.
 * @param fhss_api FHSS instance.
 * @return 0 on success, -1 on fail.
 */
int ns_sw_mac_fhss_register(struct mac_api *mac_api, struct fhss_api *fhss_api);

/**
 * @brief Unregister FHSS API instance from given software MAC instance.
 * @param mac_api MAC instance.
 * @return 0 on success, -1 on fail.
 */
int ns_sw_mac_fhss_unregister(struct mac_api *mac_api);

/**
 * @brief Request registered FHSS API instance from software MAC instance.
 * @param mac_api MAC instance.
 * @return FHSS api.
 */
struct fhss_api *ns_sw_mac_get_fhss_api(struct mac_api *mac_api);

/**
 * @brief Start collecting statistics from software MAC.
 * @param mac_api MAC instance.
 * @param mac_statistics Statistics storage.
 * @return 0 on success, -1 on fail.
 */
int ns_sw_mac_statistics_start(struct mac_api *mac_api, struct mac_statistics *mac_statistics);

/**
 * @brief Start collecting statistics from PHY driver.
 * @param mac_api MAC instance.
 * @param phy_statistics Statistics storage.
 * @return 0 on success, -1 on fail.
 */
int ns_sw_mac_phy_statistics_start(struct mac_api *mac_api, struct phy_rf_statistics *phy_statistics);

/**
 * @brief Read current timestamp.
 * @param mac_api MAC instance.
 * @return Current timestamp in us
 */
uint32_t ns_sw_mac_read_current_timestamp(struct mac_api *mac_api);

/**
 * @brief Enable or disable Frame counter per security key. SW MAC must be create before enable this feature!
 * @param mac_api MAC instance.
 * @param enable_feature True will allocate frame counter table for devices / key False will clear mode and free counter table.
 * @return 0 on success, -1 on fail.
 */
int8_t ns_sw_mac_enable_frame_counter_per_key(struct mac_api *mac_api, bool enable_feature);

#endif // NS_SW_MAC_H
