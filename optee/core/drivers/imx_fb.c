/*                                                                                                                                    
 * Copyright 2005-2016 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

// Retained the above copyright/licensing notices from the IPU driver in
// the Linux kernel, which this driver is based on. Relevant files:
//   drivers/mxc/ipu3/ipu_param_mem.h

#include <drivers/imx_fb.h>

#include <errno.h>
#include <io.h>
#include <kernel/panic.h>
#include <kernel/dt.h>
#include <malloc.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <platform_config.h>
#include <string.h>
#include <initcall.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

#define MAX_WIDTH       1280
#define MAX_HEIGHT      800
#define MAX_RESOLUTION  (MAX_WIDTH * MAX_HEIGHT)

static paddr_t g_base_paddr;
static paddr_t g_base_vaddr;
static vaddr_t g_cpmem_vaddr;
static vaddr_t g_cm_vaddr;
static uint8_t *g_nwfb = NULL;
static uint8_t *g_rb = NULL;
static uint8_t *g_mb = NULL;

static int g_overlay_enabled = 0;
static int g_mask_enabled = 0;

#define BLIT_BUFFER 0
#define BLIT_MASK   1

#define IPU_BASE            0x02400000
#define IPU_SIZE            0x00400000
#define IPU_CM_OFFSET       0x00200000
#define IPU_CPMEM_OFFSET    0x00300000
#define IPU_CHAN0 23 // Channel for primary flow (Buffers 0 and 1)
#define IPU_CHAN1 69 // Channel for primary flow (Buffer 2)
#define IPU_CHAN_27 27
#define IPU_CHAN_44 44

#define IPUx_CONF                       0x00000
#define IPUx_DP_COM_CONF_SYNC           0x18000
#define IPUx_DP_Graph_Wind_CTRL_SYNC    0x18004
#define IPUx_DP_FG_POS_SYNC             0x18008
#define IPUx_IDMAC_CH_BUSY_1            0x08100
#define IPUx_IDMAC_CH_BUSY_2            0x08104
#define IPUx_IDMAC_CH_EN_1              0x08004
#define IPUx_IDMAC_CH_EN_2              0x08008
#define IPUx_IC_CMBP_1                  0x20010


#define U32_BITMASK(from, to) \
    ((uint32_t)(((((uint64_t)0x1) << (((uint8_t)(from)) - ((uint8_t)(to)) + 1)) - 1) << ((uint8_t)(to))))

#define U32_READ_BITFIELD(u32, from, to) \
    ((uint32_t)((((uint32_t)(u32)) & U32_BITMASK(from, to)) >> ((uint8_t)(to))))

#define U32_WRITE_BITFIELD(u32, from, to, value) \
    ((u32) = ((uint32_t)( (((uint32_t)(u32)) & ~U32_BITMASK(from, to)) \
        | ((((uint32_t)(value)) & U32_BITMASK((from) - (to), 0)) << ((uint8_t)(to))) )))
#define test_bitmask(from, to) \
    printf("Bitmask for [%d:%d] : 0x%08X\n", from, to, U32_BITMASK(from, to))

#define test_read_bitfield(reg, from, to) \
    printf("reg: 0x%08X, [%d:%d] : 0x%08X\n", reg, from, to, U32_READ_BITFIELD(reg, from, to))




/* Convert RGB565 color to 8 byte color value (Red, Green, Blue) */
uint8_t color_from_rgb565(uint16_t rgb565, int color) {
    uint8_t out;

    switch (color) {
        case COLOR_RED:
            out = ((rgb565 & 0xf800) >> 11) * 256 / 0x1f;
            if (out > 0xff) out = 0xff;
            return out;

        case COLOR_GREEN:
            out = ((rgb565 & 0x07e0) >> 5) * 256 / 0x3f;
            if (out > 0xff) out = 0xff;
            return out;
        
        case COLOR_BLUE:
            out = ((rgb565 & 0x0001f) >> 0) * 256 / 0x1f;
            if (out > 0xff) out = 0xff;
            return out;

        default:
            EMSG("Undefined color received");
            return NULL;
    }
}


static uint32_t ipu_cm_read_reg(vaddr_t base, int ipu, vaddr_t offset, int bit_from, int bit_to) {
	//IMSG("[FB] ipu_cm_read_reg: Read bitfield 0x%08lx [%d:%d]\n", (base + offset), bit_from, bit_to);
	return U32_READ_BITFIELD(*((uint32_t *)(base + offset)), bit_from, bit_to);
}

static void ipu_cm_write_reg(vaddr_t base, int ipu, vaddr_t offset, int bit_from, int bit_to, uint32_t value) {
	U32_WRITE_BITFIELD(*((uint32_t *)(base + offset)), bit_from, bit_to, value);
}

static uint32_t ipu_ch_param_read_field(vaddr_t base, int channel, int w, int bit, int size) {
	int i = bit / 32;
	int off = bit % 32;
	uint32_t mask = (1UL << (size)) - 1;

	int reg_offset = channel * 64;
	reg_offset += w * 32;
	reg_offset += i * 4;

	uint32_t value = io_read32(base + reg_offset);
	value = mask & (value >> off);

	if (((bit + size - 1) / 32) > i) {
		uint32_t temp = io_read32(base + reg_offset + 4);
		temp &= mask >> (off ? (32 - off) : 0);
		value |= temp << (off ? (32 - off) : 0);
	}

	return value;
}

static void ipu_ch_param_write_field(vaddr_t base, int channel, int w, int bit, int size, uint32_t value) {
	int i = bit / 32;
	int off = bit % 32;
	uint32_t mask = (1UL << (size)) - 1;

	int reg_offset = channel * 64;
	reg_offset += w * 32;
	reg_offset += i * 4;

	uint32_t temp = io_read32(base + reg_offset);
	temp &= ~(mask << off);
	temp |= value << off;
	io_write32(base + reg_offset, temp);

	if (((bit + size - 1) / 32) > i) {
		temp = io_read32(base + reg_offset + 4);
		temp &= ~(mask >> (32 - off));
		temp |= (value >> (off ? (32 - off) : 0));
		io_write32(base + reg_offset + 4, temp);
	}
}

static void test_ipu_channel_status(int channel) {
	vaddr_t offset;
	int position;

	if (channel < 32) {
		offset = IPUx_IDMAC_CH_BUSY_1;
		position = channel;
	} else if (channel <= 52) {
		offset = IPUx_IDMAC_CH_BUSY_2;
		position = channel - 32;
	} else {
		return;
	}

	IMSG("[FB] Channel %d is %s\n", channel,
			(ipu_cm_read_reg(g_cm_vaddr, 0, offset, position, position) ? "busy" : "idle"));
}

static void _fb_blit(struct fb_info *info, uint32_t x, uint32_t y, const uint8_t *buffer, uint32_t width, uint32_t height, int mode) {
	uint8_t *dest = (mode == BLIT_BUFFER) ? info->buffer : (mode == BLIT_MASK) ? info->mask : NULL;
	if (dest == NULL)
		return;

	if (x >= info->width || y >= info->height) {
		return;
	}

	int copy_per_line;
	if (x + width > info->width) {
		copy_per_line = BYTES_PER_PIXEL * (info->width - x);
	} else {
		copy_per_line = BYTES_PER_PIXEL * width;
	}

	for (uint32_t by = 0; (by < height) && (y + by < info->height); by++) {
		uint8_t *fb_line = dest + (info->stride * (y + by)) + (BYTES_PER_PIXEL * x);
		const uint8_t *buf_line = buffer + (BYTES_PER_PIXEL * width * by);
		//IMSG("[FB] pixel: 0x%04x (at %p). copy_per_line: %d\n", (uint16_t)*buf_line, (const void *)buf_line, copy_per_line);
		memcpy(fb_line, buf_line, copy_per_line);
	}
}

#define ipu_ch_param_write_field(base, channel, w, bit, size, value) do { \
	uint32_t prev = ipu_ch_param_read_field(g_cpmem_vaddr, channel, w, bit, size); \
	IMSG("[FB] Update IPU channel param at {channel: %d, w: %d, bit: %d, size: %d} from 0x%08x to 0x%08x.\n", \
			channel, w, bit, size, prev, (unsigned int)(value)); \
	ipu_ch_param_write_field(base, channel, w, bit, size, value); \
} while (0)

#define ipu_cm_write_reg(base, ipu, offset, bit_from, bit_to, value) do { \
	uint32_t prev = ipu_cm_read_reg(base, ipu, offset, bit_from, bit_to); \
	IMSG("[FB] Update IPU channel param at {IPU%d: %s (0x%08x) [%d:%d]} from 0x%08x to 0x%08x.\n", \
			ipu, #offset, offset, bit_from, bit_to, prev, (unsigned int)(value)); \
	ipu_cm_write_reg(base, ipu, offset, bit_from, bit_to, value); \
} while (0)

#undef ipu_ch_param_write_field
#undef ipu_cm_write_reg


bool fb_acquire(struct fb_info *info /* , uint8_t r, uint8_t g, uint8_t b */) {
    
    for (int p = 0; p < 16; p++) {
		info->prev_params_ch0[p] = io_read32(g_cpmem_vaddr + (IPU_CHAN0 * 64) + (p * 4));
	}

	info->buffer = g_rb ? g_rb : (g_rb = phys_to_virt(RENDER_BUFFER, MEM_AREA_IO_SEC), g_rb);
	info->mask = g_mb ? g_mb : (g_mb = phys_to_virt(MASK_BUFFER, MEM_AREA_IO_SEC), g_mb);

    info->background_color = 0xffff;

	return true;
}

int fb_set_size(struct fb_info *info, int width, int height) {
#ifndef CHANGMIN
	if (!fb_is_overlay_enabled(info))
		return -1;
#else // CHANGMIN
    /*
	if (!fb_is_overlay_enabled(info))
		return -1;
    */
#endif // CHANGMIN

	info->width = width;
	info->height = height;
	info->stride = info->width * BYTES_PER_PIXEL;

#ifndef CHANGMIN
	//IMSG("[FB] Screen Size of %dx%d", info->width, info->height);
#else // CHANGMIN
	IMSG("[FB] Screen Size of %dx%d", info->width, info->height);
#endif // CHANGMIN

	ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 0, 125, 13, info->width - 1);
	ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 0, 138, 12, info->height - 1);
	ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 102, 14, info->stride - 1);

	if (fb_is_mask_enabled(info)) {
		ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_44, 0, 125, 13, info->width - 1);
		ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_44, 0, 138, 12, info->height - 1);
		ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_44, 1, 102, 14, info->stride - 1);
	}

	return 0;
}

int fb_set_position(struct fb_info *info, int x, int y) {
#ifndef CHANGMIN
	if (!fb_is_overlay_enabled(info))
		return -1;
#else // CHANGMIN
    /*
	if (!fb_is_overlay_enabled(info))
		return -1;
    */
#endif // CHANGMIN

	uint32_t reg = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_FG_POS_SYNC, 31, 0);
	/**
	 * DP_FGXP_SYNC: IPUx_DP_FG_POS_SYNC [26:16]
	 * FGXP partial plane Window X Position.
	 * partial plane Window X Position - Specifies the number of pixels between the start of full plane window X position and the beginning of the first data of new line.
	 **/
	U32_WRITE_BITFIELD(reg, 26, 16, x);

	/**
	 * DP_FGYP_SYNC: IPUx_DP_FG_POS_SYNC [10:0]
	 * FGXP partial plane Window Y Position.
	 * partial plane Window Y Position - Specifies the number of pixels between the start of full plane window Y position and the beginning of the first data of new line.
	 **/
	U32_WRITE_BITFIELD(reg, 10, 0, y);

	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_FG_POS_SYNC, 31, 0, reg);

	return 0;
}

int fb_enable_mask(struct fb_info *info) {
	if (!fb_is_overlay_enabled(info))
		return -1;

	g_mask_enabled = 1;

	for (int p = 0; p < 16; p++) {
		io_write32(g_cpmem_vaddr + (IPU_CHAN_44 * 64) + (p * 4), info->prev_params_ch0[p]);
	}

	ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_44, 1, 29 * 0, 29, MASK_BUFFER / 8);

#ifdef RGB24
    uint8_t *mask = PIXEL24(255, 255, 255);
#elif defined(RGB32)
    uint8_t *mask = PIXEL32(255, 255, 255, 0);
#elif defined(RGB565)
    uint8_t *mask = (uint8_t *) PIXEL565(255, 255, 255);
#endif

	for (unsigned i = 0; i < info->width; i++)
		for (unsigned j = 0; j < info->height; j++)
			_fb_blit(info, i, j, mask, 1, 1, BLIT_MASK);

	/**
	 * IDMAC_CH_EN_44: IPUx_IDMAC_CH_EN_2 [12]
	 * IDMAC Channel enable bit
	 * 0: IDMAC channel is disabled
	 * 1: IDMAC channel is enabled
	 **/
	//ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_IDMAC_CH_EN_2, 12, 12, 1);

	return 0;
}

int fb_disable_mask(struct fb_info *info) {
	g_mask_enabled = 0;

	/**
	 * IDMAC_CH_EN_44: IPUx_IDMAC_CH_EN_2 [12]
	 * IDMAC Channel enable bit
	 * 0: IDMAC channel is disabled
	 * 1: IDMAC channel is enabled
	 **/
	//ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_IDMAC_CH_EN_2, 12, 12, 0);

	return 0;
}

int fb_is_mask_enabled(struct fb_info *info) {
	return g_mask_enabled;
}

int ipu_overlay_direct_map(paddr_t pa) {

	ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 29 * 0, 29, pa / 8);

	return 0;
}

int fb_enable_overlay(struct fb_info *info /* , int width, int height, int x, int y*/) {
	g_overlay_enabled = 1;

	for (int p = 0; p < 16; p++) {
		io_write32(g_cpmem_vaddr + (IPU_CHAN_27 * 64) + (p * 4), info->prev_params_ch0[p]);
	}

	//fb_set_size(info, width, height);

	ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 29 * 0, 29, RENDER_BUFFER / 8);
     
	// IMSG("[FB] g_cm_vaddr: 0x%08lx\n", g_cm_vaddr);

    /**
     *  Bits Per Pixel: IPU_CHAN_27
     *  3'h0: 32 bits per pixel
     *  3'h1: 24 bits per pixel
     *  3'h2: 18 bits per pixel
     *  3'h3: 16 bits per pixel
     *  3'h4: 12 bits per pixel
     *  3'h5: 08 bits per pixel
     *  3'h6: 04 bits per pixel
     */
    // ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 0, 107, 109, 3);

    /**
     *  Offset: IPU_CHAN_27
     *  Number of bits between MSB of pixel and MSB of color component on input. 
     *  Offset0 [132:128]
     *  Offset1 [137:133]
     *  Offset2 [142:138}
     *  Offset3 [147:143]
     */
    // ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 128, 132, 0);
    // ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 133, 137, 5);
    // ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 138, 142, 11);
    // ipu_ch_param_write_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 143, 147, 16);


	/**
	 * DP_EN: IPUx_CONF [5]
	 * Display processor Sub-block Enable bit
	 * 0: DP is disabled
	 * 1: DP is enabled
	 **/
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_CONF, 5, 5, 1);

	/**
	 * IDMAC_CH_EN_27: IPUx_IDMAC_CH_EN_1 [27]
	 * IDMAC Channel enable bit
	 * 0: IDMAC channel is disabled
	 * 1: IDMAC channel is enabled
	 **/
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_IDMAC_CH_EN_1, 27, 27, 1);
    
    /**
	 * IPUx_IC_CMBP_1: IC Combining Parameters Register 1
     * Post-Processing task Global Alpha
	 * 15-8: This field contains the Global Alpha value of Post-Processing
	 **/
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_IC_CMBP_1, 15, 8, 255);

	uint32_t reg = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 31, 0);
   
    /**
	 * DP_GWAM_SYNC: IPUx_DP_COM_CONF_SYNC [2]
	 * Select the use of Alpha to be global or local
	 * 1: Global Alpha (Default)
	 * 0: Local Alpha
	 **/
    U32_WRITE_BITFIELD(reg, 2, 2, 1);

	/**
	 * DP_GWSEL_SYNC: IPUx_DP_COM_CONF_SYNC [1]
	 * Select graphic window to be on partial plane or full plane.
	 * 1: Graphic window is partial plane.
	 * 0: Graphic window is full plane.
	 **/
	U32_WRITE_BITFIELD(reg, 1, 1, 1);

	/**
	 * DP_FG_EN_SYNC: IPUx_DP_COM_CONF_SYNC [0]
	 * partial plane enable. This bit enables the partial plane channel.
	 * 1: partial plane channel is enabled.
	 * 0: partial plane channel is disabled.
	 **/
	U32_WRITE_BITFIELD(reg, 0, 0, 1);

	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 31, 0, reg);

    /**
     *  DP_GWAV_SYNC: IPUxDP_Graph_Wind_CTRL_SYNC [31-24]
     *  Graphic Window ALpha Value
     *  0: Totally opaque
     *  255: Totally transparent
     **/
    ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_Graph_Wind_CTRL_SYNC, 31, 24, 255);


	// fb_set_position(info, x, y);

	return 0;
}

void printIpuRegisters(void){
    uint32_t idmac_ch27 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_IDMAC_CH_EN_1, 27, 27);
    uint32_t idmac_ch31 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_IDMAC_CH_EN_1, 31, 31);
    uint32_t dp_gwav_sync = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_Graph_Wind_CTRL_SYNC, 31, 24);

    uint32_t ipu_chan_pixel_format = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 85, 4);
    uint32_t ipu_chan_alpha = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 89, 1);
    uint32_t ipu_chan_bpp = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 0, 107, 3);
    uint32_t ipu_chan_offset0 = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 128, 5);
    uint32_t ipu_chan_offset1 = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 133, 5);
    uint32_t ipu_chan_offset2 = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 138, 5);
    uint32_t ipu_chan_offset3 = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 143, 5);

    uint32_t dp_conf_13 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 13, 13);
    uint32_t dp_conf_12 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 12, 12);
    uint32_t dp_conf_11 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 11, 11);
    uint32_t dp_conf_2 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 2, 2);
    uint32_t dp_conf_1 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 1, 1);
    uint32_t dp_conf_0 = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 0, 0);

    uint32_t ipu_buffer = ipu_ch_param_read_field(g_cpmem_vaddr, IPU_CHAN_27, 1, 29 * 0, 29);
    uint32_t ipu_dp_enable = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_CONF, 5, 5);

    IMSG("idmac_ch27: %" PRIu32, idmac_ch27);
    IMSG("ipu_dp_enable: %" PRIu32, ipu_dp_enable);
    IMSG("idmac_ch31: %" PRIu32, idmac_ch31);
    IMSG("ipu_chan_pixel_format: 0x%08X", ipu_chan_pixel_format);
    IMSG("ipu_chan_alpha: 0x%08X", ipu_chan_alpha);
    IMSG("ipu_chan_bpp: 0x%08X", ipu_chan_bpp);
    IMSG("ipu_buffer: 0x%08X", ipu_buffer);
    IMSG("dp_gwav_sync: 0x%08X", dp_gwav_sync);

    IMSG("ipu_chan_offset0: 0x%08X", ipu_chan_offset0);
    IMSG("ipu_chan_offset1: 0x%08X", ipu_chan_offset1);
    IMSG("ipu_chan_offset2: 0x%08X", ipu_chan_offset2);
    IMSG("ipu_chan_offset3: 0x%08X", ipu_chan_offset3);
    IMSG("dp_conf_13: %" PRIu32, dp_conf_13);
    IMSG("dp_conf_12: %" PRIu32, dp_conf_12);
    IMSG("dp_conf_11: %" PRIu32, dp_conf_11);
    IMSG("dp_conf_2: %" PRIu32, dp_conf_2);
    IMSG("dp_conf_1: %" PRIu32, dp_conf_1);
    IMSG("dp_conf_0: %" PRIu32, dp_conf_0);

	return;
}

int fb_disable_overlay(struct fb_info *info) {
	g_overlay_enabled = 0;

	if (fb_is_mask_enabled(info))
		fb_disable_mask(info);

	uint32_t reg = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 31, 0);
	/**
	 * DP_GWSEL_SYNC: IPUx_DP_COM_CONF_SYNC [1]
	 * Select graphic window to be on partial plane or full plane.
	 * 1: Graphic window is partial plane.
	 * 0: Graphic window is full plane.
	 **/
	U32_WRITE_BITFIELD(reg, 1, 1, 0);

	/**
	 * DP_FG_EN_SYNC: IPUx_DP_COM_CONF_SYNC [0]
	 * partial plane enable. This bit enables the partial plane channel.
	 * 1: partial plane channel is enabled.
	 * 0: partial plane channel is disabled.
	 **/
	U32_WRITE_BITFIELD(reg, 0, 0, 0);

	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 31, 0, reg);

	/**
	 * IDMAC_CH_EN_27: IPUx_IDMAC_CH_EN_1 [27]
	 * IDMAC Channel enable bit
	 * 0: IDMAC channel is disabled
	 * 1: IDMAC channel is enabled
	 **/
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_IDMAC_CH_EN_1, 27, 27, 0);

	return 0;
}

int fb_is_overlay_enabled(struct fb_info *info) {
	return g_overlay_enabled;
}

int fb_enable_overlay_transparency_inner(struct fb_info *info, uint8_t alpha, uint8_t r, uint8_t g, uint8_t b) {
	/**
	 * DP_GWCKE_SYNC: IPUx_DP_COM_CONF_SYNC [3]
	 * Enable or disable graphic window color keying.
	 * 1: Enable color keying of graphic window
	 * 0: Disable color keying of graphic window
	 **/
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 3, 3, 1);

    uint32_t reg = ipu_cm_read_reg(g_cm_vaddr, 0, IPUx_DP_Graph_Wind_CTRL_SYNC, 31, 0);

	/**
	 * DP_GWAV_SYNC: IPUx_DP_Graph_Wind_CTRL_SYNC [31:24]
	 * GWAV - Graphic Window Alpha Value
	 *
	 * Defines the alpha value of graphic window used for alpha blending between graphic window and full
	 * plane. The Value the number that writing to GWAV register. The Actual Value the number, that insert to
	 * calculation in Combining Module. If Value = < 0.5- 1/256 (01111111) then Actual Value = Value. If Value
	 * >= 0.5 (10000000), then Actual Value = Value + 1/256.
	 *
	 * 00000000  Actual value is 00000000; Graphic window totally opaque i.e. overlay on LCD screen
	 * 01111111  Actual value is 01111111;
	 * 10000000  Actual value is 10000001
	 * 10000001  Actual value is 10000010
	 * 11111110  Actual value is 11111111
	 * 11111111  Actual value is 100000000;Graphic window totally transparent i.e. not displayed on LCD screen
	 **/
	U32_WRITE_BITFIELD(reg, 31, 24, alpha);

	/**
	 * DP_GWCKR_SYNC: IPUx_DP_Graph_Wind_CTRL_SYNC [23:16]
	 * GWCKR - Graphic Window Color Keying Red Component
	 *
	 * Defines the red component of graphic window color keying.
	 *
	 * 00000000  No red
	 * 11111111  Full red
	 **/
	U32_WRITE_BITFIELD(reg, 23, 16, r);

	/**
	 * DP_GWCKG_SYNC: IPUx_DP_Graph_Wind_CTRL_SYNC [15:8]
	 * GWCKG - Graphic Window Color Keying Green Component
	 *
	 * Defines the green component of graphic window color keying.
	 *
	 * 00000000  No Green
	 * 11111111  Full Green
	 **/
	U32_WRITE_BITFIELD(reg, 15, 8, g);

	/**
	 * DP_GWCKB_SYNC: IPUx_DP_Graph_Wind_CTRL_SYNC [7:0]
	 * GWCKB - Graphic Window Color Keying Blue Component
	 *
	 * Defines the blue component of graphic window color keying.
	 *
	 * 00000000  No blue
	 * 11111111  Full blue
	 **/
	U32_WRITE_BITFIELD(reg, 7, 0, b);

	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_Graph_Wind_CTRL_SYNC, 31, 0, reg);

	return 0;
}

int fb_enable_overlay_transparency(struct fb_info *info, uint8_t alpha, uint16_t background_color) {
    uint8_t rkey = color_from_rgb565(background_color, COLOR_RED);
    uint8_t gkey = color_from_rgb565(background_color, COLOR_GREEN);
    uint8_t bkey = color_from_rgb565(background_color, COLOR_BLUE);

    info->background_color = background_color;

	return fb_enable_overlay_transparency_inner(info, alpha, rkey, gkey, bkey);
}

int fb_disable_overlay_transparency(struct fb_info *info) {
	/**
	 * DP_GWCKE_SYNC: IPUx_DP_COM_CONF_SYNC [3]
	 * Enable or disable graphic window color keying.
	 * 1: Enable color keying of graphic window
	 * 0: Disable color keying of graphic window
	 **/
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_COM_CONF_SYNC, 3, 3, 0);

	/* the overlay window is opaque */
	ipu_cm_write_reg(g_cm_vaddr, 0, IPUx_DP_Graph_Wind_CTRL_SYNC, 31, 24, 255);

    info->background_color = 0xffff;

	return 0;
}


void fb_release(struct fb_info *info) {
	if (!info->buffer) {
		return;
	}

	info->buffer = NULL;
}

void fb_clear(struct fb_info *info, uint8_t r, uint8_t g, uint8_t b) {
	for (uint32_t y = 0; y < info->height; y++) {
		uint8_t *line = &info->buffer[info->stride * y];
#ifdef RGB24
        for (uint8_t x = 0; x < info->width; x += 3) { 
            line[x] = (uint8_t) r;
            line[x+1] = (uint8_t) g;
            line[x+2] = (uint8_t) b;
        }
#elif defined(RGB32)
        for (uint8_t x = 0; x < info->width; x += 1) {
            ((uint32_t *)line)[x] = PIXEL32(r, g, b, 0);
        }
#elif defined(RGB565)
        for (uint32_t x = 0; x < info->width; x++) {
            ((uint16_t *)line)[x] = PIXEL565(r, g, b);
        }
#endif
	}
}

void fb_blit(struct fb_info *info, uint32_t x, uint32_t y, const uint8_t *buffer, uint32_t width, uint32_t height) {
	_fb_blit(info, x, y, buffer, width, height, BLIT_BUFFER);
}

void fb_blit_image(struct fb_info *info, uint32_t x, uint32_t y, const struct image *image) {
	fb_blit(info, x, y, image->buffer, image->width, image->height);
}

void fb_blit_all(struct fb_info *info, const struct blit *blits, int num_blits) {
	for (int b = 0; b < num_blits; b++) {
		fb_blit_image(info, blits[b].x, blits[b].y, &blits[b].image);
	}
	// 190818 dhkim: Plot a red dot at (0,0) for testing purpose
	//uint8_t red_dot[] = {255, 0, 0};
	//for (int i = 0; i < 30; i++)
	//	for (int j = 0; j < 30; j++)
	//		fb_blit(info, i, j, red_dot, 1, 1);
}

static TEE_Result imx_fb_init(void)
{
    DMSG("[FB] imx_fb_init()");
	paddr_t paddr = IPU_BASE;
	size_t size = IPU_SIZE;

	if (!core_mmu_add_mapping(MEM_AREA_IO_SEC, paddr, size)) {
		EMSG("[FB] Could not map I/O memory (paddr %lX, size %X)\n", paddr, size);
		return TEE_ERROR_GENERIC;
	}

	// Interested in IPU0 only
	if (!g_base_paddr || (paddr < g_base_paddr)) {
		g_base_paddr = paddr;
		g_base_vaddr = (vaddr_t)phys_to_virt(paddr, MEM_AREA_IO_SEC);
		g_cpmem_vaddr = (vaddr_t)phys_to_virt(paddr + IPU_CPMEM_OFFSET, MEM_AREA_IO_SEC);
		IMSG("[FB] Registered with memory region (0x%lX, 0x%X) before cm\n", paddr, size);

		g_cm_vaddr = (vaddr_t)phys_to_virt(paddr + IPU_CM_OFFSET, MEM_AREA_IO_SEC);
		IMSG("[FB] Registered with memory region (0x%lX, 0x%X) after cm\n", paddr, size);
	}
    uint16_t background_pixel = PIXEL565(51, 76, 51);
    //uint16_t green_pixel = PIXEL565(0, 177, 64);
    vaddr_t buffer = phys_to_virt(RENDER_BUFFER, MEM_AREA_IO_SEC);
    int i;
    for (i=0; i < MAX_RESOLUTION; i++) {
        memcpy((void *) (buffer+(i*BYTES_PER_PIXEL)), &background_pixel, BYTES_PER_PIXEL); 
    }

	return TEE_SUCCESS;
}

int fb_flush(struct fb_info *info)
{
    //uint16_t background_color = PIXEL565(51, 76, 51);
    vaddr_t buffer = phys_to_virt(RENDER_BUFFER, MEM_AREA_IO_SEC);
    int i;
    for (i=0; i < MAX_RESOLUTION; i++) 
        memcpy((void*) (buffer+(i*BYTES_PER_PIXEL)), &(info->background_color), BYTES_PER_PIXEL); 

    return 0;
}


int fb_remove_region(struct fb_info *info, int width, int height, int x_pos, int y_pos)
{
    vaddr_t buffer = phys_to_virt(RENDER_BUFFER, MEM_AREA_IO_SEC);

    int x, y;
    for (y=y_pos; y < height + y_pos; y++) 
        for (x=x_pos; x < width + x_pos; x++)
            memcpy((void *) (buffer + ((x + (y * MAX_WIDTH)) * BYTES_PER_PIXEL)), 
                        &(info->background_color), BYTES_PER_PIXEL); 
    return 0;
}



/*
static const struct serial_driver imx_fb_driver = {
	.dev_init = imx_fb_init,
};
*/
static const struct dt_device_match fb_match_table[] = {
    { .compatible = "fsl, imx6q-ipu"},
    { 0 }
};

const struct dt_driver fb_dt_driver __dt_driver = {
	.name = "ipu-fb",
    .match_table = fb_match_table,
//	.driver = &imx_fb_driver,
};

driver_init(imx_fb_init);

#pragma GCC diagnostic pop
