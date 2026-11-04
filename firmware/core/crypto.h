// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "bytes.h"
namespace gw {
void sha256(const uint8_t *data, size_t n, uint8_t out[32]);
void hmac(const uint8_t key[32], const uint8_t *data, size_t n, uint8_t out[32]);
} // namespace gw
