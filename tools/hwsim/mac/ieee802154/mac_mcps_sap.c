/*
 * Copyright (c) 2014-2021, Pelion and affiliates.
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
 * \file mac_mcps_sap.c
 * \brief Add short description about this file!!!
 *
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "common/endian.h"
#include "common/rand.h"
#include "common/log_legacy.h"
#include "common/events_scheduler.h"
#include "stack/mac/ccm.h"
#include "stack/mac/mlme.h"
#include "stack/mac/mac_api.h"
#include "stack/mac/fhss_api.h"
#include "stack/mac/sw_mac.h"

#include "os_timer.h"
#include "mac/rf_driver_storage.h"
#include "mac/ieee802154/sw_mac_internal.h"
#include "mac/ieee802154/mac_defines.h"
#include "mac/ieee802154/mac_timer.h"
#include "mac/ieee802154/mac_security_mib.h"
#include "mac/ieee802154/mac_mlme.h"
#include "mac/ieee802154/mac_filter.h"
#include "mac/ieee802154/mac_pd_sap.h"
#include "mac/ieee802154/mac_header_helper_functions.h"
#include "mac/ieee802154/mac_indirect_data.h"
#include "mac/ieee802154/mac_cca_threshold.h"

#include "mac/ieee802154/mac_mcps_sap.h"

#define TRACE_GROUP "mMCp"

// Used to set TX time (us) with FHSS. Must be <= 65ms.
#define MAC_TX_PROCESSING_DELAY_INITIAL 2000
// Give up on data request after given timeout (seconds)
#define DATA_REQUEST_TIMEOUT_NORMAL_PRIORITY_S  10

typedef struct neighbour_security_update {
    uint8_t address[8];
    unsigned addr_type: 2;
    uint8_t nonce_ptr[8];
    uint32_t frameCounter;
    uint8_t keyId;
} neighbour_security_update_t;

void mcps_sap_pd_req_queue_write(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer);
static mac_pre_build_frame_t *mcps_sap_pd_req_queue_read(protocol_interface_rf_mac_setup_s *rf_mac_setup, bool is_bc_queue, bool flush);
static int8_t mcps_pd_data_request(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer);
static void mcps_data_confirm_handle(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer, mac_pre_parsed_frame_t *ack_buf);
static void mac_set_active_event(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t event_type);
static void mac_clear_active_event(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t event_type);
static bool mac_read_active_event(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t event_type);
static int8_t mcps_pd_data_cca_trig(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer);
static void mac_pd_data_confirm_failure_handle(protocol_interface_rf_mac_setup_s *rf_mac_setup);

static int8_t mac_tasklet_event_handler = -1;

/**
 * Get PHY time stamp.
 *
 * \param rf_mac_setup pointer to MAC
 * \return Timestamp from PHY
 *
 */
uint32_t mac_mcps_sap_get_phy_timestamp(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    uint32_t timestamp;
    rf_mac_setup->dev_driver->phy_driver->extension(PHY_EXTENSION_GET_TIMESTAMP, (uint8_t *)&timestamp);
    return timestamp;
}

static bool mac_data_counter_too_small(uint32_t current_counter, uint32_t packet_counter)
{
    if ((current_counter - packet_counter) >= 2) {
        return true;
    }
    return false;
}

static bool mac_data_request_confirmation_finish(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer)
{
    if (!buffer->asynch_request) {
        return true;
    }

    if (mlme_scan_analyze_next_channel(&buffer->asynch_channel_list, false) > 0x00ff) {
        mac_mlme_rf_channel_change(rf_mac_setup, buffer->asynch_channel);
        return true;
    }

    return false;

}


static void mac_data_poll_radio_disable_check(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    if (rf_mac_setup->macCapRxOnIdle || rf_mac_setup->macWaitingData) {
        return;
    }

    if (!rf_mac_setup->macRfRadioTxActive) {
        mac_mlme_mac_radio_disabled(rf_mac_setup);
    }
}

static void mcps_data_confirm_cb(protocol_interface_rf_mac_setup_s *rf_mac_setup, mcps_data_conf_t *confirm, mac_pre_parsed_frame_t *ack_buf)
{
    mac_data_poll_radio_disable_check(rf_mac_setup);

    if (get_sw_mac_api(rf_mac_setup)) {
        if (rf_mac_setup->mac_extension_enabled) {
            mcps_data_conf_payload_t data_conf;
            memset(&data_conf, 0, sizeof(mcps_data_conf_payload_t));
            if (ack_buf) {
                data_conf.payloadIeList = ack_buf->payloadsIePtr;
                data_conf.payloadIeListLength = ack_buf->payloadsIeLength;
                data_conf.headerIeList = ack_buf->headerIePtr;
                data_conf.headerIeListLength = ack_buf->headerIeLength;
                data_conf.payloadLength = ack_buf->mac_payload_length;
                data_conf.payloadPtr = ack_buf->macPayloadPtr;
            }
            //Check Payload Here
            get_sw_mac_api(rf_mac_setup)->data_conf_ext_cb(get_sw_mac_api(rf_mac_setup), confirm, &data_conf);
        } else {
            get_sw_mac_api(rf_mac_setup)->data_conf_cb(get_sw_mac_api(rf_mac_setup), confirm);
        }
    }
}

void mcps_sap_data_req_handler(protocol_interface_rf_mac_setup_s *rf_mac_setup, const mcps_data_req_t *data_req)
{
    mcps_data_req_ie_list_t ie_list;
    memset(&ie_list, 0, sizeof(mcps_data_req_ie_list_t));
    mcps_sap_data_req_handler_ext(rf_mac_setup, data_req, &ie_list, NULL, MAC_DATA_NORMAL_PRIORITY, 0);
}

static bool mac_ie_vector_length_validate(struct iovec *ie_vector, uint16_t iov_length,  uint16_t *length_out)
{
    if (length_out) {
        *length_out = 0;
    }

    if (!iov_length) {
        return true;
    }

    if (iov_length != 0 && !ie_vector) {
        return false;
    }

    uint16_t msg_length = 0;
    struct iovec *msg_iov = ie_vector;
    for (uint_fast16_t i = 0; i < iov_length; i++) {
        if (msg_iov->iov_len != 0 && !msg_iov->iov_base) {
            return false;
        }
        msg_length += msg_iov->iov_len;
        if (msg_length < msg_iov->iov_len) {
            return false;
        }
        msg_iov++;
    }

    if (length_out) {
        *length_out = msg_length;
    }

    return true;

}


void mcps_sap_data_req_handler_ext(protocol_interface_rf_mac_setup_s *rf_mac_setup, const mcps_data_req_t *data_req, const mcps_data_req_ie_list_t *ie_list, const channel_list_t *asynch_channel_list, mac_data_priority_e priority, uint8_t phy_mode_id)
{
    (void) phy_mode_id;
    uint8_t status = MLME_SUCCESS;
    mac_pre_build_frame_t *buffer = NULL;

    if (rf_mac_setup->mac_edfe_enabled && data_req->ExtendedFrameExchange) {
        if (rf_mac_setup->mac_edfe_info->state != MAC_EDFE_FRAME_IDLE) {
            tr_debug("Accept only 1 active Efde Data request push");
            status = MLME_UNSUPPORTED_LEGACY;
            goto verify_status;
        }

        if (data_req->DstAddrMode != MAC_ADDR_MODE_64_BIT) {
            status = MLME_INVALID_PARAMETER;
            goto verify_status;
        }
    }

    if (!rf_mac_setup->mac_security_enabled) {
        if (data_req->Key.SecurityLevel) {
            status = MLME_UNSUPPORTED_SECURITY;
            goto verify_status;
        }
    }

    uint16_t ie_header_length = 0;
    uint16_t ie_payload_length = 0;

    if (!mac_ie_vector_length_validate(ie_list->headerIeVectorList, ie_list->headerIovLength, &ie_header_length)) {
        status = MLME_INVALID_PARAMETER;
        goto verify_status;
    }

    if (!mac_ie_vector_length_validate(ie_list->payloadIeVectorList, ie_list->payloadIovLength, &ie_payload_length)) {
        status = MLME_INVALID_PARAMETER;
        goto verify_status;
    }

    if ((ie_header_length || ie_payload_length || asynch_channel_list) && !rf_mac_setup->mac_extension_enabled) {
        //Report error when feature is not enaled yet
        status = MLME_INVALID_PARAMETER;
        goto verify_status;
    } else if (asynch_channel_list && data_req->TxAckReq) {
        //Report Asynch Message is not allowed to call with ACK requested.
        status = MLME_INVALID_PARAMETER;
        goto verify_status;
    }

    if ((data_req->msduLength + ie_header_length + ie_payload_length) > rf_mac_setup->phy_mtu_size - MAC_DATA_PACKET_MIN_HEADER_LENGTH) {
        tr_debug("packet %u, %u", data_req->msduLength, rf_mac_setup->phy_mtu_size);
        status = MLME_FRAME_TOO_LONG;
        goto verify_status;
    }
    buffer = mcps_sap_prebuild_frame_buffer_get(0);
    //tr_debug("Data Req");
    if (!buffer) {
        //Make Confirm Here
        status = MLME_TRANSACTION_OVERFLOW;
        goto verify_status;
    }

    if (!rf_mac_setup->macUpState) {
        status = MLME_TRX_OFF;
        goto verify_status;
    }

    if (asynch_channel_list) {
        //Copy Asynch data list
        buffer->asynch_channel_list = *asynch_channel_list;
        buffer->asynch_request = true;
    }

    //Set Priority level
    switch (priority) {
        case MAC_DATA_EXPEDITE_FORWARD:
            buffer->priority = MAC_PD_DATA_EF_PRIORITY;
            // Enable FHSS expedited forwarding
            if (rf_mac_setup->fhss_api) {
                rf_mac_setup->fhss_api->synch_state_set(rf_mac_setup->fhss_api, FHSS_EXPEDITED_FORWARDING, 0);
            }
            break;
        case MAC_DATA_HIGH_PRIORITY:
            buffer->priority = MAC_PD_DATA_HIGH_PRIORITY;
            break;
        case MAC_DATA_MEDIUM_PRIORITY:
            buffer->priority = MAC_PD_DATA_MEDIUM_PRIORITY;
            break;
        default:
            buffer->priority = MAC_PD_DATA_NORMAL_PRIORITY;
            break;
    }


    buffer->upper_layer_request = true;
    buffer->fcf_dsn.frametype = FC_DATA_FRAME;
    buffer->ExtendedFrameExchange = data_req->ExtendedFrameExchange;
    buffer->WaitResponse = data_req->TxAckReq;
    buffer->phy_mode_id = phy_mode_id;
    if (data_req->ExtendedFrameExchange) {
        buffer->fcf_dsn.ackRequested = false;
    } else {
        buffer->fcf_dsn.ackRequested = data_req->TxAckReq;
    }

    buffer->mac_header_length_with_security = 3;
    mac_header_security_parameter_set(&buffer->aux_header, &data_req->Key);
    buffer->security_mic_len = mac_security_mic_length_get(buffer->aux_header.securityLevel);
    if (buffer->aux_header.securityLevel) {
        buffer->fcf_dsn.frameVersion = MAC_FRAME_VERSION_2006;
        buffer->fcf_dsn.securityEnabled = true;
    }

    buffer->mac_header_length_with_security += mac_header_security_aux_header_length(buffer->aux_header.securityLevel, buffer->aux_header.KeyIdMode);

    buffer->msduHandle = data_req->msduHandle;
    buffer->fcf_dsn.DstAddrMode = data_req->DstAddrMode;
    memcpy(buffer->DstAddr, data_req->DstAddr, 8);
    buffer->DstPANId = data_req->DstPANId;
    buffer->SrcPANId = mac_mlme_get_panid(rf_mac_setup);
    buffer->fcf_dsn.SrcAddrMode = data_req->SrcAddrMode;
    buffer->fcf_dsn.framePending = data_req->PendingBit;

    if (buffer->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_NONE && !rf_mac_setup->mac_extension_enabled) {
        if (buffer->fcf_dsn.DstAddrMode == MAC_ADDR_MODE_NONE) {
            status = MLME_INVALID_ADDRESS;
            goto verify_status;
        }

        if (rf_mac_setup->shortAdressValid) {
            buffer->fcf_dsn.SrcAddrMode = MAC_ADDR_MODE_16_BIT;
        } else {
            buffer->fcf_dsn.SrcAddrMode = MAC_ADDR_MODE_64_BIT;
        }
    }

    mac_frame_src_address_set_from_interface(buffer->fcf_dsn.SrcAddrMode, rf_mac_setup, buffer->SrcAddr);

    buffer->ie_elements.headerIeVectorList = ie_list->headerIeVectorList;
    buffer->ie_elements.headerIovLength = ie_list->headerIovLength;
    buffer->ie_elements.payloadIeVectorList = ie_list->payloadIeVectorList;
    buffer->ie_elements.payloadIovLength = ie_list->payloadIovLength;
    buffer->headerIeLength = ie_header_length;
    buffer->payloadsIeLength = ie_payload_length;


    if (rf_mac_setup->mac_extension_enabled) {
        //Handle mac extension's
        buffer->fcf_dsn.frameVersion = MAC_FRAME_VERSION_2015;
        if (ie_header_length || ie_payload_length) {
            buffer->fcf_dsn.informationElementsPresets = true;
        }

        buffer->fcf_dsn.sequenceNumberSuppress = data_req->SeqNumSuppressed;
        if (buffer->fcf_dsn.sequenceNumberSuppress) {
            buffer->mac_header_length_with_security--;
        }
        /* PAN-ID compression bit enable when necessary */
        if (buffer->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_NONE && buffer->fcf_dsn.DstAddrMode == MAC_ADDR_MODE_NONE) {
            buffer->fcf_dsn.intraPan = !data_req->PanIdSuppressed;
        } else if (buffer->fcf_dsn.DstAddrMode == MAC_ADDR_MODE_NONE) {
            buffer->fcf_dsn.intraPan = data_req->PanIdSuppressed;
        } else if (buffer->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_NONE || (buffer->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_64_BIT && buffer->fcf_dsn.DstAddrMode == MAC_ADDR_MODE_64_BIT)) {
            buffer->fcf_dsn.intraPan = data_req->PanIdSuppressed;
        } else { /* two addresses, at least one address short */
            // ignore or fault panidsuppressed
            if (buffer->DstPANId == buffer->SrcPANId) {
                buffer->fcf_dsn.intraPan = true;
            }
        }
    } else {
        /* PAN-ID compression bit enable when necessary */
        if ((buffer->fcf_dsn.DstAddrMode && buffer->fcf_dsn.SrcAddrMode) && (buffer->DstPANId == buffer->SrcPANId)) {
            buffer->fcf_dsn.intraPan = true;
        }
    }

    //Check PanID presents at header
    buffer->fcf_dsn.DstPanPresents = mac_dst_panid_present(&buffer->fcf_dsn);
    buffer->fcf_dsn.SrcPanPresents = mac_src_panid_present(&buffer->fcf_dsn);
    //Calculate address length
    buffer->mac_header_length_with_security += mac_header_address_length(&buffer->fcf_dsn);
    buffer->mac_payload = data_req->msdu;
    buffer->mac_payload_length = data_req->msduLength;
    buffer->cca_request_restart_cnt = rf_mac_setup->cca_failure_restart_max;
    // Multiply number of backoffs for higher priority packets
    if (buffer->priority == MAC_PD_DATA_EF_PRIORITY) {
        buffer->cca_request_restart_cnt *= MAC_PRIORITY_EF_BACKOFF_MULTIPLIER;
    }
    buffer->tx_request_restart_cnt = rf_mac_setup->tx_failure_restart_max;
    //check that header + payload length is not bigger than MAC MTU

    buffer->request_start_time_us = mac_mcps_sap_get_phy_timestamp(rf_mac_setup);

    mcps_sap_pd_req_queue_write(rf_mac_setup, buffer);

verify_status:
    if (status != MLME_SUCCESS) {
        tr_debug("DATA REQ Fail %u", status);
        rf_mac_setup->mac_mcps_data_conf_fail.msduHandle = data_req->msduHandle;
        rf_mac_setup->mac_mcps_data_conf_fail.status = status;
        mcps_sap_prebuild_frame_buffer_free(buffer);
        if (mcps_sap_pd_confirm_failure(rf_mac_setup) != 0) {
            // event sending failed, calling handler directly
            mac_pd_data_confirm_failure_handle(rf_mac_setup);
        }
    }
}

//static void mac_data_interface_minium_security_level_get(protocol_interface_rf_mac_setup_s *rf_mac_setup, mlme_security_level_descriptor_t *security_level_compare) {
//    //TODO get securily level table and verify current packet
//    (void)rf_mac_setup;
//    //Accept all true from mac
//    security_level_compare->SecurityMinimum = 0;
//}

static void mac_security_interface_aux_ccm_nonce_set(uint8_t *noncePtr, uint8_t *mac64, uint32_t securityFrameCounter, uint8_t securityLevel)
{
    memcpy(noncePtr, mac64, 8);
    noncePtr += 8;
    noncePtr = write_be32(noncePtr, securityFrameCounter);
    *noncePtr = securityLevel;
}

/* Compare two security levels, as per 802.15.4-2011 7.4.1.1 */
/* Returns true if sec1 is at least as secure as sec2 */
//static bool mac_security_interface_security_level_compare(uint8_t sec1, uint8_t sec2)
//{
//    /* bit 2 is "encrypted" - must be as encrypted
//    bits 1-0 are "authentication level" - must be at least as high */
//    return (sec1 & 4) >= (sec2 & 4) &&
//           (sec1 & 3) >= (sec2 & 3);
//}


static uint8_t mac_data_interface_decrypt_packet(mac_pre_parsed_frame_t *b, mlme_security_t *security_params)
{
    uint8_t *key;
    mlme_key_descriptor_t *key_description;
    mlme_key_device_descriptor_t *key_device_description = NULL;
    uint8_t device_descriptor_handle;
    uint8_t openPayloadLength = 0;
    bool security_by_pass = false;
    protocol_interface_rf_mac_setup_s *rf_mac_setup = (protocol_interface_rf_mac_setup_s *)b->mac_class_ptr;
//    mlme_security_level_descriptor_t security_level_compare;


    if (!rf_mac_setup->mac_security_enabled) {
        if (security_params->SecurityLevel) {
            return MLME_UNSUPPORTED_SECURITY;
        }
        //TODO verify this with Kevin
        return MLME_UNSUPPORTED_LEGACY;
    }

//    security_level_compare.FrameType = b->fcf_dsn.frametype;
    if (b->fcf_dsn.frametype == MAC_FRAME_CMD) {
        openPayloadLength = 1;
//        security_level_compare.CommandFrameIdentifier = mcps_mac_command_frame_id_get(b);
    }

    //TODO do this proper way when security description  is implemeted

//    mac_data_interface_minium_security_level_get(rf_mac_setup, &security_level_compare);
//
//    //Validate Security Level
//    if (!mac_security_interface_security_level_compare(security_params->SecurityLevel, security_level_compare.SecurityMinimum)) {
//        //Drop packet reason rx level was less secured than requested one
//        tr_debug("RX Security level less than minimum level");
//        return MLME_IMPROPER_SECURITY_LEVEL;
//    }

    neighbour_security_update_t neighbour_validation;
    memset(&neighbour_validation, 0, sizeof(neighbour_validation));
    neighbour_validation.frameCounter = mcps_mac_security_frame_counter_read(b);

    if (neighbour_validation.frameCounter == 0xffffffff) {
        tr_debug("Max Framecounter value..Drop");
        return MLME_COUNTER_ERROR;
    }

    //READ SRC Address

    uint16_t SrcPANId = mac_header_get_src_panid(&b->fcf_dsn, mac_header_message_start_pointer(b), rf_mac_setup->pan_id);

    if (b->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_NONE && rf_mac_setup->mac_edfe_enabled && rf_mac_setup->mac_edfe_info->state > MAC_EDFE_FRAME_CONNECTING) {
        memcpy(neighbour_validation.address, rf_mac_setup->mac_edfe_info->PeerAddr, 8);
        neighbour_validation.addr_type = MAC_ADDR_MODE_64_BIT;
    } else {
        mac_header_get_src_address(&b->fcf_dsn, mac_header_message_start_pointer(b), neighbour_validation.address);
        neighbour_validation.addr_type = b->fcf_dsn.SrcAddrMode;
    }
    neighbour_validation.keyId = security_params->KeyIndex;

    // Get A Key description
    key_description =  mac_sec_key_description_get(rf_mac_setup, security_params, b->fcf_dsn.SrcAddrMode, neighbour_validation.address, SrcPANId);
    if (!key_description) {
        return MLME_UNAVAILABLE_KEY;
    }

    if (key_description->unique_key_descriptor) {
        //Discover device table by device description handle
        key_device_description = key_description->KeyDeviceList;
        device_descriptor_handle = key_device_description->DeviceDescriptorHandle;
        //Discover device descriptor by handle
        b->neigh_info = mac_sec_mib_device_description_get_attribute_index(rf_mac_setup, device_descriptor_handle);
        if (!b->neigh_info) {
            return MLME_UNSUPPORTED_SECURITY;
        }
    } else {

        if (!b->neigh_info) {
            if (SrcPANId == rf_mac_setup->pan_id && rf_mac_setup->mac_security_bypass_unknow_device &&
                    (b->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_64_BIT && security_params->SecurityLevel > AES_SECURITY_LEVEL_ENC)) {
                security_by_pass = true;//Accept by pass only from same PAN-ID
            } else {
                return MLME_UNSUPPORTED_SECURITY;
            }
        }

        device_descriptor_handle = mac_mib_device_description_attribute_get_by_descriptor(rf_mac_setup, b->neigh_info);
        key_device_description =  mac_sec_mib_key_device_description_discover_from_list(key_description, device_descriptor_handle);
    }

    if (key_device_description) {
        //validate BlackList status
        if (key_device_description->Blacklisted) {
            tr_debug("Blacklisted key for device %s", tr_eui64(b->neigh_info->ExtAddress));
            return MLME_UNAVAILABLE_KEY;
        }

        if (b->neigh_info) {
            uint32_t min_accepted_frame_counter = mac_mib_key_device_frame_counter_get(key_description, b->neigh_info, device_descriptor_handle);
            if (neighbour_validation.frameCounter < min_accepted_frame_counter) {
                tr_debug("MLME_COUNTER_ERROR");
                return MLME_COUNTER_ERROR;
            }
        }

    }

    key = key_description->Key;
    if (security_by_pass) {
        memcpy(neighbour_validation.nonce_ptr, neighbour_validation.address, 8);
    } else {
        memcpy(neighbour_validation.nonce_ptr, b->neigh_info->ExtAddress, 8);
    }

    ccm_globals_t ccm_ptr;

    if (!ccm_sec_init(&ccm_ptr, security_params->SecurityLevel, key, AES_CCM_DECRYPT, 2)) {
        return MLME_UNSUPPORTED_SECURITY;
    }

    mac_security_interface_aux_ccm_nonce_set(ccm_ptr.exp_nonce, neighbour_validation.nonce_ptr, neighbour_validation.frameCounter, security_params->SecurityLevel);

    if (ccm_ptr.mic_len) {
        // this is asuming that there is no headroom for buffers.
        ccm_ptr.adata_len = mcps_mac_header_length_from_received_frame(b) + openPayloadLength;
        //SET MIC PTR
        ccm_ptr.mic = mcps_security_mic_pointer_get(b);
        ccm_ptr.adata_ptr = mac_header_message_start_pointer(b);
    }

    ccm_ptr.data_ptr = (mcps_mac_payload_pointer_get(b) + openPayloadLength);
    ccm_ptr.data_len = b->mac_payload_length - openPayloadLength;
    if (ccm_process_run(&ccm_ptr) != 0) {
        return MLME_SECURITY_FAIL;
    }

    //Update key device and key description tables
    if (!security_by_pass) {

        mac_sec_mib_key_device_frame_counter_set(key_description, b->neigh_info, neighbour_validation.frameCounter + 1, device_descriptor_handle);

        if (!key_device_description) {
            if (!rf_mac_setup->secFrameCounterPerKey) {
                // Black list old used keys by this device
                mac_sec_mib_device_description_blacklist(rf_mac_setup, device_descriptor_handle);
            }

            key_device_description =  mac_sec_mib_key_device_description_list_update(key_description);
            if (key_device_description) {
                tr_debug("Set new device user %u for key", device_descriptor_handle);
                key_device_description->DeviceDescriptorHandle = device_descriptor_handle;
            }
        }
    }

    return MLME_SUCCESS;
}

static void mcps_comm_status_indication_generate(uint8_t status, mac_pre_parsed_frame_t *buf, mac_api_t *mac)
{
    mlme_comm_status_t comm_status;
    protocol_interface_rf_mac_setup_s *rf_ptr = buf->mac_class_ptr;
    memset(&comm_status, 0, sizeof(mlme_comm_status_t));
    comm_status.status = status;
    //Call com status
    comm_status.PANId = mac_header_get_dst_panid(&buf->fcf_dsn, mac_header_message_start_pointer(buf), rf_ptr->pan_id);
    comm_status.DstAddrMode = buf->fcf_dsn.DstAddrMode;;
    mac_header_get_dst_address(&buf->fcf_dsn, mac_header_message_start_pointer(buf), comm_status.DstAddr);
    comm_status.SrcAddrMode = buf->fcf_dsn.SrcAddrMode;
    mac_header_get_src_address(&buf->fcf_dsn, mac_header_message_start_pointer(buf), comm_status.SrcAddr);
    mac_header_security_components_read(buf, &comm_status.Key);
    mac->mlme_ind_cb(mac, MLME_COMM_STATUS, &comm_status);
}



static int8_t mac_data_interface_host_accept_data(mcps_data_ind_t *data_ind, protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    if ((data_ind->DstAddrMode == MAC_ADDR_MODE_16_BIT) && (data_ind->DstAddr[0] == 0xff && data_ind->DstAddr[1] == 0xff)) {
        return -1;
    }


    if (data_ind->msduLength) {
        if (!rf_mac_setup->macRxDataAtPoll && rf_mac_setup->macDataPollReq) {
            os_timer_stop(rf_mac_setup->mlme_timer_id);
            rf_mac_setup->macRxDataAtPoll = true;
            os_timer_start(rf_mac_setup->mlme_timer_id, 400); //20ms
            rf_mac_setup->mac_mlme_event = ARM_NWK_MAC_MLME_INDIRECT_DATA_POLL_AFTER_DATA;
            rf_mac_setup->mlme_tick_count = 0;
        }
        return 0;
    } else {
        os_timer_stop(rf_mac_setup->mlme_timer_id);
        mac_mlme_poll_process_confirm(rf_mac_setup, MLME_NO_DATA);
        return -1;
    }

}

static int8_t mac_data_sap_rx_handler(mac_pre_parsed_frame_t *buf, protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_api_t *mac)
{
    int8_t retval = -1;
    uint8_t status;

    //allocate Data ind primitiv and parse packet to that
    mcps_data_ind_t *data_ind = malloc(sizeof(mcps_data_ind_t));

    if (!data_ind) {
        goto DROP_PACKET;
    }
    memset(data_ind, 0, sizeof(mcps_data_ind_t));
    //Parse data
    data_ind->DSN = buf->fcf_dsn.DSN;
    data_ind->DSN_suppressed = buf->fcf_dsn.sequenceNumberSuppress;
    data_ind->DstAddrMode = buf->fcf_dsn.DstAddrMode;
    mac_header_get_dst_address(&buf->fcf_dsn, mac_header_message_start_pointer(buf), data_ind->DstAddr);
    data_ind->SrcAddrMode = buf->fcf_dsn.SrcAddrMode;

    mac_header_get_src_address(&buf->fcf_dsn, mac_header_message_start_pointer(buf), data_ind->SrcAddr);

    data_ind->SrcPANId = mac_header_get_src_panid(&buf->fcf_dsn, mac_header_message_start_pointer(buf), rf_mac_setup->pan_id);
    data_ind->DstPANId = mac_header_get_dst_panid(&buf->fcf_dsn, mac_header_message_start_pointer(buf), rf_mac_setup->pan_id);

    data_ind->mpduLinkQuality = buf->LQI;
    data_ind->signal_dbm = buf->dbm;
    data_ind->timestamp = buf->timestamp;
    /* Parse security part */
    mac_header_security_components_read(buf, &data_ind->Key);
    if (data_ind->SrcAddrMode == MAC_ADDR_MODE_NONE && rf_mac_setup->mac_edfe_enabled && rf_mac_setup->mac_edfe_info->state > MAC_EDFE_FRAME_CONNECTING) {
        memcpy(data_ind->SrcAddr, rf_mac_setup->mac_edfe_info->PeerAddr, 8);
        data_ind->SrcAddrMode = MAC_ADDR_MODE_64_BIT;
    }

    buf->neigh_info = mac_sec_mib_device_description_get(rf_mac_setup, data_ind->SrcAddr, data_ind->SrcAddrMode, data_ind->SrcPANId);
    if (buf->fcf_dsn.securityEnabled) {
        status = mac_data_interface_decrypt_packet(buf, &data_ind->Key);
        if (status != MLME_SUCCESS) {
            mcps_comm_status_indication_generate(status, buf, mac);
            goto DROP_PACKET;
        }
    }

    if (!mac_payload_information_elements_parse(buf)) {
        goto DROP_PACKET;
    }
    data_ind->msduLength = buf->mac_payload_length;
    data_ind->msdu_ptr = buf->macPayloadPtr;

    /* Validate Polling device */
    if (!rf_mac_setup->macCapRxOnIdle) {
        if (mac_data_interface_host_accept_data(data_ind, rf_mac_setup) != 0) {
            //tr_debug("Drop by not Accept");
            goto DROP_PACKET;
        }
    }

    if (mac) {

        if (buf->fcf_dsn.frameVersion == MAC_FRAME_VERSION_2015) {
            if (!rf_mac_setup->mac_extension_enabled) {
                goto DROP_PACKET;
            }
            mcps_data_ie_list_t ie_list;
            ie_list.payloadIeList = buf->payloadsIePtr;
            ie_list.payloadIeListLength = buf->payloadsIeLength;
            ie_list.headerIeList = buf->headerIePtr;
            ie_list.headerIeListLength = buf->headerIeLength;
            //Swap compressed address to broadcast when dst Address is elided
            if (buf->fcf_dsn.DstAddrMode == MAC_ADDR_MODE_NONE) {
                data_ind->DstAddrMode = MAC_ADDR_MODE_16_BIT;
                data_ind->DstAddr[0] = 0xff;
                data_ind->DstAddr[1] = 0xff;
            }
            mac->data_ind_ext_cb(mac, data_ind, &ie_list);

        } else {
            mac->data_ind_cb(mac, data_ind);
        }
        retval = 0;
    }

DROP_PACKET:
    free(data_ind);
    mcps_sap_pre_parsed_frame_buffer_free(buf);
    return retval;
}

static void mac_lib_res_no_data_to_req(mac_pre_parsed_frame_t *buffer, protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    mac_pre_build_frame_t *buf = mcps_sap_prebuild_frame_buffer_get(0);
    if (!buf) {
        return;
    }

    buf->fcf_dsn.SrcAddrMode = buffer->fcf_dsn.DstAddrMode;
    buf->fcf_dsn.DstAddrMode = buffer->fcf_dsn.SrcAddrMode;
    //SET PANID
    buf->SrcPANId = mac_header_get_dst_panid(&buffer->fcf_dsn, mac_header_message_start_pointer(buffer), rf_mac_setup->pan_id);
    buf->DstPANId = buf->SrcPANId;

    mac_header_get_dst_address(&buffer->fcf_dsn, mac_header_message_start_pointer(buffer), buf->SrcAddr);
    mac_header_get_src_address(&buffer->fcf_dsn, mac_header_message_start_pointer(buffer), buf->DstAddr);

    buf->fcf_dsn.securityEnabled = buffer->fcf_dsn.securityEnabled;
    buf->fcf_dsn.intraPan = true;
    buf->WaitResponse = buf->fcf_dsn.ackRequested = true;
    buf->mac_header_length_with_security = 3;
    //Check PanID presents at header
    buf->fcf_dsn.DstPanPresents = mac_dst_panid_present(&buf->fcf_dsn);
    buf->fcf_dsn.SrcPanPresents = mac_src_panid_present(&buf->fcf_dsn);

    if (buffer->fcf_dsn.securityEnabled) {
        buf->fcf_dsn.frameVersion = MAC_FRAME_VERSION_2006;
        buf->aux_header.securityLevel = rf_mac_setup->mac_auto_request.SecurityLevel;
        buf->aux_header.KeyIdMode = rf_mac_setup->mac_auto_request.KeyIdMode;
        buf->aux_header.KeyIndex = rf_mac_setup->mac_auto_request.KeyIndex;
        memcpy(buf->aux_header.Keysource, rf_mac_setup->mac_auto_request.Keysource, 8);
    }
    buf->fcf_dsn.frametype = FC_DATA_FRAME;
    buf->priority = MAC_PD_DATA_MEDIUM_PRIORITY;
    buf->mac_payload = NULL;
    buf->mac_payload_length = 0;
    buf->security_mic_len = mac_security_mic_length_get(buf->aux_header.securityLevel);
    buf->mac_header_length_with_security += mac_header_security_aux_header_length(buf->aux_header.securityLevel, buf->aux_header.KeyIdMode);
    buf->mac_header_length_with_security += mac_header_address_length(&buf->fcf_dsn);
    mcps_sap_pd_req_queue_write(rf_mac_setup, buf);
}

static int8_t mac_command_sap_rx_handler(mac_pre_parsed_frame_t *buf, protocol_interface_rf_mac_setup_s *rf_mac_setup,  mac_api_t *mac)
{
    int8_t retval = -1;
    mlme_security_t security_params;
    uint8_t mac_command;
    uint8_t status;
    uint8_t temp_src_address[8];


    //Read address and pan-id
    mac_header_get_src_address(&buf->fcf_dsn, mac_header_message_start_pointer(buf), temp_src_address);
    uint8_t address_mode = buf->fcf_dsn.SrcAddrMode;
    uint16_t pan_id = mac_header_get_src_panid(&buf->fcf_dsn, mac_header_message_start_pointer(buf), rf_mac_setup->pan_id);
    buf->neigh_info = mac_sec_mib_device_description_get(rf_mac_setup, temp_src_address, address_mode, pan_id);
    //Decrypt Packet if secured
    if (buf->fcf_dsn.securityEnabled) {
        mac_header_security_components_read(buf, &security_params);

        status = mac_data_interface_decrypt_packet(buf, &security_params);
        if (status != MLME_SUCCESS) {

            mcps_comm_status_indication_generate(status, buf, mac);
            goto DROP_PACKET;
        }
    }

    mac_command = mcps_mac_command_frame_id_get(buf);

    switch (mac_command) {
        case MAC_DATA_REQ:
            //Here 2 check
            if (mac_indirect_data_req_handle(buf, rf_mac_setup) == 0) {
                mac_lib_res_no_data_to_req(buf, rf_mac_setup);
            }
            retval = 0;
            break;
        default:
            break;
    }

DROP_PACKET:
    mcps_sap_pre_parsed_frame_buffer_free(buf);
    return retval;
}

static void mac_data_interface_frame_handler(mac_pre_parsed_frame_t *buf)
{
    protocol_interface_rf_mac_setup_s *rf_mac_setup = buf->mac_class_ptr;
    if (!rf_mac_setup) {
        mcps_sap_pre_parsed_frame_buffer_free(buf);
        return;
    }
    mac_api_t *mac = get_sw_mac_api(rf_mac_setup);
    if (!mac) {
        mcps_sap_pre_parsed_frame_buffer_free(buf);
        return;
    }
    if (buf->fcf_dsn.ackRequested == false) {
        sw_mac_stats_update(rf_mac_setup, STAT_MAC_BC_RX_COUNT, 0);
    }
    sw_mac_stats_update(rf_mac_setup, STAT_MAC_RX_COUNT, 0);
    switch (buf->fcf_dsn.frametype) {
        case MAC_FRAME_DATA:
            mac_data_sap_rx_handler(buf, rf_mac_setup, mac);
            break;
        case MAC_FRAME_CMD:
            //Handle Command Frame
            mac_command_sap_rx_handler(buf, rf_mac_setup, mac);
            break;

        case MAC_FRAME_BEACON:
            tr_error("Beacons are not supported");
            // Fall through
        default:
            mcps_sap_pre_parsed_frame_buffer_free(buf);
    }

}

static void mac_mcps_asynch_finish(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer)
{
    if (buffer->asynch_request && rf_mac_setup->fhss_api) {
        // Must return to scheduled channel after asynch process by calling TX done
        rf_mac_setup->fhss_api->data_tx_done(rf_mac_setup->fhss_api, false, true, buffer->msduHandle);
    }
}

static bool mcps_sap_check_buffer_timeout(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer)
{
    // Convert from 1us slots to seconds
    uint32_t buffer_age_s = (mac_mcps_sap_get_phy_timestamp(rf_mac_setup) - buffer->request_start_time_us) / 1000000;
    // Do not timeout broadcast frames. Broadcast interval could be very long.
    if (buffer->fcf_dsn.ackRequested && (buffer_age_s > DATA_REQUEST_TIMEOUT_NORMAL_PRIORITY_S)) {
        return true;
    }
    return false;
}

void mac_mcps_trig_buffer_from_queue(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    if (!rf_mac_setup) {
        return;
    }
    while (!rf_mac_setup->active_pd_data_request) {
        bool is_bc_queue = false;
        mac_pre_build_frame_t *buffer;
        // With FHSS, poll broadcast queue on broadcast channel first
        if (rf_mac_setup->fhss_api) {
            if (rf_mac_setup->fhss_api->is_broadcast_channel(rf_mac_setup->fhss_api) == true) {
                if (rf_mac_setup->pd_data_request_bc_queue_to_go) {
                    is_bc_queue = true;
                }
            }
        }
        buffer = mcps_sap_pd_req_queue_read(rf_mac_setup, is_bc_queue, false);

        if (buffer) {
            if (mcps_sap_check_buffer_timeout(rf_mac_setup, buffer)) {
                // Buffer is quite old. Return it to adaptation layer with timeout event.
                rf_mac_setup->mac_tx_result = MAC_TX_TIMEOUT;
                if (buffer->ExtendedFrameExchange) {
                    rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
                }
                mac_mcps_asynch_finish(rf_mac_setup, buffer);
                mcps_data_confirm_handle(rf_mac_setup, buffer, NULL);
            } else {
                if (buffer->ExtendedFrameExchange) {
                    //Update here state and store peer
                    memcpy(rf_mac_setup->mac_edfe_info->PeerAddr, buffer->DstAddr, 8);
                    rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_CONNECTING;
                }
                rf_mac_setup->active_pd_data_request = buffer;
                if (mcps_pd_data_request(rf_mac_setup, buffer) != 0) {
                    if (buffer->ExtendedFrameExchange) {
                        rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
                    }
                    rf_mac_setup->active_pd_data_request = NULL;
                    mac_mcps_asynch_finish(rf_mac_setup, buffer);
                    mcps_data_confirm_handle(rf_mac_setup, buffer, NULL);
                } else {
                    return;
                }
            }
        } else {
            return;
        }
    }
}

static int8_t mac_ack_sap_rx_handler(mac_pre_parsed_frame_t *buf, protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    //allocate Data ind primitiv and parse packet to that
    mlme_security_t key;
    uint8_t srcAddressMode;
    uint8_t SrcAddr[8];         /**< Source address */
    memset(SrcAddr, 0, 8);
    memset(&key, 0, sizeof(mlme_security_t));
    srcAddressMode = buf->fcf_dsn.SrcAddrMode;
    if (buf->fcf_dsn.SrcAddrMode) {
        mac_header_get_src_address(&buf->fcf_dsn, mac_header_message_start_pointer(buf), SrcAddr);
    } else {
        if (rf_mac_setup->mac_edfe_enabled && (buf->fcf_dsn.frametype == FC_DATA_FRAME && !buf->fcf_dsn.ackRequested)) {
            memcpy(SrcAddr, rf_mac_setup->mac_edfe_info->PeerAddr, 8);
            srcAddressMode = MAC_ADDR_MODE_64_BIT;
        }
    }
    uint16_t pan_id = mac_header_get_src_panid(&buf->fcf_dsn, mac_header_message_start_pointer(buf), rf_mac_setup->pan_id);
    /* Parse security part */
    mac_header_security_components_read(buf, &key);

    buf->neigh_info = mac_sec_mib_device_description_get(rf_mac_setup, SrcAddr, srcAddressMode, pan_id);
    if (buf->fcf_dsn.securityEnabled) {
        uint8_t status = mac_data_interface_decrypt_packet(buf, &key);
        if (status != MLME_SUCCESS) {
            rf_mac_setup->mac_tx_result = MAC_ACK_SECURITY_FAIL;
            return -1;
        }
    }

    if (buf->mac_payload_length && !mac_payload_information_elements_parse(buf)) {
        rf_mac_setup->mac_tx_result = MAC_ACK_SECURITY_FAIL;
        return -1;
    }

    return 0;
}

static void mac_pd_data_confirm_handle(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    if (rf_mac_setup->active_pd_data_request) {
        mac_pre_build_frame_t *buffer = rf_mac_setup->active_pd_data_request;
        if (mac_data_request_confirmation_finish(rf_mac_setup, buffer)) {
            rf_mac_setup->active_pd_data_request = NULL;
            mac_mcps_asynch_finish(rf_mac_setup, buffer);
            mcps_data_confirm_handle(rf_mac_setup, buffer, NULL);
        } else {
            if (mcps_pd_data_request(rf_mac_setup, buffer) != 0) {
                rf_mac_setup->active_pd_data_request = NULL;
                mac_mcps_asynch_finish(rf_mac_setup, buffer);
                mcps_data_confirm_handle(rf_mac_setup, buffer, NULL);
            } else {
                return;
            }
        }
    }

    mac_mcps_trig_buffer_from_queue(rf_mac_setup);
}

static void mac_pd_data_confirm_failure_handle(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    mcps_data_conf_t mcps_data_conf;
    memset(&mcps_data_conf, 0, sizeof(mcps_data_conf_t));
    mcps_data_conf.msduHandle = rf_mac_setup->mac_mcps_data_conf_fail.msduHandle;
    mcps_data_conf.status = rf_mac_setup->mac_mcps_data_conf_fail.status;
    mcps_data_confirm_cb(rf_mac_setup, &mcps_data_conf, NULL);
}


static void mac_pd_data_ack_handler(mac_pre_parsed_frame_t *buf)
{
    protocol_interface_rf_mac_setup_s *rf_mac_setup = buf->mac_class_ptr;

    if (!rf_mac_setup->active_pd_data_request) {
        mcps_sap_pre_parsed_frame_buffer_free(buf);
        tr_debug("RX ack without active buffer");
    } else {
        mac_pre_build_frame_t *buffer = rf_mac_setup->active_pd_data_request;

        //Validate here ack is proper to active buffer
        if (!mac_pd_sap_ack_validation(rf_mac_setup, &buf->fcf_dsn, mac_header_message_start_pointer(buf))) {
            tr_debug("Not a valid ACK for active tx process");
            mcps_sap_pre_parsed_frame_buffer_free(buf);
            return;
        }

        if (mac_ack_sap_rx_handler(buf, rf_mac_setup)) {
            //Do not forward ACK payload but Accept ACK
            mcps_sap_pre_parsed_frame_buffer_free(buf);
            buf = NULL;

        }

        rf_mac_setup->active_pd_data_request = NULL;
        mcps_data_confirm_handle(rf_mac_setup, buffer, buf);
        mcps_sap_pre_parsed_frame_buffer_free(buf);

    }

    mac_mcps_trig_buffer_from_queue(rf_mac_setup);
}


static void mac_mcps_sap_data_tasklet(struct event_payload *event)
{
    uint8_t event_type = event->event_type;

    switch (event_type) {
        case MCPS_SAP_DATA_IND_EVENT:
            if (event->data_ptr) {
                mac_data_interface_frame_handler((mac_pre_parsed_frame_t *)event->data_ptr);
            }

            break;

        case MCPS_SAP_DATA_CNF_EVENT:
            //mac_data_interface_tx_done(event->data_ptr);
            mac_pd_data_confirm_handle((protocol_interface_rf_mac_setup_s *)event->data_ptr);
            break;

        case MCPS_SAP_DATA_CNF_FAIL_EVENT:
            mac_pd_data_confirm_failure_handle((protocol_interface_rf_mac_setup_s *)event->data_ptr);
            break;

        case MCPS_SAP_DATA_ACK_CNF_EVENT:
            mac_pd_data_ack_handler((mac_pre_parsed_frame_t *)event->data_ptr);
            break;

        case MAC_MLME_EVENT_HANDLER:
            mac_mlme_event_cb(event->data_ptr);
            break;
        case MAC_MCPS_INDIRECT_TIMER_CB:
            mac_indirect_data_ttl_handle((protocol_interface_rf_mac_setup_s *)event->data_ptr, (uint16_t)event->event_data);
            break;

        case MAC_CCA_THR_UPDATE:
            mac_cca_threshold_update((protocol_interface_rf_mac_setup_s *) event->data_ptr, event->event_data >> 8, (int8_t) event->event_data);
            break;
        case MAC_SAP_TRIG_TX:
            mac_clear_active_event((protocol_interface_rf_mac_setup_s *) event->data_ptr, MAC_SAP_TRIG_TX);
            mac_mcps_trig_buffer_from_queue((protocol_interface_rf_mac_setup_s *) event->data_ptr);
        //No break necessary
        default:
            break;
    }
}

int8_t mac_mcps_sap_tasklet_init(void)
{
    if (mac_tasklet_event_handler < 0) {
        mac_tasklet_event_handler = event_handler_create(&mac_mcps_sap_data_tasklet, 0);
    }

    return mac_tasklet_event_handler;
}

mac_pre_build_frame_t *mcps_sap_prebuild_frame_buffer_get(uint16_t payload_size)
{
    mac_pre_build_frame_t *buffer = malloc(sizeof(mac_pre_build_frame_t));
    if (!buffer) {
        return NULL;
    }
    memset(buffer, 0, sizeof(mac_pre_build_frame_t));
    buffer->initial_tx_channel = 0xffff;
    buffer->aux_header.frameCounter = 0xffffffff;
    buffer->DSN_allocated = false;
    if (payload_size) {
        //Mac interlnal payload allocate
        buffer->mac_payload = malloc(payload_size);
        if (!buffer->mac_payload) {
            free(buffer);
            return NULL;
        }
        buffer->mac_allocated_payload_ptr = true;
        buffer->mac_payload_length = payload_size;
    } else {
        buffer->mac_allocated_payload_ptr = false;
    }
    return buffer;
}


void mcps_sap_prebuild_frame_buffer_free(mac_pre_build_frame_t *buffer)
{
    if (!buffer) {
        return;
    }

    if (buffer->mac_allocated_payload_ptr) {
        free(buffer->mac_payload);
    }
    //Free Buffer frame
    free(buffer);

}

static mlme_key_descriptor_t *mac_frame_security_key_get(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    /* Encrypt the packet payload if AES encyption bit is set */
    mlme_security_t key_source;
    key_source.KeyIdMode = buffer->aux_header.KeyIdMode;
    key_source.KeyIndex = buffer->aux_header.KeyIndex;
    key_source.SecurityLevel = buffer->aux_header.securityLevel;
    memcpy(key_source.Keysource, buffer->aux_header.Keysource, 8);
    return mac_sec_key_description_get(rf_ptr, &key_source, buffer->fcf_dsn.DstAddrMode, buffer->DstAddr, buffer->DstPANId);
}


static bool mac_frame_security_parameters_init(ccm_globals_t *ccm_ptr, protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer, mlme_key_descriptor_t *key_description)
{
    /* Encrypt the packet payload if AES encyption bit is set */
    mlme_device_descriptor_t *device_description;
    uint8_t *nonce_ext_64_ptr;

    if (key_description->unique_key_descriptor) {
        device_description = mac_sec_mib_device_description_get_attribute_index(rf_ptr, key_description->KeyDeviceList->DeviceDescriptorHandle);
        if (!device_description) {

            buffer->status = MLME_UNAVAILABLE_KEY;
            return false;
        }
        nonce_ext_64_ptr = device_description->ExtAddress;
    } else {
        //Discover device descriptor only unicast packet which need ack
        if (buffer->fcf_dsn.DstAddrMode && buffer->fcf_dsn.ackRequested) {
            device_description =  mac_sec_mib_device_description_get(rf_ptr, buffer->DstAddr, buffer->fcf_dsn.DstAddrMode, buffer->DstPANId);
            if (!device_description) {
                buffer->status = MLME_UNAVAILABLE_KEY;
                return false;
            }
        }
        nonce_ext_64_ptr = rf_ptr->mac64;
    }

    uint8_t *key_ptr = key_description->Key;

    //Check If frame counter overflow is coming
    if (buffer->aux_header.frameCounter == 0xffffffff) {
        buffer->status = MLME_COUNTER_ERROR;
        return false;
    }

    if (!ccm_sec_init(ccm_ptr, buffer->aux_header.securityLevel, key_ptr, AES_CCM_ENCRYPT, 2)) {
        buffer->status = MLME_SECURITY_FAIL;
        return false;
    }

    mac_security_interface_aux_ccm_nonce_set(ccm_ptr->exp_nonce, nonce_ext_64_ptr, buffer->aux_header.frameCounter,
                                             buffer->aux_header.securityLevel);
    return true;

}


static void mac_common_data_confirmation_handle(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buf)
{
    mac_event_e m_event;
    /* Raed MAC TX state */
    m_event = (mac_event_e) rf_mac_setup->mac_tx_result;
    rf_mac_setup->mac_tx_result = MAC_STATE_IDLE;

    /* Discard Tx timeout timer */
    timer_mac_stop(rf_mac_setup);
    if (m_event == MAC_CCA_FAIL) {
        sw_mac_stats_update(rf_mac_setup, STAT_MAC_TX_CCA_FAIL, 0);
        tr_info("MAC CCA fail");
        /* CCA fail */
        //rf_mac_setup->cca_failure++;
        buf->status = MLME_BUSY_CHAN;
    } else {
        sw_mac_stats_update(rf_mac_setup, STAT_MAC_TX_COUNT, buf->mac_payload_length);
        if (m_event == MAC_TX_FAIL) {
            sw_mac_stats_update(rf_mac_setup, STAT_MAC_TX_FAIL, 0);
            tr_info("MAC tx fail");
            buf->status = MLME_TX_NO_ACK;
        } else if (m_event == MAC_TX_DONE) {
            if (mac_is_ack_request_set(buf) == false) {
                sw_mac_stats_update(rf_mac_setup, STAT_MAC_BC_TX_COUNT, 0);
            }
            if (buf->fcf_dsn.frametype == FC_CMD_FRAME && buf->mac_command_id == MAC_DATA_REQ) {
                buf->status = MLME_NO_DATA;
            } else {
                buf->status = MLME_SUCCESS;
            }

        } else if (m_event == MAC_TX_DONE_PENDING) {
            buf->status = MLME_SUCCESS;
        } else if (m_event == MAC_TX_TIMEOUT) {
            buf->status = MLME_TRANSACTION_EXPIRED;
        } else if (m_event == MAC_UNKNOWN_DESTINATION) {
            buf->status = MLME_UNAVAILABLE_KEY;
        } else if (m_event == MAC_ACK_SECURITY_FAIL) {
            buf->status = MLME_TX_NO_ACK;
        }/** else if (m_event == MAC_TX_PRECOND_FAIL) {
           * Nothing to do, status already set to buf->status.
        }**/
    }
}

void mac_data_wait_timer_start(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{
    os_timer_stop(rf_mac_setup->mlme_timer_id);
    os_timer_start(rf_mac_setup->mlme_timer_id, 200); //10ms
    rf_mac_setup->mac_mlme_event = ARM_NWK_MAC_MLME_INDIRECT_DATA_POLL;
    rf_mac_setup->mlme_tick_count = 30; //300ms
}

static void mac_data_interface_internal_tx_confirm_handle(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buf)
{
    //GET Interface

    switch (buf->mac_command_id) {
        case MAC_DATA_REQ:

            if (buf->status == MLME_SUCCESS) {
                if (!rf_mac_setup->macRxDataAtPoll) {
                    //Start Timer
                    mac_data_wait_timer_start(rf_mac_setup);
                    //Set Buffer back to
                }
                rf_mac_setup->active_pd_data_request = buf;
                return;

            } else {
                //Disable Radio
                if (!rf_mac_setup->macCapRxOnIdle) {
                    mac_mlme_mac_radio_disabled(rf_mac_setup);
                }
                rf_mac_setup->macDataPollReq = false;

                mac_api_t *mac_api = get_sw_mac_api(rf_mac_setup);

                if (mac_api) {
                    mlme_poll_conf_t confirm;
                    confirm.status = buf->status;
                    mac_api->mlme_conf_cb(mac_api, MLME_POLL, &confirm);
                }

            }
            break;

        default:
            break;
    }

    mcps_sap_prebuild_frame_buffer_free(buf);

}

static bool mcps_buffer_edfe_data_failure(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    if (!rf_ptr->mac_edfe_enabled || !buffer->ExtendedFrameExchange) {
        return false;
    }

    if (rf_ptr->mac_edfe_info->state > MAC_EDFE_FRAME_CONNECTING) {
        //Set to idle
        tr_debug("Edfe Data send fail");
        return true;
    }

    return false;
}

static void mcps_set_packet_blacklist(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer, uint8_t number_of_restarts)
{
    /*
     * Random min = configured blacklist min << attempt count, but never larger than configured blacklist max / 2.
     * Random max = random min * 2, but never larger than blacklist max.
     * Example:
     * blacklist_min_ms: 20ms
     * blacklist_max_ms: 300ms
     * blacklist_retry_attempts: 4
     *
     * Packet is blacklisted:
     * 20ms - 40ms after 1st failure
     * 40ms - 80ms after 2nd failure
     * 80ms - 160ms after 3rd failure
     * 150ms - 300ms after 4th failure
     */
    uint8_t i = 0;
    uint32_t blacklist_min_ms = 0;
    while (i < number_of_restarts) {
        blacklist_min_ms = rf_ptr->blacklist_min_ms << i;
        if (blacklist_min_ms > (rf_ptr->blacklist_max_ms / 2)) {
            break;
        }
        i++;
    }
    uint32_t blacklist_max_ms = blacklist_min_ms * 2;
    if (blacklist_min_ms > (rf_ptr->blacklist_max_ms / 2)) {
        blacklist_min_ms = (rf_ptr->blacklist_max_ms / 2);
    }
    if (blacklist_max_ms > rf_ptr->blacklist_max_ms) {
        blacklist_max_ms = rf_ptr->blacklist_max_ms;
    }
    buffer->blacklist_period_ms = rand_get_random_in_range(blacklist_min_ms, blacklist_max_ms);
    if (!buffer->blacklist_period_ms) {
        buffer->blacklist_period_ms++;
    }
    buffer->blacklist_start_time_us = mac_mcps_sap_get_phy_timestamp(rf_ptr);
}

static bool mcps_update_packet_request_restart(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    // Function returns true when buffer needs to be requeued
    if (!rf_ptr || !buffer) {
        return false;
    }
    if (rf_ptr->mac_tx_result == MAC_CCA_FAIL && buffer->cca_request_restart_cnt) {
        buffer->cca_request_restart_cnt--;
        if (buffer->priority == MAC_PD_DATA_EF_PRIORITY) {
            mcps_set_packet_blacklist(rf_ptr, buffer, (rf_ptr->cca_failure_restart_max * MAC_PRIORITY_EF_BACKOFF_MULTIPLIER) - buffer->cca_request_restart_cnt);
        } else {
            mcps_set_packet_blacklist(rf_ptr, buffer, rf_ptr->cca_failure_restart_max - buffer->cca_request_restart_cnt);
        }
        return true;
    } else if (rf_ptr->mac_tx_result == MAC_TX_FAIL && buffer->tx_request_restart_cnt) {
        buffer->tx_request_restart_cnt--;
        mcps_set_packet_blacklist(rf_ptr, buffer, rf_ptr->tx_failure_restart_max - buffer->tx_request_restart_cnt);
        return true;
    }
    return false;
}

static void mcps_data_confirm_handle(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer, mac_pre_parsed_frame_t *ack_buf)
{

    sw_mac_stats_update(rf_ptr, STAT_MAC_TX_CCA_ATT, rf_ptr->mac_tx_status.cca_cnt);
    sw_mac_stats_update(rf_ptr, STAT_MAC_TX_RETRY, rf_ptr->mac_tx_status.retry);
    mcps_data_conf_t confirm;
    if (rf_ptr->fhss_api && !buffer->asynch_request) {
        if (!mcps_buffer_edfe_data_failure(rf_ptr, buffer) && ((rf_ptr->mac_tx_result == MAC_TX_FAIL) || (rf_ptr->mac_tx_result == MAC_CCA_FAIL) || (rf_ptr->mac_tx_result == MAC_RETURN_TO_QUEUE))) {
            // Packet has return to queue status or it needs to be blacklisted and queued
            if ((rf_ptr->mac_tx_result == MAC_RETURN_TO_QUEUE) || mcps_update_packet_request_restart(rf_ptr, buffer) == true) {
                if (rf_ptr->mac_tx_result == MAC_TX_FAIL) {
                    buffer->fhss_retry_count += 1 + rf_ptr->mac_tx_status.retry;
                } else if (rf_ptr->mac_tx_result == MAC_RETURN_TO_QUEUE) {
                    buffer->stored_retry_cnt = rf_ptr->mac_tx_retry;
                    buffer->stored_cca_cnt = rf_ptr->mac_cca_retry;
                    buffer->stored_priority = buffer->priority;
                    // Use priority to transmit it first when proper channel is available
                    buffer->priority = MAC_PD_DATA_TX_IMMEDIATELY;
                } else {
                    buffer->fhss_retry_count += rf_ptr->mac_tx_status.retry;
                }
                buffer->fhss_cca_retry_count += rf_ptr->mac_tx_status.cca_cnt;
                mcps_sap_pd_req_queue_write(rf_ptr, buffer);
                return;
            }
        }

        if (rf_ptr->mac_edfe_enabled && buffer->ExtendedFrameExchange) {
            rf_ptr->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
        }

    }
    confirm.cca_retries = rf_ptr->mac_tx_status.cca_cnt + buffer->fhss_cca_retry_count;
    confirm.tx_retries = rf_ptr->mac_tx_status.retry + buffer->fhss_retry_count;
    mac_common_data_confirmation_handle(rf_ptr, buffer);
    confirm.msduHandle = buffer->msduHandle;
    confirm.status = buffer->status;
    if (ack_buf) {
        confirm.timestamp = ack_buf->timestamp;
    } else {
        confirm.timestamp = 0;
    }

    if (buffer->fcf_dsn.ackRequested) {
        // Update latency for unicast packets. Given as milliseconds.
        sw_mac_stats_update(rf_ptr, STAT_MAC_TX_LATENCY, ((mac_mcps_sap_get_phy_timestamp(rf_ptr) - buffer->request_start_time_us) + 500) / 1000);
    }

    if (buffer->upper_layer_request) {
        //Check tunnel
        mcps_sap_prebuild_frame_buffer_free(buffer);
        mcps_data_confirm_cb(rf_ptr, &confirm, ack_buf);
    } else {
        mac_data_interface_internal_tx_confirm_handle(rf_ptr, buffer);
    }
}

static void mac_security_data_params_set(ccm_globals_t *ccm_ptr, uint8_t *data_ptr, uint16_t msduLength)
{
    ccm_ptr->data_len = msduLength;
    ccm_ptr->data_ptr = data_ptr;
}


static void mac_security_authentication_data_params_set(ccm_globals_t *ccm_ptr, uint8_t *a_data_ptr,
                                                        uint8_t a_data_length)
{
    if (ccm_ptr->mic_len) {

        ccm_ptr->adata_len = a_data_length;
        ccm_ptr->adata_ptr = a_data_ptr;
        ccm_ptr->mic = ccm_ptr->data_ptr;
        ccm_ptr->mic += ccm_ptr->data_len;
    }
}

static uint32_t mcps_calculate_tx_time(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint32_t time_to_tx)
{
    // Max. time to TX is 65ms
    if (time_to_tx > 65000) {
        time_to_tx = 65000;
    }
    return mac_mcps_sap_get_phy_timestamp(rf_mac_setup) + time_to_tx;
}

static void mcps_generic_sequence_number_allocate(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    if (buffer->fcf_dsn.frameVersion < MAC_FRAME_VERSION_2015 || (buffer->fcf_dsn.frameVersion ==  MAC_FRAME_VERSION_2015 &&  !buffer->fcf_dsn.sequenceNumberSuppress)) {
        /* Allocate SQN */
        switch (buffer->fcf_dsn.frametype) {
            case MAC_FRAME_CMD:
            case MAC_FRAME_DATA:
                if (!buffer->DSN_allocated) {
                    buffer->fcf_dsn.DSN = mac_mlme_set_new_sqn(rf_ptr);
                    buffer->DSN_allocated = true;
                }
                break;
            case MAC_FRAME_BEACON:
                 tr_error("Beacons are not supported");
                break;
            default:
                break;
        }
    }
}


static uint32_t mcps_generic_backoff_calc(protocol_interface_rf_mac_setup_s *rf_ptr)
{
    uint32_t random_period = mac_csma_backoff_get(rf_ptr);
    if (rf_ptr->rf_csma_extension_supported) {
        return mcps_calculate_tx_time(rf_ptr, random_period);
    }
    return random_period;
}

static int8_t mcps_generic_packet_build(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    phy_device_driver_s *dev_driver = rf_ptr->dev_driver->phy_driver;
    dev_driver_tx_buffer_s *tx_buf = &rf_ptr->dev_driver_tx_buffer;

    ccm_globals_t ccm_ptr;

    if (buffer->mac_header_length_with_security == 0) {
        rf_ptr->mac_tx_status.length = buffer->mac_payload_length;
        uint8_t *ptr = tx_buf->buf;
        if (dev_driver->phy_header_length) {
            ptr += dev_driver->phy_header_length;
        }
        tx_buf->len = buffer->mac_payload_length;

        memcpy(ptr, buffer->mac_payload, buffer->mac_payload_length);
        buffer->tx_time = mcps_generic_backoff_calc(rf_ptr);
        return 0;
    }

    //This will prepare MHR length with Header IE
    mac_header_information_elements_preparation(buffer);

    mcps_generic_sequence_number_allocate(rf_ptr, buffer);
    mlme_key_descriptor_t *key_desc = NULL;
    if (buffer->fcf_dsn.securityEnabled) {
        bool increment_framecounter = false;
        //Remember to update security counter here!
        key_desc = mac_frame_security_key_get(rf_ptr, buffer);
        if (!key_desc) {
            buffer->status = MLME_UNAVAILABLE_KEY;
            return -2;
        }

        //GET Counter
        uint32_t new_frameCounter = mac_sec_mib_key_outgoing_frame_counter_get(rf_ptr, key_desc);
        // If buffer frame counter is set, this is FHSS channel retry, update frame counter only if something was sent after failure
        if ((buffer->aux_header.frameCounter == 0xffffffff) || buffer->asynch_request || mac_data_counter_too_small(new_frameCounter, buffer->aux_header.frameCounter)) {
            buffer->aux_header.frameCounter = new_frameCounter;
            increment_framecounter = true;
        }

        if (!mac_frame_security_parameters_init(&ccm_ptr, rf_ptr, buffer, key_desc)) {
            return -2;
        }
        //Increment security counter
        if (increment_framecounter) {
            mac_sec_mib_key_outgoing_frame_counter_increment(rf_ptr, key_desc);
        }
    }

    //Calculate Payload length here with IE extension
    uint16_t frame_length = mac_buffer_total_payload_length(buffer);
    //Storage Mac Payload length here
    uint16_t mac_payload_length = frame_length;

    if (mac_payload_length > MAC_IEEE_802_15_4_MAX_MAC_SAFE_PAYLOAD_SIZE &&
            rf_ptr->phy_mtu_size == MAC_IEEE_802_15_4_MAX_PHY_PACKET_SIZE) {
        /* IEEE 802.15.4-2003 only allowed unsecured payloads up to 102 bytes
        * (always leaving room for maximum MAC overhead).
        * IEEE 802.15.4-2006 allows bigger if MAC header is small enough, but
        * we have to set the version field.
        */
        if (buffer->fcf_dsn.frameVersion < MAC_FRAME_VERSION_2006) {
            buffer->fcf_dsn.frameVersion = MAC_FRAME_VERSION_2006;
        }
    }

    if (rf_ptr->mac_ack_tx_active) {
        if (buffer->fcf_dsn.securityEnabled) {
            ccm_free(&ccm_ptr);
        }
        return 0;
    }

    //Add MHR length to total length
    frame_length += buffer->mac_header_length_with_security + buffer->security_mic_len;
    if ((frame_length) > rf_ptr->phy_mtu_size - 2) {
        tr_debug("Too Long %u, %u pa %u header %u mic %u", frame_length, mac_payload_length, buffer->mac_header_length_with_security,  buffer->security_mic_len, rf_ptr->phy_mtu_size);
        buffer->status = MLME_FRAME_TOO_LONG;
        //decrement security counter
        if (key_desc) {
            mac_sec_mib_key_outgoing_frame_counter_decrement(rf_ptr, key_desc);
        }
        return -1;
    }

    rf_ptr->mac_tx_status.length = frame_length;
    uint8_t *ptr = tx_buf->buf;
    if (dev_driver->phy_header_length) {
        ptr += dev_driver->phy_header_length;
    }

    tx_buf->len = frame_length;
    uint8_t *mhr_start = ptr;
    if (buffer->ExtendedFrameExchange && buffer->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_NONE) {
        buffer->tx_time = mac_mcps_sap_get_phy_timestamp(rf_ptr) + 300; //Send 300 us later
    } else {
        buffer->tx_time = mcps_generic_backoff_calc(rf_ptr);
    }

    ptr = mac_generic_packet_write(rf_ptr, ptr, buffer);


    if (buffer->fcf_dsn.securityEnabled) {
        uint8_t open_payload = 0;
        if (buffer->fcf_dsn.frametype == MAC_FRAME_CMD) {
            open_payload = 1;
        }
        mac_security_data_params_set(&ccm_ptr, (mhr_start + (buffer->mac_header_length_with_security + open_payload)), (mac_payload_length - open_payload));
        mac_security_authentication_data_params_set(&ccm_ptr, mhr_start, (buffer->mac_header_length_with_security + open_payload));
        ccm_process_run(&ccm_ptr);
    }

    return 0;
}

int8_t mcps_generic_ack_data_request_init(protocol_interface_rf_mac_setup_s *rf_ptr, const mac_fcf_sequence_t *fcf, const uint8_t *data_ptr, const mcps_ack_data_payload_t *ack_payload)
{
    mac_pre_build_frame_t *buffer = &rf_ptr->enhanced_ack_buffer;
    //save timestamp
    rf_ptr->enhanced_ack_handler_timestamp = mac_mcps_sap_get_phy_timestamp(rf_ptr);

    memset(buffer, 0, sizeof(mac_pre_build_frame_t));
    buffer->fcf_dsn.frametype = FC_ACK_FRAME;
    buffer->fcf_dsn.frameVersion = fcf->frameVersion;
    buffer->fcf_dsn.framePending = rf_ptr->mac_frame_pending;
    buffer->fcf_dsn.DSN = fcf->DSN;
    buffer->fcf_dsn.intraPan = fcf->intraPan;
    buffer->fcf_dsn.sequenceNumberSuppress = fcf->sequenceNumberSuppress;
    buffer->fcf_dsn.DstPanPresents = fcf->DstPanPresents;
    buffer->fcf_dsn.SrcAddrMode = fcf->DstAddrMode;
    buffer->fcf_dsn.SrcPanPresents = fcf->SrcPanPresents;
    buffer->fcf_dsn.DstAddrMode = fcf->SrcAddrMode;

    if (buffer->fcf_dsn.sequenceNumberSuppress) {
        buffer->mac_header_length_with_security = 2;
    } else {
        buffer->mac_header_length_with_security = 3;
    }

    buffer->mac_header_length_with_security += mac_header_address_length(&buffer->fcf_dsn);

    buffer->DstPANId = mac_header_get_src_panid(fcf, data_ptr, rf_ptr->pan_id);
    buffer->SrcPANId = mac_header_get_dst_panid(fcf, data_ptr, rf_ptr->pan_id);
    mac_header_get_src_address(fcf, data_ptr, buffer->DstAddr);
    mac_header_get_dst_address(fcf, data_ptr, buffer->SrcAddr);

    //Security
    buffer->fcf_dsn.securityEnabled = fcf->securityEnabled;
    if (buffer->fcf_dsn.securityEnabled) {
        //Read Security AUX headers
        const uint8_t *ptr = data_ptr;
        ptr += mac_header_off_set_to_aux_header(fcf);
        //Start parsing AUX header
        mlme_security_t aux_parse;
        mac_header_security_aux_header_parse(ptr, &aux_parse);
        buffer->aux_header.KeyIdMode = aux_parse.KeyIdMode;
        buffer->aux_header.KeyIndex = aux_parse.KeyIndex;
        buffer->aux_header.securityLevel = aux_parse.SecurityLevel;
        memcpy(buffer->aux_header.Keysource, aux_parse.Keysource, 8);

        buffer->security_mic_len = mac_security_mic_length_get(buffer->aux_header.securityLevel);
        buffer->fcf_dsn.frameVersion = MAC_FRAME_VERSION_2006;
        buffer->mac_header_length_with_security += mac_header_security_aux_header_length(buffer->aux_header.securityLevel, buffer->aux_header.KeyIdMode);

    }

    uint16_t ie_header_length = 0;
    uint16_t ie_payload_length = 0;

    if (!mac_ie_vector_length_validate(ack_payload->ie_elements.headerIeVectorList, ack_payload->ie_elements.headerIovLength, &ie_header_length)) {
        return -1;
    }

    if (!mac_ie_vector_length_validate(ack_payload->ie_elements.payloadIeVectorList, ack_payload->ie_elements.payloadIovLength, &ie_payload_length)) {
        return -1;
    }

    buffer->ie_elements.headerIeVectorList = ack_payload->ie_elements.headerIeVectorList;
    buffer->ie_elements.headerIovLength = ack_payload->ie_elements.headerIovLength;
    buffer->ie_elements.payloadIeVectorList = ack_payload->ie_elements.payloadIeVectorList;
    buffer->ie_elements.payloadIovLength = ack_payload->ie_elements.payloadIovLength;
    buffer->headerIeLength = ie_header_length;
    buffer->payloadsIeLength = ie_payload_length;
    buffer->mac_payload = ack_payload->payloadPtr;
    buffer->mac_payload_length = ack_payload->payloadLength;

    //This will prepare MHR length with Header IE
    mac_header_information_elements_preparation(buffer);
    return 0;
}

int8_t mcps_generic_edfe_frame_init(protocol_interface_rf_mac_setup_s *rf_ptr, const mac_fcf_sequence_t *fcf, const uint8_t *data_ptr, const mcps_edfe_response_t *response)
{
    //Data Here
    mac_pre_build_frame_t *buffer;
    if (response->wait_response) {
        if (rf_ptr->active_pd_data_request == NULL) {
            return -1;
        }
        buffer = rf_ptr->active_pd_data_request;
        buffer->message_builded = false;
    } else {
        buffer = &rf_ptr->enhanced_ack_buffer;
        memset(buffer, 0, sizeof(mac_pre_build_frame_t));
        buffer->fcf_dsn.frametype = FC_DATA_FRAME;
        buffer->fcf_dsn.frameVersion = fcf->frameVersion;
        buffer->fcf_dsn.DstPanPresents = fcf->DstPanPresents;
        buffer->fcf_dsn.DstAddrMode = response->DstAddrMode;
        buffer->DstPANId = mac_header_get_src_panid(fcf, data_ptr, rf_ptr->pan_id);
        buffer->SrcPANId = mac_header_get_dst_panid(fcf, data_ptr, rf_ptr->pan_id);
        memcpy(buffer->DstAddr, response->Address, 8);
    }
    buffer->fcf_dsn.intraPan = response->PanIdSuppressed;
    buffer->fcf_dsn.SrcAddrMode = response->SrcAddrMode;
    buffer->fcf_dsn.SrcPanPresents = response->SrcAddrMode;
    buffer->fcf_dsn.framePending = false;
    buffer->fcf_dsn.sequenceNumberSuppress = fcf->sequenceNumberSuppress;

    buffer->WaitResponse = response->wait_response;
    buffer->ExtendedFrameExchange = true;

    if (buffer->fcf_dsn.sequenceNumberSuppress) {
        buffer->mac_header_length_with_security = 2;
    } else {
        buffer->mac_header_length_with_security = 3;
    }

    buffer->mac_header_length_with_security += mac_header_address_length(&buffer->fcf_dsn);

    //Security
    buffer->fcf_dsn.securityEnabled = fcf->securityEnabled;
    if (buffer->fcf_dsn.securityEnabled) {
        //Read Security AUX headers
        const uint8_t *ptr = data_ptr;
        ptr += mac_header_off_set_to_aux_header(fcf);
        //Start parsing AUX header
        mlme_security_t aux_parse;
        mac_header_security_aux_header_parse(ptr, &aux_parse);
        buffer->aux_header.KeyIdMode = aux_parse.KeyIdMode;
        buffer->aux_header.KeyIndex = aux_parse.KeyIndex;
        buffer->aux_header.securityLevel = aux_parse.SecurityLevel;
        memcpy(buffer->aux_header.Keysource, aux_parse.Keysource, 8);

        buffer->security_mic_len = mac_security_mic_length_get(buffer->aux_header.securityLevel);
        buffer->fcf_dsn.frameVersion = MAC_FRAME_VERSION_2006;
        buffer->mac_header_length_with_security += mac_header_security_aux_header_length(buffer->aux_header.securityLevel, buffer->aux_header.KeyIdMode);

    }

    uint16_t ie_header_length = 0;
    uint16_t ie_payload_length = 0;

    if (!mac_ie_vector_length_validate(response->ie_response.headerIeVectorList, response->ie_response.headerIovLength, &ie_header_length)) {
        return -1;
    }

    if (!mac_ie_vector_length_validate(response->ie_response.payloadIeVectorList, response->ie_response.payloadIovLength, &ie_payload_length)) {
        return -1;
    }

    buffer->ie_elements.headerIeVectorList = response->ie_response.headerIeVectorList;
    buffer->ie_elements.headerIovLength = response->ie_response.headerIovLength;
    buffer->ie_elements.payloadIeVectorList = response->ie_response.payloadIeVectorList;
    buffer->ie_elements.payloadIovLength = response->ie_response.payloadIovLength;
    buffer->headerIeLength = ie_header_length;
    buffer->payloadsIeLength = ie_payload_length;
    buffer->mac_payload = NULL;
    buffer->mac_payload_length = 0;

    //This will prepare MHR length with Header IE
    mac_header_information_elements_preparation(buffer);
    return 0;
}


int8_t mcps_generic_ack_build(protocol_interface_rf_mac_setup_s *rf_ptr, bool init_build)
{
    phy_device_driver_s *dev_driver = rf_ptr->dev_driver->phy_driver;
    dev_driver_tx_buffer_s *tx_buf = &rf_ptr->dev_driver_tx_buffer;

    ccm_globals_t ccm_ptr;
    mac_pre_build_frame_t *buffer = &rf_ptr->enhanced_ack_buffer;
    mlme_key_descriptor_t *key_desc = NULL;

    if (buffer->fcf_dsn.securityEnabled) {
        //Remember to update security counter here!
        key_desc = mac_frame_security_key_get(rf_ptr, buffer);
        if (!key_desc) {
#ifdef __linux__
            tr_debug("Drop a ACK missing key desc");
#endif
            buffer->status = MLME_UNAVAILABLE_KEY;
            return -2;
        }
        if (init_build) {
            buffer->aux_header.frameCounter = mac_sec_mib_key_outgoing_frame_counter_get(rf_ptr, key_desc);
        }
        if (!mac_frame_security_parameters_init(&ccm_ptr, rf_ptr, buffer, key_desc)) {
#ifdef __linux__
            tr_debug("Drop a ACK ignored by security init");
#endif
            return -2;
        }
        if (init_build) {
            //Increment security counter
            mac_sec_mib_key_outgoing_frame_counter_increment(rf_ptr, key_desc);
        }
    }

    //Calculate Payload length here with IE extension
    uint16_t frame_length = mac_buffer_total_payload_length(buffer);
    //Storage Mac Payload length here
    uint16_t mac_payload_length = frame_length;

    //Add MHR length to total length
    frame_length += buffer->mac_header_length_with_security + buffer->security_mic_len;
    uint16_t ack_mtu_size;
    if (ENHANCED_ACK_MAX_LENGTH > rf_ptr->phy_mtu_size) {
        ack_mtu_size = rf_ptr->phy_mtu_size;
    } else {
        ack_mtu_size = ENHANCED_ACK_MAX_LENGTH;
    }


    if ((frame_length) > ack_mtu_size - 2) {
        buffer->status = MLME_FRAME_TOO_LONG;

        if (key_desc) {
            //decrement security counter
            mac_sec_mib_key_outgoing_frame_counter_decrement(rf_ptr, key_desc);
            ccm_free(&ccm_ptr);
        }
#ifdef __linux__
        tr_debug("Drop a ACK send by frame too long %u", frame_length);
#endif
        return -1;
    }

    rf_ptr->mac_tx_status.length = frame_length;
    uint8_t *ptr = tx_buf->enhanced_ack_buf;
    if (dev_driver->phy_header_length) {
        ptr += dev_driver->phy_header_length;
    }

    tx_buf->ack_len = frame_length;
    uint8_t *mhr_start = ptr;
    buffer->tx_time = mac_mcps_sap_get_phy_timestamp(rf_ptr) + 1000; // 1ms delay before Ack

    ptr = mac_generic_packet_write(rf_ptr, ptr, buffer);


    if (buffer->fcf_dsn.securityEnabled) {
        mac_security_data_params_set(&ccm_ptr, (mhr_start + (buffer->mac_header_length_with_security)), (mac_payload_length));
        mac_security_authentication_data_params_set(&ccm_ptr, mhr_start, (buffer->mac_header_length_with_security));
        ccm_process_run(&ccm_ptr);
    }
    //Disable TX Time
    phy_csma_params_t csma_params;
    csma_params.backoff_time = 0;
    csma_params.cca_enabled = false;
    csma_params.mode_switch_phr = false;
    rf_ptr->dev_driver->phy_driver->extension(PHY_EXTENSION_SET_CSMA_PARAMETERS, (uint8_t *) &csma_params);
    if (rf_ptr->active_pd_data_request) {
        timer_mac_stop(rf_ptr);
        mac_pd_abort_active_tx(rf_ptr);
    }
    return mcps_pd_data_cca_trig(rf_ptr, buffer);
}


static int8_t mcps_generic_packet_rebuild(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    phy_device_driver_s *dev_driver = rf_ptr->dev_driver->phy_driver;
    dev_driver_tx_buffer_s *tx_buf = &rf_ptr->dev_driver_tx_buffer;
    ccm_globals_t ccm_ptr;

    if (!buffer) {
        return -1;
    }

    if (buffer->mac_header_length_with_security == 0) {
        rf_ptr->mac_tx_status.length = buffer->mac_payload_length;
        uint8_t *ptr = tx_buf->buf;
        if (dev_driver->phy_header_length) {
            ptr += dev_driver->phy_header_length;
        }
        tx_buf->len = buffer->mac_payload_length;

        memcpy(ptr, buffer->mac_payload, buffer->mac_payload_length);
        buffer->tx_time = mcps_generic_backoff_calc(rf_ptr);
        return 0;
    }

    if (buffer->fcf_dsn.securityEnabled) {

        mlme_key_descriptor_t *key_desc = mac_frame_security_key_get(rf_ptr, buffer);
        if (!key_desc) {
            buffer->status = MLME_UNAVAILABLE_KEY;
            return -2;
        }

        if (!mac_frame_security_parameters_init(&ccm_ptr, rf_ptr, buffer, key_desc)) {
            return -2;
        }
    }

    //Calculate Payload length here with IE extension
    uint16_t frame_length = mac_buffer_total_payload_length(buffer);
    //Storage Mac Payload length here
    uint16_t mac_payload_length = frame_length;

    //Add MHR length to total length
    frame_length += buffer->mac_header_length_with_security + buffer->security_mic_len;

    rf_ptr->mac_tx_status.length = frame_length;
    uint8_t *ptr = tx_buf->buf;
    if (dev_driver->phy_header_length) {
        ptr += dev_driver->phy_header_length;
    }

    tx_buf->len = frame_length;
    uint8_t *mhr_start = ptr;
    if (buffer->ExtendedFrameExchange && buffer->fcf_dsn.SrcAddrMode == MAC_ADDR_MODE_NONE) {
        buffer->tx_time = mac_mcps_sap_get_phy_timestamp(rf_ptr) + 300; //Send 300 us later
    } else {
        buffer->tx_time = mcps_generic_backoff_calc(rf_ptr);
    }

    ptr = mac_generic_packet_write(rf_ptr, ptr, buffer);

    if (buffer->fcf_dsn.securityEnabled) {
        uint8_t open_payload = 0;
        if (buffer->fcf_dsn.frametype == MAC_FRAME_CMD) {
            open_payload = 1;
        }
        mac_security_data_params_set(&ccm_ptr, (mhr_start + (buffer->mac_header_length_with_security + open_payload)), (mac_payload_length - open_payload));
        mac_security_authentication_data_params_set(&ccm_ptr, mhr_start, (buffer->mac_header_length_with_security + open_payload));
        ccm_process_run(&ccm_ptr);
    }

    return 0;
}

static int8_t mcps_pd_data_cca_trig(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    mac_mlme_mac_radio_enable(rf_ptr);
    rf_ptr->macTxProcessActive = true;
    if (rf_ptr->rf_csma_extension_supported) {
        //Write TX time
        bool cca_enabled;
        if (buffer->fcf_dsn.frametype == MAC_FRAME_ACK) {
            cca_enabled = false;
            rf_ptr->mac_ack_tx_active = true;
        } else {
            if (buffer->ExtendedFrameExchange) {
                if (buffer->fcf_dsn.SrcAddrMode) {
                    cca_enabled = true;
                } else {
                    //Response
                    if (!buffer->WaitResponse) {
                        //Response
                        if (rf_ptr->mac_ack_tx_active) {
                            mac_csma_backoff_start(rf_ptr);
                            return -1;
                        }
                        rf_ptr->mac_edfe_response_tx_active = true;
                    } else {
                        rf_ptr->mac_edfe_response_tx_active = false;
                    }
                    cca_enabled = false;
                    rf_ptr->mac_edfe_tx_active = true;
                }

            } else if (buffer->phy_mode_id) {
                rf_ptr->mac_mode_switch_phr_tx_active = true;
                cca_enabled = true;
            } else {
                if (rf_ptr->mac_ack_tx_active) {
                    mac_csma_backoff_start(rf_ptr);
                    return -1;
                }
                cca_enabled = true;
            }

        }
        mac_pd_sap_set_phy_tx_time(rf_ptr, buffer->tx_time, cca_enabled, rf_ptr->mac_mode_switch_phr_tx_active);
        if (mac_plme_cca_req(rf_ptr) != 0) {
            if (buffer->fcf_dsn.frametype == MAC_FRAME_ACK || (buffer->ExtendedFrameExchange && rf_ptr->mac_edfe_response_tx_active)) {
                //ACK or EFDE Response
                rf_ptr->mac_ack_tx_active = false;
                // For Ack, stop the active TX process
                rf_ptr->macTxProcessActive = false;
                rf_ptr->mac_edfe_tx_active = false;
                rf_ptr->mac_edfe_response_tx_active = false;
                // If MAC had TX process active before Ack transmission,
                // the TX process has to be restarted in case the Ack transmission failed.
                if (rf_ptr->active_pd_data_request) {
                    mac_csma_backoff_start(rf_ptr);
                }
#ifdef __linux__
                if (buffer->fcf_dsn.frametype == MAC_FRAME_ACK) {
                    tr_debug("Drop ACK by CCA request");
                }
#endif
                return -1;
            }
            mac_csma_backoff_start(rf_ptr);
        }
    } else {
        timer_mac_start(rf_ptr, MAC_TIMER_CCA, (uint16_t)(buffer->tx_time / 50));
    }
    return 0;
}

static int8_t mcps_pd_data_request(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    rf_ptr->macTxRequestAck = false;

    memset(&(rf_ptr->mac_tx_status), 0, sizeof(mac_tx_status_t));
    if (buffer->priority == MAC_PD_DATA_TX_IMMEDIATELY) {
        // Return original priority and retry/CCA counts
        buffer->priority = buffer->stored_priority;
        rf_ptr->mac_tx_retry = rf_ptr->mac_tx_status.retry = buffer->stored_retry_cnt;
        rf_ptr->mac_cca_retry = rf_ptr->mac_tx_status.cca_cnt = buffer->stored_cca_cnt;
        buffer->stored_retry_cnt = buffer->stored_cca_cnt = 0;
    } else {
        rf_ptr->mac_tx_retry = 0;
        rf_ptr->mac_cca_retry = 0;
    }
    rf_ptr->mac_tx_start_channel = rf_ptr->mac_channel;
    mac_csma_param_init(rf_ptr);
    if (mcps_generic_packet_build(rf_ptr, buffer) != 0) {
        return -1;
    }
    rf_ptr->macTxRequestAck = buffer->WaitResponse;
    if (!rf_ptr->mac_ack_tx_active && !rf_ptr->mac_edfe_tx_active) {
        return mcps_pd_data_cca_trig(rf_ptr, buffer);
    } else {
        return 0;
    }

}

int8_t mcps_edfe_data_request(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    rf_ptr->macTxRequestAck = false;
    memset(&(rf_ptr->mac_tx_status), 0, sizeof(mac_tx_status_t));
    rf_ptr->mac_cca_retry = 0;
    rf_ptr->mac_tx_retry = 0;
    rf_ptr->mac_tx_start_channel = rf_ptr->mac_channel;
    buffer->aux_header.frameCounter = 0xffffffff;
    mac_csma_param_init(rf_ptr);
    if (mcps_generic_packet_build(rf_ptr, buffer) != 0) {
        return -1;
    }
    rf_ptr->macTxRequestAck = buffer->WaitResponse;//buffer->fcf_dsn.ackRequested;

    return mcps_pd_data_cca_trig(rf_ptr, buffer);

}

int8_t mcps_pd_data_rebuild(protocol_interface_rf_mac_setup_s *rf_ptr, mac_pre_build_frame_t *buffer)
{
    if (mcps_generic_packet_rebuild(rf_ptr, buffer) != 0) {
        return -1;
    }

    return mcps_pd_data_cca_trig(rf_ptr, buffer);
}


bool mac_is_ack_request_set(mac_pre_build_frame_t *buffer)
{
    if (buffer->fcf_dsn.ackRequested || buffer->WaitResponse) {
        return true;
    }
    return false;
}

int mac_convert_frame_type_to_fhss(uint8_t frame_type)
{
    if (FC_BEACON_FRAME == frame_type) {
        return FHSS_SYNCH_FRAME;
    }
    if (FC_CMD_FRAME == frame_type) {
        return FHSS_SYNCH_REQUEST_FRAME;
    }
    return FHSS_DATA_FRAME;
}

static bool mcps_check_packet_blacklist(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer)
{
    if (!buffer->blacklist_period_ms) {
        return false;
    }
    if ((mac_mcps_sap_get_phy_timestamp(rf_mac_setup) - buffer->blacklist_start_time_us) >= (buffer->blacklist_period_ms * 1000)) {
        return false;
    }
    return true;
}

void mcps_sap_pd_req_queue_write(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer)
{
    if (!rf_mac_setup || !buffer) {
        return;
    }
    if (!rf_mac_setup->active_pd_data_request) {
        if (buffer->ExtendedFrameExchange) {
            //Update here state and store peer
            memcpy(rf_mac_setup->mac_edfe_info->PeerAddr, buffer->DstAddr, 8);
            rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_CONNECTING;
        }
        if (rf_mac_setup->fhss_api && (buffer->asynch_request == false)) {
            uint16_t frame_length = buffer->mac_payload_length + buffer->headerIeLength + buffer->payloadsIeLength;
            if ((mcps_check_packet_blacklist(rf_mac_setup, buffer) == true) || rf_mac_setup->fhss_api->check_tx_conditions(rf_mac_setup->fhss_api, !mac_is_ack_request_set(buffer),
                                                                                                                           buffer->msduHandle, mac_convert_frame_type_to_fhss(buffer->fcf_dsn.frametype), frame_length,
                                                                                                                           rf_mac_setup->dev_driver->phy_driver->phy_header_length, rf_mac_setup->dev_driver->phy_driver->phy_tail_length) == false) {
                if (buffer->ExtendedFrameExchange) {
                    rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
                }
                goto push_to_queue;
            }
        }
        //Start TX process immediately
        rf_mac_setup->active_pd_data_request = buffer;
        if (mcps_pd_data_request(rf_mac_setup, buffer) != 0) {
            rf_mac_setup->mac_tx_result = MAC_TX_PRECOND_FAIL;
            rf_mac_setup->macTxRequestAck = false;
            if (buffer->ExtendedFrameExchange) {
                rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
            }
            if (mcps_sap_pd_confirm(rf_mac_setup) != 0) {
                // can't send event, try calling error handler directly
                rf_mac_setup->mac_mcps_data_conf_fail.msduHandle = buffer->msduHandle;
                rf_mac_setup->mac_mcps_data_conf_fail.status = buffer->status;
                if (buffer->ExtendedFrameExchange) {
                    rf_mac_setup->mac_edfe_info->state = MAC_EDFE_FRAME_IDLE;
                }
                mcps_sap_prebuild_frame_buffer_free(buffer);
                rf_mac_setup->active_pd_data_request = NULL;
                mac_pd_data_confirm_failure_handle(rf_mac_setup);
            }
        }

        return;
    }
push_to_queue:
    rf_mac_setup->direct_queue_bytes += buffer->mac_payload_length;
    mac_pre_build_frame_t *prev = NULL;
    mac_pre_build_frame_t *cur = rf_mac_setup->pd_data_request_queue_to_go;
    bool use_bc_queue = false;

    // When FHSS is enabled, broadcast buffers are pushed to own queue
    if (rf_mac_setup->fhss_api && (buffer->asynch_request == false)) {
        if (rf_mac_setup->fhss_api->use_broadcast_queue(rf_mac_setup->fhss_api, !mac_is_ack_request_set(buffer),
                                                        mac_convert_frame_type_to_fhss(buffer->fcf_dsn.frametype)) == true) {
            cur = rf_mac_setup->pd_data_request_bc_queue_to_go;
            use_bc_queue = true;
            rf_mac_setup->broadcast_queue_size++;
        }
    }
    if (use_bc_queue == false) {
        rf_mac_setup->unicast_queue_size++;
    }
    sw_mac_stats_update(rf_mac_setup, STAT_MAC_TX_QUEUE, rf_mac_setup->unicast_queue_size + rf_mac_setup->broadcast_queue_size);

    //Push to queue
    if (!cur) {
        if (rf_mac_setup->fhss_api && (use_bc_queue == true)) {
            rf_mac_setup->pd_data_request_bc_queue_to_go = buffer;
            return;
        } else {
            rf_mac_setup->pd_data_request_queue_to_go = buffer;
            return;
        }
    }

    while (cur) {
        if (cur->priority < buffer->priority) {
            //Set before cur
            if (prev) {
                prev->next = buffer;
                buffer->next = cur;
            } else {
                buffer->next = cur;
                if (rf_mac_setup->fhss_api && (use_bc_queue == true)) {
                    rf_mac_setup->pd_data_request_bc_queue_to_go = buffer;
                } else {
                    rf_mac_setup->pd_data_request_queue_to_go = buffer;
                }
            }
            cur = NULL;

        } else if (cur->next == NULL) {
            cur->next = buffer;
            cur = NULL;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

static mac_pre_build_frame_t *mcps_sap_pd_req_queue_read(protocol_interface_rf_mac_setup_s *rf_mac_setup, bool is_bc_queue, bool flush)
{
    mac_pre_build_frame_t *queue = rf_mac_setup->pd_data_request_queue_to_go;
    if (is_bc_queue == true) {
        queue = rf_mac_setup->pd_data_request_bc_queue_to_go;
    }

    if (!queue) {
        return NULL;
    }

    mac_pre_build_frame_t *buffer = queue;
    mac_pre_build_frame_t *prev = NULL;
    /* With FHSS, read buffer out from queue if:
     * - Buffer has timed out, OR
     * - Buffer is asynch request, OR
     * - Queue is flushed, OR
     * - Blacklisting AND FHSS allows buffer to be transmitted
     */
    if (rf_mac_setup->fhss_api) {
        while (buffer) {
            if (mcps_sap_check_buffer_timeout(rf_mac_setup, buffer) ||
                    buffer->asynch_request ||
                    (flush == true) ||
                    ((mcps_check_packet_blacklist(rf_mac_setup, buffer) == false) &&
                     (rf_mac_setup->fhss_api->check_tx_conditions(rf_mac_setup->fhss_api, !mac_is_ack_request_set(buffer),
                                                                  buffer->msduHandle, mac_convert_frame_type_to_fhss(buffer->fcf_dsn.frametype), buffer->mac_payload_length,
                                                                  rf_mac_setup->dev_driver->phy_driver->phy_header_length, rf_mac_setup->dev_driver->phy_driver->phy_tail_length) == true))) {
                break;
            }
            prev = buffer;
            buffer = buffer->next;
        }
    }
    if (!buffer) {
        return NULL;
    }
    // When other than first buffer is read out, link next buffer to previous buffer
    if (prev) {
        prev->next = buffer->next;
    }

    // When the first buffer is read out, set next buffer as the new first buffer
    if (is_bc_queue == false) {
        if (!prev) {
            rf_mac_setup->pd_data_request_queue_to_go = buffer->next;
        }
        rf_mac_setup->unicast_queue_size--;
    } else {
        if (!prev) {
            rf_mac_setup->pd_data_request_bc_queue_to_go = buffer->next;
        }
        rf_mac_setup->broadcast_queue_size--;
    }
    buffer->next = NULL;
    rf_mac_setup->direct_queue_bytes -= buffer->mac_payload_length;

    sw_mac_stats_update(rf_mac_setup, STAT_MAC_TX_QUEUE, rf_mac_setup->unicast_queue_size + rf_mac_setup->broadcast_queue_size);
    return buffer;
}

void mcps_sap_pre_parsed_frame_buffer_free(mac_pre_parsed_frame_t *buf)
{
    if (!buf) {
        return;
    }

    if (buf->mac_class_ptr && buf->fcf_dsn.frametype == FC_ACK_FRAME) {
        struct protocol_interface_rf_mac_setup *rf_mac_setup = buf->mac_class_ptr;
        if (rf_mac_setup->rf_pd_ack_buffer_is_in_use) {
            rf_mac_setup->rf_pd_ack_buffer_is_in_use = false;
            return;
        }
    }

    free(buf);
}

mac_pre_parsed_frame_t *mcps_sap_pre_parsed_frame_buffer_get(const uint8_t *data_ptr, uint16_t frame_length)
{
    mac_pre_parsed_frame_t *buffer = malloc(sizeof(mac_pre_parsed_frame_t) + frame_length);

    if (buffer) {
        memset(buffer, 0, sizeof(mac_pre_parsed_frame_t) + frame_length);
        buffer->frameLength = frame_length;
        memcpy(mac_header_message_start_pointer(buffer), data_ptr, frame_length);
    }

    return buffer;
}

mac_pre_parsed_frame_t *mcps_sap_pre_parsed_ack_buffer_get(protocol_interface_rf_mac_setup_s *rf_ptr, const uint8_t *data_ptr, uint16_t frame_length)
{

    if (rf_ptr->rf_pd_ack_buffer_is_in_use) {
#ifdef __linux__
        tr_debug("mac ACK buffer get fail: already active");
#endif
        return NULL;
    }

    if (frame_length > rf_ptr->allocated_ack_buffer_length) {
        //Free Current
        if (rf_ptr->pd_rx_ack_buffer) {
            free(rf_ptr->pd_rx_ack_buffer);
            rf_ptr->allocated_ack_buffer_length = 0;
        }
        rf_ptr->pd_rx_ack_buffer = malloc(sizeof(mac_pre_parsed_frame_t) + frame_length);
        if (!rf_ptr->pd_rx_ack_buffer) {
            return NULL;
        }
        rf_ptr->allocated_ack_buffer_length = frame_length;
    }
    memset(rf_ptr->pd_rx_ack_buffer, 0, sizeof(mac_pre_parsed_frame_t) + rf_ptr->allocated_ack_buffer_length);
    rf_ptr->pd_rx_ack_buffer->frameLength = frame_length;
    memcpy(mac_header_message_start_pointer(rf_ptr->pd_rx_ack_buffer), data_ptr, frame_length);
    //Mark active ACK buffer state
    rf_ptr->rf_pd_ack_buffer_is_in_use = true;
    return rf_ptr->pd_rx_ack_buffer;
}


static void mac_set_active_event(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t event_type)
{
    rf_mac_setup->active_mac_events |= (1u << event_type);
}

static void mac_clear_active_event(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t event_type)
{
    rf_mac_setup->active_mac_events &= ~(1u << event_type);
}

static bool mac_read_active_event(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t event_type)
{
    if (rf_mac_setup->active_mac_events & (1u << event_type)) {
        return true;
    }
    return false;
}

int8_t mcps_sap_pd_ind(mac_pre_parsed_frame_t *buffer)
{
    if (mac_tasklet_event_handler < 0 || !buffer) {
        return -1;
    }

    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .data_ptr = buffer,
        .event_type = MCPS_SAP_DATA_IND_EVENT,
        .priority = ARM_LIB_HIGH_PRIORITY_EVENT,
    };

    return event_send(&event);
}

int8_t mcps_sap_pd_confirm(void *mac_ptr)
{
    if (mac_tasklet_event_handler < 0  || !mac_ptr) {
        return -2;
    }
    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .data_ptr = mac_ptr,
        .event_type = MCPS_SAP_DATA_CNF_EVENT,
        .priority = ARM_LIB_HIGH_PRIORITY_EVENT,
    };

    return event_send(&event);
}

int8_t mcps_sap_pd_confirm_failure(void *mac_ptr)
{
    if (mac_tasklet_event_handler < 0  || !mac_ptr) {
        return -2;
    }
    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .data_ptr = mac_ptr,
        .event_type = MCPS_SAP_DATA_CNF_FAIL_EVENT,
        .priority = ARM_LIB_HIGH_PRIORITY_EVENT,
    };

    return event_send(&event);
}

int8_t mcps_sap_pd_ack(struct protocol_interface_rf_mac_setup *rf_ptr, mac_pre_parsed_frame_t *buffer)
{
    if (mac_tasklet_event_handler < 0  || !buffer) {
        return -1;
    }

    if (buffer->fcf_dsn.frametype == FC_ACK_FRAME) {
        struct event_storage *event = &rf_ptr->mac_ack_event;
        event->data.data_ptr = buffer;
        event->data.event_data = 0;
        event->data.event_id = 0;
        event->data.event_type = MCPS_SAP_DATA_ACK_CNF_EVENT;
        event->data.priority = ARM_LIB_HIGH_PRIORITY_EVENT;
        event->data.sender = 0;
        event->data.receiver = mac_tasklet_event_handler;
        event_send_user_allocated(event);

        return 0;
    }

    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .data_ptr = buffer,
        .event_type = MCPS_SAP_DATA_ACK_CNF_EVENT,
        .priority = ARM_LIB_HIGH_PRIORITY_EVENT,
    };

    return event_send(&event);
}

void mcps_sap_trig_tx(void *mac_ptr)
{
    if (mac_tasklet_event_handler < 0  || !mac_ptr) {
        return;
    }
    if (mac_read_active_event(mac_ptr, MAC_SAP_TRIG_TX) == true) {
        return;
    }
    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .data_ptr = mac_ptr,
        .event_type = MAC_SAP_TRIG_TX,
        .priority = ARM_LIB_MED_PRIORITY_EVENT,
    };

    if (event_send(&event) == 0) {
        mac_set_active_event(mac_ptr, MAC_SAP_TRIG_TX);
    }
}

void mac_cca_threshold_event_send(protocol_interface_rf_mac_setup_s *rf_ptr, uint8_t channel, int16_t dbm)
{
    // Return if feature is not initialized
    if (!rf_ptr->cca_threshold) {
        return;
    }
    uint16_t data = channel << 8 | (uint8_t) dbm;
    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .event_data = data,
        .data_ptr = rf_ptr,
        .event_type = MAC_CCA_THR_UPDATE,
        .priority = ARM_LIB_LOW_PRIORITY_EVENT,
    };

    event_send(&event);
}

void mac_generic_event_trig(uint8_t event_type, void *mac_ptr, bool low_latency)
{
    enum event_priority priority;
    if (low_latency) {
        priority = ARM_LIB_LOW_PRIORITY_EVENT;
    } else {
        priority = ARM_LIB_HIGH_PRIORITY_EVENT;
    }
    struct event_payload event = {
        .receiver = mac_tasklet_event_handler,
        .sender = 0,
        .event_id = 0,
        .data_ptr = mac_ptr,
        .event_type = event_type,
        .priority = priority,
    };

    event_send(&event);
}

void mac_mcps_buffer_queue_free(protocol_interface_rf_mac_setup_s *rf_mac_setup)
{

    if (rf_mac_setup->active_pd_data_request) {
        mcps_sap_prebuild_frame_buffer_free(rf_mac_setup->active_pd_data_request);
        rf_mac_setup->active_pd_data_request = NULL;
    }

    while (rf_mac_setup->pd_data_request_queue_to_go) {
        mac_pre_build_frame_t *buffer = mcps_sap_pd_req_queue_read(rf_mac_setup, false, true);
        if (buffer) {
            mcps_sap_prebuild_frame_buffer_free(buffer);
        }
    }

    while (rf_mac_setup->pd_data_request_bc_queue_to_go) {
        mac_pre_build_frame_t *buffer = mcps_sap_pd_req_queue_read(rf_mac_setup, true, true);
        if (buffer) {
            mcps_sap_prebuild_frame_buffer_free(buffer);
        }
    }

    while (rf_mac_setup->indirect_pd_data_request_queue) {
        mac_pre_build_frame_t *buffer = rf_mac_setup->indirect_pd_data_request_queue;
        if (buffer) {
            rf_mac_setup->indirect_pd_data_request_queue = buffer->next;
            mcps_sap_prebuild_frame_buffer_free(buffer);
        }
    }

    if (rf_mac_setup->pd_rx_ack_buffer) {
        if (rf_mac_setup->rf_pd_ack_buffer_is_in_use) {
            event_cancel(&rf_mac_setup->mac_ack_event);
            rf_mac_setup->rf_pd_ack_buffer_is_in_use = false;
        }
        free(rf_mac_setup->pd_rx_ack_buffer);
        rf_mac_setup->pd_rx_ack_buffer = NULL;
        rf_mac_setup->allocated_ack_buffer_length = 0;
    }

}
/**
 * Function return list start pointer
 */
static mac_pre_build_frame_t *mcps_sap_purge_from_list(mac_pre_build_frame_t *list_ptr_original, uint8_t msduhandle, uint8_t *status)
{
    mac_pre_build_frame_t *list_prev = NULL;
    mac_pre_build_frame_t *list_ptr = list_ptr_original;
    while (list_ptr) {
        if (list_ptr->fcf_dsn.frametype == MAC_FRAME_DATA && list_ptr->msduHandle == msduhandle) {

            if (list_prev) {
                list_prev->next = list_ptr->next;
            } else {
                list_ptr_original = list_ptr->next;
            }
            list_ptr->next = NULL;

            //Free data and buffer
            mcps_sap_prebuild_frame_buffer_free(list_ptr);
            list_ptr = NULL;
            *status = true;
        } else {
            list_prev = list_ptr;
            list_ptr = list_ptr->next;
        }
    }
    return list_ptr_original;
}


static bool mcps_sap_purge_req_from_queue(protocol_interface_rf_mac_setup_s *rf_mac_setup, uint8_t msduhandle)
{
    //Discover from TX queue data packets with given
    uint8_t status = false;
    rf_mac_setup->pd_data_request_queue_to_go = mcps_sap_purge_from_list(rf_mac_setup->pd_data_request_queue_to_go, msduhandle, &status);

    if (!status) {
        rf_mac_setup->indirect_pd_data_request_queue = mcps_sap_purge_from_list(rf_mac_setup->indirect_pd_data_request_queue, msduhandle, &status);
    }

    return status;
}

uint8_t mcps_sap_purge_reg_handler(protocol_interface_rf_mac_setup_s *rf_mac_setup, const mcps_purge_t *purge_req)
{
    mcps_purge_conf_t confirmation;
    confirmation.msduHandle = purge_req->msduHandle;

    if (mcps_sap_purge_req_from_queue(rf_mac_setup, confirmation.msduHandle)) {
        confirmation.status = MLME_SUCCESS;
    } else {
        confirmation.status = MLME_INVALID_HANDLE;
    }

    if (get_sw_mac_api(rf_mac_setup)) {
        get_sw_mac_api(rf_mac_setup)->purge_conf_cb(get_sw_mac_api(rf_mac_setup), &confirmation);
    }

    return confirmation.status;
}

void mcps_pending_packet_counter_update_check(protocol_interface_rf_mac_setup_s *rf_mac_setup, mac_pre_build_frame_t *buffer)
{
    if (buffer->fcf_dsn.securityEnabled) {
        mlme_key_descriptor_t *key_desc = mac_frame_security_key_get(rf_mac_setup, buffer);
        if (key_desc) {
            uint32_t current_counter = mac_sec_mib_key_outgoing_frame_counter_get(rf_mac_setup, key_desc);
            if (mac_data_counter_too_small(current_counter, buffer->aux_header.frameCounter)) {
                buffer->aux_header.frameCounter = current_counter;
                mac_sec_mib_key_outgoing_frame_counter_increment(rf_mac_setup, key_desc);
            }
        }
    }
}
