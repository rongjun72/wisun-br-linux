/*
 * Copyright (c) 2013-2017, Pelion and affiliates.
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
#ifndef ND_ROUTER_OBJECT_H_
#define ND_ROUTER_OBJECT_H_
#include <stdint.h>
#include <stdbool.h>
#include "6lowpan/nd/nd_defines.h"

struct nd_parameters;
struct aro;
enum addrtype;

extern struct nd_parameters nd_params;
struct ipv6_nd_opt_earo;

void icmp_nd_routers_init(void);


bool nd_ns_earo_handler(struct net_if *cur_interface, const uint8_t *earo_ptr, size_t earo_len,
                        const uint8_t *slla_ptr, const uint8_t src_addr[16], const uint8_t target[16],
                        struct ipv6_nd_opt_earo *na_earo);
void nd_remove_registration(struct net_if *cur_interface, enum addrtype ll_type, const uint8_t *ll_address);

#endif
