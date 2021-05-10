#include "../include/ta.h"
#include <drivers/imx_fb.h>
#include <string.h>


uint8_t key[] __attribute__ ((aligned (32))) = 
    {   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F };


uint16_t background_color = PIXEL565(51, 76, 51);


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

    return TEE_ERROR_GENERIC;
}


TEE_Result ta_remove() {
    
    TEE_Result res = TEE_ERROR_GENERIC;

    /* Remove entire screen */
    res = tee_remove_all();

    return res;
}



TEE_Result ta_render(EncryptedImage (*ei)[], int num_images) {

    TEE_Result res = TEE_ERROR_GENERIC;
    
    /* Decrypt directly on overlay buffer */
    res = tee_decrypt_and_render_images(ei, num_images, key, sizeof(key));

    return res;
}



