#include "../include/ta.h"
#include "include/keypad_images.h"

#include <drivers/imx_fb.h>
#include <string.h>

struct meta_data {
    uint8_t cipherText[16];
    uint16_t x_pos[10];
    uint16_t y_pos[10];
    uint16_t widths[10];
    uint16_t heights[10];
};

uint16_t background_color = 0x9cd3;



TEE_Result ta_init(bool single_img_render) {

    TEE_Result res = TEE_ERROR_GENERIC;

    /* App developer can add whatever they want to initialize here. */
    res = tee_init(single_img_render, true, background_color);
    
    return res;
}


TEE_Result ta_release() {

    TEE_Result res = TEE_ERROR_GENERIC;
    res = tee_release();

    return res;
}


TEE_Result ta_invoke(void* buffer, size_t buffer_size) {
    
    TEE_Result res = TEE_ERROR_GENERIC;
    /* TODO: Developers can implement whatever they want to */
    uint8_t key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };

    uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

    uint8_t out[] = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a };


    struct meta_data *md = (struct meta_data*) buffer;
    for (int i=0; i<10; i++)
        EMSG(" - before md->cipher_text[%d]: %d", i, md->cipherText[i]);

    enum crypto_mode mode = SOFTWARE_AES;
    res = tee_decrypt((uint8_t *) md->cipherText, (uint8_t *) out, 
                        sizeof(md->cipherText), mode, key, sizeof(key), iv, sizeof(iv));
    
    for (int i=0; i<10; i++)
        EMSG(" - after out[%d]: %d", i, out[i]);
    
    EncryptedImage (*ei)[10] = malloc(sizeof(EncryptedImage) * 10);
    for (int i=0; i<10; i++) {
        (*ei)[i].decrypted = true;
        switch (out[i]) {
            case 0:
                (*ei)[i].buffer = (void*) keypad_0;
                break;

            case 1:
                (*ei)[i].buffer = (void*) keypad_1;
                break;

            case 2:
                (*ei)[i].buffer = (void*) keypad_2;
                break;

            case 3:
                (*ei)[i].buffer = (void*) keypad_3;
                break;

            case 4:
                (*ei)[i].buffer = (void*) keypad_4;
                break;

            case 5:
                (*ei)[i].buffer = (void*) keypad_5;
                break;

            case 6:
                (*ei)[i].buffer = (void*) keypad_6;
                break;

            case 7:
                (*ei)[i].buffer = (void*) keypad_7;
                break;

            case 8:
                (*ei)[i].buffer = (void*) keypad_8;
                break;

            case 9:
                (*ei)[i].buffer = (void*) keypad_9;
                break;

            default:
                EMSG("[!] Incorrect keypad number is given: %d", out[i]);

        }

        (*ei)[i].width = md->widths[i];
        (*ei)[i].height = md->heights[i];
        (*ei)[i].x_pos = md->x_pos[i];
        (*ei)[i].y_pos = md->y_pos[i];
        
        EMSG(" - [%d]: num: %d, width: %d, height: %d, x_pos: %d, y_pos: %d, buffer: %p", 
                i, out[i], (*ei)[i].width, (*ei)[i].height, (*ei)[i].x_pos, (*ei)[i].y_pos, (*ei)[i].buffer);
    }
    
    //EMSG("TEE_RAM_START: 0x%x", TEE_RAM_START);
    //EMSG("TEE_RAM_VA_SIZE: 0x%x", TEE_RAM_VA_SIZE);
    res = tee_render_images(ei, 10);
    //EMSG("DONE");

    return res;
}


TEE_Result ta_remove(EncryptedImage (*ei)[], int num_images) {
    
    TEE_Result res = TEE_ERROR_GENERIC;

    /* Remove entire screen */
    if (num_images == 0) 
        res = tee_remove_all();

    /* Remove only the region of images rendered */
    else 
        res = tee_remove_images(ei, num_images);

    return res;
}


TEE_Result ta_render(EncryptedImage (*ei)[], int num_images) {

    return TEE_ERROR_GENERIC;
}

