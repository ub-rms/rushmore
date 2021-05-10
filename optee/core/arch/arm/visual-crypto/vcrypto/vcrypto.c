#include <stdarg.h>
#include <string.h>
#include "rilib.h"
#include "vcrypto.h"
#include <trace.h>

#include <arm_neon.h>

int vcrypto_gen_key(region_t *out, uint32_t (*rand32)(void), int scale)
{
	int i = 0;
	uint32_t r;
	pxl_rgb565_t const b = { .r = 0, .g = 0, .b = 0 };
	pxl_rgb565_t const w = { .r = 255 >> 3, .g = 255 >> 2, .b = 255 >> 3 };

	if (!out || !rand32)
		return -1;

	RILIB_FOR_EACH_PIXEL_BEGIN(out, x, y, pixel) {
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

	for (i = 0; i < _VCRYPTO_RAINBOW_TABLE_CASES; i++) {
		for (j = 0; j < _VCRYPTO_RAINBOW_TABLE_DEGREE; j++)
			if (i & (0x1 << j)) /* TODO: check server/client endianness */
				cache->data[i][j] = b;
			else
				cache->data[i][j] = w;
	}

	return 0;
}

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

