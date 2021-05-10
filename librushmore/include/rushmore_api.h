#ifndef RUSHMORE_API_H
#define RUSHMORE_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "rei.h"

#define TEEC_RESULT_ERROR       0            
#define TEEC_RESULT_SUCCESS     1


typedef struct {
    int x_pos, y_pos;
    rei_fh_t *buffer;
	size_t buflen;
} EncryptedImage;


/* Holding mutliple images (pass 1 frame to SW at a time) */
typedef struct {
    EncryptedImage (*images)[];
    int num_images;
	bool last;
} Frame;


typedef struct {
    /* enum crypto_mode crypto_mode; */
    uint8_t ta_id;
    bool single_img_render;     /* TRUE: Overlay window == Image                    */
                                /*       new image replaces previous rendered image */
                                /* FALSE: Overlay window == whole screen            */

    bool _init;                 /* TRUE if initialized */
    int _smc_fd;                /* SMC driver file descriptor */

    uint8_t* _buffer;
    int _frames_in_buffer;
    int _next_offset, _end_offset;

} RushmoreClient;


int teec_init(RushmoreClient *rc);
int teec_release(RushmoreClient *rc);

/* General Invoking function */
int teec_invoke(RushmoreClient *rc, void *buffer, size_t buffer_size);

/* Render images on the overlay buffer */
int teec_render_images(RushmoreClient *rc, EncryptedImage (*ei)[], int num_images);

/* Render animation */
int teec_render_animation(RushmoreClient *rc, Frame *ei, int num_frames, bool fixed_fps);
int teec_buffer_animation(RushmoreClient *rc, 
                    Frame (*func_allocate)(void *addr, size_t available_size), 
                    size_t frame_size, bool fixed_fps);

/* Remove rendered images */
int teec_remove(RushmoreClient *rc);

/* 121520 dhkim: testing API forcing decryption in the normal world.
 * This MUST be called prior to any other teec_ calls or never. */
void __teec_force_nw(bool force);


#endif
