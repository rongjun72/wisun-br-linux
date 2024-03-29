/*
 * Copyright (c) 2020, Pelion and affiliates.
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

/**
 * \file fhss_test_api.h
 * \brief
 */

#ifndef FHSS_TEST_API_H
#define FHSS_TEST_API_H
#include <stdint.h>

struct fhss_api;

/**
  * \brief Set optimal packet length
  *
  * \param fhss_api FHSS instance.
  * \param packet_length Optimal packet length
  *
  * \return  0 Success
  * \return -1 Failure
  */
int8_t fhss_set_optimal_packet_length(const struct fhss_api *fhss_api, uint16_t packet_length);

/**
  * \brief Set number of channel retries
  *
  * \param fhss_api FHSS instance.
  * \param number_of_channel_retries Number of channel retries
  *
  * \return  0 Success
  * \return -1 Failure
  */
int8_t fhss_set_number_of_channel_retries(const struct fhss_api *fhss_api, uint8_t number_of_channel_retries);

#endif // FHSS_TEST_API_H
