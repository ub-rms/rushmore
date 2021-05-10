#include <mm/core_mmu.h>
#include <drivers/imx_fb.h>
#include <crypto/crypto.h>
#include <string.h>
#include <rushmore_entry.h>

#include <ta/include/ta.h>


static TEE_Result handle_init(struct tee_arg *arg) {
    
    TEE_Result res = TEE_ERROR_GENERIC;
    res = ta_init(arg->single_img_render);

    return res;
}


static TEE_Result handle_release() {

    TEE_Result res = TEE_ERROR_GENERIC;
    res = ta_release();

    return res;
}


static TEE_Result handle_render(struct tee_arg *arg, uint32_t buffer_base) {
    
    TEE_Result res = TEE_ERROR_GENERIC;

    /* If render request doesn't contain images, return error*/
    int num_images = (*arg).num_tee_imgs;
    if (num_images == 0) return res;

    /* Converting tee_img to EncrypteImage (pass to TA) */
    struct tee_img (*imgs)[num_images] = phys_to_virt((paddr_t) (*arg).imgs, MEM_AREA_RAM_NSEC);
    EncryptedImage (*ei)[num_images] = malloc(sizeof(EncryptedImage) * num_images);
    
    int i;
    for (i=0; i<num_images; i++) {
		(*ei)[i].crypto_mode = (*imgs)[i].crypto_mode;
		memcpy((*ei)[i].iv, (*imgs)[i].iv, (*imgs)[i].iv_len);
		(*ei)[i].iv_len = (*imgs)[i].iv_len;
		
		(*ei)[i].width = (*imgs)[i].width;
		(*ei)[i].height = (*imgs)[i].height;
		(*ei)[i].x_pos = (*imgs)[i].x_pos;
		(*ei)[i].y_pos = (*imgs)[i].y_pos;
		(*ei)[i].decrypted = false;
		(*ei)[i].buffer = 
			phys_to_virt((paddr_t) buffer_base + (*imgs)[i].offset, MEM_AREA_RAM_NSEC);

    }

    res = ta_render(ei, num_images);
    free(ei);

    return res;
}


static TEE_Result handle_remove(struct tee_arg *arg, uint32_t buffer_base) {
    
    TEE_Result res = TEE_ERROR_GENERIC;
    res = ta_remove();

    return res;
}


static TEE_Result handle_invoke(struct tee_arg *arg, uint32_t buffer_base) {
    
    TEE_Result res = TEE_ERROR_GENERIC;

    /* Passes buffer address and size */
    void* buffer = phys_to_virt((paddr_t) (*arg).buffer, MEM_AREA_RAM_NSEC);
    size_t buffer_size = (*arg).buffer_size;

    res = ta_invoke(buffer, buffer_size);

    return res;
}


/* ENTRY POINT - Redirect the request to a specific function in TA */
TEE_Result rushmore_entry(uint32_t buffer_base, uint32_t tee_arg_addr) {

    TEE_Result res = TEE_ERROR_GENERIC;
    
    /* Get tee_arg and func_id */
    struct tee_arg *arg = phys_to_virt((paddr_t) tee_arg_addr, MEM_AREA_RAM_NSEC);
    int func_id = (*arg).func_id;
    
    /* Handle requested the function */
    switch(func_id) {
        case FUNC_ID_INIT:
            res = handle_init(arg);
            break;

        case FUNC_ID_RELEASE:
            res = handle_release();
            break;

        case FUNC_ID_RENDER:
            res = handle_render(arg, buffer_base);
            break;
 
        case FUNC_ID_REMOVE:
            res = handle_remove(arg, buffer_base);
            break;
       
        case FUNC_ID_INVOKE:
            res = handle_invoke(arg, buffer_base);
            break;


        default:
            break;

    }

    return res;
}


