/*
 * Copyright (c) 2018, Pelion and affiliates.
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

#ifndef MAC_NEIGHBOR_TABLE_H_
#define MAC_NEIGHBOR_TABLE_H_
#include <stdint.h>
#include <stdbool.h>
#include "common/ns_list.h"

#define NEIGHBOR_CLASS_LINK_DEFAULT_LIFETIME 240

#define ACTIVE_NUD_PROCESS_MAX 3 //Limit That how many activate NUD process is active in same time

#define NORMAL_NEIGHBOUR                0
#define SECONDARY_PARENT_NEIGHBOUR      1
#define CHILD_NEIGHBOUR                 2
#define PRIORITY_PARENT_NEIGHBOUR       3
/**
 * Generic Neighbor table entry
 */
typedef struct mac_neighbor_table_entry {
    uint8_t         index;                  /*!< Unique Neighbour index */
    uint8_t         mac64[8];               /*!< MAC64 */
    uint16_t        mac16;                  /*!< MAC16 address for neighbor 0xffff when no 16-bit address is unknown */
    uint32_t        lifetime;               /*!< Life time in seconds which goes down */
    uint32_t        link_lifetime;          /*!< Configured link timeout*/
    uint8_t         phy_mode_ids[16];       /*!< List of PhyModeId supported by this neighbor */
    uint8_t         phy_mode_id_count;      /*!< Number of PhyModeId in phy_mode_ids */
    uint8_t         ms_phy_mode_id;         /*!< PhyModeId selected for Mode Switch with this neighbor */
    uint8_t         ms_mode;                /*!< Mode switch mode */
    uint32_t        ms_tx_count;            /*!< Mode switch Tx success count */ // TODO: implement fallback mechanism in wbsrd
    uint32_t        ms_retries_count;       /*!< Mode switch Tx retries */ // TODO: implement fallback mechanism in wsbrd
    bool            mdr_capable:1;          /*!< Indicate if the neighbor supports MAC Mode Switch */
    bool            connected_device: 1;    /*!< True Link is connected and data rx is accepted , False RX data is not accepted*/
    bool            trusted_device: 1;      /*!< True mean use normal group key, false for enable pairwise key */
    bool            nud_active: 1;          /*!< True Neighbor NUD process is active, false not active process */
    unsigned        link_role: 2;           /*!< Link role: NORMAL_NEIGHBOUR, PRIORITY_PARENT_NEIGHBOUR, SECONDARY_PARENT_NEIGHBOUR, CHILD_NEIGHBOUR */
    uint8_t         node_role;
    ns_list_link_t  link;
} mac_neighbor_table_entry_t;

typedef NS_LIST_HEAD(mac_neighbor_table_entry_t, link) mac_neighbor_table_list_t;

/**
 * Remove entry notify
 *
 * \param entry_ptr Pointer to removed entry
 * \param user_data pointer for user to detect interface
 */
typedef void neighbor_entry_remove_notify(mac_neighbor_table_entry_t *entry_ptr, void *user_data);


/**
 * NUD entry notify
 *
 * \param entry_ptr Pointer to neighbor entry
 * \param user_data pointer for user to detect interface
 *
 * \return true NUD message generated
 * \return false When NUD is not generated
 */
typedef bool neighbor_entry_nud_notify(mac_neighbor_table_entry_t *entry_ptr, void *user_data);

/**
 * Neighbor table class structure
 */
typedef struct mac_neighbor_table {
    mac_neighbor_table_list_t neighbour_list;               /*!< List of active neighbors */
    mac_neighbor_table_list_t free_list;                    /*!< List of free neighbors entries */
    uint32_t nud_threshold;                                 /*!< NUD threshold time which generates keep alive message */
    uint8_t list_total_size;                                /*!< Total number allocated neighbor entries */
    uint8_t active_nud_process;                             /*!< Indicate Active NUD Process */
    uint8_t neighbour_list_size;                            /*!< Active Neighbor list size */
    void *table_user_identifier;                            /*!< Table user identifier like interface pointer */
    neighbor_entry_remove_notify *user_remove_notify_cb;    /*!< Neighbor Remove Callback notify */
    neighbor_entry_nud_notify *user_nud_notify_cb;          /*!< Trig NUD process for neighbor */
    mac_neighbor_table_entry_t neighbor_entry_buffer[];     /*!< Pointer for allocated neighbor table entries*/
} mac_neighbor_table_t;


/**
 * \brief mac_neighbor_table_create Allocate Neighbour table class
 *
 * Call this only one's for interface
 *
 * \param table_size size of neighbor table
 * \param remove_cb callback pointer for notify removed neighbor
 * \param nud_cb Interface NUD operation trgger callback
 * \param user_indentifier user identifier pointer like interface pointer
 *
 * \return pointer to neighbor table class when create is OK
 * \return NULL when memory allocation happen
 *
 */
mac_neighbor_table_t *mac_neighbor_table_create(uint8_t table_size, neighbor_entry_remove_notify *remove_cb, neighbor_entry_nud_notify *nud_cb, void *user_indentifier);

/**
 * mac_neighbor_table_delete Delete Neigbor table class
 *
 * \param table_class neighbor table class
 */
void mac_neighbor_table_delete(mac_neighbor_table_t *table_class);

/**
 * mac_neighbor_table_neighbor_list_clean Clean neighbour_list from giving class
 */
void mac_neighbor_table_neighbor_list_clean(mac_neighbor_table_t *table_class);

/**
 * mac_neighbor_table_neighbor_timeout_update Update Neighbor table timeout values
 *
 * \param interface pointer to interface
 * \param time_update in seconds
 *
 */
void mac_neighbor_table_neighbor_timeout_update(int time_update);


/**
 * mac_neighbor_table_entry_allocate Allocate Neighbour table class entry
 *
 * \param table_class pointer to table class
 * \param mac64 neighbor 64-bit mac address
 *
 * \return NULL allocate fail
 * \return pointer to allocated neighbor table entry
 */
mac_neighbor_table_entry_t *mac_neighbor_table_entry_allocate(mac_neighbor_table_t *table_class, const uint8_t *mac64);

/**
 * mac_neighbor_table_neighbor_remove Remove neighbor from list
 *
 * \param table_class pointer to table class
 * \param neighbor_entry pointer to removed entry
 *
 */
void mac_neighbor_table_neighbor_remove(mac_neighbor_table_t *table_class, mac_neighbor_table_entry_t *neighbor_entry);

/**
 * mac_neighbor_table_neighbor_refresh Refresh neigbor timeout and time to live values based on giving time
 *
 * \param table_class pointer to table class
 * \param neighbor_entry pointer to refreshed entry
 * \param life_time define lifetime for neighbor
 */
void mac_neighbor_table_neighbor_refresh(mac_neighbor_table_t *table_class, mac_neighbor_table_entry_t *neighbor_entry, uint32_t life_time);

/**
 * mac_neighbor_table_neighbor_connected Mark neighbour connected state and data is accepted from device
 *
 * Call this function when node is trusted connected
 *
 * \param table_class pointer to table class
 * \param neighbor_entry pointer to refreshed entry
 */
void mac_neighbor_table_neighbor_connected(mac_neighbor_table_t *table_class, mac_neighbor_table_entry_t *neighbor_entry);

/**
 * mac_neighbor_table_trusted_neighbor Function for manage neighbor role at mesh network
 *
 * Call this function when node is trusted connected
 *
 * \param table_class pointer to table class
 * \param neighbor_entry pointer to refreshed entry
 * \param trusted_device True neigbor is part of mesh and will use group key , false enable pairwose key
 */
void mac_neighbor_table_trusted_neighbor(mac_neighbor_table_t *table_class, mac_neighbor_table_entry_t *neighbor_entry, bool trusted_device);

/**
 * mac_neighbor_table_address_discover Discover neighbor from list by address
 *
 *  \param table_class pointer to table class
 *  \param address pointer to 16-bit MAC or 64-bit address for discover
 *  \param address_type 2 for 16-bit address and 3 for 64-bit (same than 802.15.4 define)
 *
 *  \return pointer to discover neighbor entry if it exist
 */
mac_neighbor_table_entry_t *mac_neighbor_table_address_discover(mac_neighbor_table_t *table_class, const uint8_t *address, uint8_t address_type);

/**
 * mac_neighbor_table_attribute_discover Discover neighbor from list by attribute index
 *
 *  \param table_class pointer to table class
 *  \param index neighbor index
 *
 *  \return pointer to discover neighbor entry if it exist
 */
mac_neighbor_table_entry_t *mac_neighbor_table_attribute_discover(mac_neighbor_table_t *table_class, uint8_t index);

mac_neighbor_table_entry_t *mac_neighbor_entry_get_by_ll64(mac_neighbor_table_t *table_class, const uint8_t *ipv6Address, bool allocateNew, bool *new_entry_allocated);

mac_neighbor_table_entry_t *mac_neighbor_entry_get_by_mac64(mac_neighbor_table_t *table_class, const uint8_t *mac64, bool allocateNew, bool *new_entry_allocated);

mac_neighbor_table_entry_t *mac_neighbor_entry_get_priority(mac_neighbor_table_t *table_class);

/**
 * Update a neighbor entry with the last POM-IE received
 *
 * \param neighbor_entry pointer to neighbor
 * \param phy_mode_id_count number of PhyModeId in \p phy_mode_ids
 * \param phy_mode_ids list of supported PhyModeId
 * \param mdr_capable indicate if the device support MDR Mode Switching capability
 */
void mac_neighbor_update_pom(mac_neighbor_table_entry_t *neighbor_entry, uint8_t phy_mode_id_count, const uint8_t *phy_mode_ids, uint8_t mdr_capable);

/**
 * Find a PhyModeId matching both transmitter and received capabilities
 *
 * \details if the receiver POM-IE isn't known, use base operating mode
 *          otherwise use the \p PhyModeId if it part of POM-IE
 *
 * \param neighbor_entry pointer to neighbor (receiver)
 * \
 * \param phy_mode_id phy_mode_id the transmitter want to transit the packet
 */
uint8_t mac_neighbor_find_phy_mode_id(mac_neighbor_table_entry_t *neighbor_entry, uint8_t phy_mode_id);

int mac_neighbor_lfn_count(const struct mac_neighbor_table *table);

#endif
