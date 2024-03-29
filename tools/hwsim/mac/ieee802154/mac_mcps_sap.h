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

/*
 * mac_mcps_sap.h
 *
 *  Created on: 19 Jan 2016
 *      Author: juhhei01
 */

#ifndef MAC_IEEE802_15_4_MAC_MCPS_SAP_H_
#define MAC_IEEE802_15_4_MAC_MCPS_SAP_H_

#include "stack/mac/mlme.h"
#include "stack/mac/mac_common_defines.h"

#include "mac/ieee802154/mac_data_buffer.h"

struct protocol_interface_rf_mac_setup;
struct mcps_data_req;
struct arm_phy_sap_msg;
struct mcps_purge;
struct mcps_data_req_ie_list;
struct channel_list;
struct mcps_enhanced_frame_response;
struct mac_pre_parsed_frame;

/** Address types */
typedef enum {
    MAC_FRAME_REQ, /*!< MAC upper layer data request message */
    MAC_PHY_RAW_REQ,
    MAC_PARSED_FRAME_IND, /*!< PD-SAP parsed MAC Frame */
} mac_mcps_primitiv_type;


#define MAC_PD_DATA_NORMAL_PRIORITY 0    //Normal MCPS DATA REQ
#define MAC_PD_DATA_MEDIUM_PRIORITY 1    //Indirect Data which is polled
#define MAC_PD_DATA_HIGH_PRIORITY   2    //Beacon request Beacon response
#define MAC_PD_DATA_EF_PRIORITY     3    //Expedited forwarding
#define MAC_PD_DATA_TX_IMMEDIATELY  4    //Only for packets whose transmission was interrupted by wrong channel type. E.g. unicast on broadcast channel.

#define MCPS_SAP_DATA_IND_EVENT         1
#define MCPS_SAP_DATA_CNF_EVENT         2
#define MCPS_SAP_DATA_CNF_FAIL_EVENT    3
#define MAC_MLME_EVENT_HANDLER          4
#define MAC_MCPS_INDIRECT_TIMER_CB      5
#define MAC_MLME_SCAN_CONFIRM_HANDLER   6
#define MAC_SAP_TRIG_TX                 7
#define MCPS_SAP_DATA_ACK_CNF_EVENT     8
#define MAC_CCA_THR_UPDATE              9

// Default number of CSMA-CA periods
#define MAC_DEFAULT_NUMBER_OF_CSMA_PERIODS  1
// Interval between two CCA checks
#define MAC_DEFAULT_CSMA_MULTI_CCA_INTERVAL 1000


void mac_generic_event_trig(uint8_t event_type, void *mac_ptr, bool low_latency);

/**
 * Create MAC MCPS event handler tasklet
 */
int8_t mac_mcps_sap_tasklet_init(void);

/**
 * Free MAC MCPS SAP layer pre parsed buffer
 */
void mcps_sap_pre_parsed_frame_buffer_free(mac_pre_parsed_frame_t *buf);


/**
 * Allocate MCPS SAP data request buffer frame
 *
 * \param  payload_size set 0 when MCPS-DATA-REQUSEST other wise MAC internal packet send need to set dynamic payload size!!
 */
mac_pre_build_frame_t *mcps_sap_prebuild_frame_buffer_get(uint16_t payload_size);

/**
 * Free MCPS SAP data request buffer frame
 *
 * \param  buffer pointer to allocated frame
 */
void mcps_sap_prebuild_frame_buffer_free(mac_pre_build_frame_t *buffer);

void mcps_sap_pd_req_queue_write(struct protocol_interface_rf_mac_setup *rf_mac_setup, mac_pre_build_frame_t *buffer);


/**
 * Allocate MAC MCPS SAP layer buffer for received PHY layer Data Indication
 */
mac_pre_parsed_frame_t *mcps_sap_pre_parsed_frame_buffer_get(const uint8_t *data_ptr, uint16_t frame_length);

mac_pre_parsed_frame_t *mcps_sap_pre_parsed_ack_buffer_get(struct protocol_interface_rf_mac_setup *rf_ptr, const uint8_t *data_ptr, uint16_t frame_length);

/**
 * Forward Buffer for MAC MCPS SAP layer event handler
 */
int8_t mcps_sap_pd_ind(mac_pre_parsed_frame_t *buffer);

/**
 * MAC MCPS SAP layer data confirmation event trig
 */
int8_t mcps_sap_pd_confirm(void *mac_ptr);

int8_t mcps_sap_pd_confirm_failure(void *mac_ptr);

int8_t mcps_sap_pd_ack(struct protocol_interface_rf_mac_setup *rf_ptr, struct mac_pre_parsed_frame *buffer);

void mcps_sap_data_req_handler(struct protocol_interface_rf_mac_setup *rf_mac_setup, const struct mcps_data_req *data_req);

void mcps_sap_data_req_handler_ext(struct protocol_interface_rf_mac_setup *rf_mac_setup, const struct mcps_data_req *data_req, const struct mcps_data_req_ie_list *ie_list, const channel_list_t *asynch_channel_list, mac_data_priority_e priority, uint8_t phy_mode_id);

void mac_mcps_trig_buffer_from_queue(struct protocol_interface_rf_mac_setup *rf_mac_setup);

void mac_mcps_buffer_queue_free(struct protocol_interface_rf_mac_setup *rf_mac_setup);

bool mac_is_ack_request_set(mac_pre_build_frame_t *buffer);

int mac_convert_frame_type_to_fhss(uint8_t frame_type);

void mcps_sap_trig_tx(void *mac_ptr);

uint8_t mcps_sap_purge_reg_handler(struct protocol_interface_rf_mac_setup *rf_mac_setup, const struct mcps_purge *purge_req);

int8_t mcps_pd_data_rebuild(struct protocol_interface_rf_mac_setup *rf_ptr, mac_pre_build_frame_t *buffer);

int8_t mcps_edfe_data_request(struct protocol_interface_rf_mac_setup *rf_ptr, mac_pre_build_frame_t *buffer);

int8_t mcps_generic_ack_data_request_init(struct protocol_interface_rf_mac_setup *rf_ptr, const mac_fcf_sequence_t *fcf, const uint8_t *data_ptr, const mcps_ack_data_payload_t *ack_payload);

int8_t mcps_generic_edfe_frame_init(struct protocol_interface_rf_mac_setup *rf_ptr, const mac_fcf_sequence_t *fcf, const uint8_t *data_ptr, const struct mcps_edfe_response *response);

int8_t mcps_generic_ack_build(struct protocol_interface_rf_mac_setup *rf_ptr, bool init_build);

int mcps_packet_ingress_rate_limit_by_memory(uint8_t free_heap_percentage);

uint32_t mac_mcps_sap_get_phy_timestamp(struct protocol_interface_rf_mac_setup *rf_mac_setup);

void mcps_pending_packet_counter_update_check(struct protocol_interface_rf_mac_setup *rf_mac_setup, mac_pre_build_frame_t *buffer);

void mac_cca_threshold_event_send(struct protocol_interface_rf_mac_setup *rf_mac_setup, uint8_t channel, int16_t dbm);

#endif /* MAC_IEEE802_15_4_MAC_MCPS_SAP_H_ */
