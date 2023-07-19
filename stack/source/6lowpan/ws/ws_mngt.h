/*
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */
#ifndef WS_MNGT_H
#define WS_MNGT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/trickle.h"

/*
 * Processing of Wi-SUN management frames
 * - PAN Advertisement (PA)
 * - PAN Advertisement Solicit (PAS)
 * - PAN Configuration (PC)
 * - PAN Configuration Solicit (PCS)
 * - LFN PAN Advertisement (LPA)
 * - LFN PAN Advertisement Solicit (LPAS)
 * - LFN PAN Configuration (LPC)
 * - LFN PAN Configuration Solicit (LPCS)
 * - LFN Time Sync (LTS)
 */

struct mcps_data_ie_list;
struct mcps_data_ind;
struct net_if;

struct ws_mngt {
    trickle_params_t trickle_params;
    trickle_t trickle_pa;
    trickle_t trickle_pc;
    bool trickle_pa_running: 1;
    bool trickle_pc_running: 1;
#ifdef HAVE_WS_ROUTER
    trickle_t trickle_pas;
    trickle_t trickle_pcs;
    bool trickle_pas_running: 1;
    bool trickle_pcs_running: 1;
    int pcs_max_timeout;
    int pcs_count;
#endif
    uint8_t lpa_dst[8];
};

void ws_mngt_pa_analyze(struct net_if *net_if,
                        const struct mcps_data_ind *data,
                        const struct mcps_data_ie_list *ie_ext);
void ws_mngt_pas_analyze(struct net_if *net_if,
                         const struct mcps_data_ind *data,
                         const struct mcps_data_ie_list *ie_ext);
void ws_mngt_pc_analyze(struct net_if *net_if,
                        const struct mcps_data_ind *data,
                        const struct mcps_data_ie_list *ie_ext);
void ws_mngt_pcs_analyze(struct net_if *net_if,
                         const struct mcps_data_ind *data,
                         const struct mcps_data_ie_list *ie_ext);
void ws_mngt_lpas_analyze(struct net_if *net_if,
                          const struct mcps_data_ind *data,
                          const struct mcps_data_ie_list *ie_ext);
void ws_mngt_lpcs_analyze(struct net_if *net_if,
                          const struct mcps_data_ind *data,
                          const struct mcps_data_ie_list *ie_ext);

void ws_mngt_lpa_timer_cb(int ticks);
void ws_mngt_lts_timer_cb(int ticks);

// Broadcast an LPC frame on LGTK hash, or active LGTK index change
void ws_mngt_lpc_pae_cb(struct net_if *net_if);

#endif
