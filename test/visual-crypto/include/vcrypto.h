#pragma once

#include <stdint.h>
#include "rilib.h"

#define _VCRYPTO_RAINBOW_TABLE_DEGREE (16)
#define _VCRYPTO_RAINBOW_TABLE_CASES  (0x1<<_VCRYPTO_RAINBOW_TABLE_DEGREE)

#define VCRYPTO_GEN_KEY_W_CACHE_FL_SIMD (0x1)

#if _VCRYPTO_RAINBOW_TABLE_DEGREE > 64
#error Too large visual crypto rainbow table degree
#elif _VCRYPTO_RAINBOW_TABLE_DEGREE >= 32
#define _VTYPE uint64_t
#elif _VCRYPTO_RAINBOW_TABLE_DEGREE > 0
#define _VTYPE uint32_t
#else
#error Visual crypto rainbow table degree must be a positive integer
#endif

typedef struct _vcrypto_cache_t {
//	uint32_t ok;
	/* DO NOT PLACE ANYTHING THAT COULD BREAK ALIGNMENT AT HERE!!! */
	pxl_rgb565_t data[_VCRYPTO_RAINBOW_TABLE_CASES][_VCRYPTO_RAINBOW_TABLE_DEGREE];
} vcrypto_cache_t;

int vcrypto_enc(region_t *key, region_t *data, region_t *out);
int vcrypto_dec(region_t *key, region_t *cipher, region_t *out);
int vcrypto_gen_key(region_t *out, uint32_t (*rand32)(void), int scale);
int vcrypto_init_cache(vcrypto_cache_t *cache);
int vcrypto_gen_key_w_cache(region_t *out, uint16_t (*rand16)(void), vcrypto_cache_t *cache, uint32_t flags);
int vcrypto_gen_key_w_cache_and_enc(region_t *plainimage, region_t *out,
		uint32_t (*rand32)(void), vcrypto_cache_t *cache, uint32_t flags);
int vcrypto_gen_key_w_cache_and_dec(region_t *cipherimage, region_t *out,
		uint32_t (*rand32)(void), vcrypto_cache_t *cache, uint32_t flags);
