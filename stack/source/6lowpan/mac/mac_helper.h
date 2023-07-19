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
#ifndef MAC_HELPER_H
#define MAC_HELPER_H

#include "stack/mac/mlme.h"

struct net_if;
struct ns_sockaddr;
struct buffer;
struct mac_api;
enum addrtype;

uint16_t mac_helper_panid_get(const struct net_if *interface);
void mac_helper_set_default_key_source(struct net_if *interface);
int8_t mac_helper_security_key_to_descriptor_set(struct net_if *interface, const uint8_t *key, uint8_t id, uint8_t descriptor);
int8_t mac_helper_security_key_descriptor_clear(struct net_if *interface, uint8_t descriptor);
bool mac_helper_write_our_addr(struct net_if *interface, struct ns_sockaddr *ptr);
uint_fast16_t mac_helper_max_payload_size(struct net_if *cur, uint_fast16_t frame_overhead);
uint_fast8_t mac_helper_frame_overhead(struct net_if *cur, const struct buffer *buf);
int8_t mac_helper_key_link_frame_counter_read(int8_t interface_id, uint32_t *seq_ptr, uint8_t descriptor);

#endif
