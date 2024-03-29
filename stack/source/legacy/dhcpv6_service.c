/*
 * Copyright (c) 2013-2021, Pelion and affiliates.
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
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include "common/endian.h"
#include "common/log.h"
#include "common/rand.h"
#include "common/named_values.h"
#include "common/log_legacy.h"
#include "common/ns_list.h"
#include "stack/net_interface.h"
#include "stack/timers.h"

#include "legacy/net_socket.h"
#include "nwk_interface/protocol.h"
#include "common_protocols/ip.h"
#include "6lowpan/ws/ws_bbr_api_internal.h"

#include "dhcpv6_utils.h"
#include "dhcpv6_service.h"

#define TRACE_GROUP    "dhcp"

#define MAX_SERVERS 20

/* Fixed-point randomisation limits for randlib_randomise_base() - RFC 3315
 * says RAND is uniformly distributed between -0.1 and +0.1
 */
#define RAND1_LOW   0x7333 // 1 - 0.1; minimum for "1+RAND"
#define RAND1_HIGH  0x8CCD // 1 + 0.1; maximum for "1+RAND"

typedef struct server_instance {
    dhcp_service_receive_req_cb *recv_req_cb;
    uint16_t instance_id;
    int8_t interface_id;
    dhcp_instance_type_e instance_type;
    ns_list_link_t link;
} server_instance_t;
typedef NS_LIST_HEAD(server_instance_t, link) server_instance_list_t;


typedef struct relay_instance {
    uint16_t instance_id;
    int8_t interface_id;
    uint8_t server_address[16];
    bool    relay_activated;
    ns_list_link_t link;
} relay_instance_t;
typedef NS_LIST_HEAD(relay_instance_t, link) relay_instance_list_t;

typedef struct msg_tr {
    ns_address_t addr;
    dhcp_service_receive_resp_cb *recv_resp_cb;
    uint16_t instance_id;
    int8_t interface_id;
    int8_t socket;
    uint8_t options;
    void  *client_obj_ptr;
    uint32_t msg_tr_id;
    uint32_t message_tr_id;
    uint32_t transmit_time;
    uint32_t first_transmit_time;
    uint16_t delayed_tx;
    uint16_t timeout;
    uint16_t timeout_init;
    uint16_t timeout_max;
    uint8_t retrans_max;
    uint8_t retrans;
    uint8_t *msg_ptr;
    uint16_t msg_len;
    uint8_t *relay_start;
    uint8_t *opt_interface_id;
    uint16_t opt_interface_id_length;
    ns_list_link_t link;
} msg_tr_t;
typedef NS_LIST_HEAD(msg_tr_t, link) tr_list_t;

typedef struct relay_notify {
    dhcp_relay_neighbour_cb *recv_notify_cb;
    int8_t interface_id;
    ns_list_link_t link;
} relay_notify_t;
typedef NS_LIST_HEAD(relay_notify_t, link) relay_notify_list_t;

typedef struct dhcp_service_class {
    ns_address_t src_address;
    server_instance_list_t srv_list;
    relay_instance_list_t relay_list;
    relay_notify_list_t notify_list;
    tr_list_t tr_list;
    int    dhcp_server_socket;
    int8_t dhcp_client_socket;
    int8_t dhcp_relay_socket;
    int8_t dhcpv6_socket_service_tasklet;
} dhcp_service_class_t;

dhcp_service_class_t *dhcp_service = NULL;
static bool dhcpv6_socket_timeout_timer_active = false;

static const struct name_value dhcp_frames[] = {
    { "sol",        DHCPV6_SOLICITATION_TYPE },
    { "adv",        DHCPV6_ADVERTISMENT_TYPE },
    { "req",        DHCPV6_REQUEST_TYPE },
    { "renew",      DHCPV6_RENEW_TYPE },
    { "rply",       DHCPV6_REPLY_TYPE },
    { "release",    DHCPV6_RELEASE_TYPE },
    { "rel-fwd",    DHCPV6_RELAY_FORWARD },
    { "rel-rply",   DHCPV6_RELAY_REPLY },
    { "lease-qury", DHCPV6_LEASEQUERY_TYPE },
    { "lease-rply", DHCPV6_LEASEQUERY_REPLY_TYPE },
    { NULL },
};

void dhcp_service_send_message(msg_tr_t *msg_tr_ptr);

void dhcp_service_timer_cb(int ticks)
{
    if (dhcp_service_timer_tick(ticks)) {
        dhcpv6_socket_timeout_timer_active = true;
        ws_timer_start(WS_TIMER_DHCPV6_SOCKET);
    } else {
        dhcpv6_socket_timeout_timer_active = false;
    }
}

bool dhcp_service_allocate(void)
{
    if (dhcp_service == NULL) {
        dhcp_service = malloc(sizeof(dhcp_service_class_t));
        if (dhcp_service) {
            ns_list_init(&dhcp_service->srv_list);
            ns_list_init(&dhcp_service->relay_list);
            ns_list_init(&dhcp_service->notify_list);
            ns_list_init(&dhcp_service->tr_list);
            dhcp_service->dhcp_client_socket = -1;
            dhcp_service->dhcp_server_socket = -1;
            dhcp_service->dhcp_relay_socket = -1;
            ws_timer_start(WS_TIMER_DHCPV6_SOCKET);
            dhcpv6_socket_timeout_timer_active = true;
        }
    }
    return true;
}

/*Subclass instances*/
msg_tr_t *dhcp_tr_find(uint32_t msg_tr_id)
{
    msg_tr_t *result = NULL;
    ns_list_foreach(msg_tr_t, cur_ptr, &dhcp_service->tr_list) {
        if (cur_ptr->msg_tr_id == msg_tr_id) {
            result = cur_ptr;
        }
    }
    return result;
}


msg_tr_t *dhcp_tr_create(void)
{
    uint32_t tr_id;
    msg_tr_t *msg_ptr = NULL;
    msg_ptr = malloc(sizeof(msg_tr_t));
    if (msg_ptr == NULL) {
        return NULL;
    }

    memset(msg_ptr, 0, sizeof(msg_tr_t));
    msg_ptr->msg_ptr = NULL;
    msg_ptr->recv_resp_cb = NULL;

    tr_id = rand_get_32bit() & 0xffffff;// 24 bits for random
    // Ensure a unique non-zero transaction id for each transaction
    while (tr_id == 0 || dhcp_tr_find(tr_id) != NULL) {
        tr_id = (tr_id + 1) & 0xffffff;
    }
    msg_ptr->msg_tr_id = tr_id;
    ns_list_add_to_start(&dhcp_service->tr_list, msg_ptr);
    return msg_ptr;
}

void dhcp_tr_delete(msg_tr_t *msg_ptr)
{
    if (msg_ptr != NULL) {
        ns_list_remove(&dhcp_service->tr_list, msg_ptr);
        free(msg_ptr->msg_ptr);
        free(msg_ptr);
    }
    return;
}

void dhcp_tr_set_retry_timers(msg_tr_t *msg_ptr, uint8_t msg_type)
{
    if (msg_ptr != NULL) {
        if (msg_type == DHCPV6_SOLICITATION_TYPE) {
            msg_ptr->timeout_init = SOL_TIMEOUT;
            msg_ptr->timeout_max = SOL_MAX_RT;
            msg_ptr->retrans_max = 0;
        } else if (msg_type == DHCPV6_RENEW_TYPE) {
            msg_ptr->timeout_init = REN_TIMEOUT;
            msg_ptr->timeout_max = REN_MAX_RT;
            msg_ptr->retrans_max = 0;
        } else if (msg_type == DHCPV6_LEASEQUERY_TYPE) {
            msg_ptr->timeout_init = LQ_TIMEOUT;
            msg_ptr->timeout_max = LQ_MAX_RT;
            msg_ptr->retrans_max = LQ_MAX_RC;
        } else {
            msg_ptr->timeout_init = REL_TIMEOUT;
            msg_ptr->timeout_max = 0;
            msg_ptr->retrans_max = REL_MAX_RC;
        }

        // Convert from seconds to 1/10s ticks, with initial randomisation factor
        msg_ptr->timeout_init = rand_randomise_base(msg_ptr->timeout_init * 10, RAND1_LOW, RAND1_HIGH);
        msg_ptr->timeout_max *= 10;

        msg_ptr->timeout = msg_ptr->timeout_init;
        if (!dhcpv6_socket_timeout_timer_active) {
            ws_timer_start(WS_TIMER_DHCPV6_SOCKET);
            dhcpv6_socket_timeout_timer_active = true;
        }
    }
    return;
}

server_instance_t *dhcp_service_client_find(uint16_t instance_id)
{
    server_instance_t *result = NULL;
    ns_list_foreach(server_instance_t, cur_ptr, &dhcp_service->srv_list) {
        if (cur_ptr->instance_id == instance_id) {
            result = cur_ptr;
        }
    }
    return result;
}


static uint16_t dhcp_service_relay_interface_get(int8_t  interface_id)
{
    ns_list_foreach(server_instance_t, cur_ptr, &dhcp_service->srv_list) {
        if (cur_ptr->interface_id == interface_id && cur_ptr->instance_type == DHCP_INTANCE_RELAY_AGENT) {
            return cur_ptr->instance_id;
        }
    }

    return 0;
}

static relay_notify_t *dhcp_service_notify_find(int8_t interface_id)
{
    relay_notify_t *result = NULL;
    ns_list_foreach(relay_notify_t, cur_ptr, &dhcp_service->notify_list) {
        if (cur_ptr->interface_id == interface_id) {
            result = cur_ptr;
        }
    }
    return result;
}


static relay_instance_t *dhcp_service_relay_find(uint16_t instance_id)
{
    relay_instance_t *result = NULL;
    ns_list_foreach(relay_instance_t, cur_ptr, &dhcp_service->relay_list) {
        if (cur_ptr->instance_id == instance_id) {
            result = cur_ptr;
        }
    }
    return result;
}

static relay_instance_t *dhcp_service_relay_interface(int8_t  interface_id)
{
    relay_instance_t *result = NULL;
    ns_list_foreach(relay_instance_t, cur_ptr, &dhcp_service->relay_list) {
        if (cur_ptr->interface_id == interface_id) {
            result = cur_ptr;
        }
    }
    return result;
}

void recv_dhcp_server_msg()
{
    server_instance_t *srv_ptr = NULL;
    msg_tr_t *msg_tr_ptr;
    uint8_t *msg_ptr, *allocated_ptr;
    dhcpv6_relay_msg_t relay_msg;
    ssize_t msg_len;
    uint8_t msg[2048];
    uint8_t msg_type;
    struct sockaddr_in6 src_addr;
    socklen_t src_addr_len = sizeof(struct sockaddr_in6);

    msg_tr_ptr = dhcp_tr_create();
    msg_ptr = msg;
    msg_len = recvfrom(dhcp_service->dhcp_server_socket, msg, sizeof(msg), 0, (struct sockaddr *) &src_addr, &src_addr_len);
    memcpy(msg_tr_ptr->addr.address, &(src_addr.sin6_addr), 16);
    msg_type = *msg_ptr;

    msg_ptr = malloc(msg_len);
    memcpy(msg_ptr, msg, msg_len);
    allocated_ptr = msg_ptr;

    TRACE(TR_DHCP, "rx-dhcp %-9s src:%s",
          val_to_str(msg_type, dhcp_frames, "[UNK]"),
          tr_ipv6(msg_tr_ptr->addr.address));
    if (msg_type == DHCPV6_RELAY_FORWARD) {
        if (!libdhcpv6_relay_msg_read(msg_ptr, msg_len, &relay_msg)) {
            tr_error("Relay forward not correct");
            goto cleanup;
        }
        //Update Source and data
        msg_tr_ptr->relay_start = msg_ptr;
        msg_tr_ptr->opt_interface_id = relay_msg.relay_interface_id.msg_ptr;
        msg_tr_ptr->opt_interface_id_length = relay_msg.relay_interface_id.len;
        memcpy(msg_tr_ptr->addr.address, relay_msg.peer_address, 16);
        msg_ptr = relay_msg.relay_options.msg_ptr;
        msg_len = relay_msg.relay_options.len;
        msg_type = *msg_ptr;


    } else if (msg_type == DHCPV6_RELAY_REPLY) {
        tr_error("Relay reply drop at server");
        goto cleanup;
    }

    //TODO use real function from lib also call validity check
    msg_tr_ptr->message_tr_id = read_be24(&msg_ptr[1]);

    if (0 != libdhcpv6_message_malformed_check(msg_ptr, msg_len)) {
        tr_error("Malformed packet");
        goto cleanup;
    }

    msg_tr_ptr->socket = dhcp_service->dhcp_server_socket;

cleanup:
    dhcp_tr_delete(msg_tr_ptr);
    free(allocated_ptr);
    if (srv_ptr == NULL) {
        //no owner found
        tr_warn("No handler for this message found");
    }

    return;
}

void recv_dhcp_relay_msg(void *cb_res)
{
    socket_callback_t *sckt_data;
    uint16_t msg_len;

    sckt_data = cb_res;

    if (sckt_data->event_type != SOCKET_DATA || sckt_data->d_len < 4) {
        return;
    }

    struct net_if *interface_ptr = protocol_stack_interface_info_get_by_id(sckt_data->interface_id);

    relay_instance_t *relay_srv = dhcp_service_relay_interface(sckt_data->interface_id);

    if (!interface_ptr || !relay_srv || !relay_srv->relay_activated) {
        return;
    }
    ns_address_t src_address;

    //Relay vector added space for relay frame + Interface ID
    uint8_t relay_frame[DHCPV6_RELAY_LENGTH + 4 + 5];


    uint8_t *socket_data = malloc(sckt_data->d_len);

    if (socket_data == NULL) {
        // read actual message
        tr_error("Out of resources");
        goto cleanup;
    }


    struct msghdr msghdr;
    struct iovec msg_data;
    msg_data.iov_base = socket_data;
    msg_data.iov_len = sckt_data->d_len;
    //Set messages name buffer
    msghdr.msg_name = &src_address;
    msghdr.msg_namelen = sizeof(src_address);
    msghdr.msg_iov = &msg_data;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = NULL;
    msghdr.msg_controllen = 0;

    msg_len = socket_recvmsg(sckt_data->socket_id, &msghdr, NS_MSG_LEGACY0);

    //Parse type
    uint8_t msg_type = *socket_data;
    TRACE(TR_DHCP, "rx-dhcp %-9s src:%s",
          val_to_str(msg_type, dhcp_frames, "[UNK]"),
          tr_ipv6(src_address.address));

    int16_t tc = 0;
    if (msg_type == DHCPV6_RELAY_FORWARD) {
        //Parse and validate Relay
        dhcpv6_relay_msg_t relay_msg;
        if (!libdhcpv6_relay_msg_read(socket_data, msg_len, &relay_msg)) {
            tr_error("Not valid relay");
            goto cleanup;
        }
        if (0 != libdhcpv6_message_malformed_check(relay_msg.relay_options.msg_ptr, relay_msg.relay_options.len)) {
            tr_error("Malformed packet");
            goto cleanup;
        }
        memcpy(src_address.address, relay_srv->server_address, 16);
        src_address.type = ADDRESS_IPV6;
        src_address.identifier = DHCPV6_SERVER_PORT;
        tc = IP_DSCP_CS6 << IP_TCLASS_DSCP_SHIFT;
    } else if (msg_type == DHCPV6_RELAY_REPLY) {
        //Parse and validate Relay
        dhcpv6_relay_msg_t relay_msg;
        if (!libdhcpv6_relay_msg_read(socket_data, msg_len, &relay_msg)) {
            tr_error("Not valid relay");
            goto cleanup;
        }
        if (0 != libdhcpv6_message_malformed_check(relay_msg.relay_options.msg_ptr, relay_msg.relay_options.len)) {
            tr_error("Malformed packet");
            goto cleanup;
        }
        //Copy DST address
        if (interface_ptr->id == ws_bbr_get_backbone_id()) {
            memcpy(src_address.address, relay_msg.link_address, 16);
            src_address.type = ADDRESS_IPV6;
            src_address.identifier = DHCPV6_SERVER_PORT;
            tc = IP_DSCP_CS6 << IP_TCLASS_DSCP_SHIFT;
        } else {
            memcpy(src_address.address, relay_msg.peer_address, 16);
            src_address.type = ADDRESS_IPV6;
            src_address.identifier = DHCPV6_CLIENT_PORT;
            msghdr.msg_iov = &msg_data;
            msghdr.msg_iovlen = 1;
            msg_data.iov_base = relay_msg.relay_options.msg_ptr;
            msg_data.iov_len = relay_msg.relay_options.len;
        }
    } else {
        if (0 != libdhcpv6_message_malformed_check(socket_data, msg_len)) {
            tr_error("Malformed packet");
            goto cleanup;
        }
        uint8_t gp_address[16];
        //Get blobal address from interface
        if (addr_interface_select_source(interface_ptr, gp_address, relay_srv->server_address, 0) != 0) {
            // No global prefix available
            tr_error("No GP address");
            goto cleanup;
        }
        struct iovec msg_iov[2];
        uint8_t *ptr = relay_frame;
        //Build
        //ADD relay frame vector front of original data
        msghdr.msg_iov = &msg_iov[0];
        msg_iov[0].iov_base = relay_frame;
        msghdr.msg_iovlen = 2;
        //SET Original Data
        msg_iov[1].iov_base = socket_data;
        msg_iov[1].iov_len = msg_len;

        ptr = libdhcpv6_dhcp_relay_msg_write(ptr, DHCPV6_RELAY_FORWARD, 0, src_address.address, gp_address);
        ptr = libdhcpv6_dhcp_option_header_write(ptr, DHCPV6_OPTION_RELAY, msg_len);
        //Update length of relay vector
        msg_iov[0].iov_len = ptr - relay_frame;

        //Update Neighbour table if necessary
        relay_notify_t *neigh_notify = dhcp_service_notify_find(sckt_data->interface_id);
        if (neigh_notify && neigh_notify->recv_notify_cb) {
            neigh_notify->recv_notify_cb(sckt_data->interface_id, src_address.address);
        }

        //Copy DST address
        memcpy(src_address.address, relay_srv->server_address, 16);
        src_address.type = ADDRESS_IPV6;
        src_address.identifier = DHCPV6_SERVER_PORT;
        tc = IP_DSCP_CS6 << IP_TCLASS_DSCP_SHIFT;

    }
    TRACE(TR_DHCP, "tx-dhcp %-9s dst:%s",
          val_to_str(*(char *)(msghdr.msg_iov[0].iov_base), dhcp_frames, "[UNK]"),
          tr_ipv6(src_address.address));
    socket_setsockopt(sckt_data->socket_id, SOCKET_IPPROTO_IPV6, SOCKET_IPV6_TCLASS, &tc, sizeof(tc));
    socket_sendmsg(sckt_data->socket_id, &msghdr, NS_MSG_LEGACY0);
cleanup:
    free(socket_data);

    return;
}

void recv_dhcp_client_msg(void *cb_res)
{
    ns_address_t address;
    socket_callback_t *sckt_data;
    msg_tr_t *msg_tr_ptr = NULL;
    uint8_t *msg_ptr = NULL;
    int16_t msg_len = 0;
    uint_fast24_t tr_id = 0;
    int retVal = RET_MSG_ACCEPTED;

    sckt_data = cb_res;

    if (sckt_data->event_type != SOCKET_DATA || sckt_data->d_len < 4) {
        return;
    }
    // read actual message
    msg_ptr = malloc(sckt_data->d_len);

    if (msg_ptr == NULL) {
        tr_error("Out of memory");
        goto cleanup;
    }
    msg_len = socket_read(sckt_data->socket_id, &address, msg_ptr, sckt_data->d_len);

    tr_id = read_be24(&msg_ptr[1]);
    msg_tr_ptr = dhcp_tr_find(tr_id);
    TRACE(TR_DHCP, "rx-dhcp %-9s src:%s",
          val_to_str(*msg_ptr, dhcp_frames, "[UNK]"), tr_ipv6(address.address));

    if (msg_tr_ptr == NULL) {
        tr_error("invalid tr id");
        goto cleanup;
    }
    if (0 != libdhcpv6_message_malformed_check(msg_ptr, msg_len)) {
        msg_tr_ptr->recv_resp_cb(msg_tr_ptr->instance_id, msg_tr_ptr->client_obj_ptr, 0, NULL, 0);
        tr_error("Malformed packet");
        goto cleanup;
    }
    // read msg tr id from message and find transaction. and then instance
    // TODO use real function from dhcp lib

    if (msg_tr_ptr != NULL && msg_tr_ptr->recv_resp_cb) {
        // call receive callback should not modify pointers but library requires
        retVal = msg_tr_ptr->recv_resp_cb(msg_tr_ptr->instance_id, msg_tr_ptr->client_obj_ptr, *msg_ptr, msg_ptr + 4, msg_len - 4);
    } else {
        tr_error("no receiver for this message found");
    }

cleanup:
    free(msg_ptr);
    if (retVal != RET_MSG_WAIT_ANOTHER) {
        //Transaction is not killed yet
        dhcp_tr_delete(dhcp_tr_find(tr_id));
    }
    return ;
}

uint16_t dhcp_service_init(int8_t interface_id, dhcp_instance_type_e instance_type, dhcp_service_receive_req_cb *receive_req_cb)
{
    uint16_t id = 1;
    server_instance_t *srv_ptr;
    struct sockaddr_in6 sockaddr = { .sin6_family = AF_INET6, .sin6_addr = IN6ADDR_ANY_INIT, .sin6_port = htons(DHCPV6_SERVER_PORT) };

    if (!dhcp_service_allocate()) {
        tr_error("dhcp Sockets data base alloc fail");
        return 0;
    }
    if (instance_type == DHCP_INSTANCE_SERVER && dhcp_service->dhcp_server_socket < 0) {
        if (dhcp_service->dhcp_relay_socket >= 0) {
            tr_error("dhcp Server socket can't open because Agent open already");
        }
        dhcp_service->dhcp_server_socket = socket(AF_INET6, SOCK_DGRAM, 0);
        if (dhcp_service->dhcp_server_socket < 0)
            FATAL(1, "%s: socket: %m", __func__);
        if (bind(dhcp_service->dhcp_server_socket, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0)
            FATAL(1, "%s: bind: %m", __func__);
    }

    if (instance_type == DHCP_INTANCE_RELAY_AGENT && dhcp_service->dhcp_relay_socket < 0) {
        if (dhcp_service->dhcp_server_socket >= 0) {
            tr_error("dhcp Relay agent can't open because server open already");
        }
        dhcp_service->dhcp_relay_socket = socket_open(SOCKET_UDP, DHCPV6_SERVER_PORT, recv_dhcp_relay_msg);
    }

    if (instance_type == DHCP_INSTANCE_CLIENT && dhcp_service->dhcp_client_socket < 0) {
        dhcp_service->dhcp_client_socket = socket_open(SOCKET_UDP, DHCPV6_CLIENT_PORT, recv_dhcp_client_msg);
    }
    if (instance_type == DHCP_INSTANCE_SERVER && dhcp_service->dhcp_server_socket < 0) {
        tr_error("No sockets available for DHCP server");
        return 0;
    }
    if (instance_type == DHCP_INSTANCE_CLIENT && dhcp_service->dhcp_client_socket < 0) {
        tr_error("No sockets available for DHCP client");
        return 0;
    }

    if (instance_type == DHCP_INTANCE_RELAY_AGENT) {
        if (dhcp_service->dhcp_relay_socket < 0) {
            tr_error("No sockets available for DHCP server");
        }

        uint16_t temp_id = dhcp_service_relay_interface_get(interface_id);
        if (temp_id) {
            return temp_id;
        }
    }

    for (; id < MAX_SERVERS; id++) {
        if (dhcp_service_client_find(id) == NULL) {
            break;
        }
    }
    srv_ptr = malloc(sizeof(server_instance_t));
    if (id == MAX_SERVERS || srv_ptr == NULL) {
        tr_error("Out of server instances");
        free(srv_ptr);
        return 0;
    }

    if (instance_type == DHCP_INTANCE_RELAY_AGENT) {
        //Allocate Realay Agent
        relay_instance_t *relay_srv = malloc(sizeof(relay_instance_t));
        if (!relay_srv) {
            tr_error("Out of realy instances");
            free(srv_ptr);
            return 0;
        }
        ns_list_add_to_start(&dhcp_service->relay_list, relay_srv);
        relay_srv->instance_id = id;
        relay_srv->interface_id = interface_id;
        relay_srv->relay_activated = false;
    }

    ns_list_add_to_start(&dhcp_service->srv_list, srv_ptr);
    srv_ptr->instance_id = id;
    srv_ptr->instance_type = instance_type;
    srv_ptr->interface_id = interface_id;
    srv_ptr->recv_req_cb = receive_req_cb;
    return srv_ptr->instance_id;
}

void dhcp_service_relay_instance_enable(uint16_t instance, uint8_t *server_address)
{
    relay_instance_t *relay_srv = dhcp_service_relay_find(instance);
    if (relay_srv) {
        relay_srv->relay_activated = true;
        memcpy(relay_srv->server_address, server_address, 16);
    }
}

uint8_t *dhcp_service_relay_global_addres_get(uint16_t instance)
{
    relay_instance_t *relay_srv = dhcp_service_relay_find(instance);
    if (!relay_srv || !relay_srv->relay_activated) {
        return NULL;
    }

    return relay_srv->server_address;
}

void dhcp_service_delete(uint16_t instance)
{
    server_instance_t *srv_ptr;
    if (dhcp_service == NULL) {
        return;
    }
    srv_ptr = dhcp_service_client_find(instance);
    //TODO delete all transactions
    if (srv_ptr != NULL) {
        ns_list_remove(&dhcp_service->srv_list, srv_ptr);
        if (srv_ptr->instance_type == DHCP_INTANCE_RELAY_AGENT) {
            //Free relay service
            relay_instance_t *relay = dhcp_service_relay_find(instance);
            if (relay) {
                ns_list_remove(&dhcp_service->relay_list, relay);
                free(relay);
            }
        }
        free(srv_ptr);

    }
    ns_list_foreach_safe(msg_tr_t, cur_ptr, &dhcp_service->tr_list) {
        if (cur_ptr->instance_id == instance) {
            dhcp_tr_delete(cur_ptr);
        }
    }

    int8_t server_instances = 0, client_instances = 0, relay_instances = 0;

    ns_list_foreach(server_instance_t, srv, &dhcp_service->srv_list) {
        if (srv->instance_type == DHCP_INSTANCE_SERVER) {
            ++server_instances;
        } else if (srv->instance_type == DHCP_INSTANCE_CLIENT) {
            ++client_instances;
        } else if (srv->instance_type == DHCP_INTANCE_RELAY_AGENT) {
            ++relay_instances;
        }
    }

    if ((server_instances == 0 && relay_instances == 0) && dhcp_service->dhcp_server_socket > -1) {
        close(dhcp_service->dhcp_server_socket);
        dhcp_service->dhcp_server_socket = -1;
    }

    if (client_instances == 0 && dhcp_service->dhcp_client_socket > -1) {
        socket_close(dhcp_service->dhcp_client_socket);
        dhcp_service->dhcp_client_socket = -1;
    }
    return;
}
uint32_t dhcp_service_send_req(uint16_t instance_id, uint8_t options, void *ptr, const uint8_t addr[static 16], uint8_t *msg_ptr, uint16_t msg_len, dhcp_service_receive_resp_cb *receive_resp_cb, uint16_t delay_tx)
{
    msg_tr_t *msg_tr_ptr;
    server_instance_t *srv_ptr;
    srv_ptr = dhcp_service_client_find(instance_id);
    msg_tr_ptr = dhcp_tr_create();

    if (msg_tr_ptr == NULL || srv_ptr == NULL || msg_ptr == NULL || receive_resp_cb == NULL || msg_len < 5) {
        tr_error("Request sending failed");
        return 0;
    }

    msg_tr_ptr->msg_ptr = msg_ptr;
    msg_tr_ptr->msg_len = msg_len;
    msg_tr_ptr->options = options;
    msg_tr_ptr->client_obj_ptr = ptr;
    memcpy(msg_tr_ptr->addr.address, addr, 16);
    msg_tr_ptr->addr.identifier = DHCPV6_SERVER_PORT;
    msg_tr_ptr->addr.type = ADDRESS_IPV6;
    msg_tr_ptr->interface_id = srv_ptr->interface_id;
    msg_tr_ptr->instance_id = instance_id;
    msg_tr_ptr->socket = dhcp_service->dhcp_client_socket;
    msg_tr_ptr->recv_resp_cb = receive_resp_cb;
    msg_tr_ptr->delayed_tx = delay_tx;
    msg_tr_ptr->first_transmit_time = 0;
    msg_tr_ptr->transmit_time = 0;
    dhcp_tr_set_retry_timers(msg_tr_ptr, msg_tr_ptr->msg_ptr[0]);
    write_be24(&msg_tr_ptr->msg_ptr[1], msg_tr_ptr->msg_tr_id);

    dhcp_service_send_message(msg_tr_ptr);
    return msg_tr_ptr->msg_tr_id;
}

void dhcp_service_set_retry_timers(uint32_t msg_tr_id, uint16_t timeout_init, uint16_t timeout_max, uint8_t retrans_max)
{
    msg_tr_t *msg_tr_ptr;
    msg_tr_ptr = dhcp_tr_find(msg_tr_id);

    if (msg_tr_ptr != NULL) {
        msg_tr_ptr->timeout_init = rand_randomise_base(timeout_init * 10, RAND1_LOW, RAND1_HIGH);
        msg_tr_ptr->timeout = msg_tr_ptr->timeout_init;
        msg_tr_ptr->timeout_max = timeout_max * 10;
        msg_tr_ptr->retrans_max = retrans_max;
    }
    return;
}

void dhcp_service_update_server_address(uint32_t msg_tr_id, uint8_t *server_address)
{
    msg_tr_t *msg_tr_ptr;
    msg_tr_ptr = dhcp_tr_find(msg_tr_id);

    if (msg_tr_ptr != NULL) {
        memcpy(msg_tr_ptr->addr.address, server_address, 16);
    }
}

void dhcp_service_req_remove(uint32_t msg_tr_id)
{
    if (dhcp_service) {
        dhcp_tr_delete(dhcp_tr_find(msg_tr_id));
    }
    return;
}

void dhcp_service_req_remove_all(void *msg_class_ptr)
{
    if (dhcp_service) {
        ns_list_foreach_safe(msg_tr_t, cur_ptr, &dhcp_service->tr_list) {
            if (cur_ptr->client_obj_ptr == msg_class_ptr) {
                dhcp_tr_delete(cur_ptr);
            }
        }
    }
}

void dhcp_service_send_message(msg_tr_t *msg_tr_ptr)
{
    int8_t retval;
    int16_t multicast_hop_limit = -1;
    const uint32_t address_pref = SOCKET_IPV6_PREFER_SRC_6LOWPAN_SHORT;
    int16_t tc;
    uint8_t relay_header[4];
    struct msghdr msghdr = { };
    struct iovec data_vector[4];
    uint32_t t;
    uint16_t cs;
    uint8_t *ptr;
    dhcp_options_msg_t elapsed_time;

    if (msg_tr_ptr->first_transmit_time && libdhcpv6_message_option_discover((msg_tr_ptr->msg_ptr + 4), (msg_tr_ptr->msg_len - 4), DHCPV6_ELAPSED_TIME_OPTION, &elapsed_time) == 0 &&
            elapsed_time.len == 2) {

        t = g_monotonic_time_100ms - msg_tr_ptr->first_transmit_time; // time in 1/10s ticks
        if (t > 0xffff / 10) {
            cs = 0xffff;
        } else {
            cs = (uint16_t) t * 10;
        }
        write_be16(elapsed_time.msg_ptr, cs);
    }

    if ((msg_tr_ptr->options & TX_OPT_USE_SHORT_ADDR) == TX_OPT_USE_SHORT_ADDR) {
        socket_setsockopt(msg_tr_ptr->socket, SOCKET_IPPROTO_IPV6, SOCKET_IPV6_ADDR_PREFERENCES, &address_pref, sizeof address_pref);
    }
    if ((msg_tr_ptr->options & TX_OPT_MULTICAST_HOP_LIMIT_64) == TX_OPT_MULTICAST_HOP_LIMIT_64) {
        multicast_hop_limit = 64;
    }
    socket_setsockopt(msg_tr_ptr->socket, SOCKET_IPPROTO_IPV6, SOCKET_IPV6_MULTICAST_HOPS, &multicast_hop_limit, sizeof multicast_hop_limit);
    socket_setsockopt(msg_tr_ptr->socket, SOCKET_IPPROTO_IPV6, SOCKET_INTERFACE_SELECT, &msg_tr_ptr->interface_id, sizeof(int8_t));

    if (msg_tr_ptr->relay_start) {
        //Build Relay Reply only server do this
        tc = IP_DSCP_CS6 << IP_TCLASS_DSCP_SHIFT;
        socket_setsockopt(msg_tr_ptr->socket, SOCKET_IPPROTO_IPV6, SOCKET_IPV6_TCLASS, &tc, sizeof(tc));
        libdhcpv6_dhcp_option_header_write(relay_header, DHCPV6_OPTION_RELAY, msg_tr_ptr->msg_len);
        msghdr.msg_iovlen = 0;
        memcpy(msg_tr_ptr->addr.address, msg_tr_ptr->relay_start + 2, 16);
        msg_tr_ptr->addr.identifier = DHCPV6_SERVER_PORT;
        //SET IOV vectors
        //Relay Reply
        data_vector[msghdr.msg_iovlen].iov_base = (void *) msg_tr_ptr->relay_start;
        data_vector[msghdr.msg_iovlen].iov_len = DHCPV6_RELAY_LENGTH;
        msghdr.msg_iovlen++;
        if (msg_tr_ptr->opt_interface_id) {
            data_vector[msghdr.msg_iovlen].iov_base = (void *)(msg_tr_ptr->opt_interface_id - 4);
            data_vector[msghdr.msg_iovlen].iov_len = msg_tr_ptr->opt_interface_id_length + 4;
            msghdr.msg_iovlen++;
        }
        //Relay reply header
        data_vector[msghdr.msg_iovlen].iov_base = (void *) relay_header;
        data_vector[msghdr.msg_iovlen].iov_len = 4;
        msghdr.msg_iovlen++;
        //DHCPV normal message vector
        data_vector[msghdr.msg_iovlen].iov_base = (void *) msg_tr_ptr->msg_ptr;
        data_vector[msghdr.msg_iovlen].iov_len = msg_tr_ptr->msg_len;
        msghdr.msg_iovlen++;

        //Set message name

        msghdr.msg_name = (void *) &msg_tr_ptr->addr;
        msghdr.msg_namelen = sizeof(ns_address_t);

        msghdr.msg_iov = &data_vector[0];
        //No ancillary data
        msghdr.msg_control = NULL;
        msghdr.msg_controllen = 0;

        ptr = msg_tr_ptr->relay_start;
        *ptr = DHCPV6_RELAY_REPLY;
        if (msg_tr_ptr->delayed_tx) {
            retval = 0;
        } else {
            TRACE(TR_DHCP, "tx-dhcp %-9s dst:%s",
                  val_to_str(*(char *)(msghdr.msg_iov[0].iov_base), dhcp_frames, "[UNK]"),
                  tr_ipv6(msg_tr_ptr->addr.address));
            retval = socket_sendmsg(msg_tr_ptr->socket, &msghdr, NS_MSG_LEGACY0);
        }

    } else {
        if (msg_tr_ptr->delayed_tx) {
            retval = 0;
        } else {
            tc = 0;
            TRACE(TR_DHCP, "tx-dhcp %-9s dst:%s",
                  val_to_str(*msg_tr_ptr->msg_ptr, dhcp_frames, "[UNK]"),
                  tr_ipv6(msg_tr_ptr->addr.address));
            socket_setsockopt(msg_tr_ptr->socket, SOCKET_IPPROTO_IPV6, SOCKET_IPV6_TCLASS, &tc, sizeof(tc));
            retval = socket_sendto(msg_tr_ptr->socket, &msg_tr_ptr->addr, msg_tr_ptr->msg_ptr, msg_tr_ptr->msg_len);
            msg_tr_ptr->transmit_time = g_monotonic_time_100ms ? g_monotonic_time_100ms : 1;
            if (msg_tr_ptr->first_transmit_time == 0 && retval == 0) {
                //Mark first pushed message timestamp
                msg_tr_ptr->first_transmit_time = g_monotonic_time_100ms ? g_monotonic_time_100ms : 1;
            }
        }
    }
    if (retval != 0) {
        tr_warn("dhcp service socket_sendto fails: %i", retval);
    }
}
bool dhcp_service_timer_tick(uint16_t ticks)
{
    bool activeTimerNeed = false;
    ns_list_foreach_safe(msg_tr_t, cur_ptr, &dhcp_service->tr_list) {

        if (cur_ptr->delayed_tx) {
            activeTimerNeed = true;
            if (cur_ptr->delayed_tx <= ticks) {
                cur_ptr->delayed_tx = 0;
                dhcp_service_send_message(cur_ptr);
            } else {
                cur_ptr->delayed_tx -= ticks;
            }
            continue;
        }

        if (cur_ptr->timeout == 0) {
            continue;
        }

        if (cur_ptr->timeout <= ticks) {
            activeTimerNeed = true;
            cur_ptr->retrans++;
            if (cur_ptr->retrans_max != 0 && cur_ptr->retrans >= cur_ptr->retrans_max) {
                // retransmission count exceeded.
                cur_ptr->recv_resp_cb(cur_ptr->instance_id, cur_ptr->client_obj_ptr, 0, NULL, 0);
                dhcp_tr_delete(cur_ptr);
                continue;
            }
            if (memcmp(cur_ptr->addr.address, ADDR_LINK_LOCAL_PREFIX, 8) == 0)
                FATAL(1, "external dhcp server couldn't be reached");
            dhcp_service_send_message(cur_ptr);
            // RFC 3315 says:
            //     RT = 2*RTprev + RAND*RTprev,
            // We calculate this as
            //     RT = RTprev + (1+RAND)*RTprev
            cur_ptr->timeout = cur_ptr->timeout_init + rand_randomise_base(cur_ptr->timeout_init, RAND1_LOW, RAND1_HIGH);
            // Catch 16-bit integer overflow
            if (cur_ptr->timeout < cur_ptr->timeout_init) {
                cur_ptr->timeout = 0xFFFF;
            }
            // Check against MRT
            if (cur_ptr->timeout_max != 0 && cur_ptr->timeout > cur_ptr->timeout_max) {
                cur_ptr->timeout = rand_randomise_base(cur_ptr->timeout_max, RAND1_LOW, RAND1_HIGH);
            }
            cur_ptr->timeout_init = cur_ptr->timeout;
        } else {
            cur_ptr->timeout -= ticks;
            activeTimerNeed = true;
        }
    }
    return activeTimerNeed;
}

int dhcp_service_link_local_rx_cb_set(int8_t interface_id, dhcp_relay_neighbour_cb *notify_cb)
{
    if (dhcp_service == NULL) {
        return -1;
    }

    relay_notify_t *notify_srv = dhcp_service_notify_find(interface_id);
    if (notify_srv) {
        notify_srv->recv_notify_cb = notify_cb;
        return 0;
    }


    notify_srv = malloc(sizeof(relay_notify_t));
    if (!notify_srv) {
        return -1;
    }
    ns_list_add_to_start(&dhcp_service->notify_list, notify_srv);

    notify_srv->recv_notify_cb = notify_cb;
    notify_srv->interface_id = interface_id;
    return 0;
}
