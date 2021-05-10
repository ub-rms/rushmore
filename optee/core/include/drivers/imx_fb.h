#ifndef DRIVERS_IMX_FB_H
#define DRIVERS_IMX_FB_H

#include <stdint.h>
#include <stdbool.h>
#include <mm/core_mmu.h>
#include <drivers/serial.h>

// #define CHANGMIN

//#define RGB24
//#define RGB32
#define RGB565

/* To extract each color value from different color format */
#define COLOR_RED 0
#define COLOR_GREEN 1
#define COLOR_BLUE 2


#include <platform_config.h>
/* CFG_FBMEM_SIZE 0x00800000 */
#define CFG_FBMEM_END (CFG_FBMEM_START + 0x800000)
#ifdef CHANGMIN
#define RENDER_BUFFER (CFG_FBMEM_START + 0x000000) // 1280 * 800 * 4 == 0x3e8000
#define MASK_BUFFER (CFG_FBMEM_START + 0x400000) // 1280 * 800 * 4 == 0x3e8000
#else // CHANGMIN
#define RENDER_BUFFER (CFG_FBMEM_START + 0x200000) // size: 1280 * 800 * 2 == 0x1f4000
#define MASK_BUFFER (RENDER_BUFFER + 0x1f4000) // size: 1280 * 800 * 2 == 0x1f4000
#endif // CHANGMIN
#define TEMP_BUFFER (MASK_BUFFER) // Assuming we don't use mask
#define CACHE_BUFFER (MASK_BUFFER + 0x1f4000) // size: 2^16 * (16 * 2) == 0x200000
// Each of TEMP2,3,4 buffers have the one-third of remainders (0x800000 - offset == 98,304). 
#define TEMP2_BUFFER (CACHE_BUFFER + 0x200000)
#define TEMP2_BUFFER_SIZE ((CFG_FBMEM_END - TEMP2_BUFFER) / 3)
#define TEMP3_BUFFER (CACHE_BUFFER + TEMP2_BUFFER_SIZE)
#define TEMP3_BUFFER_SIZE ((CFG_FBMEM_END - TEMP3_BUFFER) / 2)
#define TEMP4_BUFFER (CACHE_BUFFER + TEMP3_BUFFER_SIZE)
#define TEMP4_BUFFER_SIZE (CFG_FBMEM_END - TEMP4_BUFFER)

#ifdef RGB32
#define PIXEL32(r, g, b, a) ((uint8_t[4]) {})
#define BYTES_PER_PIXEL 4

#elif defined(RGB24)
#define BYTES_PER_PIXEL 3
#define PIXEL24(r, g, b) ((uint8_t[3]) {})

#elif defined(RGB565)
// PIXEL565(r, g, b): uint8_t r, g, b values into a RGB565 pixel
#define PIXEL565(r, g, b) ((uint16_t)( ((((uint16_t)r)*0x1f/0xff)<<0xb) \
			| ((((uint16_t)g)*0x3f/0xff)<<0x5) \
			| ((((uint16_t)b)*0x1f/0xff)) ))
#define BYTES_PER_PIXEL 2

#else
#error pixel format undefined
#endif

struct fb_info {
	uint8_t *buffer;
	uint8_t *mask;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t prev_params_ch0[16];
	uint32_t prev_params_ch1[16];

    uint16_t background_color;
};

struct image {
	const uint8_t *buffer;
	uint32_t width;
	uint32_t height;
};

struct blit {
	uint32_t x;
	uint32_t y;
	struct image image;
};

void printIpuRegisters(void);

int fb_set_size(struct fb_info *info, int width, int height);
int fb_set_position(struct fb_info *info, int x, int y);

int fb_enable_mask(struct fb_info *info);
int fb_disable_mask(struct fb_info *info);
int fb_is_mask_enabled(struct fb_info *info);

int ipu_overlay_direct_map(paddr_t pa);
int fb_enable_overlay(struct fb_info *info);
int fb_disable_overlay(struct fb_info *info);
int fb_is_overlay_enabled(struct fb_info *info);

int fb_enable_overlay_transparency(struct fb_info *info, uint8_t alpha, uint16_t background_color);
int fb_enable_overlay_transparency_inner(struct fb_info *info, uint8_t alpha, uint8_t r, uint8_t g, uint8_t b);
int fb_disable_overlay_transparency(struct fb_info *info);
int fb_set_overlay_transparency(struct fb_info *info, uint8_t alpha, uint8_t rkey, uint8_t gkey, uint8_t bkey);
bool fb_acquire(struct fb_info *info);
void fb_release(struct fb_info *info);

void fb_clear(struct fb_info *info, uint8_t r, uint8_t g, uint8_t b);
void fb_blit(struct fb_info *info, uint32_t x, uint32_t y, const uint8_t *buffer, uint32_t width, uint32_t height);
void fb_blit_image(struct fb_info *info, uint32_t x, uint32_t y, const struct image *image);
void fb_blit_all(struct fb_info *info, const struct blit *blits, int num_blits);

int fb_flush(struct fb_info *info);
int fb_remove_region(struct fb_info *info, int width, int height, int x_pos, int y_pos);


uint8_t color_from_rgb565(uint16_t rgb565, int color);

#endif

