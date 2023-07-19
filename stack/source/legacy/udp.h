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

#ifndef UDP_H_
#define UDP_H_
#include "common/log.h"

typedef struct buffer buffer_t;

#ifdef HAVE_UDP

void udp_checksum_write(buffer_t *buf);
buffer_t *udp_down(buffer_t *buf);
buffer_t *udp_up(buffer_t *buf);

#else

static inline buffer_t *udp_down(buffer_t *buf)
{
    WARN("6lbr shouldn't reach this point");
    return NULL;
}

static inline buffer_t *udp_up(buffer_t *buf)
{
    WARN("6lbr shouldn't reach this point");
    return NULL;
}

#endif


#endif
