/*
 * Copyright (c) 2015-2017, 2019, Pelion and affiliates.
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "common/bits.h"
#include "common/log_legacy.h"
#include "common/ns_list.h"

#include "common_protocols/icmpv6_prefix.h"

/*
 * \file icmpv6_prefix.c
 * \brief Add short description about this file!!!
 *
 */
prefix_entry_t *icmpv6_prefix_add(prefix_list_t *list, const uint8_t *prefixPtr, uint8_t prefix_len, uint32_t lifeTime, uint32_t prefTime, uint8_t flags)
{
    prefix_entry_t *entry;

    entry = icmpv6_prefix_compare(list, prefixPtr, prefix_len);
    if (entry) {
        if (flags != 0xff) {
            entry->options = flags;
            entry->lifetime = lifeTime;
            entry->preftime = prefTime;
        }
        return entry;
    }

    entry = malloc(sizeof(prefix_entry_t));
    if (entry) {
        entry->prefix_len = prefix_len;
        entry->options = 0xff;
        entry->lifetime = lifeTime;
        entry->preftime = prefTime;
        memset(entry->prefix, 0, 16);
        bitcpy0(entry->prefix, prefixPtr, prefix_len);
        ns_list_add_to_end(list, entry);
    }
    return entry;
}

prefix_entry_t *icmpv6_prefix_compare(prefix_list_t *list, const uint8_t *addr, uint8_t prefix_len)
{
    ns_list_foreach(prefix_entry_t, cur, list) {
        if (cur->prefix_len == prefix_len && !bitcmp(cur->prefix, addr, prefix_len)) {
            return cur;
        }
    }
    return NULL;
}

