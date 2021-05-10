#include "rushmore_api.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <fcntl.h>
#include <assert.h>
#include <chacha20.h>

#define MAX_RESOLUTION          (1280 * 800)
#define BYTES_PER_PIXEL          2
#define KB                      1024
#define MB                      (1024 * KB)
#define RUSHMORE_BUFFER_SIZE    (1 * MB)

#define DELAY_BETWEEN_FRAMES    32          /* in milliseconds (FPS: 30) */

#define FUNC_ID_INIT        1
#define FUNC_ID_RELEASE     2
#define FUNC_ID_RENDER      3
#define FUNC_ID_REMOVE      4
#define FUNC_ID_INVOKE      5

#define TIME_IMG_BUFFER     false

// only for evaluation
bool DIRECT_RENDER = false;
uint8_t key[] __attribute__ ((aligned (32))) =
	{   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F };

extern int write_to_fb(int x_pos, int y_pos, int width, int height, void *buffer, size_t buflen);

/* 110220 dhkim:
 * The same with tee_arg below, tee_img should NOT be duplicated as it is now
 */
struct tee_img {
	int crypto_mode;
	/* 110220 dhkim:
	 * TODO: iv buffer size should be variable. one right way of doing so could be
	 * defining a protocol between NW and SW with variable header size. We leave it
	 * as a future work.
	 */
	uint8_t iv[16];
	uint8_t iv_len;
    int width, height;
    int x_pos, y_pos;
    int offset;
	size_t len;
} __attribute__((packed));


/* 110220 dhkim: 
 * this structure, tee_arg, MUST be the same with the structure with the same name
 * at /optee/core/include/rushmore_entry.h.
 * __attrubute__((packed)) is for removing paddings added by compilers.
 * FYI, different compiler may have different padding rules. Similar to removing
 * paddings when we make network transactions, paddings in this sturcture must
 * be removed too.
 * TODO: Make /librushmore to find struct tee_arg from rushmore_entry.h. Let's not
 * have duplicated structure definitions if we can.
 */
struct tee_arg {
    struct tee_img (*imgs)[];
    int num_tee_imgs;
    int func_id;
    //enum crypto_mode crypto_mode;
    bool single_img_render;

    void *buffer;
    size_t buffer_size;
} __attribute__((packed));


struct thread_arg {
    RushmoreClient *rc;
    Frame *frames;
    int num_frames;
    bool fixed_fps;
    size_t frame_size;
};


struct buffered_frame {
    struct tee_img (*imgs)[];
    int num_tee_imgs;

    bool last;
    struct buffered_frame *next;
};


/* To control buffer */
pthread_mutex_t lock;
struct buffered_frame *prev_buffered_frame;
struct buffered_frame *head_buffered_frame;


void __teec_force_nw(bool force) {
	DIRECT_RENDER = force;
}


static bool allocate_cma_buffer(RushmoreClient *rc) {

    /* Allocate Rushmore buffer */
    int smc_fd = open("/dev/smc_driver", O_RDWR);
    if (!smc_fd) return false;

    uint8_t* buffer = (uint8_t *) mmap(0, RUSHMORE_BUFFER_SIZE,
                    PROT_READ | PROT_WRITE, MAP_SHARED, smc_fd, 0);
    if (!buffer) return false;

    pthread_mutex_lock(&lock);
    rc->_smc_fd = smc_fd;
    rc->_buffer = buffer;
    rc->_frames_in_buffer = 0;
    rc->_next_offset = 0;
    rc->_end_offset = RUSHMORE_BUFFER_SIZE;
    pthread_mutex_unlock(&lock);

//    printf("\mAllocated CMA buffer size: %d bytes at 0x%p\n", RUSHMORE_BUFFER_SIZE, buffer);
    return true;
}


static bool release_cma_buffer(RushmoreClient *rc) {

    if (!rc->_smc_fd) return false;
    close(rc->_smc_fd);
    return true;
}


static uint8_t calculate_latency(struct timespec *sTime, struct timespec *eTime) {

    return (eTime->tv_sec - sTime->tv_sec)*1000 + (eTime->tv_nsec - sTime->tv_nsec)/1000000;
}


static int get_next_offset(RushmoreClient *rc, int size) {

    int offset = -1;

    pthread_mutex_lock(&lock);

    /*                          _next_offset       _end_offset
     *                                |                |
     *  Rushmore buffer:  [###########                 ######]
     *    - #: used
     */
    if (rc->_next_offset <= rc->_end_offset) {

        /* Available contigous memory exists */
        if (size <= (rc->_end_offset - rc->_next_offset)) {
            offset = rc->_next_offset;
            rc->_next_offset = offset + size;
        }
    }


    /*                          _end_offset       _next_offset
     *                                |                |
     *  Rushmore buffer:  [           #################      ]
     *    - #: used
     */
    else {

        /* Available contigous memory exists */
        if (size <= (RUSHMORE_BUFFER_SIZE - rc->_next_offset)) {
            offset = rc->_next_offset;
            rc->_next_offset = offset + size;
        }

        /* No available contiguous memory exists (find from a starting offset) */
        else if (size <= (rc->_end_offset - 0)){
            offset = 0;
            rc->_next_offset = offset + size;
        }
    }

    pthread_mutex_unlock(&lock);

    return offset;
}


static int get_end_offset(RushmoreClient *rc, int size) {

    int offset = -1;
    pthread_mutex_lock(&lock);

    if (rc->_frames_in_buffer > 0) {

        /* Available contigous memory exists */
        if (size <= (RUSHMORE_BUFFER_SIZE - rc->_end_offset))
            offset = rc->_end_offset;

        /* No available contiguous memory exists (find from a starting offset) */
        else if (size <= (rc->_next_offset - 0))
            offset = 0;
    }

    pthread_mutex_unlock(&lock);
    return offset;
}


static int get_frame_size(EncryptedImage (*ei)[], int num_images) {
    int frame_size = 0;
    int n;

    /* Get size of the frame */
    for (n=0; n<num_images; n++)
        //frame_size += (*ei)[n].width * (*ei)[n].height * BYTES_PER_PIXEL;
        frame_size += (*ei)[n].buflen;

    return frame_size;
}



static void *thread_render_frame(void* t_arg) {

    struct thread_arg *arg = (struct thread_arg *) t_arg;
    RushmoreClient *rc = arg->rc;
    size_t frame_size = arg->frame_size;
    bool fixed_fps = arg->fixed_fps;

    /* Rendering buffered frames - request to SW */
    int n=0;
    struct timespec prevTime, newTime;
    while (true) {
		int m;

        /* If no frame to render in buffer, wait in a loop */
        pthread_mutex_lock(&lock);
        struct buffered_frame *frame = head_buffered_frame;
        pthread_mutex_unlock(&lock);

        if (frame == NULL || rc->_frames_in_buffer == 0)
            continue;

        struct tee_img (*imgs)[(*frame).num_tee_imgs] = (*frame).imgs;

        if (frame_size == 0) {
            for (m=0; m<(*frame).num_tee_imgs; m++)
                //frame_size += (*imgs)[m].width * (*imgs)[m].height * BYTES_PER_PIXEL;
                frame_size += (*imgs)[m].len;
        }

        int offset = get_end_offset(rc, frame_size);
        if (offset == -1) continue;

        /* Start measuring time */
        if (fixed_fps) clock_gettime(CLOCK_REALTIME, &prevTime);

        /* Create arguments to pass */
        struct tee_arg *arg = malloc(sizeof(struct tee_arg));
        (*arg).func_id = FUNC_ID_RENDER;     // TODO: temporary
        (*arg).imgs = imgs;
        (*arg).num_tee_imgs = (*frame).num_tee_imgs;
       

		if (!DIRECT_RENDER) {
			for (m=0; m<(*frame).num_tee_imgs; m++) {
				if ((*imgs)[m].crypto_mode != 4 /*VCRYPTO_BW_NOISY*/)
					continue;
				int image_size = (*imgs)[m].len - sizeof(rei_fh_t) - (*imgs)[m].iv_len;
                write_to_fb((*imgs)[m].x_pos, (*imgs)[m].y_pos,
						(*imgs)[m].width, (*imgs)[m].height,
						/* rc->_buffer + */ (*imgs)[m].offset,
						image_size);
			}

//        printf(" [ Before Write] - (*arg).imgs: %p, (*frame).imgs: %p, imgs: %p\n", (*arg).imgs, (*frame).imgs, imgs);

			/* SMC call (NW -> SW) */
            write(rc->_smc_fd, arg, 0);
		} else {
			for (m=0; m<(*frame).num_tee_imgs; m++) {
				int image_size = (*imgs)[m].len - sizeof(rei_fh_t) - (*imgs)[m].iv_len;
				switch ((*imgs)[m].crypto_mode) {
					case (2 /* CHACHA20 */):
						assert((*imgs)[m].iv_len >= 12);
						ChaCha20XOR(rc->_buffer + (*imgs)[m].offset, /* out */
									rc->_buffer + (*imgs)[m].offset, /* in */
									/* dhkim: ChaCha20XOR in SIMD mode requires padding but our code is not capable of */
									(image_size / 64) * 64, /* inLen */
									key, /* key (32-byte) */
									(*imgs)[m].iv, /* nonce (12-byte) */
									0); /* counter */
						break;
					case (5 /* NONE */):
						break;
					default:
						continue;
				}
				write_to_fb((*imgs)[m].x_pos, (*imgs)[m].y_pos,
						(*imgs)[m].width, (*imgs)[m].height,
						rc->_buffer + (*imgs)[m].offset,
						image_size);
			}
		}

//        printf(" [ After Write] - (*arg).imgs: %p, (*frame).imgs: %p, imgs: %p\n", (*arg).imgs, (*frame).imgs, imgs);


        /* Put delay between frames to make 30 FPS */
        if (fixed_fps) {
            clock_gettime(CLOCK_REALTIME, &newTime);
            int prev_latency = calculate_latency(&prevTime, &newTime);
            if (prev_latency < DELAY_BETWEEN_FRAMES)
                usleep((DELAY_BETWEEN_FRAMES-prev_latency) * 1000);
        }


        free(imgs);
        free(arg);

        /* Update buffer information */
        pthread_mutex_lock(&lock);
        rc->_frames_in_buffer--;
        if (rc->_frames_in_buffer == 0) {
            rc->_next_offset = 0;
            rc->_end_offset = RUSHMORE_BUFFER_SIZE;
        } else
            rc->_end_offset = offset + frame_size;

        bool last = (*head_buffered_frame).last;

        if ((*head_buffered_frame).next == NULL) {
            free(head_buffered_frame);
            head_buffered_frame = NULL;
        }
        else {
            free(head_buffered_frame);
            head_buffered_frame = (*head_buffered_frame).next;
        }
        pthread_mutex_unlock(&lock);

        n++;

        if (last) break;
    }

}



static int _buffer_animation(RushmoreClient *rc,
        Frame (*func_allocate)(void *addr, size_t available_size),
        size_t frame_size, bool fixed_fps) {


    /* Separate thread - Rendering the buffered frames */
    pthread_t thread_id;
    struct thread_arg t_arg;
    t_arg.rc = rc;
    t_arg.fixed_fps = fixed_fps;
    t_arg.frame_size = frame_size;
    pthread_create(&thread_id, NULL, thread_render_frame, (void *) &t_arg);



    bool prev_wait = false;
    while (true) {

        /* Wait until there's availalbe memory in buffer */
        int offset = get_next_offset(rc, frame_size);

        if (offset == -1 ) {
            if (prev_wait) usleep(1);
            prev_wait = true;

            continue;
        }

        prev_wait = false;


        /* Trigger user defined callback to get frame */
        Frame (*func)(void *addr, size_t available_size) = func_allocate;
        Frame frame = func(rc->_buffer + offset, frame_size);

        int num_images = frame.num_images;
        EncryptedImage (*ei)[num_images] = frame.images;

        /* Copy all images to buffer */
        struct tee_img (*imgs)[num_images] = malloc(sizeof(struct tee_img) * num_images);
        int frame_offset = 0;
        int m;
        for (m=0; m<num_images; m++) {
            (*imgs)[m].offset = offset + frame_offset;
			(*imgs)[m].crypto_mode = (*ei)[m].buffer->crypto_mode;
			memcpy((*imgs)[m].iv, (*ei)[m].buffer->iv, (*ei)[m].buffer->iv_len);
			(*imgs)[m].iv_len = (*ei)[m].buffer->iv_len;
			(*imgs)[m].width = (*ei)[m].buffer->width;
			(*imgs)[m].height = (*ei)[m].buffer->height;
			(*imgs)[m].len = (*ei)[m].buflen;
            (*imgs)[m].x_pos = (*ei)[m].x_pos;
            (*imgs)[m].y_pos = (*ei)[m].y_pos;

            int image_size = (*ei)[m].buflen - sizeof(rei_fh_t) - (*ei)[m].buffer->iv_len;
            if ((*imgs)[m].crypto_mode == 4) 
			    (*imgs)[m].offset = ((uint8_t *)(&(*ei)[m].buffer[1])) + (*ei)[m].buffer->iv_len;
            else
                memcpy(rc->_buffer + offset + frame_offset,
                    ((uint8_t *) (&(*ei)[m].buffer[1])) + (*ei)[m].buffer->iv_len, image_size);

            frame_offset += image_size;
        }

        struct buffered_frame *buffered_frame = malloc(sizeof(struct buffered_frame));
        (*buffered_frame).imgs = imgs;
        (*buffered_frame).num_tee_imgs = num_images;
        (*buffered_frame).last = frame.last;

        pthread_mutex_lock(&lock);
        rc->_frames_in_buffer++;

        if (head_buffered_frame == NULL)
            head_buffered_frame = buffered_frame;

        if (prev_buffered_frame != NULL)
            prev_buffered_frame->next = buffered_frame;

        prev_buffered_frame = buffered_frame;
        pthread_mutex_unlock(&lock);

        if (frame.last) break;
    }

    /* Reset buffer information */
    pthread_join(thread_id, NULL);
    pthread_mutex_lock(&lock);
    rc->_frames_in_buffer = 0;
    rc->_next_offset = 0;
    rc->_end_offset = RUSHMORE_BUFFER_SIZE;
    prev_buffered_frame = NULL;
    head_buffered_frame = NULL;
    pthread_mutex_unlock(&lock);

    return TEEC_RESULT_SUCCESS;

}



static int _render_animation(
                    RushmoreClient *rc, Frame *frames, int num_frames, bool fixed_fps) {

    if (num_frames <= 0) return TEEC_RESULT_ERROR;

    /* Separate thread - Rendering the buffered frames */
	pthread_t thread_id;
	struct thread_arg t_arg;
	t_arg.rc = rc;
	t_arg.frame_size = 0;
	t_arg.fixed_fps = fixed_fps;
	pthread_create(&thread_id, NULL, thread_render_frame, (void *) &t_arg);

    bool prev_wait = false;

    struct timespec prevTime, newTime;
    int n=0;
    while(n<num_frames) {
        /* Get frame and set last flag */
        Frame frame = frames[n];
        if (n == num_frames - 1)
            frame.last = true;
        else
            frame.last = false;


        int num_images = frame.num_images;
        EncryptedImage (*ei)[num_images] = frame.images;

        /* Wait until there's availalbe memory in buffer */
        int frame_size = get_frame_size(ei, num_images);
        int offset = get_next_offset(rc, frame_size);
        if (offset == -1 ) {
            if (prev_wait) usleep(1);
            prev_wait = true;
            continue;
        }
       
        prev_wait = false;

        /* Copy all images to buffer */
        struct tee_img (*imgs)[num_images] = malloc(sizeof(struct tee_img) * num_images);

        int frame_offset = 0;
        int m;
        for (m=0; m<num_images; m++) {
            (*imgs)[m].offset = offset + frame_offset;
			(*imgs)[m].crypto_mode = (*ei)[m].buffer->crypto_mode;
			memcpy((*imgs)[m].iv, (*ei)[m].buffer->iv, (*ei)[m].buffer->iv_len);
			(*imgs)[m].iv_len = (*ei)[m].buffer->iv_len;
			(*imgs)[m].width = (*ei)[m].buffer->width;
			(*imgs)[m].height = (*ei)[m].buffer->height;
			(*imgs)[m].len = (*ei)[m].buflen;
            (*imgs)[m].x_pos = (*ei)[m].x_pos;
            (*imgs)[m].y_pos = (*ei)[m].y_pos;

            int image_size = (*ei)[m].buflen - sizeof(rei_fh_t) - (*ei)[m].buffer->iv_len;
            if ((*imgs)[m].crypto_mode == 4) 
			    (*imgs)[m].offset = ((uint8_t *)(&(*ei)[m].buffer[1])) + (*ei)[m].buffer->iv_len;

            else
                memcpy(rc->_buffer + offset + frame_offset,
					    ((uint8_t *)(&(*ei)[m].buffer[1])) + (*ei)[m].buffer->iv_len, image_size);


            frame_offset += image_size;
        }

        struct buffered_frame *buffered_frame = malloc(sizeof(struct buffered_frame));
        (*buffered_frame).imgs = imgs;
        (*buffered_frame).num_tee_imgs = num_images;
        (*buffered_frame).last = frame.last;

        pthread_mutex_lock(&lock);
        rc->_frames_in_buffer++;

        if (head_buffered_frame == NULL)
            head_buffered_frame = buffered_frame;

        if (prev_buffered_frame != NULL)
            prev_buffered_frame->next = buffered_frame;

        prev_buffered_frame = buffered_frame;
        pthread_mutex_unlock(&lock);

        n++;
    }


    /* Reset buffer information */
	pthread_join(thread_id, NULL);
    pthread_mutex_lock(&lock);
    rc->_frames_in_buffer = 0;
    rc->_next_offset = 0;
    rc->_end_offset = RUSHMORE_BUFFER_SIZE;
    prev_buffered_frame = NULL;
    head_buffered_frame = NULL;
    pthread_mutex_unlock(&lock);

    return TEEC_RESULT_SUCCESS;
}


/*******************************************************************************/


int teec_init(RushmoreClient *rc) {

    /* Check if all the required settings are provided */
    if (rc->ta_id == 0) return TEEC_RESULT_ERROR;

    /* Allocate Rushmore buffer (shared between user and kernel space) */
	allocate_cma_buffer(rc);


    // TODO: Add if there's anything to init before start
    //          If reuqires SMC call, use FUNC_ID_INIT
    struct tee_arg *arg = malloc(sizeof(struct tee_arg));
    (*arg).func_id = FUNC_ID_INIT;
    (*arg).single_img_render = rc->single_img_render;

    if (!DIRECT_RENDER)
        write(rc->_smc_fd, arg, 0);
    free(arg);


    pthread_mutex_lock(&lock);
    rc->_init = true;
    pthread_mutex_unlock(&lock);

    return TEEC_RESULT_SUCCESS;
}


int teec_release(RushmoreClient *rc) {

    bool res = TEEC_RESULT_ERROR;

    struct tee_arg *arg = malloc(sizeof(struct tee_arg));
    (*arg).func_id = FUNC_ID_RELEASE;


    /* Release SMC call */
    if (!DIRECT_RENDER)
        write(rc->_smc_fd, arg, 0);
    free(arg);

    if (!DIRECT_RENDER) {
        if (!rc->_smc_fd) return res;
        close(rc->_smc_fd);
    }

    pthread_mutex_lock(&lock);
    rc->_init = false;
    pthread_mutex_unlock(&lock);

    return res;
}


int teec_invoke(RushmoreClient *rc, void *buffer, size_t buffer_size) {

    /* Passing buffer address and size */
    struct tee_arg *arg = malloc(sizeof(struct tee_arg));
    (*arg).func_id = FUNC_ID_INVOKE;
    (*arg).buffer = buffer;
    (*arg).buffer_size = buffer_size;

    write(rc->_smc_fd, arg, 0);
    free(arg);

    return TEEC_RESULT_SUCCESS;
}


/* Rendering function for a single frame */
int teec_render_images(RushmoreClient *rc, EncryptedImage (*ei)[], int num_images) {

    if (num_images <= 0) return TEEC_RESULT_ERROR;

    struct timespec sTime, eTime;

    if (TIME_IMG_BUFFER)
        clock_gettime(CLOCK_REALTIME, &sTime);

    /* Create arguments to pass */
    struct tee_img (*imgs)[num_images] = malloc(sizeof(struct tee_img) * num_images);
    int offset = 0;
    int m;
    for (m=0; m<num_images; m++) {
        (*imgs)[m].offset = offset;
		(*imgs)[m].crypto_mode = (*ei)[m].buffer->crypto_mode;
		memcpy((*imgs)[m].iv, (*ei)[m].buffer->iv, (*ei)[m].buffer->iv_len);
		(*imgs)[m].iv_len = (*ei)[m].buffer->iv_len;
		(*imgs)[m].width = (*ei)[m].buffer->width;
		(*imgs)[m].height = (*ei)[m].buffer->height;
        //(*imgs)[m].width = (*ei)[m].width;
        //(*imgs)[m].height = (*ei)[m].height;
		(*imgs)[m].len = (*ei)[m].buflen;
        (*imgs)[m].x_pos = (*ei)[m].x_pos;
        (*imgs)[m].y_pos = (*ei)[m].y_pos;

		//int image_size = (*ei)[m].width * (*ei)[m].height * BYTES_PER_PIXEL;
		int image_size = (*ei)[m].buflen - sizeof(rei_fh_t) - (*ei)[m].buffer->iv_len;
        
		if ((*imgs)[m].crypto_mode == 4 ) {
			write_to_fb((*imgs)[m].x_pos, (*imgs)[m].y_pos,
					(*imgs)[m].width, (*imgs)[m].height,
					((uint8_t *)(&(*ei)[m].buffer[1])) + (*ei)[m].buffer->iv_len,
					image_size);
            
           
		} else {
			memcpy(rc->_buffer + offset,
					((uint8_t *)(&(*ei)[m].buffer[1])) + (*ei)[m].buffer->iv_len,
					image_size);
		}


        offset += image_size;
    }

    struct tee_arg *arg = malloc(sizeof(struct tee_arg));
    (*arg).imgs = imgs;
    (*arg).num_tee_imgs = num_images;
    (*arg).func_id = FUNC_ID_RENDER;     // TODO: temporary

    if (TIME_IMG_BUFFER) {
        clock_gettime(CLOCK_REALTIME, &eTime);
        printf("[Buffer] - before, %ld, %ld\n", sTime.tv_sec, sTime.tv_nsec); 
        printf("[Buffer] - after, %ld, %ld\n", eTime.tv_sec, eTime.tv_nsec); 
        usleep(10000);
    }

    printf("(*arg).imgs: %p\n", (*arg).imgs);

    /* SMC call */
    write(rc->_smc_fd, arg, 0);

    free(imgs);
    free(arg);

    return TEEC_RESULT_SUCCESS;
}



int teec_render_animation(RushmoreClient *rc, Frame *frames, int num_frames, bool fixed_fps) {

    if (num_frames <= 0) return TEEC_RESULT_ERROR;

	return _render_animation(rc, frames, num_frames, fixed_fps);
}


int teec_buffer_animation(RushmoreClient *rc,
        Frame (*func_allocate)(void *addr, size_t available_size),
        size_t frame_size, bool fixed_fps) {

	return _buffer_animation(rc, func_allocate, frame_size, fixed_fps);
}


int teec_remove(RushmoreClient *rc) {

    struct tee_arg *arg = malloc(sizeof(struct tee_arg));
    (*arg).func_id = FUNC_ID_REMOVE;

    /* SMC call */
    if (!DIRECT_RENDER) 
        write(rc->_smc_fd, arg, 0);

    free(arg);
    return TEEC_RESULT_SUCCESS;
}


