/*
 * Copyright (c) 2016-2017, 2019-2020, Pelion and affiliates.
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

/**
 * \file net_ipv6_api.h
 * \brief IPv6 configuration API.
 */

#ifndef NET_IPV6_API_H_
#define NET_IPV6_API_H_
#include <stdint.h>
#include <stdbool.h>

/**
 * \brief Set maximum IPv6 fragmented datagram reception size.
 *
 * Set the maximum size limit for fragmented datagram reception.
 *
 * RFC 2460 requires this to be at least 1500. It should also be at least
 * as large as the MTU of each attached link.
 *
 * \param frag_mru The fragmented Maximum Receive Unit in octets.
 * \return 0 Change OK - actual MRU is at least the requested value.
 * \return <0 Change invalid - unable to set the specified MRU.
 */
int8_t arm_nwk_ipv6_frag_mru(uint16_t frag_mru);

/**
 * \brief Set the maximum number of entries for the neighbour cache and
 * destination cache. Default value is 64 and minimum allowed value is 4.
 *
 * Note: This must be called before arm_nwk_interface_lowpan_init()
 *
 * \param max_entries The absolute maximum entries allowed in cache at any time.
 * \return 0 Change OK.
 * \return <0 Change invalid - unable to change the maximum for cache.
 */
int8_t arm_nwk_ipv6_max_cache_entries(uint16_t max_entries);

/**
 * \brief Configure destination cache.
 *
 * Set destination cache maximum entry count, thresholds where amount of entries is kept during operation and lifetime.
 *
 * Note: This must be called before arm_nwk_interface_lowpan_init()
 *
 * \param max_entries Maximum number of entries allowed in destination cache at any time.
 * \param short_term_threshold Amount of cache entries kept in memory in short term. Must be less than max_entries.
 * \param long_term_threshold Amount of entries kept in memory over long period of time. Must be less than short_term_threshold.
 * \param lifetime Lifetime of cache entry, must be over 120 seconds
 *
 * \return 0 Change OK.
 * \return <0 Change invalid - unable to change the maximum for cache.
 */
int8_t arm_nwk_ipv6_destination_cache_configure(uint16_t max_entries, uint16_t short_term_threshold, uint16_t long_term_threshold, uint16_t lifetime);

/**
 * \brief Configure neighbour cache.
 *
 * Set neighbour cache maximum entry count, thresholds where amount of entries is kept during operation and lifetime.
 *
 * Note: This must be called before arm_nwk_interface_lowpan_init()
 *
 * \param max_entries Maximum number of entries allowed in neighbour cache at any time.
 * \param short_term_threshold Amount of cache entries kept in memory in short term. Must be less than max_entries.
 * \param long_term_threshold Amount of entries kept in memory over long period of time. Must be less than short_term_threshold.
 * \param lifetime Lifetime of cache entry, must be over 120 seconds
 *
 * \return 0 Change OK.
 * \return <0 Change invalid - unable to change the maximum for cache.
 */
int8_t arm_nwk_ipv6_neighbour_cache_configure(uint16_t max_entries, uint16_t short_term_threshold, uint16_t long_term_threshold, uint16_t lifetime);

/**
 * \brief Configure automatic flow label calculation.
 *
 * Enable or disable automatic generation of IPv6 flow labels for outgoing
 * packets.
 *
 * \param auto_flow_label True to enable auto-generation.
 */
void arm_nwk_ipv6_auto_flow_label(bool auto_flow_label);

#endif /* NET_IPV6_API_H_ */
