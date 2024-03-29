/*
 * Copyright (c) 2015-2021, Pelion and affiliates.
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
#ifndef FHSS_CONFIG_H
#define FHSS_CONFIG_H
#include <stdint.h>

/**
 * \file fhss_config.h
 * \brief
 */

typedef struct fhss_api fhss_api_t;

/**
 * @brief WS channel functions.
 */
typedef enum {
    /** Fixed channel. */
    WS_FIXED_CHANNEL,
    /** TR51 channel function. */
    WS_TR51CF,
    /** Direct Hash channel function. */
    WS_DH1CF,
    /** Vendor Defined channel function. */
    WS_VENDOR_DEF_CF
} fhss_ws_channel_functions_e;

/**
 * @brief Get channel using vendor defined channel function.
 * @param api FHSS instance.
 * @param slot Slot number in channel schedule.
 * @param eui64 EUI-64 address of node for which the (unicast) schedule is calculated. NULL for broadcast schedule.
 * @param bsi Broadcast schedule identifier used in (broadcast) schedule calculation.
 * @param number_of_channels Number of channels in schedule.
 * @return Channel.
 */
typedef int32_t fhss_vendor_defined_cf(const fhss_api_t *api, uint16_t slot, uint8_t eui64[8], uint16_t bsi, uint16_t number_of_channels);

/**
 * \brief Struct fhss_config_parameters defines FHSS configuration parameters.
 *
 */
typedef struct fhss_config_parameters {
    /** Number of channel retries defines how many consecutive channels are used when retransmitting a frame after initial transmission channel. */
    uint8_t number_of_channel_retries;
} fhss_config_parameters_t;


/**
 * \brief Struct fhss_ws_configuration defines configuration of WS FHSS.
 */
typedef struct fhss_ws_configuration {
    /** WS unicast channel function. */
    fhss_ws_channel_functions_e ws_uc_channel_function;

    /** WS broadcast channel function. */
    fhss_ws_channel_functions_e ws_bc_channel_function;

    /** Broadcast schedule identifier. */
    uint16_t bsi;

    /** Unicast dwell interval. Range: 15-250 milliseconds. */
    uint8_t fhss_uc_dwell_interval;

    /** Broadcast interval. Duration between broadcast dwell intervals. Range: 0-16777216 milliseconds. */
    uint32_t fhss_broadcast_interval;
    uint32_t lfn_bc_interval;

    /** Broadcast dwell interval. Range: 15-250 milliseconds. */
    uint8_t fhss_bc_dwell_interval;

    /** Unicast fixed channel */
    uint8_t unicast_fixed_channel;

    /** Broadcast fixed channel */
    uint8_t broadcast_fixed_channel;

    /** Domain channel mask, Wi-SUN uses it to exclure channels on US-IE and BS-IE. */
    uint8_t domain_channel_mask[32];

    /** Wi-SUN specific unicast channel mask */
    uint8_t unicast_channel_mask[32];

    /** Wi-SUN specific broadcast channel mask */
    uint8_t broadcast_channel_mask[32];

    /** Channel mask size */
    uint16_t channel_mask_size;

    /** Vendor defined channel function. */
    fhss_vendor_defined_cf *vendor_defined_cf;

    /** Configuration parameters. */
    fhss_config_parameters_t config_parameters;

} fhss_ws_configuration_t;

/**
 * \brief Struct fhss_timer defines interface between FHSS and FHSS platform timer.
 * Application must implement FHSS timer driver which is then used by FHSS with this interface.
 */
typedef struct fhss_timer {
    /** Start timeout (1us). Timer must support multiple simultaneous timeouts */
    int (*fhss_timer_start)(uint32_t, void (*fhss_timer_callback)(const fhss_api_t *fhss_api, uint16_t), const fhss_api_t *fhss_api);

    /** Stop timeout */
    int (*fhss_timer_stop)(void (*fhss_timer_callback)(const fhss_api_t *fhss_api, uint16_t), const fhss_api_t *fhss_api);

    /** Get remaining time of started timeout*/
    uint32_t (*fhss_get_remaining_slots)(void (*fhss_timer_callback)(const fhss_api_t *fhss_api, uint16_t), const fhss_api_t *fhss_api);

    /** Get timestamp since initialization of driver. Overflow of 32-bit counter is allowed (1us) */
    uint32_t (*fhss_get_timestamp)(const fhss_api_t *fhss_api);

    /** Divide 1 microsecond resolution. E.g. to use 64us resolution, use fhss_resolution_divider = 64*/
    uint8_t fhss_resolution_divider;
} fhss_timer_t;

/**
 * \brief Struct fhss_synch_configuration defines the synchronization time configurations.
 * Border router application must define and set these configuration for FHSS network.
 */
typedef struct fhss_synch_configuration {
    /** Number of broadcast channels. */
    uint8_t fhss_number_of_bc_channels;

    /** TX slots per channel. */
    uint8_t fhss_number_of_tx_slots;

    /** Length of superframe(microseconds) * Number of superframes defines the
        channel dwell time. E.g. 50000us * 8 -> Channel dwell time 400ms */
    uint16_t fhss_superframe_length;

    /** Number of superframes. */
    uint8_t fhss_number_of_superframes;
} fhss_synch_configuration_t;


/**
 * \brief Struct fhss_statistics defines the available FHSS statistics.
 */
typedef struct fhss_statistics {
    /** FHSS synchronization drift compensation (us/channel). */
    int16_t fhss_drift_compensation;

    /** FHSS hop count. */
    uint8_t fhss_hop_count;

    /** FHSS synchronization interval (s). */
    uint16_t fhss_synch_interval;

    /** Average of 5 preceding synchronization fixes (us). Updated after every fifth synch fix. */
    int16_t fhss_prev_avg_synch_fix;

    /** FHSS synchronization lost counter. */
    uint32_t fhss_synch_lost;

    /** FHSS TX to unknown neighbour counter. */
    uint32_t fhss_unknown_neighbor;

    /** FHSS channel retry counter. */
    uint32_t fhss_channel_retry;
} fhss_statistics_t;

/**
 * \brief Enumeration fhss_channel_mode_e defines the channel modes.
 */
typedef enum fhss_channel_mode_e {
    SINGLE_CHANNEL,     //< Single channel
    FHSS,               //< Frequency hopping mode
} fhss_channel_mode_e;

#endif // FHSS_CONFIG_H
