/*
 * Copyright (c) 2008, 2010-2020, Pelion and affiliates.
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
 *
 * \file ns_address_internal.h
 * \brief address type definitions.
 *
 *  nanoStack: supported address types and associated data structures.
 *
 *
 */


#ifndef _NS_ADDRESS_H
#define _NS_ADDRESS_H
#include <stdbool.h>
#include "common/ns_list.h"

#define ADDR_MULTICAST_MAX 3
#define ADDR_SIZE 16
#define PAN_ID_LEN 2
/** Address types */
typedef enum addrtype {
    ADDR_NONE = 0,                              /*!< No address */
    ADDR_802_15_4_SHORT = 2,                    /*!< 16-bit PAN with 16-bit 802.15.4 address */
    ADDR_802_15_4_LONG = 3,                     /*!< 16-bit PAN with 64-bit 802.15.4 address */
    ADDR_IPV6 = 4,                              /*!< 128 bit IPv6 address containing both Prefix (high 64 bits) and Interface identifier (low 64 bits) */
    //ADDR_DATA=5,                              /*!< Attribute-based data-centric query */
    ADDR_BROADCAST = 6,                         /*!< Broadcast (inc RFC 4944 multicast) address (obsolescent) */
    ADDR_EUI_48 = 7,                            /*!< 48-bit Extended Unique Identifier (eg Ethernet) */
} addrtype_e;                               /*!< Address types in the stack */

/** Address field */
typedef uint8_t address_t[ADDR_SIZE];

/** Address structure */
typedef struct ns_sockaddr {
    addrtype_e    addr_type;              /*!< Type of address */
    address_t     address;                /*!< Source or destination address */
    uint16_t      port;                   /*!< Source or destination port */
} sockaddr_t;

struct net_if;
struct if_address_entry;
struct socket;

/** \name Flags for SOCKET_IPV6_ADDR_PREFERENCES - opposites 16 bits apart. */
///@{
#define SOCKET_IPV6_PREFER_SRC_TMP              0x00000001 /**< Prefer temporary address (RFC 4941); default. */
#define SOCKET_IPV6_PREFER_SRC_PUBLIC           0x00010000 /**< Prefer public address (RFC 4941). */
#define SOCKET_IPV6_PREFER_SRC_6LOWPAN_SHORT    0x00000100 /**< Prefer 6LoWPAN short address. */
#define SOCKET_IPV6_PREFER_SRC_6LOWPAN_LONG     0x01000000 /**< Prefer 6LoWPAN long address. */
///@}

typedef enum if_address_source {
    ADDR_SOURCE_UNKNOWN,
    ADDR_SOURCE_SLAAC,
    ADDR_SOURCE_DHCP,
    ADDR_SOURCE_STATIC,
    ADDR_SOURCE_THREAD_ALOC,
    ADDR_SOURCE_THREAD_DOMAIN
} if_address_source_e;

typedef enum if_address_callback {
    ADDR_CALLBACK_DAD_COMPLETE, // Duplicate Address Detection complete - address now valid
    ADDR_CALLBACK_DAD_FAILED,   // Duplicate Address Detection failed - address is a duplicate, about to be deleted
    ADDR_CALLBACK_PARENT_FULL,  // Neighbour Cache full at parent or 6LBR - couldn't register address, about to be deleted
    ADDR_CALLBACK_TIMER,        // state_timer reached 0
    ADDR_CALLBACK_DEPRECATED,   // preferred lifetime reached 0
    ADDR_CALLBACK_INVALIDATED,  // valid lifetime reached 0 - about to be deleted, unless callback increases lifetime
    ADDR_CALLBACK_REFRESHED,    // valid lifetime updated (up or down, aside from usual expiry)
    ADDR_CALLBACK_DELETED,      // address is deleted (no longer on interface at point of call)
} if_address_callback_e;

typedef void if_address_callback_fn(struct net_if *interface, struct if_address_entry *addr, if_address_callback_e reason);
typedef void if_address_notification_fn(struct net_if *interface, const struct if_address_entry *addr, if_address_callback_e reason);

typedef struct if_address_entry {
    uint8_t address[16];        // IPv6 (or IPv4-mapped IPv6 in future)
    uint8_t prefix_len;         // length of prefix part
    uint32_t valid_lifetime;    // seconds remaining; 0xffffffff = infinite
    uint32_t preferred_lifetime; // seconds remaining; 0 if deprecated, 0xffffffff = infinite
    uint32_t state_timer;       // ticks to state change - used by DAD, then can be used by protocol
    uint8_t count;              // general count field - used by DAD, then can be used by protocol
    bool temporary: 1;          // RFC 4941 temporary address
    bool tentative: 1;          // Tentative address (Duplicate Address Detection running)
    bool group_added: 1;        // Solicited-Node group added
    if_address_source_e source; //
    if_address_callback_fn *cb; // Address protocol callback function
    void *data;                 // Address protocol data
    ns_list_link_t link;
} if_address_entry_t;

typedef NS_LIST_HEAD(if_address_entry_t, link) if_address_list_t;

/* Groups we are a member of on an interface */
typedef struct if_group_entry {
    uint8_t group[16];
    bool mld_last_reporter: 1;
    uint16_t mld_timer;         // Or used by MLD alternative, eg Thread registration
    uint16_t ref_count;
    ns_list_link_t link;
} if_group_entry_t;

typedef NS_LIST_HEAD(if_group_entry_t, link) if_group_list_t;

/* Groups we will forward onto an interface */
typedef struct if_group_fwd_entry {
    uint8_t group[16];
    uint32_t lifetime;
    ns_list_link_t link;
} if_group_fwd_entry_t;

typedef NS_LIST_HEAD(if_group_fwd_entry_t, link) if_group_fwd_list_t;

extern uint32_t addr_preferences_default;   // default SOCKET_IPV6_ADDR_PREFERENCES

extern const uint8_t ADDR_SHORT_ADR_SUFFIC[6];                      // 0000:00ff:fe00
extern const uint8_t ADDR_LINK_LOCAL_PREFIX[8];                     // fe80::/64
extern const uint8_t ADDR_MULTICAST_SOLICITED[13];                  // ff02::1:ffxx:xxxx/104
extern const uint8_t ADDR_IF_LOCAL_ALL_NODES[16];                   // ff01::1
extern const uint8_t ADDR_IF_LOCAL_ALL_ROUTERS[16];                 // ff01::2
extern const uint8_t ADDR_LINK_LOCAL_ALL_NODES[16];                 // ff02::1
extern const uint8_t ADDR_LINK_LOCAL_ALL_ROUTERS[16];               // ff02::2
extern const uint8_t ADDR_LINK_LOCAL_ALL_MLDV2_ROUTERS[16];         // ff02::16
extern const uint8_t ADDR_LINK_LOCAL_MDNS[16];                      // ff02::fb
extern const uint8_t ADDR_REALM_LOCAL_ALL_NODES[16];                // ff03::1
extern const uint8_t ADDR_REALM_LOCAL_ALL_ROUTERS[16];              // ff03::2
extern const uint8_t ADDR_SITE_LOCAL_ALL_ROUTERS[16];               // ff05::2
extern const uint8_t ADDR_ALL_MPL_FORWARDERS[16];                   // ff03::fc
extern const uint8_t ADDR_ALL_DHCP_RELAY_AGENTS_AND_SERVERS[16];    // ff02::1:2
extern const uint8_t ADDR_LINK_LOCAL_ALL_RPL_NODES[16];             // ff02::1a
extern const uint8_t ADDR_LOOPBACK[16];                             // ::1
extern const uint8_t ADDR_UNSPECIFIED[16];                          // ::
extern const uint8_t ADDR_6TO4[16];                                 // 2002::

/* Don't bother having another 8 zero bytes for this - reuse ADDR_UNSPECIFIED */
#define ADDR_EUI64_ZERO ADDR_UNSPECIFIED

/** Functions provided by address.c */
uint8_t addr_check_broadcast(const address_t addr, addrtype_e addr_type);

void address_module_init(void);
void addr_fast_timer(int ticks);
void addr_slow_timer(int seconds);
struct if_address_entry *addr_add(struct net_if *cur, const uint8_t address[static 16], uint_fast8_t prefix_len, if_address_source_e source, uint32_t valid_lifetime, uint32_t preferred_lifetime, bool skip_dad);
int_fast8_t addr_delete(struct net_if *cur, const uint8_t address[static 16]);
int_fast8_t addr_deprecate(struct net_if *cur, const uint8_t address[static 16]);
void addr_delete_matching(struct net_if *cur, const uint8_t *prefix, uint8_t prefix_len, if_address_source_e source);
void addr_delete_entry(struct net_if *cur, if_address_entry_t *addr);

void addr_notification_register(if_address_notification_fn fn);
void addr_cb(struct net_if *interface, if_address_entry_t *addr, if_address_callback_e reason);
void addr_set_valid_lifetime(struct net_if *interface, if_address_entry_t *entry, uint32_t valid_lifetime);
void addr_set_preferred_lifetime(struct net_if *interface, if_address_entry_t *entry, uint32_t preferred_lifetime);

void addr_lifetime_update(struct net_if *interface, if_address_entry_t *address, uint32_t valid_lifetime, uint32_t preferred_lifetime, uint32_t threshold);

int_fast8_t addr_policy_table_add_entry(const uint8_t *prefix, uint8_t len, uint8_t precedence, uint8_t label);
int_fast8_t addr_policy_table_delete_entry(const uint8_t *prefix, uint8_t len);
uint8_t addr_len_from_type(addrtype_e addr_type);
const char *trace_sockaddr(const sockaddr_t *addr, bool panid_prefix);

const uint8_t *addr_select_source(struct net_if *interface, const uint8_t dest[static 16], uint32_t addr_preferences);
const uint8_t *addr_select_with_prefix(struct net_if *cur, const uint8_t *prefix, uint8_t prefix_len, uint32_t addr_preferences);
int8_t addr_interface_select_source(struct net_if *cur, uint8_t *src_ptr, const uint8_t *dest, uint32_t addr_preferences);
struct if_address_entry *addr_get_entry(const struct net_if *interface, const uint8_t addr[static 16]);
bool addr_is_assigned_to_interface(const struct net_if *interface, const uint8_t addr[static 16]);
bool addr_is_tentative_for_interface(const struct net_if *interface, const uint8_t addr[static 16]);

void addr_policy_remove_by_label(uint8_t label);

void addr_duplicate_detected(struct net_if *interface, const uint8_t addr[static 16]);

struct if_group_entry *addr_add_group(struct net_if *interface, const uint8_t group[static 16]);
void addr_remove_group(struct net_if *interface, const uint8_t group[static 16]);
bool addr_am_group_member_on_interface(const struct net_if *interface, const uint8_t group[static 16]);
struct if_group_entry *addr_get_group_entry(const struct net_if *interface, const uint8_t group[static 16]);
void addr_delete_group(struct net_if *interface, const uint8_t group[static 16]);
void addr_delete_group_entry(struct net_if *interface, if_group_entry_t *entry);

void addr_add_router_groups(struct net_if *interface);

#define addr_is_ipv6_unspecified(addr) (memcmp(addr, ADDR_UNSPECIFIED, 16) == 0)
#define addr_is_ipv6_loopback(addr) (memcmp(addr, ADDR_LOOPBACK, 16) == 0)
bool addr_is_ipv6_link_local(const uint8_t addr[static 16]);
#define addr_is_ipv6_multicast(addr) (*(addr) == 0xFF)
uint_fast8_t addr_ipv6_scope(const uint8_t addr[static 16], const struct net_if *interface);
#define addr_ipv6_multicast_scope(addr) ((addr)[1] & 0x0F)
bool addr_ipv6_equal(const uint8_t a[static 16], const uint8_t b[static 16]);
bool addr_iid_matches_eui64(const uint8_t iid[static 8], const uint8_t eui64[static 8]);
bool addr_iid_matches_lowpan_short(const uint8_t iid[static 8], uint16_t short_addr);
bool addr_iid_from_outer(uint8_t iid_out[static 8], const sockaddr_t *addr_in);

uint8_t *addr_ipv6_write_from_lowpan_short(uint8_t dst[static 16], const uint8_t prefix[static 8], uint16_t short_addr);

void memswap(uint8_t *restrict a, uint8_t *restrict b, uint_fast8_t len);

int addr_interface_set_ll64(struct net_if *cur, if_address_callback_fn *cb);
bool addr_interface_all_address_ready(struct net_if *cur);

/* address_type 0 means "any" address - we return short by preference */
/* address_type 1 means long address - we ignore short addresses */
int8_t addr_interface_get_ll_address(struct net_if *cur, uint8_t *address_ptr, uint8_t address_type);
int8_t addr_interface_gp_prefix_compare(struct net_if *cur, const uint8_t *prefix);
int8_t addr_interface_address_compare(struct net_if *cur, const uint8_t *addr);
#endif /*_NS_ADDRESS_H*/
