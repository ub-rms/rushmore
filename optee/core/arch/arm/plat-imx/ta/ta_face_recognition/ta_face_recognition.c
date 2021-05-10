#include "../include/ta.h"

#include <drivers/imx_fb.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "db.h"
#include "input_to_sw.h"

#define RENDER_IMAGE                0x00000001
#define GET_INFO_AND_RENDER_IMAGE   0x00000002



uint16_t background_color = 0x9cd3;

uint16_t prev_left = 0;
uint16_t prev_top = 0;

bool ta_render_image(void* buffer) {

    TEE_Result res = TEE_ERROR_GENERIC;

    struct input_to_sw *input = (struct input_to_sw *) buffer;
    uint8_t *img = get_face_information(input->face_data).face_information;
    tee_remove_all();

    if (strcmp(DB_NULL, img) != 0) {
//        EMSG("input->face_data is not NONE");

//        if (prev_left == input->left && prev_top == input->top)
//            return true;

        size_t filelen = 10908;
        prev_left = input->left;
        prev_top = input->top;

        EncryptedImage (*ei)[1] = malloc(sizeof(EncryptedImage) * 1);
        (*ei)[0].decrypted = true;
        (*ei)[0].buffer = (void*) img;


        (*ei)[0].width = 202;
        (*ei)[0].height = 27;
        (*ei)[0].x_pos = input->top+40;
        (*ei)[0].y_pos = input->left+80;
        
//        EMSG(" - width: %d, height: %d, x_pos: %d, y_pos: %d, buffer: %p", 
//                (*ei)[0].width, (*ei)[0].height, (*ei)[0].x_pos, (*ei)[0].y_pos, (*ei)[0].buffer);
        
//        tee_remove_all();
        res = tee_render_images(ei, 1);
    }
    


    return true;
}


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

    if (ta_render_image(buffer))
        res = TEE_SUCCESS;


    return res;
}


TEE_Result ta_remove(EncryptedImage (*ei)[], int num_images) {
    
    TEE_Result res = TEE_ERROR_GENERIC;

    /* Remove entire screen */
    if (num_images == 0) 
        res = tee_remove_all();

    return res;
}


TEE_Result ta_render(EncryptedImage (*ei)[], int num_images) {

    return TEE_ERROR_GENERIC;
}






