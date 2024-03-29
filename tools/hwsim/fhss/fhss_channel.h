/*
 * Copyright (c) 2016-2019, Pelion and affiliates.
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
#ifndef FHSS_CHANNEL_H_
#define FHSS_CHANNEL_H_

// Enable this flag to use channel traces
// #define FHSS_CHANNEL_DEBUG
// Enable this flag to use debug callbacks
// #define FHSS_CHANNEL_DEBUG_CBS

#ifdef FHSS_CHANNEL_DEBUG_CBS
extern void (*fhss_uc_switch)(void);
extern void (*fhss_bc_switch)(void);
#endif /*FHSS_CHANNEL_DEBUG_CBS*/

#endif /*FHSS_CHANNEL_H_*/
