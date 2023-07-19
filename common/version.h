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
#ifndef COMMON_VERSION_H
#define COMMON_VERSION_H

#include <stdint.h>
#include <stdbool.h>

// FIELD_PREP cannot be evaluated by preprocessor expressions because it uses
// __builtin_ctz. Use explicit offsets instead.
#define VERSION(major, minor, patch) ((((major) << 24) & 0xff000000) | \
                                      (((minor) <<  8) & 0x00ffff00) | \
                                      ( (patch)        & 0x000000ff))

static inline bool version_older_than(uint32_t version,
                                      uint8_t major, uint16_t minor, uint8_t patch)
{
    return version < VERSION(major, minor, patch);
}

#endif
