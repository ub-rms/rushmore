#pragma once

#include <stdint.h>


uint16_t rand16_with_rand32(uint32_t (*rand32)(void));

void xor_rand(uint8_t *in, uint8_t *out, size_t size, uint16_t (*rand16)(void), bool simd);

