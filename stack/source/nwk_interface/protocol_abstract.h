/*
 * Copyright (c) 2015-2018, Pelion and affiliates.
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

#ifndef NWK_INTERFACE_INCLUDE_PROTOCOL_ABSTRACT_H_
#define NWK_INTERFACE_INCLUDE_PROTOCOL_ABSTRACT_H_

#include <stdint.h>

struct rpl_domain;
struct fhss_api;

/*!
 * \enum nwk_interface_id_e
 * \brief Determines the ID of the network interface.
 */
/** Network interface IDs. */
typedef enum nwk_interface_id {
    IF_6LoWPAN,
    IF_IPV6,
} nwk_interface_id_e;

extern int protocol_core_buffers_in_event_queue;

struct net_if *protocol_stack_interface_info_get_by_id(int8_t nwk_id);
struct net_if *protocol_stack_interface_info_get_by_bootstrap_id(int8_t id);
struct net_if *protocol_stack_interface_info_get_by_rpl_domain(const struct rpl_domain *domain, int8_t last_id);
struct net_if *protocol_stack_interface_info_get_by_fhss_api(const struct fhss_api *fhss_api);
struct net_if *protocol_stack_interface_info_get_wisun_mesh(void);

#endif
