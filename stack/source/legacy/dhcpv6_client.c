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
#include "common/endian.h"
#include "common/rand.h"
#include "common/log_legacy.h"
#include "common/ns_list.h"

#include "nwk_interface/protocol.h"

#include "dhcpv6_utils.h"
#include "dhcpv6_service.h"
#include "dhcpv6_client.h"

#define TRACE_GROUP "DHP"

typedef struct dhcp_client_class {
    dhcp_client_global_adress_cb *global_address_cb;
    uint16_t service_instance;
    uint16_t relay_instance;
    uint16_t sol_max_delay;
    uint16_t sol_timeout;
    uint16_t sol_max_rt;
    uint8_t sol_max_rc;
    uint8_t libDhcp_instance;
    dhcp_duid_options_params_t duid;
    int8_t interface;
    bool renew_uses_solicit: 1;
    bool one_instance_interface: 1;
    bool no_address_hint: 1;
    ns_list_link_t      link;                   /*!< List link entry */
} dhcp_client_class_t;

static NS_LIST_DEFINE(dhcp_client_list, dhcp_client_class_t, link);

static bool dhcpv6_client_set_address(int8_t interface_id, dhcpv6_client_server_data_t *srv_data_ptr);
void dhcpv6_renew(struct net_if *interface, if_address_entry_t *addr, if_address_callback_e reason);



static dhcp_client_class_t *dhcpv6_client_entry_allocate(int8_t interface, uint8_t duid_length)
{
    dhcp_client_class_t *entry = malloc(sizeof(dhcp_client_class_t));
    uint8_t *duid  = malloc(duid_length);
    if (!entry || !duid) {
        free(entry);
        free(duid);
        return NULL;

    }
    memset(entry, 0, sizeof(dhcp_client_class_t));
    entry->interface = interface;
    entry->service_instance = dhcp_service_init(interface, DHCP_INSTANCE_CLIENT, NULL);
    entry->libDhcp_instance = libdhcpv6_nonTemporal_entry_get_unique_instance_id();
    entry->duid.duid = duid;
    entry->duid.duid_length = duid_length;
    entry->duid.type = DHCPV6_DUID_LINK_LAYER_TYPE;
    ns_list_add_to_end(&dhcp_client_list, entry);
    return entry;
}

//Discover DHCPv6 client
static dhcp_client_class_t *dhcpv6_client_entry_discover(int8_t interface)
{
    ns_list_foreach(dhcp_client_class_t, cur, &dhcp_client_list) {
        //Check All allocated address in this module
        if (cur->interface == interface) {
            return cur;
        }
    }

    return NULL;
}


void dhcp_client_init(int8_t interface, uint16_t link_type)
{
    struct net_if *interface_ptr = protocol_stack_interface_info_get_by_id(interface);
    if (!interface_ptr) {
        return;
    }


    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);

    if (dhcp_client) {
        dhcp_client->sol_timeout = 0;
        dhcp_client->sol_max_rt = 0;
        dhcp_client->sol_max_rc = 0;
        dhcp_client->sol_max_delay = 0;
        dhcp_client->renew_uses_solicit = false;
        dhcp_client->one_instance_interface = false;
        dhcp_client->no_address_hint = false;
        dhcp_client->service_instance = dhcp_service_init(interface, DHCP_INSTANCE_CLIENT, NULL);
        dhcp_client->libDhcp_instance = libdhcpv6_nonTemporal_entry_get_unique_instance_id();
        return;
    }
    //Allocate new
    //define DUID-LL length based on MAC64 or MAC48

    dhcp_client = dhcpv6_client_entry_allocate(interface, libdhcpv6_duid_linktype_size(link_type) + 2);
    if (!dhcp_client) {
        return;
    }

    uint8_t *ptr = dhcp_client->duid.duid;
    ptr = write_be16(ptr, link_type);
    memcpy(ptr, interface_ptr->mac, libdhcpv6_duid_linktype_size(link_type));

    //Define DUID


}
void dhcp_client_configure(int8_t interface, bool renew_uses_solicit, bool one_client_for_this_interface, bool no_address_hint)
{
    // Set true if RENEW is not used and SOLICIT sent instead.
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);
    if (!dhcp_client) {
        return;
    }
    dhcp_client->renew_uses_solicit = renew_uses_solicit;
    dhcp_client->one_instance_interface = one_client_for_this_interface;
    dhcp_client->no_address_hint = no_address_hint;
}

void dhcp_client_solicit_timeout_set(int8_t interface, uint16_t timeout, uint16_t max_rt, uint8_t max_rc, uint8_t max_delay)
{
    // Set the default retry values for SOLICIT and RENEW messages.
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);
    if (!dhcp_client) {
        return;
    }
    dhcp_client->sol_max_delay = max_delay * 10; //Convert to ticks
    dhcp_client->sol_timeout = timeout;
    dhcp_client->sol_max_rt = max_rt;
    dhcp_client->sol_max_rc = max_rc;

    return;
}

void dhcp_relay_agent_enable(int8_t interface, uint8_t border_router_address[static 16])
{
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);
    if (!dhcp_client) {
        return;
    }

    struct net_if * rcp_if_entry = protocol_stack_interface_info_get_by_id(interface);
    rcp_if_entry->is_dhcp_relay_agent_enabled = true;

    dhcp_client->relay_instance = dhcp_service_init(interface, DHCP_INTANCE_RELAY_AGENT, NULL);
    dhcp_service_relay_instance_enable(dhcp_client->relay_instance, border_router_address);
}

void dhcp_relay_agent_disable(int8_t interface)
{
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);
    if (!dhcp_client) {
        return;
    }

    dhcp_service_delete(dhcp_client->relay_instance);
}

void dhcp_client_delete(int8_t interface)
{
    struct net_if *cur = NULL;
    dhcpv6_client_server_data_t *srv_data_ptr;
    uint8_t temporary_address[16];
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);
    if (!dhcp_client) {
        return;
    }

    dhcp_service_delete(dhcp_client->service_instance);

    cur = protocol_stack_interface_info_get_by_id(interface);

    if (!cur) {
        return;
    }

    do {
        srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_instance(dhcp_client->libDhcp_instance);
        if (srv_data_ptr != NULL) {
            tr_debug("Free DHCPv6 Client");
            memcpy(temporary_address, srv_data_ptr->iaNontemporalAddress.addressPrefix, 16);
            dhcp_service_req_remove_all(srv_data_ptr);// remove all pending retransmissions
            libdhcvp6_nontemporalAddress_server_data_free(srv_data_ptr);
            addr_delete(cur, temporary_address);

        }
    } while (srv_data_ptr != NULL);
    dhcp_client->service_instance = 0;

    free(dhcp_client->duid.duid);
    ns_list_remove(&dhcp_client_list, dhcp_client);
    free(dhcp_client);

    return;
}


void dhcpv6_client_send_error_cb(dhcpv6_client_server_data_t *srv_data_ptr)
{
    if (srv_data_ptr != NULL) {
        dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(srv_data_ptr->interfaceId);
        if (!dhcp_client) {
            return;
        }

        // error for Global address
        if (dhcp_client->global_address_cb != NULL) {
            dhcp_client->global_address_cb(dhcp_client->interface, srv_data_ptr->server_address, srv_data_ptr->iaNontemporalAddress.addressPrefix, false);
        }
    }
}

/* solicitation response received for either global address or router id assignment */
int dhcp_solicit_resp_cb(uint16_t instance_id, void *ptr, uint8_t msg_name,  uint8_t *msg_ptr, uint16_t msg_len)
{
    dhcp_ia_non_temporal_params_t dhcp_ia_non_temporal_params;
    dhcp_duid_options_params_t clientId;
    dhcp_duid_options_params_t serverId;
    dhcpv6_client_server_data_t *srv_data_ptr = NULL;
    (void)instance_id;

    //Validate that started TR ID class is still at list
    srv_data_ptr = libdhcpv6_nonTemporal_validate_class_pointer(ptr);


    if (srv_data_ptr == NULL) {
        tr_error("server data not found");
        goto error_exit;
    }

    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(srv_data_ptr->interfaceId);
    if (!dhcp_client) {
        tr_error("No DHCPv6 client avilabale");
        goto error_exit;
    }

    //Clear Active Transaction state
    srv_data_ptr->transActionId = 0;

    // Validate message
    if (msg_name != DHCPV6_REPLY_TYPE) {
        tr_error("invalid response");
        goto error_exit;
    }

    if (libdhcpv6_reply_message_option_validate(&clientId, &serverId, &dhcp_ia_non_temporal_params, msg_ptr, msg_len) != 0) {
        tr_error("Reply Not include all Options");
        goto error_exit;
    }

    if (libdhcpv6_nonTemporal_entry_get_by_iaid(dhcp_ia_non_temporal_params.iaId) != srv_data_ptr) {
        /* Validate server data availability */
        tr_error("Valid instance not found");
        goto error_exit;
    }

    if (srv_data_ptr->IAID != dhcp_ia_non_temporal_params.iaId) {
        tr_error("Wrong IAID");
        goto error_exit;
    }

    if (!dhcp_ia_non_temporal_params.nonTemporalAddress) {
        tr_error("No associated address");
        goto error_exit;
    }

    if (libdhcpv6_compare_DUID(&srv_data_ptr->clientDUID, &clientId) != 0) {
        tr_error("Not Valid Client Id");
        goto error_exit;
    }

    //Allocate dynamically new Server DUID if needed
    if (!srv_data_ptr->serverDynamic_DUID || serverId.duid_length > srv_data_ptr->dyn_server_duid_length) {
        //Allocate dynamic new bigger
        srv_data_ptr->dyn_server_duid_length = 0;
        free(srv_data_ptr->serverDynamic_DUID);
        srv_data_ptr->serverDynamic_DUID = malloc(serverId.duid_length);
        if (!srv_data_ptr->serverDynamic_DUID) {
            tr_error("Dynamic DUID alloc fail");
            goto error_exit;
        }
        srv_data_ptr->dyn_server_duid_length = serverId.duid_length;
    }

    //Copy Server DUID
    srv_data_ptr->serverDUID.duid = srv_data_ptr->serverDynamic_DUID;
    srv_data_ptr->serverDUID.type = serverId.type;
    srv_data_ptr->serverDUID.duid_length = serverId.duid_length;
    memcpy(srv_data_ptr->serverDUID.duid, serverId.duid, serverId.duid_length);

    if (dhcp_client->one_instance_interface && memcmp(srv_data_ptr->iaNontemporalAddress.addressPrefix, dhcp_ia_non_temporal_params.nonTemporalAddress, 16)) {

        struct net_if *cur = protocol_stack_interface_info_get_by_id(dhcp_client->interface);
        if (cur) {
            addr_deprecate(cur, srv_data_ptr->iaNontemporalAddress.addressPrefix);
        }
    }

    memcpy(srv_data_ptr->iaNontemporalAddress.addressPrefix, dhcp_ia_non_temporal_params.nonTemporalAddress, 16);
    srv_data_ptr->iaNontemporalAddress.preferredTime = dhcp_ia_non_temporal_params.preferredValidLifeTime;
    srv_data_ptr->iaNontemporalAddress.validLifetime = dhcp_ia_non_temporal_params.validLifeTime;
    srv_data_ptr->T0 = dhcp_ia_non_temporal_params.T0;
    srv_data_ptr->T1  = dhcp_ia_non_temporal_params.T1;


    bool status = dhcpv6_client_set_address(dhcp_client->interface, srv_data_ptr);


    if (dhcp_client->global_address_cb) {
        dhcp_client->global_address_cb(dhcp_client->interface, srv_data_ptr->server_address, srv_data_ptr->iaNontemporalAddress.addressPrefix, status);
    }

    return RET_MSG_ACCEPTED;
error_exit:
    dhcpv6_client_send_error_cb(srv_data_ptr);
    return RET_MSG_ACCEPTED;
}

int dhcp_client_get_global_address(int8_t interface, uint8_t dhcp_addr[static 16], uint8_t prefix[static 16], dhcp_client_global_adress_cb *error_cb)
{
    dhcpv6_solicitation_base_packet_s solPacket = {0};

    uint8_t *payload_ptr;
    uint32_t payload_len;
    dhcpv6_client_server_data_t *srv_data_ptr;
    bool add_prefix;
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);

    if (dhcp_addr == NULL  || !dhcp_client) {
        tr_error("Invalid parameters");
        return -1;
    }

    if (!prefix || dhcp_client->one_instance_interface) {
        //NULL Definition will only check That Interface is not generated
        srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_instance(dhcp_client->libDhcp_instance);
        if (srv_data_ptr) {
            //Already Created to same interface
            if (dhcp_client->one_instance_interface && prefix) {
                if (srv_data_ptr->iaNonTemporalStructValid) {
                    if (memcmp(srv_data_ptr->iaNontemporalAddress.addressPrefix, prefix, 8) == 0) {
                        return 0;
                    }

                    struct net_if *cur = protocol_stack_interface_info_get_by_id(interface);
                    if (!cur) {
                        return -1;
                    }

                    dhcp_service_req_remove_all(srv_data_ptr);// remove all pending retransmissions
                    addr_deprecate(cur, srv_data_ptr->iaNontemporalAddress.addressPrefix);
                    srv_data_ptr->iaNonTemporalStructValid = false;
                    memcpy(srv_data_ptr->iaNontemporalAddress.addressPrefix, prefix, 8);
                    goto dhcp_address_get;

                } else if (dhcp_client_server_address_update(interface, prefix, dhcp_addr) == 0) {
                    //DHCP server address OK
                    return 0;
                }
            }
            return -1;

        }
    } else if (dhcp_client_server_address_update(interface, prefix, dhcp_addr) == 0) {
        //No need for allocate new
        return 0;
    }

    tr_debug("GEN new Dhcpv6 client %u", dhcp_client->libDhcp_instance);
    srv_data_ptr = libdhcvp6_nontemporalAddress_server_data_allocate(interface, dhcp_client->libDhcp_instance, prefix, dhcp_addr);

    if (!srv_data_ptr) {
        tr_error("OOM srv_data_ptr");
        return -1;
    }
    //SET DUID
    srv_data_ptr->clientDUID = dhcp_client->duid;

dhcp_address_get:

    if (!prefix || dhcp_client->no_address_hint) {
        add_prefix = false;
    } else {
        add_prefix = prefix != NULL;
    }

    payload_len = libdhcpv6_solicitation_message_length(srv_data_ptr->clientDUID.duid_length, add_prefix, 0);


    payload_ptr = malloc(payload_len);
    if (!payload_ptr) {
        libdhcvp6_nontemporalAddress_server_data_free(srv_data_ptr);
        tr_error("OOM payload_ptr");
        return -1;
    }

    dhcp_client->global_address_cb = error_cb; //TODO Only supporting one instance globaly if we need more for different instances needs code.
    srv_data_ptr->GlobalAddress = true;
    // Build solicit
    solPacket.clientDUID = srv_data_ptr->clientDUID;
    solPacket.iaID = srv_data_ptr->IAID;
    solPacket.messageType = DHCPV6_SOLICITATION_TYPE;
    solPacket.transActionId = libdhcpv6_txid_get();
    /*Non Temporal Address */

    if (prefix && !dhcp_client->no_address_hint) {
        dhcpv6_ia_non_temporal_address_s nonTemporalAddress = {0};
        nonTemporalAddress.requestedAddress = prefix;
        libdhcpv6_generic_nontemporal_address_message_write(payload_ptr, &solPacket, &nonTemporalAddress, NULL);
    } else {
        libdhcpv6_generic_nontemporal_address_message_write(payload_ptr, &solPacket, NULL, NULL);
    }

    uint16_t delay_tx = 0;
    if (dhcp_client->sol_max_delay) {
        delay_tx = rand_get_random_in_range(0, dhcp_client->sol_max_delay);
    }
    // send solicit
    srv_data_ptr->transActionId = dhcp_service_send_req(dhcp_client->service_instance, 0, srv_data_ptr, dhcp_addr, payload_ptr, payload_len, dhcp_solicit_resp_cb, delay_tx);
    if (srv_data_ptr->transActionId == 0) {
        free(payload_ptr);
        libdhcvp6_nontemporalAddress_server_data_free(srv_data_ptr);
        return -1;
    }
    srv_data_ptr->iaNonTemporalStructValid = false;
    if (dhcp_client->sol_timeout != 0) {
        // Default retry values are modified from specification update to message
        dhcp_service_set_retry_timers(srv_data_ptr->transActionId, dhcp_client->sol_timeout, dhcp_client->sol_max_rt, dhcp_client->sol_max_rc);
    }

    return 0;
}

int dhcp_client_server_address_update(int8_t interface, uint8_t *prefix, uint8_t server_address[static 16])
{
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface);
    if (!dhcp_client) {
        tr_debug("Interface not match");
        return -1;
    }

    dhcpv6_client_server_data_t *srv_data_ptr = NULL;

    if (prefix) {
        srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_prefix(interface, prefix);
    } else if (dhcp_client->one_instance_interface) {
        srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_instance(dhcp_client->libDhcp_instance);
    }
    if (!srv_data_ptr) {
        return -1;
    }

    if (memcmp(srv_data_ptr->server_address, server_address, 16) == 0) {
        return 0;
    }

    memcpy(srv_data_ptr->server_address, server_address, 16);
    if (srv_data_ptr->transActionId) {
        dhcp_service_update_server_address(srv_data_ptr->transActionId, server_address);
    }
    return 0;
}

void dhcp_client_global_address_delete(int8_t interface, uint8_t *dhcp_addr, uint8_t prefix[static 16])
{
    struct net_if *cur;
    dhcpv6_client_server_data_t *srv_data_ptr;
    dhcp_client_class_t *dhcp_client;
    (void) dhcp_addr;

    srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_prefix(interface, prefix);
    cur = protocol_stack_interface_info_get_by_id(interface);
    dhcp_client = dhcpv6_client_entry_discover(interface);

    if (cur == NULL || srv_data_ptr == NULL || !dhcp_client) {
        return;
    }

    dhcp_service_req_remove_all(srv_data_ptr);// remove all pending retransmissions
    if (dhcp_client->one_instance_interface) {
        addr_deprecate(cur, srv_data_ptr->iaNontemporalAddress.addressPrefix);
    } else {
        addr_delete(cur, srv_data_ptr->iaNontemporalAddress.addressPrefix);
    }

    libdhcvp6_nontemporalAddress_server_data_free(srv_data_ptr);
}

void dhcpv6_renew(struct net_if *interface, if_address_entry_t *addr, if_address_callback_e reason)
{

    uint8_t *payload_ptr;
    uint32_t payload_len;
    dhcp_client_class_t *dhcp_client = dhcpv6_client_entry_discover(interface->id);
    if (!dhcp_client) {
        return;
    }

    dhcpv6_client_server_data_t *srv_data_ptr;
    if (addr) {
        srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_prefix(interface->id, addr->address);
    } else {
        srv_data_ptr = libdhcpv6_nonTemporal_entry_get_by_instance(dhcp_client->libDhcp_instance);
    }

    if (srv_data_ptr == NULL) {
        return;
    }
    if (reason == ADDR_CALLBACK_INVALIDATED) {
        dhcp_service_req_remove_all(srv_data_ptr);// remove all pending retransmissions
        libdhcvp6_nontemporalAddress_server_data_free(srv_data_ptr);
        tr_warn("Dhcp address lost");
        return;
    }

    if (reason != ADDR_CALLBACK_TIMER) {
        return;
    }

    if (srv_data_ptr->transActionId) {
        //Do not trig new Renew process
        tr_warn("Do not trig new pending renew request");
        return;
    }

    payload_len = libdhcpv6_address_request_message_len(srv_data_ptr->clientDUID.duid_length, 0, 0, !dhcp_client->no_address_hint);
    payload_ptr = malloc(payload_len);
    if (payload_ptr == NULL) {
        if (addr) {
            addr->state_timer = 200; //Retry after 20 seconds
        }
        tr_error("Out of memory");
        return ;
    }
    dhcpv6_solicitation_base_packet_s packetReq = {
        .messageType = DHCPV6_RENEW_TYPE,
        .clientDUID = srv_data_ptr->clientDUID,
        .requestedOptionCnt = 0,
        .iaID = srv_data_ptr->IAID,
        .timerT0 = srv_data_ptr->T0,
        .timerT1 = srv_data_ptr->T1,
        .requestedOptionList = NULL,
    };

    if (dhcp_client->renew_uses_solicit) {
        packetReq.messageType = DHCPV6_SOLICITATION_TYPE;
    }



    if (dhcp_client->no_address_hint && dhcp_client->renew_uses_solicit) {
        packetReq.timerT0 = 0;
        packetReq.timerT1 = 0;
        libdhcpv6_generic_nontemporal_address_message_write(payload_ptr, &packetReq, NULL, NULL);
    } else {
        // Set Address information
        dhcpv6_ia_non_temporal_address_s nonTemporalAddress = {0};
        nonTemporalAddress.requestedAddress = srv_data_ptr->iaNontemporalAddress.addressPrefix;
        nonTemporalAddress.preferredLifeTime = srv_data_ptr->iaNontemporalAddress.preferredTime;
        nonTemporalAddress.validLifeTime = srv_data_ptr->iaNontemporalAddress.validLifetime;
        libdhcpv6_generic_nontemporal_address_message_write(payload_ptr, &packetReq, &nonTemporalAddress, NULL);
    }
    // send solicit
    uint8_t *server_address = dhcp_service_relay_global_addres_get(dhcp_client->relay_instance);
    if (!server_address) {
        server_address = srv_data_ptr->server_address;
    }

    if (interface->is_dhcp_relay_agent_enabled) {
        uint8_t ll[16];
        ns_list_foreach(if_address_entry_t, ll_addr, &interface->ip_addresses) {
            if (memcmp(ll_addr, ADDR_LINK_LOCAL_PREFIX, 8) == 0)
                memcpy(ll, ll_addr, 16);
        }
        srv_data_ptr->transActionId = dhcp_service_send_req(dhcp_client->service_instance, 0, srv_data_ptr, ll,
                                                            payload_ptr, payload_len, dhcp_solicit_resp_cb, 0);
    } else
        srv_data_ptr->transActionId = dhcp_service_send_req(dhcp_client->service_instance, 0, srv_data_ptr, server_address,
                                                            payload_ptr, payload_len, dhcp_solicit_resp_cb, 0);
    if (srv_data_ptr->transActionId == 0) {
        free(payload_ptr);
        if (addr) {
            addr->state_timer = 200; //Retry after 20 seconds
        }
        tr_error("DHCP renew send failed");
    }
    if (packetReq.messageType == DHCPV6_SOLICITATION_TYPE && dhcp_client->sol_timeout != 0) {
        // Default retry values are modified from specification update to message
        dhcp_service_set_retry_timers(srv_data_ptr->transActionId, dhcp_client->sol_timeout, dhcp_client->sol_max_rt, dhcp_client->sol_max_rc);
    }
    tr_info("DHCP renew send OK");
}

static bool dhcpv6_client_set_address(int8_t interface_id, dhcpv6_client_server_data_t *srv_data_ptr)
{
    struct net_if *cur = NULL;
    if_address_entry_t *address_entry = NULL;
    uint32_t renewTimer;
    cur = protocol_stack_interface_info_get_by_id(interface_id);
    if (!cur) {
        return false;
    }
    renewTimer = libdhcpv6_renew_time_define(srv_data_ptr);

    srv_data_ptr->iaNonTemporalStructValid = true;
    address_entry = addr_get_entry(cur, srv_data_ptr->iaNontemporalAddress.addressPrefix);
    if (address_entry == NULL) {
        // create new
        address_entry = addr_add(cur, srv_data_ptr->iaNontemporalAddress.addressPrefix, 64, ADDR_SOURCE_DHCP, srv_data_ptr->iaNontemporalAddress.validLifetime, srv_data_ptr->iaNontemporalAddress.preferredTime, false);
    } else {
        addr_set_valid_lifetime(cur, address_entry, srv_data_ptr->iaNontemporalAddress.validLifetime);
        addr_set_preferred_lifetime(cur, address_entry, srv_data_ptr->iaNontemporalAddress.preferredTime);
    }

    if (address_entry == NULL) {
        tr_error("Address add failed");
        srv_data_ptr->iaNonTemporalStructValid = false;
        return false;
    }

    if (renewTimer) {
        // translate seconds to 100ms ticks
        if (renewTimer  <  0xffffffff / 10) {
            renewTimer *= 10;
        } else {
            renewTimer = 0xfffffffe;
        }
    }
    address_entry->state_timer = renewTimer;
    address_entry->cb = dhcpv6_renew;
    return true;
}
