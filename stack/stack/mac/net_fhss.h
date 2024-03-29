/*
 * Copyright (c) 2015-2019, Pelion and affiliates.
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
 * \file net_fhss.h
 * \brief FHSS API
 *
 */
#ifndef NET_FHSS_H_
#define NET_FHSS_H_
#include <stdint.h>


typedef struct fhss_ws_configuration fhss_ws_configuration_t;
typedef struct fhss_timer fhss_timer_t;
typedef struct fhss_api fhss_api_t;

/**
 * @brief Creates FHSS WS API instance which will be registered to software MAC.
 * @param fhss_configuration Basic FHSS configuration.
 * @param fhss_timer FHSS platform timer interface and configuration.
 * @return New FHSS instance if successful, NULL otherwise.
 */
fhss_api_t *ns_fhss_ws_create(const fhss_ws_configuration_t *fhss_configuration, const fhss_timer_t *fhss_timer);

/**
 * @brief Get WS configuration.
 * @param fhss_api FHSS instance.
 * @return WS configuration.
 */
const fhss_ws_configuration_t *ns_fhss_ws_configuration_get(const fhss_api_t *fhss_api);

/**
 * @brief Set WS configuration.
 * @param fhss_api FHSS instance.
 * @param fhss_configuration Basic FHSS configuration.
 * @return 0 on success, -1 on fail.
 */
int ns_fhss_ws_configuration_set(const fhss_api_t *fhss_api, const fhss_ws_configuration_t *fhss_configuration);

/**
 * @brief Deletes a FHSS API instance and removes it from software MAC.
 * @param fhss_api FHSS instance.
 * @return 0 on success, -1 on fail.
 */
int ns_fhss_delete(fhss_api_t *fhss_api);

#endif
