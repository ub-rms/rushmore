/*
 *  Author: Chang Min Park
 *
 *  This script displays example images encrypted by Rushmore.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netdb.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "rushmore_api.h"
#include "rei.h"

#define BYTES_PER_PIXEL 2
#define EVALUATION_TA_ID 99

#define X_MAX   1280
#define Y_MAX   800

#ifndef MIN
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#endif


/* In loadded buffer (for animation buffering function) */
int next_frame_num = 0;
int num_frames = 0;
Frame *frames;


int compose_Frame(Frame *frame, rei_t *img, int idx, int num_dups) {
	int i;
	void *buf = NULL;

	int fw = rei_get_frame_width(img, idx);
	int fh = rei_get_frame_height(img, idx);
	int cols = MIN(X_MAX / fw, num_dups);
	int rows = MIN(Y_MAX / fh, (num_dups - 1) / cols + 1);
	int x_pad = (X_MAX - fw * cols) / (cols + 1);
	int y_pad = (Y_MAX - fh * rows) / (rows + 1);

	x_pad = (x_pad / 16) * 16;
	y_pad = (y_pad / 16) * 16;

	if (fw > X_MAX || fh > Y_MAX) {
		frame->num_images = 0;
		return -1;
	}

	ssize_t frame_size = rei_get_frame_size(img, idx);


	if (!(buf = malloc(frame_size)) || !(frame->images = malloc(sizeof(EncryptedImage) * num_dups))) {
		if (buf)
			free(buf);
		
		frame->num_images = 0;
		return -1;
	}

	if (rei_read_frame(img, idx, buf, frame_size) < 0)
		return -1;
	

	frame->num_images = num_dups;

	for (i = 0; i < num_dups; i++) {
		int col = i % cols;
		int row = i / cols;
		if (row >= rows) {
			frame->num_images = i;
			break;
		}

		(*frame->images)[i].x_pos = col * fw + x_pad * (col + 1);
		(*frame->images)[i].y_pos = row * fh + y_pad * (row + 1);
          
		(*frame->images)[i].buffer = buf;
		(*frame->images)[i].buflen = frame_size;
	}

	return 0;
}


void free_Frame(Frame *frame) {
	void *buf = (*frame->images)[0].buffer;

	if (!buf)
		free(buf);
	free(frame->images);
}


/* Callback function to load frame to animation buffer */
Frame load_frame(void *addr, size_t available_size) {

    Frame frame;
    frame = frames[next_frame_num];
    next_frame_num++;

    if (next_frame_num == num_frames) 
        frame.last = true;
    else
        frame.last = false;

    return frame;
}


int main(int argc, char *argv[])
{
	char const *rei_path = NULL;
	void *imgbuf = NULL;
    rei_t *img = NULL;
    struct timespec sTime, eTime;

	int i, j;
	int opt;

	int num_runs = 1;
	int num_dups = 1;
	int prefetch = 0;
    
	while ((opt = getopt(argc, argv, "r:d:p")) > 0)
		switch (opt) {
			case 'r':
				num_runs = atoi(optarg);
				break;
			case 'd':
				num_dups = atoi(optarg);
				break;
			case 'p':
				prefetch = 1;
				break;
            case '?':
			default:
				return -1;
		}


    /* Check if xres and yres are given */
    if (argc - optind != 1) {
        printf("\nUsage: run_test <rei file path> [options...]\n");
		printf("  options:\n");
		printf("    -r <n>      Repeat experiment for <n> times\n");
		printf("    -d <n>      Show the <n> duplicated images\n");
		printf("    -p          Prefetch image to memory\n");

        printf("  e.g., display three animated cube images 5 times\n");
        printf("        $ ./run_test cube_400x400_chacha20.rei -r 5 -d 3\n\n");
        exit(1);
    }
    
	rei_path = argv[optind];
    size_t size = strlen(rei_path);


    /* Initialize RushmoreClient */
    RushmoreClient rc;
    rc.ta_id=EVALUATION_TA_ID;
    if (num_dups == 1)
        rc.single_img_render = true;
    else
        rc.single_img_render = false;

    if (teec_init(&rc) == TEEC_RESULT_ERROR) {
        printf("[!] Error occured while initializing RushmoreClient. \n");
        exit(1);
    }

	if (!(img = rei_open(rei_path))) {
		printf("[!] Failed to open %s\n", rei_path);
		return -1;
	}



    /* Get # of frames in the given encrypted file */
    num_frames = rei_get_num_frames(img);
    frames = malloc(sizeof(Frame) * num_frames);
    printf(" - # frames received: %d\n", num_frames);

	if (!frames)
		return -1;


    for (i = 0; i < num_frames; i++) 
        compose_Frame(&frames[i], img, i, num_dups);
    

    /*
     *  Animated Image Rendering
     */
    if (num_frames > 1) {
    	if (prefetch) {
    		for (i = 0; i < num_runs; i++) { 
    			printf("\n[Run] %d - Animated (prefetched)\n", i);

    			/* BEFORE Time */
    			clock_gettime(CLOCK_REALTIME, &sTime); 
                printf(" - before: %ld (s), %ld (ns)\n", sTime.tv_sec, sTime.tv_nsec);
    			teec_render_animation(&rc, frames, num_frames, false /* fixed_fps */);

    			/* AFTER Time */
    			clock_gettime(CLOCK_REALTIME, &eTime); 
                printf(" - after: %ld (s), %ld (ns)\n", eTime.tv_sec, eTime.tv_nsec);
    		}

		    teec_remove(&rc);
    

        } else {
            for (i = 0; i < num_runs; i++) { 
                printf("\n[Run] %d - Animated (buffered)\n", i);
                next_frame_num=0;

                /* BEFORE Time */
                clock_gettime(CLOCK_REALTIME, &sTime); 
                printf(" - before: %d (s), %d(ns)\n", sTime.tv_sec, sTime.tv_nsec);

                size_t max_frame_size = 1280 * 800 * BYTES_PER_PIXEL; 
                teec_buffer_animation(&rc, &load_frame, max_frame_size, false /* fixed_fps */);

                /* AFTER Time */
                clock_gettime(CLOCK_REALTIME, &eTime); 
                printf(" - after: %d (s), %d (ns)\n", eTime.tv_sec, eTime.tv_nsec);
            }

            teec_remove(&rc);
        }
    }

    

    /*
     *  Single Image Rendering
     */
    else {
        Frame frame = frames[0];
        for (i = 0; i < num_runs; i++) { 
    		printf("\n[Run] %d - Single\n", i);

    		/* BEFORE Time */
    		clock_gettime(CLOCK_REALTIME, &sTime); 
    		printf(" - before: %ld (s), %ld (ns)\n", sTime.tv_sec, sTime.tv_nsec);

            teec_render_images(&rc, frame.images, frame.num_images);

    		/* AFTER Time */
    		clock_gettime(CLOCK_REALTIME, &eTime); 
    		printf(" - after: %ld (s), %ld (ns)\n", eTime.tv_sec, eTime.tv_nsec);
        }
        usleep(3000000);
        teec_remove(&rc);
    }



    /*
     *  Free loaded frames
     */
    for (i = 0; i < num_frames; i++)
	    free_Frame(&frames[i]);


    /* Release Rushmore client */
    teec_release(&rc); 

    return 0;
}

