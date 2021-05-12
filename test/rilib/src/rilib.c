#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include <stdio.h>
#include <stdarg.h>
#include "rilib.h"
#include "font.h"

#define MIN(x, y) ((x) > (y) ? (y) : (x))

int rilib_init_canvas(canvas_t *canvas, int width, int height, int format, uint8_t *buffer)
{
	if (!canvas || !buffer)
		return -1;

	if (width <= 0 || height <= 0)
		return -1;

	if (format != IMG_FMT_RGB565 && format != IMG_FMT_RGB888)
		return -1;

	canvas->width = width;
	canvas->height = height;
	canvas->format = format;
	canvas->pixel_size = (format == IMG_FMT_RGB565) ? sizeof(pxl_rgb565_t) :
						(format == IMG_FMT_RGB888) ? sizeof(pxl_rgb888_t) : 0;
	canvas->buffer = buffer;

	return 0;
}

int rilib_init_region(canvas_t *canvas, region_t *region, int x, int y, int width, int height)
{
	if (!canvas || !region)
		return -1;

	if (x < 0 || width <= 0 || x + width > canvas->width)
		return -1;

	if (y < 0 || height <= 0 || y + height > canvas->height)
		return -1;

	region->canvas = canvas;
	region->x = x;
	region->y = y;
	region->width = width;
	region->height = height;

	return 0;
}

int rilib_canvas_to_region(canvas_t *canvas, region_t *region)
{
	if (!canvas || !region)
		return -1;

	return rilib_init_region(canvas, region, 0, 0, canvas->width, canvas->height);
}

int rilib_fill_region(region_t *region, uint8_t r, uint8_t g, uint8_t b)
{
	if (!region)
		return -1;

	const int format = region->canvas->format;
	
	assert(format == IMG_FMT_RGB565
			|| format == IMG_FMT_RGB888);

	if (format == IMG_FMT_RGB565) {
#ifdef ARM_NEON_SIMD
#error ARM Neon ISA SIMD is not yet implemented
#else
		RILIB_FOR_EACH_PIXEL_BEGIN(region, x, y, pixel) {
			((pxl_rgb565_t *)pixel)->r = r >> 3;
			((pxl_rgb565_t *)pixel)->g = g >> 2;
			((pxl_rgb565_t *)pixel)->b = b >> 3;
		} RILIB_FOR_EACH_PIXEL_END
#endif

	} else { /* format == IMG_FMT_RGB888 */
#ifdef ARM_NEON_SIMD
#error ARM Neon ISA SIMD is not yet implemented
#else
#endif
	}

	return 0;
}

//char *__g_rilib_printf_buf = NULL;
//int __g_rilib_printf_buflen = 0;

int rilib_printf(region_t *region, pxl_rgb888_t fg, int scale, int flags, const char * restrict format, ...)
{
	va_list args;

	int row, col, len, n;
	region_t char_region = {0};
	
	/* we avoid malloc to be compatible with TrustZone env. */
	char buf[1024];
//	char stack_buf[1024];
//	int stack_buflen = 1024;

//	static char *buf = ;

	if (!region || scale < 1)
		return -1;

	if ((col = region->width / (8 * scale)) <= 0
		|| (row = region->height / (13 * scale)) <= 0)
		return -1;

	va_start(args, format);
	if ((len = vsnprintf(buf, MIN(1024, row * col + 1), format, args)) <= 0) {
		va_end(args);
		return -1;
	}
	va_end(args);
	len = MIN(len, 1024);

	rilib_init_region(region->canvas, &char_region, 0, 0, 8 * scale, 13 * scale);

	for (n = 0; n < len; n++) {
		/* we print only printable ASCII characters */
		if (buf[n] >= 32 && buf[n] < 32 + sizeof(font_ogl)/sizeof(font_ogl[0])) {
			char_region.x = region->x + (n % col) * 8 * scale;
			char_region.y = region->y + (n / col) * 13 * scale;

			RILIB_FOR_EACH_PIXEL_BEGIN(&char_region, x, y, pixel) {
				if (font_ogl[buf[n] - 32][12-(y/scale)] & (uint8_t)(0x80 >> (x/scale))) {
					((pxl_rgb565_t *)pixel)->r = fg.r >> 3;
					((pxl_rgb565_t *)pixel)->g = fg.g >> 2;
					((pxl_rgb565_t *)pixel)->b = fg.b >> 3;
				}
			} RILIB_FOR_EACH_PIXEL_END
		}
	}

	return len;
}

#ifndef __KERNEL__

int rilib_write_ppm(region_t *region, FILE *fp, int scale)
{
	int i, j, si, sj;

	if (!region || scale < 1)
		return -1;

	const int format = region->canvas->format;
	
	assert(format == IMG_FMT_RGB565
			|| format == IMG_FMT_RGB888);

//	fprintf(fp, "P6\n%d %d\n255\n", region->width * scale, region->height * scale);

	if (format == IMG_FMT_RGB565) {
#ifdef ARM_NEON_SIMD
#error ARM Neon ISA SIMD is not yet implemented
#else
		const int r_width = region->width;
		const int r_height = region->height;
		const int c_width = region->canvas->width;
		pxl_rgb565_t * const buffer = &((pxl_rgb565_t *)region->canvas->buffer)[region->x + c_width * region->y];
		int i, j;

		for (j = 0; j < r_height; j++) for (sj = 0; sj < scale; sj++)
			for (i = 0; i < r_width; i++) for (si = 0; si < scale; si++) {
				pxl_rgb565_t const pixel565 = buffer[i + j * c_width];
				pxl_rgb888_t const pixel888 = {
					.r = PIXEL565_GET_R8(pixel565),
					.g = PIXEL565_GET_G8(pixel565),
					.b = PIXEL565_GET_B8(pixel565),
				};
				fwrite(&pixel888, sizeof(pxl_rgb888_t), 1, fp);
			}
#endif

	} else { /* format == IMG_FMT_RGB888 */
#ifdef ARM_NEON_SIMD
#error ARM Neon ISA SIMD is not yet implemented
#else
#endif
	}

	return 0;
}

int rilib_write_bmp(region_t *region, FILE *fp, int scale)
{
	int i, j, si, sj;

	if (!region || scale < 1)
		return -1;

	const int format = region->canvas->format;
	
	assert(format == IMG_FMT_RGB565
			|| format == IMG_FMT_RGB888);

//	fprintf(fp, "P6\n%d %d\n255\n", region->width * scale, region->height * scale);
	int w = region->width * scale;
	int h = region->height * scale;
	int filesize = 54 + 3*w*h;
	unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
	unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
	unsigned char bmppad[3] = {0,0,0};

	bmpfileheader[ 2] = (unsigned char)(filesize    );
	bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
	bmpfileheader[ 4] = (unsigned char)(filesize>>16);
	bmpfileheader[ 5] = (unsigned char)(filesize>>24);

	bmpinfoheader[ 4] = (unsigned char)(       w    );
	bmpinfoheader[ 5] = (unsigned char)(       w>> 8);
	bmpinfoheader[ 6] = (unsigned char)(       w>>16);
	bmpinfoheader[ 7] = (unsigned char)(       w>>24);
	bmpinfoheader[ 8] = (unsigned char)(       h    );
	bmpinfoheader[ 9] = (unsigned char)(       h>> 8);
	bmpinfoheader[10] = (unsigned char)(       h>>16);
	bmpinfoheader[11] = (unsigned char)(       h>>24);

	fwrite(bmpfileheader,1,14,fp);
	fwrite(bmpinfoheader,1,40,fp);

	if (format == IMG_FMT_RGB565) {
#ifdef ARM_NEON_SIMD
#error ARM Neon ISA SIMD is not yet implemented
#else
		const int r_width = region->width;
		const int r_height = region->height;
		const int c_width = region->canvas->width;
		pxl_rgb565_t * const buffer = &((pxl_rgb565_t *)region->canvas->buffer)[region->x + c_width * region->y];
		int i, j;

		for (j = 0; j < r_height; j++) for (sj = 0; sj < scale; sj++) {
			for (i = 0; i < r_width; i++) for (si = 0; si < scale; si++) {
				pxl_rgb565_t const pixel565 = buffer[i + (r_height - j - 1) * c_width];
				pxl_bgr888_t const pixel888 = {
					.r = PIXEL565_GET_R8(pixel565),
					.g = PIXEL565_GET_G8(pixel565),
					.b = PIXEL565_GET_B8(pixel565),
				};
				fwrite(&pixel888, sizeof(pxl_bgr888_t), 1, fp);
			}
			fwrite(bmppad,1,(4-(w*3)%4)%4,fp);
		}
#endif

	} else { /* format == IMG_FMT_RGB888 */
#ifdef ARM_NEON_SIMD
#error ARM Neon ISA SIMD is not yet implemented
#else
#endif
	}

	return 0;
}

#endif // __KERNEL__

#pragma GCC diagnostic pop
