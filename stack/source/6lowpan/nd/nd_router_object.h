/*
 * Copyright (c) 2013-2017, Pelion and affiliates.
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
#ifndef ND_ROUTER_OBJECT_H_
#define ND_ROUTER_OBJECT_H_
#include <stdint.h>
#include <stdbool.h>

#include "6lowpan/nd/nd_defines.h"

struct nd_parameters;
enum nwk_interface_id;
enum addrtype;

#define ND_OBJECT_MAX 1

#define ND_MAX_PROXY_CONTEXT_COUNT 5
#define ND_MAX_PROXY_PREFIX_COUNT 5

/* RFC4861 AdvCurHopLimit: value placed in Router Advertisement Cur Hop Limit */
#ifndef ADV_CUR_HOP_LIMIT
#define ADV_CUR_HOP_LIMIT 64
#endif
extern uint8_t nd_base_tick;
extern struct nd_parameters nd_params;
struct aro;

#ifdef HAVE_WS_BORDER_ROUTER
int8_t icmp_nd_router_prefix_proxy_update(uint8_t *dptr, nd_router_setup_t *nd_router_object);
int8_t nd_set_br(nd_router_t *br);
#else
#define icmp_nd_router_prefix_proxy_update(dptr, nd_router_object) -1
#define nd_set_br(br) -1
#endif
void icmp_nd_prefixs_parse(buffer_t *buf, nd_router_t *nd_router_object, struct net_if *cur_interface);
int8_t icmp_nd_router_prefix_update(uint8_t *dptr, nd_router_t *nd_router_object, struct net_if *cur_interface);
void gp_address_add_to_end(gp_ipv6_address_list_t *list, const uint8_t address[static 16]);
void gp_address_list_free(gp_ipv6_address_list_t *list);
uint8_t nd_set_adr_by_dest_prefix(uint8_t *ptr, uint8_t *prefix);
bool nd_object_active(void);
void icmp_nd_set_nd_def_router_address(uint8_t *ptr, nd_router_t *cur);
nd_router_t *icmp_nd_router_object_get(const uint8_t *border_router, enum nwk_interface_id nwk_id);
void icmp_nd_set_next_hop(nd_router_next_hop *hop, sockaddr_t *adr);



/** 6LoWPAN specific ICMP message Handler */
buffer_t *nd_dar_parse(buffer_t *buf, struct net_if *cur_interface);
buffer_t *nd_dac_handler(buffer_t *buf, struct net_if *cur);
void nd_ns_build(nd_router_t *cur, struct net_if *cur_interface, uint8_t *address_ptr);
int8_t nd_parent_loose_indcate(uint8_t *neighbor_address, struct net_if *cur_interface);

void nd_router_base_init(nd_router_t *new_entry);


void icmp_nd_routers_init(void);

ipv6_ra_timing_t *nd_ra_timing(const uint8_t abro[16]);
void nd_ra_build_by_abro(const uint8_t *abro, const uint8_t *dest, struct net_if *cur_interface);
void nd_trigger_ras_from_rs(const uint8_t *unicast_adr, struct net_if *cur_interface);
/** 6LoWPAN specific ICMP message Handler */
bool nd_ns_aro_handler(struct net_if *cur_interface, const uint8_t *aro_opt, const uint8_t *slaa_opt, const uint8_t *target, struct aro *aro_out);
void nd_remove_registration(struct net_if *cur_interface, enum addrtype ll_type, const uint8_t *ll_address);

nd_router_t *nd_get_pana_address(void);

/** ND Routing Part */
uint8_t nd_prefix_dst_check(uint8_t *ptr);
nd_router_t *nd_get_object_by_nwk_id(nwk_interface_id_e nwk_id);
/* Original ABRO-based all-in-one parser. This needs some rework to separate ABRO-related and unrelated bits */
/* Returns "false" if ABRO suggested it was a stale message, so not worth handling in the normal code */
bool nd_ra_process_abro(struct net_if *cur, buffer_t *buf, const uint8_t *dptr, uint8_t ra_flags, uint16_t router_lifetime);
void nd_object_timer(int ticks_update);
uint32_t nd_object_time_to_next_nd_reg(void);

void icmp_nd_router_object_reset(nd_router_t *router_object);
void icmp_nd_border_router_release(nd_router_t *router_object);
void nd_6lowpan_set_radv_params(struct net_if *cur_interface);
#endif
