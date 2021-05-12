#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <arm_neon.h>

#include "xor_rand.h"

#define GET_UPPER_16_BITS(u32) ((uint16_t)((u32)>>16))
#define GET_LOWER_16_BITS(u32) ((uint16_t)((u32)&(0xFFFF)))

uint16_t rand16_with_rand32(uint32_t (*rand32)(void))
{
	uint8_t m = 0;
	static uint32_t r32 = 0;

	if ((m = ~m))
		r32 = rand32();

	return m ? GET_UPPER_16_BITS(r32) : GET_LOWER_16_BITS(r32);
}

void xor_rand(uint8_t *in, uint8_t *out, size_t size, uint16_t (*rand16)(void), bool simd)
{
	register uint16_t r16;
	int i;

	for (i = 0; i < size; i += 32) {
		uint16x8x2_t reg_plain, reg_cipher, reg_key;

		if (i + 32 > size)
			break;

		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 0);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 1);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 2);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 3);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 4);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 5);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 6);
		r16 = rand16();
		reg_key.val[0] = vsetq_lane_u16(r16, reg_key.val[0], 7);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 0);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 1);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 2);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 3);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 4);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 5);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 6);
		r16 = rand16();
		reg_key.val[1] = vsetq_lane_u16(r16, reg_key.val[1], 7);

		reg_cipher = vld2q_u16(&in[i]);

		reg_plain.val[0] = veorq_u16(reg_key.val[0], reg_cipher.val[0]);
		reg_plain.val[1] = veorq_u16(reg_key.val[1], reg_cipher.val[1]);
		
		vst2q_u16(&out[i], reg_plain);
	}

	if (i == size)
		return;

	/* handle remainders of 32-byte unit copies */

	for (; i < size; i += 2) {
		if (i + 2 > size)
			break;

		r16 = rand16();
		*((uint16_t *)&out[i]) = r16 ^ *((uint16_t *)&in[i]);
	}

	if (i == size)
		return;

	/* handle remainders of 2-byte unit copies */

	r16 = rand16();
	out[i] = ((r16 & 0xFF00) >> 8) ^ in[i];
}

