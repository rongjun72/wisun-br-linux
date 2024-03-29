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
#include <stdbool.h>
#include <mbedtls/aes.h>
#include "stack/mac/platform/arm_hal_aes.h"

struct arm_aes_context {
    mbedtls_aes_context ctx;
    bool reserved;
};

static arm_aes_context_t context_list[ARM_AES_MBEDTLS_CONTEXT_MIN];

static arm_aes_context_t *mbed_tls_context_get(void)
{
    for (int i = 0; i < ARM_AES_MBEDTLS_CONTEXT_MIN; i++) {
        if (!context_list[i].reserved) {
            //Reserve context
            context_list[i].reserved = true;
            return &context_list[i];
        }
    }
    return NULL;
}

arm_aes_context_t *arm_aes_start(const uint8_t key[static 16])
{
    arm_aes_context_t *context = mbed_tls_context_get();
    if (context) {
        mbedtls_aes_init(&context->ctx);
        if (0 != mbedtls_aes_setkey_enc(&context->ctx, key, 128)) {
            return NULL;
        }
    }
    return context;
}

void arm_aes_encrypt(arm_aes_context_t *aes_context, const uint8_t src[static 16], uint8_t dst[static 16])
{
    mbedtls_aes_crypt_ecb(&aes_context->ctx, MBEDTLS_AES_ENCRYPT, src, dst);
}

void arm_aes_finish(arm_aes_context_t *aes_context)
{
    mbedtls_aes_free(&aes_context->ctx);
    aes_context->reserved = false;
}
