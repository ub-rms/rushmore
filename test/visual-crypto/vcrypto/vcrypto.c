#include <stdarg.h>
#include <string.h>
#include "rilib.h"
#include "xoshiro128starstar.h"
#include "vcrypto.h"
#include <trace.h>

#include <arm_neon.h>

pxl_rgb565_t __pxl_black = { 0, 0, 0 };
pxl_rgb565_t __pxl_white = { 255 >> 3, 255 >> 2, 255 >> 3 };

#define GET_UPPER_16_BITS(u32) ((uint16_t)((u32)>>16))
#define GET_LOWER_16_BITS(u32) ((uint16_t)((u32)&(0xFFFF)))

inline static uint16_t rand16_with_rand32(uint32_t (*rand32)(void))
{
	static uint8_t m = 0;
	static uint32_t r32 = 0;

	if ((m = ~m))
		r32 = rand32();

	return m ? GET_UPPER_16_BITS(r32) : GET_LOWER_16_BITS(r32);
}

int vcrypto_enc(region_t *key, region_t *data, region_t *out)
{
	if (!key || !data || !out)
		return -1;

	if (key->width != data->width || key->width != out->width)
		return -1;

	if (key->height != data->height || key->height != out->height)
		return -1;

	RILIB_FOR_EACH_PIXEL_BEGIN(out, x, y, pixel) {
		*((uint16_t *)pixel) = ~(*((uint16_t *)RILIB_GET_PIXEL(key, x, y)) ^ *((uint16_t *)RILIB_GET_PIXEL(data, x, y)));
//		if (*((uint16_t *)RILIB_GET_PIXEL(key, x, y)) == *((uint16_t *)RILIB_GET_PIXEL(data, x, y)))
//			*((pxl_rgb565_t *)pixel) = __pxl_white;
//		else
//			*((pxl_rgb565_t *)pixel) = __pxl_black;
	} RILIB_FOR_EACH_PIXEL_END
	
	return 0;
}

int vcrypto_dec(region_t *key, region_t *data, region_t *out)
{
	if (!key || !data || !out)
		return -1;

	if (key->width != data->width || key->width != out->width)
		return -1;

	if (key->height != data->height || key->height != out->height)
		return -1;

	RILIB_FOR_EACH_PIXEL_BEGIN(out, x, y, pixel) {
		*((uint16_t *)pixel) = *((uint16_t *)RILIB_GET_PIXEL(key, x, y)) & *((uint16_t *)RILIB_GET_PIXEL(data, x, y));
	} RILIB_FOR_EACH_PIXEL_END

	return 0;
}

int vcrypto_gen_key(region_t *out, uint32_t (*rand32)(void), int scale)
{
	int i = 0;
	uint32_t r;
	pxl_rgb565_t const b = { .r = 0, .g = 0, .b = 0 };
	pxl_rgb565_t const w = { .r = 255 >> 3, .g = 255 >> 2, .b = 255 >> 3 };

	if (!out || !rand32)
		return -1;

	RILIB_FOR_EACH_PIXEL_BEGIN(out, x, y, pixel) {
#if 0
		if ((x % scale) || (y % scale)) {
			*((pxl_rgb565_t *)pixel) = *((pxl_rgb565_t *)&(__buffer)[(((x/scale) * scale) + ((y/scale) * scale) * (__c_width)) * __pxl_size]);
		} else {
			if (i == 0)
				r = rand32();

			if (r & (0x1 << i)) /* TODO: check server/client endianness */
				*((pxl_rgb565_t *)pixel) = w;
			else
				*((pxl_rgb565_t *)pixel) = b;

			i = (i + 1) % 32;
		}
#else
		if (y % 2)
			continue;

		if (i == 0)
			r = rand32();

		if (r & (0x1 << i)) { /* TODO: check server/client endianness */
			*((pxl_rgb565_t *)pixel) = w;
			((pxl_rgb565_t *)pixel)[out->width] = b;
		} else {
			*((pxl_rgb565_t *)pixel) = b;
			((pxl_rgb565_t *)pixel)[out->width] = w;
		}

		i = (i + 1) % 32;
#endif
	} RILIB_FOR_EACH_PIXEL_END

	return 0;
}

int vcrypto_init_cache(vcrypto_cache_t *cache) {
	_VTYPE i;
	int j;
	pxl_rgb565_t const b = { .r = 0, .g = 0, .b = 0 };
	pxl_rgb565_t const w = { .r = 255 >> 3, .g = 255 >> 2, .b = 255 >> 3 };

	if (!cache)
		return -1;

//	if (cache->ok)
//		return 0;

	for (i = 0; i < _VCRYPTO_RAINBOW_TABLE_CASES; i++) {
		for (j = 0; j < _VCRYPTO_RAINBOW_TABLE_DEGREE; j++)
			if (i & (0x1 << j)) /* TODO: check server/client endianness */
				cache->data[i][j] = b;
			else
				cache->data[i][j] = w;
	}

//	cache->ok = 1;

#if 0
	canvas_t c;
	region_t r;
	FILE *f = fopen("cache.bmp", "w");

	rilib_init_canvas(&c, _VCRYPTO_RAINBOW_TABLE_DEGREE * 8, 100, IMG_FMT_RGB565, (uint8_t *)cache->data[0]);
	rilib_canvas_to_region(&c, &r);
	rilib_write_bmp(&r, f, 1);

	fclose(f);
#endif

	return 0;
}

uint16_t FILTERR[16] = { 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800,
						 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800 };
uint16_t FILTERG[16] = { 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0,
						 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x07e0 };
uint16_t FILTERB[16] = { 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f,
						 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f };

int vcrypto_gen_key_w_cache(region_t *out, uint16_t (*rand16)(void), vcrypto_cache_t *cache, uint32_t flags)
{
	int j; /* m must be initialized as 0 */
	uint16_t r = 0;

	const int simd = flags & VCRYPTO_GEN_KEY_W_CACHE_FL_SIMD;
	uint16_t *leftover = NULL, *inv_leftover = NULL;
	int lo_pos = _VCRYPTO_RAINBOW_TABLE_DEGREE;

	if (!out)
		return -1;

	if (!cache)
		return -1;

	for (j = 0; j < out->height; j += 2) {
		/* start of a new row */
		int i = 0;
		uint16_t *dst_row = (uint16_t *)RILIB_GET_PIXEL(out, 0, j);
		uint16_t *dst_row2 = (uint16_t *)RILIB_GET_PIXEL(out, 0, j + 1);

		/* drain leftover from previous row */
		for (; lo_pos < _VCRYPTO_RAINBOW_TABLE_DEGREE; lo_pos++, i++) {
			dst_row[i] = leftover[lo_pos];
			dst_row2[i] = inv_leftover[lo_pos];
		}

		/* ensure there are no leftovers */
		leftover = inv_leftover = NULL;

		/* fast copy */
		for (; i + _VCRYPTO_RAINBOW_TABLE_DEGREE <= out->width; i += _VCRYPTO_RAINBOW_TABLE_DEGREE) {
			r = rand16();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
			uint16_t *dst = (uint16_t *)RILIB_GET_PIXEL(out, i, j);
			uint16_t *dst2 = (uint16_t *)RILIB_GET_PIXEL(out, i, j + 1);
			uint16_t *src = (uint16_t *)cache->data[r];
			uint16_t *inv_src = (uint16_t *)cache->data[(uint16_t)~r];
#pragma GCC diagnostic pop

			if (simd) {
				uint16x8x2_t line = vld2q_u16(src);
				vst2q_u16(dst, line);
				line.val[0] = vmvnq_u16(line.val[0]);
				line.val[1] = vmvnq_u16(line.val[1]);
				vst2q_u16(dst2, line);
			} else {
				memcpy(dst, src, _VCRYPTO_RAINBOW_TABLE_DEGREE * sizeof(uint16_t));
				memcpy(dst2, inv_src, _VCRYPTO_RAINBOW_TABLE_DEGREE * sizeof(uint16_t));
			}
		}

		/* fill remainders and leave leftover */
		if (i < out->width) {
			r = rand16();
			lo_pos = 0;
			leftover = (uint16_t *)cache->data[r];
			inv_leftover = (uint16_t *)cache->data[(uint16_t)~r];

			for (; i < out->width; lo_pos++, i++) {
				dst_row[i] = leftover[lo_pos];
				dst_row2[i] = inv_leftover[lo_pos];
			}
		}
	}

	return 0;
}

int vcrypto_gen_key_w_cache_8_clr(region_t *out, uint32_t (*rand32)(void), vcrypto_cache_t *cache, uint32_t flags)
{
	int i, j, m = 0; /* m must be initialized as 0 */
	uint32_t r = 0;

	const int simd = flags & VCRYPTO_GEN_KEY_W_CACHE_FL_SIMD;

	int width;
#if 0
	if (!out || (out->width % _VCRYPTO_RAINBOW_TABLE_DEGREE))
		return -1;
	width = out->width;
#else
	if (!out)
		return -1;
	if (out->width % _VCRYPTO_RAINBOW_TABLE_DEGREE)
		width = (out->width / _VCRYPTO_RAINBOW_TABLE_DEGREE) * _VCRYPTO_RAINBOW_TABLE_DEGREE;
	else
		width = out->width;
#endif

	if (!cache /* || !cache->ok */)
		return -1;

	uint16x8x2_t reg_filterr, reg_filterg, reg_filterb;

	reg_filterr = vld2q_u16(FILTERR);
	reg_filterg = vld2q_u16(FILTERG);
	reg_filterb = vld2q_u16(FILTERB);

	for (j = 0; j < out->height; j += 2) {
		for (i = 0; i + _VCRYPTO_RAINBOW_TABLE_DEGREE < width; i += _VCRYPTO_RAINBOW_TABLE_DEGREE) {
			uint16_t r16r = rand16_with_rand32(rand32);
			uint16_t r16g = rand16_with_rand32(rand32);
			uint16_t r16b = rand16_with_rand32(rand32);

			if (simd) {
				// 200408 dhkim: Turn off compile warning termporaily
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
				uint16_t *r0_dst = (uint16_t *)RILIB_GET_PIXEL(out, i, j);
				uint16_t *r1_dst = (uint16_t *)RILIB_GET_PIXEL(out, i, j + 1);
				uint16_t *keyr = (uint16_t *)cache->data[r16r];
				uint16_t *keyg = (uint16_t *)cache->data[r16g];
				uint16_t *keyb = (uint16_t *)cache->data[r16b];
#pragma GCC diagnostic pop

				uint16x8x2_t reg_keyr, reg_keyg, reg_keyb, reg_key;

				reg_keyr = vld2q_u16(keyr);
				reg_keyg = vld2q_u16(keyg);
				reg_keyb = vld2q_u16(keyb);

				reg_keyr.val[0] = vandq_u16(reg_keyr.val[0], reg_filterr.val[0]);
				reg_keyr.val[1] = vandq_u16(reg_keyr.val[1], reg_filterr.val[1]);
				reg_keyg.val[0] = vandq_u16(reg_keyg.val[0], reg_filterg.val[0]);
				reg_keyg.val[1] = vandq_u16(reg_keyg.val[1], reg_filterg.val[1]);
				reg_keyb.val[0] = vandq_u16(reg_keyb.val[0], reg_filterb.val[0]);
				reg_keyb.val[1] = vandq_u16(reg_keyb.val[1], reg_filterb.val[1]);

				reg_key.val[0] = vorrq_u16(reg_keyr.val[0], reg_keyg.val[0]);
				reg_key.val[0] = vorrq_u16(reg_key.val[0], reg_keyb.val[0]);
				reg_key.val[1] = vorrq_u16(reg_keyr.val[1], reg_keyg.val[1]);
				reg_key.val[1] = vorrq_u16(reg_key.val[1], reg_keyb.val[1]);

				vst2q_u16(r0_dst, reg_key);

				reg_key.val[0] = vmvnq_u16(reg_key.val[0]);
				reg_key.val[1] = vmvnq_u16(reg_key.val[1]);

				vst2q_u16(r1_dst, reg_key);
			} else {
				memcpy((void *)RILIB_GET_PIXEL(out, i, j),
						(void *)cache->data[m ? GET_UPPER_16_BITS(r) : GET_LOWER_16_BITS(r)],
						_VCRYPTO_RAINBOW_TABLE_DEGREE * sizeof(pxl_rgb565_t));
			}
		}
	}

	return 0;
}

int vcrypto_gen_key_w_cache_and_enc(region_t *plainimage, region_t *out,
		uint32_t (*rand32)(void), vcrypto_cache_t *cache, uint32_t flags)
{
	// DMSG("[SeCloak] Enc 0");
	int i, j;

	const int simd = flags & VCRYPTO_GEN_KEY_W_CACHE_FL_SIMD;

	if (!out || (out->width % _VCRYPTO_RAINBOW_TABLE_DEGREE) || (out->height % 2))
		return -1;
	// DMSG("[SeCloak] Enc 1");

	if (!cache /* || !cache->ok */)
		return -1;
	// DMSG("[SeCloak] Enc 2");

	for (j = 0; j < out->height; j++) {
		for (i = 0; i < out->width; i += _VCRYPTO_RAINBOW_TABLE_DEGREE) {
			uint16x8x2_t reg_r0_cip, reg_r1_cip, reg_pln;
			uint16x8x2_t reg_keyb, reg_key;

			// 200408 dhkim: Turn off compile warning termporaily
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
			uint16_t *r0_dst = (uint16_t *)RILIB_GET_PIXEL(out, i, j);
			uint16_t *r0_pln = (uint16_t *)RILIB_GET_PIXEL(plainimage, i, j);
			// uint16_t *r1_pln = (uint16_t *)RILIB_GET_PIXEL(plainimage, i, j + 1);
#pragma GCC diagnostic pop

			register uint16_t r16;
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 0);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 1);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 2);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 3);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 4);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 5);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 6);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 7);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 0);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 1);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 2);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 3);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 4);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 5);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 6);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 7);

			// r1: row 1: j + 1
			reg_pln = vld2q_u16(r0_pln);
			reg_r0_cip.val[0] = veorq_u16(reg_key.val[0], reg_pln.val[0]);
			reg_r0_cip.val[1] = veorq_u16(reg_key.val[1], reg_pln.val[1]);

			vst2q_u16(r0_dst, reg_r0_cip);
		}
	}

	return 0;
}

int vcrypto_gen_key_w_cache_and_dec(region_t *cipherimage, region_t *out,
		uint32_t (*rand32)(void), vcrypto_cache_t *cache, uint32_t flags)
{
	// DMSG("[SeCloak] Dec 0");
	int i, j;

	const int simd = flags & VCRYPTO_GEN_KEY_W_CACHE_FL_SIMD;

	if (!out || (out->width % _VCRYPTO_RAINBOW_TABLE_DEGREE) || (out->height % 2))
		return -1;
	// DMSG("[SeCloak] Dec 1");

	if (!cache /* || !cache->ok */)
		return -1;
	// DMSG("[SeCloak] Dec 2");

	uint16x8x2_t reg_filter[16];

	for (j = 0; j < out->height; j++) {
		for (i = 0; i < out->width; i += _VCRYPTO_RAINBOW_TABLE_DEGREE) {
			uint16x8x2_t reg_r0_plain, reg_r1_plain, reg_cipher;
			uint16x8x2_t reg_keyb, reg_key;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
			uint16_t *r0_dst = (uint16_t *)RILIB_GET_PIXEL(out, i, j);
			uint16_t *r0_cip = (uint16_t *)RILIB_GET_PIXEL(cipherimage, i, j);
#pragma GCC diagnostic pop

#if 0
			for (k = 0; k < 16; k++) {
				uint16_t r16 = rand16_with_rand32(rand32);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
				uint16_t *key = (uint16_t *)cache->data[r16];
#pragma GCC diagnostic pop

				reg_keyb = vld2q_u16(key);

				reg_key.val[0] = vbslq_u16(reg_filter[k].val[0], reg_keyb.val[0], reg_key.val[0]);
				reg_key.val[1] = vbslq_u16(reg_filter[k].val[1], reg_keyb.val[1], reg_key.val[1]);
			}
#else
			register uint16_t r16;
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 0);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 1);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 2);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 3);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 4);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 5);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 6);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 7);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 0);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 1);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 2);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 3);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 4);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 5);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 6);
			r16 = rand16_with_rand32(rand32);
			reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 7);
#endif

			// r0: row 0: j
			reg_cipher = vld2q_u16(r0_cip);
			reg_r0_plain.val[0] = veorq_u16(reg_key.val[0], reg_cipher.val[0]);
			reg_r0_plain.val[1] = veorq_u16(reg_key.val[1], reg_cipher.val[1]);
			
			vst2q_u16(r0_dst, reg_r0_plain);
		}
	}

	return 0;
}
