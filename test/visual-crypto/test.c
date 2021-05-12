#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rilib.h"
#include "xoshiro128starstar.h"
#include "vcrypto.h"

#include <sys/time.h>
#include <unistd.h>

#define W (320)
#define H (60)

#define CAPTION_H (13)

int main(int argc, char **argv)
{
	FILE *fp_data1, *fp_data2, *fp_key, *fp_ctext1, *fp_ctext2, *fp_ptext, *fp_highlight;
	canvas_t c_data1, c_data2, c_key, c_ctext1, c_ctext2, c_ptext, c_highlight;
	region_t r_data1, r_data2, r_key, r_ctext1, r_ctext2, r_ptext, r_highlight;
	region_t r_d_data1, r_d_data2, r_d_key, r_d_ctext1, r_d_ctext2, r_d_ptext, r_d_highlight;
	region_t r_c_data1, r_c_data2, r_c_key, r_c_ctext1, r_c_ctext2, r_c_ptext, r_c_highlight;
	uint32_t seed[4] = {
		0x19564e0e,
		0x06ae576e,
		0x5a2e83a4,
		0x5d100782,
	};

	uint8_t *buf_data1 = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));
	uint8_t *buf_data2 = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));
	uint8_t *buf_key = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));
	uint8_t *buf_ctext1 = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));
	uint8_t *buf_ctext2 = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));
	uint8_t *buf_ptext = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));
	uint8_t *buf_highlight = malloc(W*(H + CAPTION_H)*sizeof(pxl_rgb565_t));

	xoshiro128ss_seed(seed);

	rilib_init_canvas(&c_data1, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_data1);
	rilib_init_canvas(&c_data2, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_data2);
	rilib_init_canvas(&c_key, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_key);
	rilib_init_canvas(&c_ctext1, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_ctext1);
	rilib_init_canvas(&c_ctext2, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_ctext2);
	rilib_init_canvas(&c_ptext, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_ptext);
	rilib_init_canvas(&c_highlight, W, (H + CAPTION_H), IMG_FMT_RGB565, buf_highlight);

	rilib_canvas_to_region(&c_data1, &r_data1);
	rilib_canvas_to_region(&c_data2, &r_data2);
	rilib_canvas_to_region(&c_key, &r_key);
	rilib_canvas_to_region(&c_ctext1, &r_ctext1);
	rilib_canvas_to_region(&c_ctext2, &r_ctext2);
	rilib_canvas_to_region(&c_ptext, &r_ptext);
	rilib_canvas_to_region(&c_highlight, &r_highlight);

	rilib_init_region(&c_data1, &r_d_data1, 0, 0, W, H);
	rilib_init_region(&c_data2, &r_d_data2, 0, 0, W, H);
	rilib_init_region(&c_key, &r_d_key, 0, 0, W, H);
	rilib_init_region(&c_ctext1, &r_d_ctext1, 0, 0, W, H);
	rilib_init_region(&c_ctext2, &r_d_ctext2, 0, 0, W, H);
	rilib_init_region(&c_ptext, &r_d_ptext, 0, 0, W, H);
	rilib_init_region(&c_highlight, &r_d_highlight, 0, 0, W, H);

	rilib_init_region(&c_data1, &r_c_data1, 0, H, W, CAPTION_H);
	rilib_init_region(&c_data2, &r_c_data2, 0, H, W, CAPTION_H);
	rilib_init_region(&c_key, &r_c_key, 0, H, W, CAPTION_H);
	rilib_init_region(&c_ctext1, &r_c_ctext1, 0, H, W, CAPTION_H);
	rilib_init_region(&c_ctext2, &r_c_ctext2, 0, H, W, CAPTION_H);
	rilib_init_region(&c_ptext, &r_c_ptext, 0, H, W, CAPTION_H);
	rilib_init_region(&c_highlight, &r_c_highlight, 0, H, W, CAPTION_H);

	rilib_fill_region(&r_c_data1, 255, 255, 255);
	rilib_fill_region(&r_c_data2, 255, 255, 255);
	rilib_fill_region(&r_c_key, 255, 255, 255);
	rilib_fill_region(&r_c_ctext1, 255, 255, 255);
	rilib_fill_region(&r_c_ctext2, 255, 255, 255);
	rilib_fill_region(&r_c_ptext, 255, 255, 255);
	rilib_fill_region(&r_c_highlight, 255, 255, 255);

	rilib_fill_region(&r_d_data1, 255, 255, 255);
	rilib_fill_region(&r_d_data2, 255, 255, 255);

	pxl_rgb888_t pxl_black = { 0, 0, 0 };

	rilib_printf(&r_d_data1, pxl_black, 4, 0, "HELLO");
	rilib_printf(&r_d_data2, pxl_black, 4, 0, "BYE");

	rilib_printf(&r_c_data1, pxl_black, 1, 0, "data1");
	rilib_printf(&r_c_data2, pxl_black, 1, 0, "data2");
	rilib_printf(&r_c_key, pxl_black, 1, 0, "key");
	rilib_printf(&r_c_ctext1, pxl_black, 1, 0, "enc(data1)");
	rilib_printf(&r_c_ctext2, pxl_black, 1, 0, "enc(data2)");
	rilib_printf(&r_c_ptext, pxl_black, 1, 0, "dec(enc(data1))");
	rilib_printf(&r_c_highlight, pxl_black, 1, 0, "xor(enc(data1),enc(data2))");

	struct timeval tv_before, tv_after;

	vcrypto_cache_t *cache = malloc(sizeof(vcrypto_cache_t));
	vcrypto_init_cache(cache);
	gettimeofday(&tv_before, NULL);
	//vcrypto_gen_key(&r_d_key, xoshiro128ss_next, 1);
	vcrypto_gen_key_w_cache(&r_d_key, xoshiro128ss_next, cache);
	gettimeofday(&tv_after, NULL);

	fprintf(stderr, "%ld usec\n", tv_after.tv_usec - tv_before.tv_usec);

	vcrypto_enc(&r_d_key, &r_d_data1, &r_d_ctext1);

	vcrypto_enc(&r_d_key, &r_d_data2, &r_d_ctext2);

	vcrypto_dec(&r_d_key, &r_d_ctext1, &r_d_ptext);

	RILIB_FOR_EACH_PIXEL_BEGIN(&r_d_highlight, x, y, pixel) {
		*((uint16_t *)pixel) = ~(*((uint16_t *)RILIB_GET_PIXEL(&r_d_ctext1, x, y)) ^ *((uint16_t *)RILIB_GET_PIXEL(&r_d_ctext2, x, y)));
	} RILIB_FOR_EACH_PIXEL_END

	fp_data1 = fopen("data1.bmp", "w");
	fp_data2 = fopen("data2.bmp", "w");
	fp_key = fopen("key.bmp", "w");
	fp_ctext1 = fopen("ctext1.bmp", "w");
	fp_ctext2 = fopen("ctext2.bmp", "w");
	fp_ptext = fopen("ptext.bmp", "w");
	fp_highlight = fopen("highlight.bmp", "w");

	rilib_write_bmp(&r_data1, fp_data1, 1);
	rilib_write_bmp(&r_data2, fp_data2, 1);
	rilib_write_bmp(&r_key, fp_key, 1);
	rilib_write_bmp(&r_ctext1, fp_ctext1, 1);
	rilib_write_bmp(&r_ctext2, fp_ctext2, 1);
	rilib_write_bmp(&r_ptext, fp_ptext, 1);
	rilib_write_bmp(&r_highlight, fp_highlight, 1);

	fclose(fp_data1);
	fclose(fp_data2);
	fclose(fp_key);
	fclose(fp_ctext1);
	fclose(fp_ctext2);
	fclose(fp_ptext);
	fclose(fp_highlight);
}
