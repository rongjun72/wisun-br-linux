/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------

#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#include "mempool.h"
#include "wisun_collector.h"
#include "wisun_meter_collector_config.h"
#include "common/log.h"
#include "common/log_legacy.h"

#define TRACE_GROUP "collector"
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

/// Meter response timeout factor
#define SL_WISUN_COLLECTOR_TIMEOUT_FACTOR                               4U

/// Collector receive buffer size
#define SL_WISUN_COLLECTOR_BUFFER_LEN                                   256U

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

/**************************************************************************//**
 * @brief Create the socket shared by the sender and recevier threads
 *****************************************************************************/
static void _create_common_socket(void);

/**************************************************************************//**
 * @brief Collector receive response
 * @details Handler function
 * @param[in] sockid The socket used for receiving
 * @param[out] remote_addr The address of the sender
 * @param[out] packet_data_len Size of the received packet
 * @return SL_STATUS_OK On success
 * @return SL_STATUS_FAIL On failure
 *****************************************************************************/
static sl_status_t _collector_recv_response(const int32_t sockid,
                                            sockaddr_in6_t *remote_addr,
                                            int32_t * const packet_data_len);

/**************************************************************************//**
 * @brief Collector parse
 * @details Handler function
 * @param[in] raw Received data buffer
 * @param[in] packet_data_len Length of the received packet
 * @param[in] remote_addr Address of the sender
 * @return sl_wisun_meter_entry_t* Meter entry or NULL on failure
 *****************************************************************************/
static sl_wisun_meter_entry_t *_collector_parse_response(void *raw,
                                                         int32_t packet_data_len,
                                                         sockaddr_in6_t* const remote_addr);

/**************************************************************************//**
 * @brief Remove broken meters
 * @details Remove meters from the given mempool that not responded for the
 *          request in time.
 * @param[in] mempool Mempool that stores the meter entry
 *****************************************************************************/
static void _collector_remove_broken_meters(sl_mempool_t *mempool);

/**************************************************************************//**
 * @brief Collector receiver thread
 * @details Receiver thread handler function
 * @param[in] args Arguments
 *****************************************************************************/
static void *_collector_recv_thread_fnc(void *args);

/**************************************************************************//**
 * @brief Collector get meter entry by address from a specific mempool
 * @details Helper function
 * @param remote_addr Remote address
 * @param block Pointer to the first block in the mempool
 * @return sl_wisun_meter_entry_t* Meter entry or NULL on error
 *****************************************************************************/
static sl_wisun_meter_entry_t *_collector_get_meter_entry_by_address_from_mempool(const sockaddr_in6_t* const remote_addr,
                                                                                  sl_mempool_block_hnd_t *block);

/**************************************************************************//**
 * @brief Collector print async meters
 *****************************************************************************/
static void _collector_print_async_meters(void);

/**************************************************************************//**
 * @brief Collector print registered meters
 *****************************************************************************/
static void _collector_print_registered_meters(void);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

/// Mempool to store registered meters
static sl_mempool_t _reg_meters_mempool               = { 0 };

/// Mempool to store async meters
static sl_mempool_t _async_meters_mempool             = { 0 };

/// Registered meter internal storage
static sl_wisun_meter_entry_t _reg_meters[SL_WISUN_COLLECTOR_MAX_REG_METER]     = { 0 };

/// Async meter internal storage
static sl_wisun_meter_entry_t _async_meters[SL_WISUN_COLLECTOR_MAX_ASYNC_METER] = { 0 };

static sl_wisun_meter_request_t _async_meas_req       = { 0 };
static sl_wisun_meter_request_t _registration_req     = { 0 };
static sl_wisun_meter_request_t _removal_req          = { 0 };

/// Internal storage for raw rx data
static uint8_t _rx_buf[SL_WISUN_COLLECTOR_BUFFER_LEN] = { 0U };

/// Collector internal handler
static sl_wisun_collector_hnd_t _collector_hnd        = { 0 };
 
/// Collector receiver thread ID
static pthread_t _collector_recv_thr_id;

/// Socket shared among the sender and receiver threads
static int32_t _common_socket                         = SOCKET_INVALID_ID;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
void sl_wisun_app_core_util_dispatch_thread(void)
{
    /* sleep 1ms */
    usleep(1000);
}

int32_t sl_wisun_collector_get_shared_socket(void)
{
  return _common_socket;
}

void sl_wisun_collector_init(void)
{
  sl_wisun_meter_request_t req = { 0 };

  sl_wisun_collector_set_handler(&_collector_hnd,
                                 _collector_parse_response,
                                 NULL);

  sl_wisun_collector_init_common_resources();

  // init collector-meter token
  sl_wisun_mc_init_token(SL_WISUN_METER_COLLECTOR_TOKEN);

  req.length  = strlen(SL_WISUN_METER_REQUEST_TYPE_STR_ASYNC)
                + 1U
                + sl_wisun_mc_get_token_size();
  req.buff    = (uint8_t *) SL_WISUN_METER_REQUEST_TYPE_STR_ASYNC \
                SL_WISUN_METER_REQUEST_DELIMITER                  \
                SL_WISUN_METER_COLLECTOR_TOKEN;
  sl_wisun_collector_set_async_measurement_request(&req);

  req.length  = strlen(SL_WISUN_METER_REQUEST_TYPE_STR_REGISTER)
                + 1U
                + sl_wisun_mc_get_token_size();
  req.buff    = (uint8_t *) SL_WISUN_METER_REQUEST_TYPE_STR_REGISTER \
                SL_WISUN_METER_REQUEST_DELIMITER                     \
                SL_WISUN_METER_COLLECTOR_TOKEN;
  sl_wisun_collector_set_registration_request(&req);

  req.length  = strlen(SL_WISUN_METER_REQUEST_TYPE_STR_REMOVE)
                + 1U
                + sl_wisun_mc_get_token_size();
  req.buff    = (uint8_t *) SL_WISUN_METER_REQUEST_TYPE_STR_REMOVE \
                SL_WISUN_METER_REQUEST_DELIMITER                   \
                SL_WISUN_METER_COLLECTOR_TOKEN;
  sl_wisun_collector_set_removal_request(&req);
}

void sl_wisun_collector_inherit_common_hnd(sl_wisun_collector_hnd_t * const hnd)
{
  // do not overwrite resource_hnd and get_meter
  _collector_hnd.parse = hnd->parse;
  _collector_hnd.timeout = hnd->timeout;
}

void sl_wisun_collector_init_common_resources(void)
{
  sl_status_t stat = SL_STATUS_FAIL;
  // Create mempool for registered meters
  stat = sl_mempool_create(&_reg_meters_mempool,
                           SL_WISUN_COLLECTOR_MAX_REG_METER,
                           sizeof(sl_wisun_meter_entry_t),
                           _reg_meters,
                           sizeof(_reg_meters));
  assert(stat == SL_STATUS_OK);

  // Create mempool for async meters
  stat = sl_mempool_create(&_async_meters_mempool,
                           SL_WISUN_COLLECTOR_MAX_ASYNC_METER,
                           sizeof(sl_wisun_meter_entry_t),
                           _async_meters,
                           sizeof(_async_meters));
  assert(stat == SL_STATUS_OK);

  // Init collector handler
  sl_wisun_collector_init_hnd(&_collector_hnd);

  pthread_create(&_collector_recv_thr_id, NULL, _collector_recv_thread_fnc, NULL);
  assert(_collector_recv_thr_id != 0);
}

/* Register meter */
sl_status_t sl_wisun_collector_register_meter(sockaddr_in6_t *meter_addr)
{
  const sl_mempool_block_hnd_t *block     = NULL;
  sl_wisun_meter_entry_t *tmp_meter_entry = NULL;
  sl_status_t res                         = SL_STATUS_FAIL;

  sl_wisun_mc_mutex_acquire(_collector_hnd);

  if (meter_addr == NULL) {
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }

  block = _reg_meters_mempool.blocks;

  // Check if meter is already registered
  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *) block->start_addr;
    if (sl_wisun_mc_compare_address(&tmp_meter_entry->addr, meter_addr)) {
      sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_ALREADY_EXISTS);
    }
    block = block->next;
  }

  tmp_meter_entry = sl_mempool_alloc(&_reg_meters_mempool);
  if (tmp_meter_entry == NULL) {
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }
  tmp_meter_entry->type = SL_WISUN_MC_REQ_REGISTER;
  tmp_meter_entry->resp_recv_timestamp = 0U;
  tmp_meter_entry->req_sent_timestamp = get_monotonic_ms();
  memcpy(&tmp_meter_entry->addr, meter_addr, sizeof(sockaddr_in6_t));

  // Send a registration request to the meter
  res = sl_wisun_collector_send_request(_common_socket, &tmp_meter_entry->addr, &_registration_req);
  if (res != SL_STATUS_OK) {
    tr_info("[Collector cannot send registration request to the meter: %s]", tr_ipv6(meter_addr->sin6_addr.s6_addr));
    free(meter_addr);
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }

  sl_wisun_mc_mutex_release(_collector_hnd);
  return SL_STATUS_OK;
}

/* Remove meter */
sl_status_t sl_wisun_collector_remove_meter(sockaddr_in6_t *meter_addr)
{
  const sl_mempool_block_hnd_t *block           = NULL;
  const sl_wisun_meter_entry_t *tmp_meter_entry = NULL;
  sl_status_t res                               = SL_STATUS_FAIL;

  sl_wisun_mc_mutex_acquire(_collector_hnd);

  if (meter_addr == NULL) {
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }

  block = _reg_meters_mempool.blocks;

  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *) block->start_addr;
    if (sl_wisun_mc_compare_address(&tmp_meter_entry->addr, meter_addr)) {
      break;
    }
    block = block->next;
  }
  if (tmp_meter_entry == NULL) {
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }

  // Send a remove request to the meter
  res = sl_wisun_collector_send_request(_common_socket, &tmp_meter_entry->addr, &_removal_req);
  if (res != SL_STATUS_OK) {
    tr_info("[Collector cannot send removal request to the meter: %s]", tr_ipv6(meter_addr->sin6_addr.s6_addr));
    free(meter_addr);
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }

  sl_mempool_free(&_reg_meters_mempool, tmp_meter_entry);

  sl_wisun_mc_mutex_release(_collector_hnd);

  return SL_STATUS_OK;
}

sl_status_t sl_wisun_send_async_request(sockaddr_in6_t *meter_addr)
{
  const sl_mempool_block_hnd_t *block     = NULL;
  sl_wisun_meter_entry_t *tmp_meter_entry = NULL;
  sl_status_t res                         = SL_STATUS_FAIL;

  if (meter_addr == NULL) {
    return SL_STATUS_FAIL;
  }

  sl_wisun_mc_mutex_acquire(_collector_hnd);

  block = _async_meters_mempool.blocks;

  // Check if async request has already been sent to the given meter
  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *) block->start_addr;
    if (sl_wisun_mc_compare_address(&tmp_meter_entry->addr, meter_addr)) {
      sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_OK);
    }
    block = block->next;
  }

  tmp_meter_entry = sl_mempool_alloc(&_async_meters_mempool);
  if (tmp_meter_entry == NULL) {
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }
  tmp_meter_entry->req_sent_timestamp = get_monotonic_ms();
  tmp_meter_entry->resp_recv_timestamp = 0U;
  memcpy(&tmp_meter_entry->addr, meter_addr, sizeof(sockaddr_in6_t));

  // Send a async measurement request to the meter
  res = sl_wisun_collector_send_request(_common_socket, &tmp_meter_entry->addr, &_async_meas_req);
  if (res != SL_STATUS_OK) {
    tr_info("[Collector cannot send async measurement request to the meter: %s]", tr_ipv6(meter_addr->sin6_addr.s6_addr));
    free(meter_addr);
    sl_wisun_mc_release_mtx_and_return_val(_collector_hnd, SL_STATUS_FAIL);
  }
  sl_wisun_mc_mutex_release(_collector_hnd);
  return SL_STATUS_OK;
}

void sl_wisun_collector_set_async_measurement_request(const sl_wisun_meter_request_t * const req)
{
  sl_wisun_mc_mutex_acquire(_collector_hnd);
  memcpy(&_async_meas_req, req, sizeof(_async_meas_req));
  sl_wisun_mc_mutex_release(_collector_hnd);
}

void sl_wisun_collector_set_registration_request(const sl_wisun_meter_request_t * const req)
{
  sl_wisun_mc_mutex_acquire(_collector_hnd);
  memcpy(&_registration_req, req, sizeof(_registration_req));
  sl_wisun_mc_mutex_release(_collector_hnd);
}

void sl_wisun_collector_set_removal_request(const sl_wisun_meter_request_t * const req)
{
  sl_wisun_mc_mutex_acquire(_collector_hnd);
  memcpy(&_removal_req, req, sizeof(_removal_req));
  sl_wisun_mc_mutex_release(_collector_hnd);
}

void sl_wisun_collector_print_meters(void)
{
  _collector_print_async_meters();
  _collector_print_registered_meters();
}

sl_wisun_meter_entry_t *sl_wisun_collector_get_async_meter_entry_by_address(const sockaddr_in6_t* const meter_addr)
{
  sl_wisun_meter_entry_t *tmp_meter_entry = NULL;

  if (meter_addr == NULL) {
    return NULL;
  }
  tmp_meter_entry = _collector_get_meter_entry_by_address_from_mempool(meter_addr,
                                                                       _async_meters_mempool.blocks);
  return tmp_meter_entry;
}

sl_wisun_meter_entry_t *sl_wisun_collector_get_registered_meter_entry_by_address(const sockaddr_in6_t* const meter_addr)
{
  sl_wisun_meter_entry_t *tmp_meter_entry = NULL;

  if (meter_addr == NULL) {
    return NULL;
  }
  tmp_meter_entry = _collector_get_meter_entry_by_address_from_mempool(meter_addr,
                                                                       _reg_meters_mempool.blocks);
  return tmp_meter_entry;
}

sl_wisun_meter_entry_t *sl_wisun_collector_get_meter(const sockaddr_in6_t* const meter_addr)
{
  sl_wisun_meter_entry_t *tmp_meter_entry = NULL;

  tmp_meter_entry = sl_wisun_collector_get_registered_meter_entry_by_address(meter_addr);
  if (tmp_meter_entry != NULL) {
    return tmp_meter_entry;
  }
  tmp_meter_entry = sl_wisun_collector_get_async_meter_entry_by_address(meter_addr);
  return tmp_meter_entry;
}

sl_status_t sl_wisun_collector_send_request(const int32_t sockid,
                                            const sockaddr_in6_t *addr,
                                            const sl_wisun_meter_request_t * const req)
{
  socklen_t len       = 0U;
  int32_t res         = SOCKET_INVALID_ID;
  sl_status_t retval  = SL_STATUS_OK;

  if (sockid == SOCKET_INVALID_ID) {
    return SL_STATUS_FAIL;
  }

  len = sizeof(*addr);

  res = sendto(sockid,
               req->buff,
               req->length,
               0,
               (const struct sockaddr *)addr,
               len);

  if (res == -1) {
    retval = SL_STATUS_FAIL;
  }

  return retval;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

static void _create_common_socket(void)
{
  static sockaddr_in6_t collector_addr  = { 0 };
  int32_t res                         = SOCKET_INVALID_ID;

  _common_socket = socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  assert(_common_socket != SOCKET_INVALID_ID);

  collector_addr.sin6_family = AF_INET6;
  collector_addr.sin6_addr = in6addr_any;
  collector_addr.sin6_port = htons(SL_WISUN_COLLECTOR_PORT);

  res = bind(_common_socket,
             (const struct sockaddr *) &collector_addr,
             sizeof(sockaddr_in6_t));
  assert(res >= 0);
}

static sl_status_t _collector_recv_response(const int32_t sockid,
                                            sockaddr_in6_t *remote_addr,
                                            int32_t * const packet_data_len)
{
  socklen_t addrlen = 0U;
  int32_t res       = SOCKET_INVALID_ID;

  if (remote_addr == NULL || packet_data_len == NULL) {
    return SL_STATUS_FAIL;
  }

  *packet_data_len = -1;
  addrlen = sizeof(sockaddr_in6_t);
  res = recvfrom(sockid,
                 (void *)_rx_buf,
                 SL_WISUN_COLLECTOR_BUFFER_LEN,
                 0,
                 (struct sockaddr *)remote_addr,
                 &addrlen);

  if (res <= 0) {
    return SL_STATUS_FAIL;
  }

  if (!memcmp(&remote_addr->sin6_addr, &in6addr_any, sizeof(in6addr_any))) {
    tr_warn("[Invalid address received]");
    return SL_STATUS_FAIL;
  }

  *packet_data_len = res;
  return SL_STATUS_OK;
}

static sl_wisun_meter_entry_t *_collector_parse_response(void *raw,
                                                         int32_t packet_data_len,
                                                         sockaddr_in6_t* const remote_addr)
{
#if !defined(SL_CATALOG_WISUN_COAP_PRESENT)
  uint16_t packet_size              = 0U;
  sl_wisun_meter_entry_t *meter     = NULL;
  char ip_addr[STR_MAX_LEN_IPV6];
  const char *packet                = NULL;
  sl_wisun_request_type_t resp_type = SL_WISUN_MC_REQ_UNKNOWN;

  packet_size = sizeof(sl_wisun_meter_packet_packed_t);
  if (packet_data_len == packet_size) {
    resp_type = SL_WISUN_MC_REQ_ASYNC;
  } else {
    resp_type = SL_WISUN_MC_REQ_REGISTER;
  }

  sl_wisun_mc_mutex_acquire(_collector_hnd);
  if (resp_type == SL_WISUN_MC_REQ_ASYNC) {
    meter = sl_wisun_collector_get_async_meter_entry_by_address(remote_addr);
  }
  if (meter == NULL) {
    meter = sl_wisun_collector_get_registered_meter_entry_by_address(remote_addr);
  }
  sl_wisun_mc_mutex_release(_collector_hnd);

  if (meter == NULL) {
    tr_warn("[Unknown remote message received!]");
    return NULL;
  }

  tr_info("[%s response]", resp_type == SL_WISUN_MC_REQ_ASYNC ? "Async" : "Periodic");
  str_ipv6(remote_addr->sin6_addr.s6_addr, ip_addr);
  packet = raw;
  while (packet_data_len >= packet_size) {
    sl_wisun_mc_print_mesurement(ip_addr, packet, true);
    packet += packet_size;
    packet_data_len -= packet_size;
  }
  free(remote_addr);
  return meter;
#else
  (void) raw;
  (void) packet_data_len;
  (void) remote_addr;
  return NULL;
#endif
}

static void _collector_remove_broken_meters(sl_mempool_t *mempool)
{
  const sl_mempool_block_hnd_t *block           = NULL;
  const sl_wisun_meter_entry_t *tmp_meter_entry = NULL;
  uint64_t timestamp                            = 0U;
  uint32_t elapsed_ms                           = 0U;
  char ip_addr[STR_MAX_LEN_IPV6];

  if (mempool == NULL) {
    return;
  }

  sl_wisun_mc_mutex_acquire(_collector_hnd);

  block = mempool->blocks;

  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *) block->start_addr;
    timestamp = get_monotonic_ms();
    elapsed_ms = timestamp - tmp_meter_entry->req_sent_timestamp;

    block = block->next;
    if ((tmp_meter_entry->resp_recv_timestamp) || (elapsed_ms <= SL_WISUN_COLLECTOR_REQUEST_TIMEOUT)) {
      continue;
    }

    str_ipv6(tmp_meter_entry->addr.sin6_addr.s6_addr, ip_addr);
    tr_info("[%s not responded for the %s request in time, therefore has been removed]",
           ip_addr, tmp_meter_entry->type == SL_WISUN_MC_REQ_ASYNC ? "async" : "registration");

    sl_mempool_free(mempool, tmp_meter_entry);
  }

  sl_wisun_mc_mutex_release(_collector_hnd);
}

static void *_collector_recv_thread_fnc(void *args)
{
  uint32_t response_time_ms         = 0U;
  sl_wisun_meter_entry_t *meter     = NULL;
  sl_status_t res                   = SL_STATUS_FAIL;
  static sockaddr_in6_t remote_addr = { 0 };
  int32_t packet_data_len           = -1;

  (void) args;

  _create_common_socket();

  SL_WISUN_THREAD_LOOP {
////    if (!sl_wisun_app_core_util_network_is_connected()) {
////      osDelay(1000);
////      continue;
////    }

    _collector_remove_broken_meters(&_async_meters_mempool);
    _collector_remove_broken_meters(&_reg_meters_mempool);

    meter = NULL;

    res = _collector_recv_response(_common_socket,
                                   &remote_addr,
                                   &packet_data_len);

    if (res != SL_STATUS_OK) {
      sl_wisun_app_core_util_dispatch_thread();
      continue;
    }

    meter = _collector_hnd.parse(_rx_buf,
                                 packet_data_len,
                                 &remote_addr);

    if (meter == NULL) {
      sl_wisun_app_core_util_dispatch_thread();
      continue;
    }

    meter->resp_recv_timestamp = get_monotonic_ms();
    if (meter->type == SL_WISUN_MC_REQ_ASYNC) {
      response_time_ms = meter->resp_recv_timestamp - meter->req_sent_timestamp;
      tr_info("[Response time: %dms]", response_time_ms);
      sl_mempool_free(&_async_meters_mempool, meter);
    }
    sl_wisun_app_core_util_dispatch_thread();
  }

  return NULL;
}

static sl_wisun_meter_entry_t *_collector_get_meter_entry_by_address_from_mempool(const sockaddr_in6_t* const remote_addr,
                                                                                  sl_mempool_block_hnd_t *block)
{
  sl_wisun_meter_entry_t *tmp_meter_entry = NULL;

  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *) block->start_addr;
    if (sl_wisun_mc_compare_address(&tmp_meter_entry->addr, remote_addr)) {
      return tmp_meter_entry;
    }
    block = block->next;
  }
  return NULL;
}

static void _collector_print_async_meters(void)
{
  uint32_t timestamp                            = 0U;
  const sl_mempool_block_hnd_t *block           = NULL;
  const sl_wisun_meter_entry_t *tmp_meter_entry = NULL;
  uint32_t remaining_ms                         = 0U;
  uint32_t elapsed_ms                           = 0U;
  char ip_addr[STR_MAX_LEN_IPV6];

  sl_wisun_mc_mutex_acquire(_collector_hnd);

  tr_info("[Async meters:]");
  timestamp = get_monotonic_ms();
  block = _async_meters_mempool.blocks;

  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *)block->start_addr;
    elapsed_ms = timestamp - tmp_meter_entry->req_sent_timestamp;
    if (elapsed_ms > SL_WISUN_COLLECTOR_REQUEST_TIMEOUT) {
      remaining_ms = 0U;
    } else {
      remaining_ms = SL_WISUN_COLLECTOR_REQUEST_TIMEOUT - elapsed_ms;
    }
    str_ipv6(tmp_meter_entry->addr.sin6_addr.s6_addr, ip_addr);
    tr_info("[%s - time to live: %d ms]", ip_addr, remaining_ms);
    block = block->next;
  }
  sl_wisun_mc_mutex_release(_collector_hnd);
}

static void _collector_print_registered_meters(void)
{
  uint32_t timestamp                            = 0U;
  const sl_mempool_block_hnd_t *block           = NULL;
  const sl_wisun_meter_entry_t *tmp_meter_entry = NULL;
  uint32_t elapsed_ms                           = 0U;
  char ip_addr[STR_MAX_LEN_IPV6];

  sl_wisun_mc_mutex_acquire(_collector_hnd);

  printf("[Registered meters:]\n");
  timestamp = get_monotonic_ms();
  block = _reg_meters_mempool.blocks;

  while (block != NULL) {
    tmp_meter_entry = (sl_wisun_meter_entry_t *)block->start_addr;
    elapsed_ms = timestamp - tmp_meter_entry->req_sent_timestamp;
    str_ipv6(tmp_meter_entry->addr.sin6_addr.s6_addr, ip_addr);
    tr_info("[%s - registered %d seconds ago]", ip_addr, elapsed_ms / 1000);
    block = block->next;
  }
  sl_wisun_mc_mutex_release(_collector_hnd);
}
