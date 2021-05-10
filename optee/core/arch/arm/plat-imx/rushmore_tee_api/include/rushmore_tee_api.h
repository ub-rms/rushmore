#ifndef TEE_API_H
#define TEE_API_H

#include <stdint.h>
#include <stdbool.h>
#include <mm/core_mmu.h>

#include "rilib.h"

/* 110220 dhkim:
 * These numbers MUST match with img2rei.py
 */
enum crypto_mode {
	SOFTWARE_AES = 0,
    CAAM_AES = 1,
	CHACHA20 = 2,
	/* XOSHIROSS = 3, */
	VCRYPTO_BW_NOISY = 4,
	NONE = 5,
};


enum crypto_flags {
	FL_LAST           = 0x00000001,
	FL_NEON           = 0x00000002,
	FL_VCRYPTO_BW     = 0x00000004,
	FL_VCRYPTO_CACHE  = 0x00000008,
};


typedef struct {
	enum crypto_mode crypto_mode;
	uint8_t iv[16] __attribute__ ((aligned (32)));
	int iv_len;
    int width, height;
    int x_pos, y_pos;
    bool decrypted;
    void* buffer;
} EncryptedImage;


TEE_Result tee_init(bool single_img_render, bool enable_color_keying, uint16_t background_color);
TEE_Result tee_release();



/* Decrypt EncryptedImages */
TEE_Result tee_decrypt(uint8_t *in, uint8_t *out, size_t size, enum crypto_mode mode, 
           uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size); 
TEE_Result tee_decrypt_images(EncryptedImage (*ei)[], int num_images,
           uint8_t key[], size_t key_size);

/* Render already decrypted images on overlay buffer */
TEE_Result tee_render_images(EncryptedImage (*ei)[], int num_images);

/* Decrypt and directly render on overlay buffer */
TEE_Result tee_decrypt_and_render_images(EncryptedImage (*ei)[], int num_images,
           uint8_t key[], size_t key_size);


/* Remove images on overlay buffer */
TEE_Result tee_remove_all();
TEE_Result tee_remove_images(EncryptedImage (*ei)[], int num_images);


/* TODO: Later, will be hidden from developer's view */
TEE_Result decrypt_region(enum crypto_mode mode, region_t *in, region_t *out,
        uint8_t key[], size_t key_size, uint8_t iv[], size_t iv_size, enum crypto_flags flags);

#endif
