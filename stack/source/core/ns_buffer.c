/*
 * Copyright (c) 2011-2020, Pelion and affiliates.
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
#include <inttypes.h>
#include <limits.h>
#include <sys/socket.h>
#include "common/log_legacy.h"
#include "stack/net_interface.h"

#include "nwk_interface/protocol_stats.h"
#include "core/ns_address_internal.h"
#include "legacy/ns_socket.h"

#include "core/ns_buffer.h"

#define TRACE_GROUP "buff"

// Get module working also on 16-bit platform
#if INT_MAX < 0xFFFF
#define BUFFER_MAX_SIZE ((size_t)INT_MAX)
#else
#define BUFFER_MAX_SIZE ((size_t)0xFFFF)
#endif

volatile unsigned int buffer_count = 0;

uint8_t *buffer_corrupt_check(buffer_t *buf)
{
    if (buf == NULL) {
        return NULL;
    }

    if (buf->buf_ptr > buf->buf_end || buf->buf_end > buf->size) {
        tr_error("Invalid buffer, size=%"PRIu16", buf_ptr=%"PRIu16", buf_end=%"PRIu16"", buf->size, buf->buf_ptr, buf->buf_end);
        tr_error("Data: %s", trace_array(buffer_data_pointer(buf), 56));
        BUG();
    }

    return buffer_data_pointer(buf);
}

buffer_t *buffer_get(uint16_t size)
{
    return buffer_get_specific(BUFFER_DEFAULT_HEADROOM, size, BUFFER_DEFAULT_MIN_SIZE);
}

buffer_t *buffer_get_minimal(uint16_t size)
{
    return buffer_get_specific(0, size, 0);
}

/**
 * Get pointer to a buffer_t structure and reserve memory for it from the dynamic heap.
 *
 * \param headroom required headroom in addition to basic size
 * \param size basic size of data allocate memory for
 * \param minspace minimum size of buffer
 * \return a pointer of type buffer_t to the allocated memory area
 *
 */
buffer_t *buffer_get_specific(uint16_t headroom, uint16_t size, uint16_t minspace)
{
    buffer_t *buf = NULL;
    uint32_t total_size;

    total_size = headroom + size;
    if (total_size < minspace) {
        total_size = minspace;
    }

    /* Round total size up to at least be a neat multiple - allocation must
     * anyway be this much aligned. */
    total_size = (total_size + 3) & ~ 3;

    if (total_size <= BUFFER_MAX_SIZE) {
        // Note - as well as this alloc+init, buffers can also be "realloced"
        // in buffer_headroom()
        buf = malloc(sizeof(buffer_t) + total_size);
    }

    if (buf) {
        buffer_count++;
        memset(buf, 0, sizeof(buffer_t));
        buf->buf_ptr = total_size - size;
        buf->buf_end = buf->buf_ptr;
        buf->socket = NULL;
        buf->interface = NULL;
        //buf->bad_channel = 0xffff;
        //buf->bc_sending_superframe = 0xff;
        buf->rpl_instance = 0xff;
        // Socket TX always overrides this, so this is the default for non-socket buffers.
        // Setting it to 0 would remove flow labels on internal ICMP messages without affecting sockets.
        buf->options.flow_label = IPV6_FLOW_UNSPECIFIED;
        buf->options.hop_limit = 255;
        buf->options.mpl_permitted = true;
        buf->link_specific.ieee802_15_4.useDefaultPanId = true;
#ifdef HAVE_IPV6_PMTUD
        buf->options.ipv6_use_min_mtu = -1;
#endif
        buf->size = total_size;
    } else {
        tr_error("buffer_get failed: alloc(%"PRIu32")", (uint32_t)(sizeof(buffer_t) + total_size));
    }

    protocol_stats_update(STATS_BUFFER_ALLOC, 1);
    return (buf);
}

/**
 * Make sure buffer has enough room for header.
 *
 * \param buf buffer to check
 * \param size required header space
 * \return a pointer of type buffer_t to the newly allocated buffer (or the original one)
 *
 */
buffer_t *buffer_headroom(buffer_t *buf, uint16_t size)
{
    uint16_t curr_len = buffer_data_length(buf);

    if (buf->size < (curr_len + size)) {
        buffer_t *restrict new_buf = NULL;
        /* This buffer isn't big enough at all - allocate a new block */
        // TODO - should we be giving them extra? probably
        uint32_t new_total = (curr_len + size + 3) & ~ 3;
        if (new_total <= BUFFER_MAX_SIZE) {
            new_buf = malloc(sizeof(buffer_t) + new_total);
        }

        if (new_buf) {
            // Copy the buffer_t header
            *new_buf = *buf;
            // Set new pointers, leaving specified headroom
            new_buf->buf_ptr = size;
            new_buf->buf_end = size + curr_len;
            new_buf->size = new_total;
            // Copy the current data
            memcpy(buffer_data_pointer(new_buf), buffer_data_pointer(buf), curr_len);
            protocol_stats_update(STATS_BUFFER_HEADROOM_REALLOC, 1);
            free(buf);
            buf = new_buf;
        } else {
            tr_error("HeadRoom Fail");
            protocol_stats_update(STATS_BUFFER_HEADROOM_FAIL, 1);
            socket_tx_buffer_event_and_free(buf, SOCKET_NO_RAM);
            buf = NULL;
        }
    } else if (buf->buf_ptr < size) {
        /* This buffer is big enough, but not enough headroom - shuffle */
        // TODO - surely better to shuffle all the way to the end in one go?
        uint8_t *orig_ptr = buffer_data_pointer(buf);
        buf->buf_ptr = size;
        buf->buf_end = size + curr_len;
        if (curr_len != 0) {
            memmove(buffer_data_pointer(buf), orig_ptr, curr_len);
            protocol_stats_update(STATS_BUFFER_HEADROOM_SHUFFLE, 1);
        }
    }
    buffer_corrupt_check(buf);
    return buf;
}

buffer_t *buffer_free_route(buffer_t *buf)
{
    if (buf->route) {
        if (--buf->route->ref_count == 0) {
            free(buf->route);
        }
        buf->route = NULL;
    }
    return buf;
}

/**
 * Free a memory block from the heap.
 *
 * \param buf pointer to buffer to be freed
 * \return (buffer_t *) NULL
 *
 */
buffer_t *buffer_free(buffer_t *buf)
{
    if (buf) {
        if (buffer_count) {
            buffer_count--;
        } else {
            tr_error("bc neg");
        }

        buf = buffer_free_route(buf);
        socket_dereference(buf->socket);
        free(buf->predecessor);
        free(buf->rpl_option);
        free(buf);

    } else {
        tr_error("nullp F");
    }
    return NULL;
}

void buffer_free_list(buffer_list_t *list)
{
    ns_list_foreach_safe(buffer_t, buf, list) {
        ns_list_remove(list, buf);
        buffer_free(buf);
    }
}

/* Prepare a buffer that came from a received packet for use as a new
 * transmission (eg ICMP error response). Kill fields which should not be
 * carried over. This is distinct from a packet we are forwarding.
 */
buffer_t *buffer_turnaround(buffer_t *buf)
{
    if (buf->predecessor) {
        free(buf->predecessor);
        buf->predecessor = NULL;
    }

    if (buf->rpl_option) {
        free(buf->rpl_option);
        buf->rpl_option = NULL;
    }
    buf->options.tunnelled = false;
    buf->rpl_flag_error = 0;
    buf->rpl_instance_known = false;
    buf->link_specific.ieee802_15_4.useDefaultPanId = true;

    buffer_socket_set(buf, NULL);

    /* Most cases this will be a response to an RX, so no existing routing
     * info, but in the case of TX resolution failure, we're reversing and
     * need to re-evaluate routing.
     */
    return buffer_free_route(buf);
}

void buffer_note_predecessor(buffer_t *buf, const sockaddr_t *addr)
{
    if (buf->options.need_predecessor && !buf->predecessor) {
        buf->predecessor = malloc(sizeof * buf->predecessor);
        if (buf->predecessor) {
            memcpy(buf->predecessor, addr, sizeof * buf->predecessor);
        }
    }
}

socket_t *buffer_socket_set(buffer_t *buf, socket_t *socket)
{
    buf->socket = socket_dereference(buf->socket);
    buf->socket = socket_reference(socket);
    return buf->socket;
}

/* Copy metadata information from src into dst.
 *
 * Data size and pointers left unmodified in destination.
 * Other in-buffer metadata copied from src.
 * Any route information pointer cloned from src (reference count increased).
 * Other metadata pointers transfered either to dst or left in src, as requested
 */
void buffer_copy_metadata(buffer_t *dst, buffer_t *src, bool non_clonable_to_dst)
{
    uint16_t buf_size = dst->size;
    uint16_t buf_end = dst->buf_end;
    uint16_t buf_ptr = dst->buf_ptr;
    *dst = *src;
    if (dst->route) {
        dst->route->ref_count++;
    }
    dst->size = buf_size;
    dst->buf_ptr = buf_ptr;
    dst->buf_end = buf_end;

    /* Extra data pointers now attached to both buffers - there can be only one */
    buffer_t *to_wipe = non_clonable_to_dst ? src : dst;
    to_wipe->rpl_option = NULL;
    to_wipe->predecessor = NULL;
    to_wipe->socket = NULL;
}

/**
 * Add buffer at the end of data.
 *
 * \param buf pointer to buffer where data is added
 * \param data_ptr data pointer where data is copied
 * \data_len length of data copied.
 *
 */
void buffer_data_add(buffer_t *buf, const uint8_t *data_ptr, uint16_t data_len)
{
    memcpy(buffer_data_end(buf), data_ptr, data_len);
    buffer_data_end_set(buf, buffer_data_end(buf) + data_len);
    buffer_corrupt_check(buf);
    return;
}

/**
 * Create new buffer that has the same data and fields.
 *
 * \param buf pointer to buffer to be freed
 * \return (buffer_t *) new clone.
 *
 */
buffer_t *buffer_clone(buffer_t *buf)
{
    buffer_t *result_ptr = NULL;

    if (buf == NULL) {
        return NULL;
    }

    result_ptr = buffer_get(buffer_data_length(buf));

    if (result_ptr == NULL) {
        return NULL;
    }

    uint16_t buf_ptr = result_ptr->buf_ptr;
    uint16_t buf_end = result_ptr->buf_end;
    uint16_t size = result_ptr->size;

    *result_ptr = *buf;
    result_ptr->predecessor = NULL;
    result_ptr->route = NULL; // Don't clone routing info
    result_ptr->socket = NULL; // Don't clone Socket info
    result_ptr->options.multicast_loop = false; // Don't loop back more copies!
    result_ptr->rpl_option = NULL;
    result_ptr->buf_ptr = buf_ptr;
    result_ptr->buf_end = buf_end;
    result_ptr->size = size;
    buffer_data_add(result_ptr, buffer_data_pointer(buf), buffer_data_length(buf));

    return result_ptr;
}

static uint16_t ip_fcf_v(uint_fast8_t count, const struct iovec vec[static count])
{
    uint_fast32_t acc32 = 0;
    bool odd = false;
    while (count) {
        const uint8_t *data_ptr = vec->iov_base;
        uint_fast16_t data_length = vec->iov_len;
        if (odd && data_length > 0) {
            acc32 += *data_ptr++;
            data_length--;
            odd = false;
        }
        while (data_length >= 2) {
            acc32 += (uint_fast16_t) data_ptr[0] << 8 | data_ptr[1];
            data_ptr += 2;
            data_length -= 2;
        }
        if (data_length) {
            acc32 += (uint_fast16_t) data_ptr[0] << 8;
            odd = true;
        }
        vec++;
        count--;
    }

    // Fold down up to 0xffff carries in the 32-bit accumulator
    acc32 = (acc32 >> 16) + (acc32 & 0xffff);

    // Could be one more carry from the previous addition (result <= 0x1fffe)
    uint16_t sum16 = (uint16_t)((acc32 >> 16) + (acc32 & 0xffff));
    return ~sum16;
}

static uint16_t ipv6_fcf(const uint8_t src_address[static 16],
                         const uint8_t dest_address[static 16],
                         uint16_t data_length,
                         const uint8_t data_ptr[static data_length],
                         uint8_t next_protocol)
{
    // Use gather vector to lay out IPv6 pseudo-header (RFC 2460) and data
    uint8_t hdr_data[] = { data_length >> 8, data_length, 0, next_protocol };
    struct iovec vec[4] = {
        { (void *) src_address, 16 },
        { (void *) dest_address, 16 },
        { hdr_data, 4 },
        { (void *) data_ptr, data_length }
    };

    return ip_fcf_v(4, vec);
}

uint16_t buffer_ipv6_fcf(const buffer_t *buf, uint8_t next_header)
{
    return ipv6_fcf(buf->src_sa.address, buf->dst_sa.address,
                    buffer_data_length(buf), buffer_data_pointer(buf),
                    next_header);
}
