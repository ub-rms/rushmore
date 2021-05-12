#pragma once

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define IMG_FMT_RGB565	1
#define IMG_FMT_RGB888	2

#define PIXEL565_GET_R8(p565) (((pxl_rgb565_t)p565).r<<3)
#define PIXEL565_GET_G8(p565) (((pxl_rgb565_t)p565).g<<2)
#define PIXEL565_GET_B8(p565) (((pxl_rgb565_t)p565).b<<3)

#define RILIB_GET_PIXEL(region, __x, __y) \
	(&((uint8_t *)(region)->canvas->buffer)[((region)->x + (__x) + (region)->canvas->width * ((region)->y + (__y))) * (region)->canvas->pixel_size])

#define RILIB_FOR_EACH_PIXEL_BEGIN(region, __i, __j, __pixel) \
{ \
	const int __r_width = (region)->width; \
	const int __r_height = (region)->height; \
	const int __c_width = (region)->canvas->width; \
	const int __pxl_size = (region)->canvas->pixel_size; \
	uint8_t * const __buffer = &((uint8_t *)(region)->canvas->buffer)[((region)->x + (__c_width) * (region)->y) * __pxl_size]; \
	int __i, __j; \
	void * __pixel = __buffer; \
	for ((__j) = 0; (__j) < __r_height; __j++) \
	for ((__i) = 0; (__i) < __r_width; __i++) { \
		__pixel = &(__buffer)[((__i) + (__j) * (__c_width)) * __pxl_size];

#define RILIB_FOR_EACH_PIXEL_END \
	} \
}

typedef struct _pxl_rgb565_t {
	/* little endian. r includes MSB and b includes LSB */
	uint16_t b:5,
			 g:6,
			 r:5;
} __attribute__((packed)) pxl_rgb565_t;

typedef struct _pxl_rgb888_t {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __attribute__((packed)) pxl_rgb888_t;

typedef struct _pxl_bgr888_t {
	uint8_t b;
	uint8_t g;
	uint8_t r;
} __attribute__((packed)) pxl_bgr888_t;

typedef struct _canvas_t {
	int width;
	int height;

	int format;
	int pixel_size;

	uint8_t *buffer;
} canvas_t;

typedef struct _region_t {
	canvas_t *canvas;

	int x;
	int y;
	int width;
	int height;
} region_t;

/** Initialize a canvas.
 * [out] canvas
 * [in]  width
 * [in]  height
 * [in]  format: Either RGB888 or RGB565.
 * [in]  buffer
 * [ret] 0 on success, -1 on failure
 */
int rilib_init_canvas(canvas_t *canvas, int width, int height, int format, uint8_t *buffer);

/** Initialize a region.
 * [in]  canvas
 * [out] region
 * [in]  x
 * [in]  y
 * [in]  width
 * [in]  height
 * [ret] 0 on success, -1 on failure
 */
int rilib_init_region(canvas_t *canvas, region_t *region, int x, int y, int width, int height);

/** Initialize a region to cover an entire canvas.
 * [in]  canvas
 * [out] region
 * [ret] 0 on success, -1 on failure
 */
int rilib_canvas_to_region(canvas_t *canvas, region_t *region);

/** Fill a region with a given color
 * [in]  region
 * [in]  r
 * [in]  g
 * [in]  b
 * [ret] 0 on success, -1 on failure
 */
int rilib_fill_region(region_t *region, uint8_t r, uint8_t g, uint8_t b);

/** Set the color of a pixel
 * [in]  region
 * [in]  x
 * [in]  y
 * [in]  r
 * [in]  g
 * [in]  b
 * [ret] 0 on success, -1 on failure
 */
int rilib_set_color(region_t *region, int x, int y, uint8_t r, uint8_t g, uint8_t b);

/** Render text to a region. Maximum string length is 1024.
 * [in]  region
 * [in]  scale
 * [in]  flags: TEXT_LEFT, TEXT_CENTER_HORIZONTAL, TEXT_RIGHT, TEXT_TOP, TEXT_BOTTOM, TEXT_CENTER_VERTICAL
 * [in]  format
 * [ret] the number of characters rendered on success, -1 on failure
 */
int rilib_printf(region_t *region, pxl_rgb888_t fg, int scale, int flags, const char * restrict format, ...);

#ifndef __KERNEL__
int rilib_write_ppm(region_t *region, FILE *fp, int scale);

int rilib_write_bmp(region_t *region, FILE *fp, int scale);
#endif // __KERNEL__
