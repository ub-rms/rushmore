#ifndef __RUSHMORE_ENTRY_H
#define __RUSHMORE_ENTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <tee_api_types.h>
#include <rushmore_tee_api/include/rushmore_tee_api.h>

#define FUNC_ID_INIT        1
#define FUNC_ID_RELEASE     2
#define FUNC_ID_RENDER      3
#define FUNC_ID_REMOVE      4
#define FUNC_ID_INVOKE      5

/* 110220 dhkim:
 * The same with tee_arg below, tee_img should NOT be duplicated as it is now
 */
struct tee_img {
	int crypto_mode;
	uint8_t iv[16];
	uint8_t iv_len;
    int width, height;
    int x_pos, y_pos;
    int offset;
	size_t len;
} __attribute__((packed));

/* 110220 dhkim: 
 * this structure, tee_arg, MUST be the same with the structure with the same name
 * at /librushmore/src/rushmore_api.c.
 * __attrubute__((packed)) is for removing paddings added by compilers.
 * FYI, different compiler may have different padding rules. Similar to removing
 * paddings when we make network transactions, paddings in this sturcture must
 * be removed too.
 * TODO: Make /librushmore to find struct tee_arg from this file. Let's not have
 * duplicated structure definitions if we can.
 */
struct tee_arg {
    struct tee_img (*imgs)[];
    int num_tee_imgs;
    int func_id;
    //enum crypto_mode crypto_mode;
    bool single_img_render;

    void* buffer;
    size_t buffer_size;
} __attribute__((packed));


TEE_Result rushmore_entry(uint32_t buffer_base, uint32_t tee_arg_addr);

#endif

