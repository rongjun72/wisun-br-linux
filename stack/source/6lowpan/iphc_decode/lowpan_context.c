/*
 * Copyright (c) 2015-2017, 2020, Pelion and affiliates.
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

/*
 * \file lowpan_context.c
 * \brief API for Add,Remove and update timeouts for lowpan context's
 *
 */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "common/bits.h"
#include "common/log_legacy.h"
#include "stack-services/ns_list.h"
#include "nwk_interface/protocol.h"

#include "6lowpan/iphc_decode/lowpan_context.h"

#define TRACE_GROUP "lCon"

lowpan_context_t *lowpan_context_get_by_id(const lowpan_context_list_t *list, uint8_t id)
{
    id &=  LOWPAN_CONTEXT_CID_MASK;
    /* Check to see we already have info for this context */
    ns_list_foreach(lowpan_context_t, entry, list) {
        if (entry->cid == id) {
            return entry;
        }
    }
    return NULL;
}

lowpan_context_t *lowpan_context_get_by_address(const lowpan_context_list_t *list, const uint8_t *ipv6Address)
{
    /* Check to see we already have info for this context
     * List is already listed that longest prefix are first at list
     */
    ns_list_foreach(lowpan_context_t, entry, list) {
        if (!bitcmp(entry->prefix, ipv6Address, entry->length)) {
            //Take always longest match prefix
            return entry;
        }
    }
    return NULL;
}


int_fast8_t lowpan_context_update(lowpan_context_list_t *list, uint8_t cid_flags, uint16_t lifetime, const uint8_t *prefix, uint_fast8_t len, bool stable)
{
    uint8_t cid = cid_flags & LOWPAN_CONTEXT_CID_MASK;
    lowpan_context_t *ctx = NULL;

    /* Check to see we already have info for this context */

    ctx = lowpan_context_get_by_id(list, cid);
    if (ctx) {
        //Remove from the list - it will be reinserted below, sorted by its
        //new context length. (Don't need "safe" foreach, as we break
        //immediately after the removal).
        ns_list_remove(list, ctx);
    }

    if (lifetime == 0) {
        /* This is a removal request: delete any existing entry, then exit */
        if (ctx) {
            free(ctx);
        }
        return 0;
    }

    if (!ctx) {
        ctx = malloc(sizeof(lowpan_context_t));
    }

    if (!ctx) {
        tr_error("No heap for New 6LoWPAN Context");
        return -2;
    }

    bool inserted = false;
    ns_list_foreach(lowpan_context_t, entry, list) {
        if (len >= entry->length) {
            ns_list_add_before(list, entry, ctx);
            inserted = true;
            break;
        }
    }
    if (!inserted) {
        ns_list_add_to_end(list, ctx);
    }

    ctx->length = len;
    ctx->cid = cid;
    ctx->expiring = false;
    ctx->stable = stable;
    ctx->compression = cid_flags & LOWPAN_CONTEXT_C;
    ctx->lifetime = (uint32_t) lifetime * 600u; /* minutes -> 100ms ticks */

    // Do our own zero-padding, just in case sender has done something weird
    memset(ctx->prefix, 0, sizeof ctx->prefix);
    bitcpy(ctx->prefix, prefix, len);

    return 0;
}

void lowpan_context_list_free(lowpan_context_list_t *list)
{
    ns_list_foreach_safe(lowpan_context_t, cur, list) {
        ns_list_remove(list, cur);
        free(cur);
    }
}

/* ticks is in 1/10s */
void lowpan_context_timer(int ticks)
{
    struct net_if *interface = protocol_stack_interface_info_get(IF_6LoWPAN);
    lowpan_context_list_t *list = &interface->lowpan_contexts;

    if (!(interface->lowpan_info & INTERFACE_NWK_ACTIVE))
        return;

    ns_list_foreach_safe(lowpan_context_t, ctx, list) {
        if (ctx->lifetime > ticks) {
            ctx->lifetime -= ticks;
            continue;
        }
        if (!ctx->expiring) {
            /* Main lifetime has run out. Clear compression flag, and retain a
             * bit longer (RFC 6775 5.4.3).
             */
            ctx->compression = false;
            ctx->expiring = true;
            ctx->lifetime = 2 * 18000u; /* 2 * default Router Lifetime = 2 * 1800s = 1 hour */
            tr_debug("Context timed out - compression disabled");
        } else {
            /* 1-hour expiration timer set above has run out */
            ns_list_remove(list, ctx);
            free(ctx);
            tr_debug("Delete Expired context");
        }
    }
}

