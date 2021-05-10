#include "include/rushmore_tee_api.h"
#include <crypto/crypto.h>
#include <drivers/imx_fb.h>
#include <string.h>

#include "chacha20.h"
#include "vcrypto.h"

#define MAX_WIDTH   1280
#define MAX_HEIGHT  800


static int overlay_xres = 0;
static int overlay_yres = 0;
static int overlay_xpos = 0;
static int overlay_ypos = 0;


static void *ctx = NULL;

static vcrypto_cache_t *vcache = NULL;

struct fb_info fb;
uint16_t bg_color;
bool partial_overlay;


static TEE_Result init_overlay_buffer(bool enable_color_keying) {

    /* Acquire imx_fb */
    fb_acquire(&fb);

    /* Enable overlay buffer */
    fb_enable_overlay(&fb);

    /* Enable color keying */
    if (enable_color_keying)
        fb_enable_overlay_transparency(&fb, 255 /* Alpha */, bg_color);
    else
        fb_enable_overlay_transparency(&fb, 255 /* Alpha */, 0xffff /* WHITE */);

    fb_flush(&fb);

    /* If FULL, set overlay's width, height, xpos, and ypos */
    if (!partial_overlay) {
        overlay_xres = MAX_WIDTH;
        overlay_yres = MAX_HEIGHT;
        overlay_xpos = 0;
        overlay_ypos = 0;

        fb_set_size(&fb, overlay_xres, overlay_yres);
        fb_set_position(&fb, overlay_xpos, overlay_ypos);
    }

    return TEE_SUCCESS;

}


static TEE_Result release_overlay_buffer() {

    /* Disable overlay buffer */
    fb_disable_overlay(&fb);

    /* Disable color keying */
    fb_disable_overlay_transparency(&fb);

    /* Flush the whole overlay buffer */
    fb_flush(&fb);

    /* Release */
    fb_release(&fb);

    return TEE_SUCCESS;

}


TEE_Result aes_decrypt(uint8_t *in, uint8_t *out, size_t size,
        uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size, bool last) {

    TEE_Result res;
    TEE_OperationMode mode = TEE_MODE_DECRYPT;
    bool row_by_row = false;


    if (ctx == NULL) {

        res = crypto_cipher_alloc_ctx(&ctx, TEE_ALG_AES_CBC_NOPAD);
        if (res != TEE_SUCCESS) return res;


        res = crypto_cipher_init(ctx, mode,
                                    key, key_size, NULL, 0, iv, iv_size);
        if (res != TEE_SUCCESS) {
            crypto_cipher_free_ctx(ctx);
            return res;
        }
    }


    if (row_by_row){

        int row;
        int row_size = overlay_xres * BYTES_PER_PIXEL;

        for (row = 0; row < overlay_yres; row++) {

            if (row != overlay_yres-1) {
                res = crypto_cipher_update(ctx, mode, false,
                            in + (row * row_size), row_size, out + (row * row_size));
            }

            else {
                res = crypto_cipher_update(ctx, mode, true,
                            in + (row * row_size), row_size, out + (row * row_size));
            }
        }
    }

    else
        res = crypto_cipher_update(ctx, mode, true, in, size, out);



    if (last) crypto_cipher_final(ctx);


    return res;
}

TEE_Result decrypt(enum crypto_mode mode, uint8_t *in, uint8_t *out, size_t size,
        uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size, enum crypto_flags flags) {


    TEE_Result res = TEE_ERROR_GENERIC;

	uint32_t vfp_status = thread_kernel_enable_vfp();

	/* ARM Neon intrinsics are enabled here */

    switch (mode) {
        case SOFTWARE_AES:
        case CAAM_AES:
            res = aes_decrypt(in, out, size, key, key_size, iv, iv_size, FL_LAST & flags);
            break;

		case CHACHA20:
			if (key_size < 32) {
				EMSG("ChaCha needs a key longer than 32 byte.");
				res = TEE_ERROR_BAD_PARAMETERS;
				break;
			}
			if (iv_size < 12) {
				EMSG("ChaCha needs a nonce longer than 12 byte.");
				res = TEE_ERROR_BAD_PARAMETERS;
				break;
			}
			ChaCha20XOR(out, /* out */
						in, /* in */
						size, /* inLen */
						key, /* key (32-byte) */
						iv, /* nonce (12-byte) */
						0); /* counter */
			res = TEE_SUCCESS;
			break;

        /* TODO: Add other decryption methods here */


        default:
            EMSG("Invalid Crypto type provided.");

    }

	thread_kernel_disable_vfp(vfp_status);

    return res;
}

TEE_Result decrypt_region_chacha20(region_t *in, region_t *out,
        uint8_t _key[], size_t key_size, uint8_t _iv[], size_t iv_size, enum crypto_flags flags) {
	uint8_t key[32] __attribute__ ((aligned (32)));
	uint8_t iv[12] __attribute__ ((aligned (32)));

	memcpy(key, _key, 32);
	memcpy(iv, _iv, 12);

	/* ChaCha's counter skips blocks which is 64-byte long */
#define CHACHA_BLKSZ (64)
	uint8_t zeros[CHACHA_BLKSZ] __attribute__((aligned (32))) = {0};
	uint8_t block[CHACHA_BLKSZ] __attribute__((aligned (32))) = {0};
	int blk_pos = CHACHA_BLKSZ;
	int y, n_blocks, counter = 0, width_in_bytes;

	if (in->width != out->width || in->height != out->height)
		return TEE_ERROR_GENERIC;

	if (key_size < 32) {
		EMSG("ChaCha needs a key longer than 32 byte.");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (iv_size < 12) {
		EMSG("ChaCha needs a nonce longer than 12 byte.");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	width_in_bytes = in->width * in->canvas->pixel_size;

	for (y = 0; y < in->height; y++) {
		/* start of a new row */
		int x = 0;
		uint8_t *in_row = (uint8_t *)RILIB_GET_PIXEL(in, 0, y);
		uint8_t *out_row = (uint8_t *)RILIB_GET_PIXEL(out, 0, y);

		/* drain unused key stream in 'block' buffer */
		for (; blk_pos < CHACHA_BLKSZ; blk_pos++, x++) {
			out_row[x] = in_row[x] ^ block[blk_pos];
		}

		/* do chacha */
		if (n_blocks = (width_in_bytes - x) / CHACHA_BLKSZ) {
			ChaCha20XOR(&out_row[x], /* out */
						&in_row[x], /* in */
						CHACHA_BLKSZ * n_blocks, /* inLen */
						key, /* key (32-byte) */
						iv, /* nonce (12-byte) */
						counter); /* counter */

			x += CHACHA_BLKSZ * n_blocks;
			counter += n_blocks;
		}

		/* create a new key block for remainder */
		if (x < width_in_bytes) {
			assert((width_in_bytes - x) < CHACHA_BLKSZ);

			ChaCha20XOR(block, /* out */
						zeros, /* in */
						CHACHA_BLKSZ, /* inLen */
						key, /* key (32-byte) */
						iv, /* nonce (12-byte) */
						counter); /* counter */

			counter++;
			blk_pos = 0;

			for (; x < width_in_bytes; x++, blk_pos++) {
				out_row[x] = in_row[x] ^ block[blk_pos];
			}
		}
	}
    
	assert(counter == (width_in_bytes * in->height + (CHACHA_BLKSZ-1)) / CHACHA_BLKSZ);

	return TEE_SUCCESS;
}

TEE_Result decrypt_region(enum crypto_mode mode, region_t *in, region_t *out,
        uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size, enum crypto_flags flags) {
	TEE_Result ret = TEE_ERROR_GENERIC;

	uint32_t vfp_status = thread_kernel_enable_vfp();

	/* ARM Neon intrinsics are enabled here */

	switch (mode) {
	case CHACHA20:
		ret = decrypt_region_chacha20(in, out, key, key_size, iv, iv_size, flags);
		break;
	default:
		ret = TEE_ERROR_GENERIC;
		break;
	}

	thread_kernel_disable_vfp(vfp_status);

	return ret;
}

uint8_t *__rand16_by_ChaCha_key = NULL;
uint8_t *__rand16_by_ChaCha_nonce = NULL;
uint32_t __rand16_by_ChaCha_counter = 0;

uint32_t rand16_by_ChaCha_init(uint8_t *key, uint8_t *nonce) {
	__rand16_by_ChaCha_key = key;
	__rand16_by_ChaCha_nonce = nonce;
	__rand16_by_ChaCha_counter = 0;
}

uint16_t rand16_by_ChaCha() {
	static uint16_t buf[32] __attribute__ ((aligned (16))) = {0}; /* 64 byte buffer */
	uint16_t const zeros[32] __attribute__ ((aligned (16))) = {0}; /* 64 byte buffer */

	if (!(__rand16_by_ChaCha_counter % 32)) {
		ChaCha20XOR(buf, /* out */
				zeros, /* in */
				64, /* inLen */
				__rand16_by_ChaCha_key, /* key (32-byte) */
				__rand16_by_ChaCha_nonce, /* nonce (12-byte) */
				__rand16_by_ChaCha_counter/32); /* counter */
	}

	return buf[__rand16_by_ChaCha_counter++%32];
}

void vcrypto_fill_key(region_t *region,
		uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size, enum crypto_flags flags) {
	uint32_t vfp_status;

	bool cache = flags & FL_VCRYPTO_CACHE;
	bool neon = flags & FL_NEON;

	vfp_status = thread_kernel_enable_vfp();

	/* ARM Neon intrinsics are enabled here */

	if (cache && !vcache) {
		vcache = (vcrypto_cache_t *)phys_to_virt(CACHE_BUFFER, MEM_AREA_IO_SEC);

//		DMSG("[SeCloak] Initialize vcrypto cache at %p (0x%x bytes)", (void *)vcache, sizeof(vcrypto_cache_t));
		vcrypto_init_cache(vcache);
	}

	rand16_by_ChaCha_init(key, iv);

	if (!cache) /* default */
		vcrypto_gen_key(region, rand16_by_ChaCha, 1 /* scale is deprecated */);
	else if (!neon) /* use cache */
		vcrypto_gen_key_w_cache(region, rand16_by_ChaCha, vcache, 0);
	else { /* use cache and accelerate with SIMD instructions. */
		vcrypto_gen_key_w_cache(region, rand16_by_ChaCha, vcache,
				VCRYPTO_GEN_KEY_W_CACHE_FL_SIMD);
	}

	//		DMSG("[SeCloak] Key gen %s\n", ret ? "failure" : "success");
	//		DMSG("[SeCloak] vcache: %p, status-ok: 0x%x\n", (void *)vcache, vcache ? vcache->ok : 0);
	thread_kernel_disable_vfp(vfp_status);
}

/* ======================================================================= */


TEE_Result tee_init(bool single_img_render, bool enable_color_keying, uint16_t background_color) {

    TEE_Result res = TEE_ERROR_GENERIC;

    /* Save settings */
    partial_overlay = single_img_render;
    bg_color = background_color;

    /* Init overlay buffer */
    res = init_overlay_buffer(enable_color_keying);

    return res;
}


TEE_Result tee_release() {

    TEE_Result res = TEE_ERROR_GENERIC;

    /* Release overlay buffer */
    res = release_overlay_buffer();
    
    overlay_xres = 0;
    overlay_yres = 0;
    overlay_xpos = 0;
    overlay_ypos = 0;

    partial_overlay=false;

    return res;
}


TEE_Result tee_decrypt(uint8_t *in, uint8_t *out, size_t size, enum crypto_mode mode, 
        uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size) {

    decrypt(mode, in, out, size, key, key_size, iv, iv_size, FL_LAST);
}


TEE_Result tee_decrypt_and_render_images(EncryptedImage (*ei)[], int num_images,
        uint8_t key[], size_t key_size) {

    TEE_Result res = TEE_SUCCESS;

    /* Single image with (AES mode) - uses partial overlay */
	enum crypto_mode mode0 = (*ei)[0].crypto_mode;

    if (num_images == 1 && partial_overlay && (mode0 == SOFTWARE_AES || mode0 == CAAM_AES)) {
		
        int xRes = (*ei)[0].width;
		int yRes = (*ei)[0].height;
		int x_pos = (*ei)[0].x_pos;
		int y_pos = (*ei)[0].y_pos;
		void *image_addr = (*ei)[0].buffer;
		uint8_t *iv = (*ei)[0].iv;
		size_t iv_size = (*ei)[0].iv_len;


		/* Update changed width and height (overlay buffer) */
		if (overlay_xres != xRes || overlay_yres != yRes) {
			overlay_xres = xRes;
			overlay_yres = yRes;
			fb_set_size(&fb, overlay_xres, overlay_yres);
		}

		/* Update changed x, y positions (overlay buffer) */
		if (overlay_xpos != x_pos || overlay_ypos != y_pos) {
			overlay_xpos = x_pos;
			overlay_ypos = y_pos;
			fb_set_position(&fb, overlay_xpos, overlay_ypos);
		}

		decrypt(mode0, (uint8_t *) image_addr, (uint8_t *) fb.buffer,
				overlay_xres * overlay_yres * BYTES_PER_PIXEL,
				key, key_size, iv, iv_size, FL_LAST);
    }


    /* Multiple image rendering (or single image width no AES mode) */
    else {

        int i;
        for (i=0; i<num_images; i++) {
            int xRes = (*ei)[i].width;
            int yRes = (*ei)[i].height;
            int x_pos = (*ei)[i].x_pos;
            int y_pos = (*ei)[i].y_pos;
            void *image_addr = (*ei)[i].buffer;
			enum crypto_mode mode = (*ei)[i].crypto_mode;
			uint8_t *iv = (*ei)[i].iv;
			size_t iv_size = (*ei)[i].iv_len;

            bool last = false;                  /* TODO not used, remove later */

            int row;
            int row_size = xRes * BYTES_PER_PIXEL;

            switch (mode) {
				case SOFTWARE_AES: {

					for (row = 0; row < yRes; row++) {
                        if (last)
							decrypt(SOFTWARE_AES, image_addr + (row * row_size),
                                    fb.buffer+(x_pos*BYTES_PER_PIXEL)
                                    + ((row+y_pos) * MAX_WIDTH * BYTES_PER_PIXEL),
                                    row_size, key, key_size, iv, iv_size, FL_LAST);

                        else
                            decrypt(SOFTWARE_AES, image_addr + (row * row_size),
                                    fb.buffer +( x_pos*BYTES_PER_PIXEL)
                                    + ((row+y_pos) * MAX_WIDTH * BYTES_PER_PIXEL),
                                    row_size, key, key_size, iv, iv_size, FL_NEON);
                    }

				} break;

                case CAAM_AES: {
                    void *decrypted = phys_to_virt(MASK_BUFFER, MEM_AREA_IO_SEC);

                    decrypt(CAAM_AES, (uint8_t *) image_addr, (uint8_t *) decrypted,
					        xRes * yRes * BYTES_PER_PIXEL,
						    key, key_size, iv, iv_size, FL_LAST);

					for (row = 0; row < yRes; row++)
				        memcpy(fb.buffer +(x_pos*BYTES_PER_PIXEL) \
                                + ((row+y_pos) * MAX_WIDTH * BYTES_PER_PIXEL),
                                decrypted + (row * row_size), row_size);
				} break;

                case CHACHA20: {
					canvas_t in_canvas, out_canvas;
					region_t in_region, out_region;

					rilib_init_canvas(&in_canvas, xRes, yRes,
                                        IMG_FMT_RGB565, image_addr);
					rilib_init_canvas(&out_canvas, MAX_WIDTH, MAX_HEIGHT,
                                        IMG_FMT_RGB565, fb.buffer);

					rilib_init_region(&in_canvas, &in_region, 0, 0, xRes, yRes);
					rilib_init_region(&out_canvas, &out_region, x_pos, y_pos,
                                        xRes, yRes);

					decrypt_region(mode, &in_region, &out_region,
                                        key, key_size, iv, iv_size, FL_NEON);
				} break;

				case VCRYPTO_BW_NOISY: {
                    
                    fb_enable_overlay_transparency(&fb, 255 /* Alpha */, 0xffff /* WHITE */);
                    fb_enable_overlay_transparency_inner(&fb, 255 /* Alpha */, 255, 255, 255);
                    fb_flush(&fb);

					canvas_t out_canvas;
					region_t out_region;

					rilib_init_canvas(&out_canvas, MAX_WIDTH, MAX_HEIGHT,
                                        IMG_FMT_RGB565, fb.buffer);

					rilib_init_region(&out_canvas, &out_region, x_pos, y_pos,
                                        xRes, yRes);

					// vcrypto_fill_key(&out_region, key, sizeof(key), iv, sizeof(iv), 0);
					vcrypto_fill_key(&out_region, key, key_size, iv, iv_size, FL_NEON | FL_VCRYPTO_CACHE);
				} break;
			}

        }

    }

    return res;
}


TEE_Result tee_render_images(EncryptedImage (*ei)[], int num_images) {

    TEE_Result res = TEE_SUCCESS;

    /* For manual rendering, always enable full overaly buffer */
    if (partial_overlay) {
        overlay_xres = MAX_WIDTH;
        overlay_yres = MAX_HEIGHT;
        overlay_xpos = 0;
        overlay_ypos = 0;

        fb_set_size(&fb, overlay_xres, overlay_yres);
        fb_set_position(&fb, overlay_xpos, overlay_ypos);

        partial_overlay = false;
    }

    int i;
    for (i=0; i<num_images; i++) {

        int xRes = (*ei)[i].width;
        int yRes = (*ei)[i].height;
        int x_pos = (*ei)[i].x_pos;
        int y_pos = (*ei)[i].y_pos;

        void *image_addr = (*ei)[i].buffer;
        bool decrypted = (*ei)[i].decrypted;

        if (!decrypted) {
            EMSG("[!] Image is not decrypted.");
            return TEE_ERROR_GENERIC;
        }


        int row;
        int row_size = xRes * BYTES_PER_PIXEL;

	    for (row = 0; row < yRes; row++)
		    memcpy(fb.buffer + (x_pos*BYTES_PER_PIXEL) \
                                + ((row+y_pos) * MAX_WIDTH * BYTES_PER_PIXEL),
                                image_addr + (row * row_size), row_size);

    }

    return res;
}


TEE_Result tee_remove_all() {
    fb_flush(&fb);
    return TEE_SUCCESS;
}


TEE_Result tee_remove_images(EncryptedImage (*ei)[], int num_images) {

    if (partial_overlay)
        fb_flush(&fb);

    int i;
    for (i=0; i<num_images; i++) {

        int width = (*ei)[i].width;
        int height = (*ei)[i].height;
        int x_pos = (*ei)[i].x_pos;
        int y_pos = (*ei)[i].y_pos;
        fb_remove_region(&fb, width, height, x_pos, y_pos);

    }

    return TEE_SUCCESS;
}


TEE_Result tee_decrypt_images(EncryptedImage (*ei)[], int num_images,
        uint8_t key[], size_t key_size) {

    TEE_Result res = TEE_ERROR_GENERIC;

    int i;
    for (i=0; i<num_images; i++) {
		enum crypto_mode mode = (*ei)[i].crypto_mode;

		uint8_t *iv = (*ei)[i].iv;
		size_t iv_size = (*ei)[i].iv_len;

        decrypt(mode, (*ei)[i].buffer, fb.mask,
                (*ei)[i].height * (*ei)[i].width * BYTES_PER_PIXEL,
                key, key_size, iv, iv_size, FL_LAST);

        /* Replace encrypted buffer to decrypted buffer */
        (*ei)[i].buffer = fb.mask;
        (*ei)[i].decrypted = true;
    }

    return TEE_SUCCESS;

}

