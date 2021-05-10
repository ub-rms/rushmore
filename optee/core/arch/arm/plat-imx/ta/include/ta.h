#ifndef TA_SQUARE_ANIM_H
#define TA_SQUARE_ANIM_H


#include <rushmore_tee_api/include/rushmore_tee_api.h>

#define TEE_FUNCTION_RENDER 0


TEE_Result ta_init(bool single_img_render);
TEE_Result ta_release();

TEE_Result ta_render(EncryptedImage (*ei)[], int num_images);
TEE_Result ta_remove();

TEE_Result ta_invoke(void* buffer, size_t buffer_size);

#endif
