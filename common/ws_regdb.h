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
#ifndef WS_REGDB_H
#define WS_REGDB_H

#include <stdint.h>
#include <stdbool.h>

enum {
    // Value of domains is specified by the Wi-SUN specification
    REG_DOMAIN_WW =   0x00, // World Wide
    REG_DOMAIN_NA =   0x01, // North America
    REG_DOMAIN_JP =   0x02, // Japan
    REG_DOMAIN_EU =   0x03, // European Union
    REG_DOMAIN_CN =   0x04, // China
    REG_DOMAIN_IN =   0x05, // India
    REG_DOMAIN_MX =   0x06, // Mexico
    REG_DOMAIN_BZ =   0x07, // Brazil
    REG_DOMAIN_AZ =   0x08, // Australia
    REG_DOMAIN_NZ =   0x08, // New zealand
    REG_DOMAIN_KR =   0x09, // Korea
    REG_DOMAIN_PH =   0x0A, // Philippines
    REG_DOMAIN_MY =   0x0B, // Malaysia
    REG_DOMAIN_HK =   0x0C, // Hong Kong
    REG_DOMAIN_SG =   0x0D, // Singapore
    REG_DOMAIN_TH =   0x0E, // Thailand
    REG_DOMAIN_VN =   0x0F, // Vietnam
    REG_DOMAIN_UNDEF,
};

enum {
    // These values are specified by the Wi-SUN specification
    CHANNEL_SPACING_200  = 0x00,
    CHANNEL_SPACING_400  = 0x01,
    CHANNEL_SPACING_600  = 0x02,
    CHANNEL_SPACING_100  = 0x03,
    CHANNEL_SPACING_250  = 0x04,
    CHANNEL_SPACING_800  = 0x05,
    CHANNEL_SPACING_1200 = 0x06,
};

enum {
    // These values are part of the RCP API.
    REG_REGIONAL_NONE = 0,
    REG_REGIONAL_ARIB = 1,
    REG_REGIONAL_UNDEF,
};

enum {
    // These values are part of the RCP API.
    MODULATION_INDEX_0_5 = 0,
    MODULATION_INDEX_1_0 = 1,
    MODULATION_INDEX_UNDEF
};

enum {
    // These values are part of the RCP API.
    MODULATION_OFDM  = 0,
    MODULATION_OQPSK = 1,
    MODULATION_BPSK  = 2,
    MODULATION_GFSK  = 3,
    MODULATION_2FSK  = 4,
    MODULATION_UNDEFINED,
};

struct phy_params {
    uint8_t rail_phy_mode_id;
    uint8_t phy_mode_id;
    uint8_t modulation;
    uint32_t datarate;
    uint8_t op_mode;
    uint8_t fsk_modulation_index;
    uint8_t ofdm_mcs;
    uint8_t ofdm_option;
    bool fec;
};

struct chan_params {
    uint8_t reg_domain;
    uint8_t op_class;           // 0 if not specified by FAN 1.0
    uint8_t regional_reg;       // Unused for now
    uint8_t chan_plan_id;       // 0 if not specified by FAN 1.1
    uint32_t chan0_freq;
    uint32_t chan_spacing;
    uint16_t chan_count;
    uint16_t chan_count_valid;  // number of channels after chan_mask applied
    uint8_t valid_phy_modes[8];
    const char *chan_allowed;
};

extern const struct phy_params phy_params_table[];
extern const struct chan_params chan_params_table[];

bool ws_regdb_check_phy_chan_compat(const struct phy_params *phy_params, const struct chan_params *chan_params);
const struct phy_params *ws_regdb_phy_params(int phy_mode_id, int operating_mode);
const struct chan_params *ws_regdb_chan_params(int domain, int chan_plan_id, int operating_class);
const struct chan_params *ws_regdb_chan_params_universal(int chan0_freq, int chan_spacing, int chan_count_valid);

int ws_regdb_chan_spacing_id(int val);
int ws_regdb_chan_spacing_value(int id);

bool ws_regdb_is_std(uint8_t reg_domain, uint8_t phy_mode_id);

#endif
