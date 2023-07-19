/*
 * Copyright (c) 2023 Silicon Laboratories Inc. (www.silabs.com)
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
#ifndef DROP_PRIVILEGES_H
#define DROP_PRIVILEGES_H

struct wsbrd_conf;

#ifdef HAVE_LIBCAP

void drop_privileges(struct wsbrd_conf *conf);

#else

#include "common/log.h"

static inline void drop_privileges(struct wsbrd_conf *conf)
{
    FATAL(3, "options user and group need support for libcap");
}

#endif

#endif
