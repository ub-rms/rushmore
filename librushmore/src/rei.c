#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "rei.h"

rei_t *rei_open(char const * const path) {
	rei_t *rei;
	reih_t fileh;
	int i;

	if (!(rei = malloc(sizeof(rei_t))))
		return NULL;

	rei->frame_pos = NULL;
	rei->frameh = NULL;

	if ((rei->fd = open(path, O_RDONLY)) < 0)
		goto error;

	if (read(rei->fd, &fileh, sizeof(fileh)) != sizeof(fileh))
		goto error;

	fileh.magic = ntohs(fileh.magic);
	fileh.hdr_len = ntohs(fileh.hdr_len);
	// fileh.fps = ntohs(fileh.fps);
	fileh.num_frames = ntohl(fileh.num_frames);

	if (fileh.magic != 0x5718)
		goto error;

	if (fileh.ver != 1 || fileh.hdr_len != sizeof(fileh))
		goto error;

	if (!(rei->frame_pos = malloc(sizeof(size_t *) * fileh.num_frames))
			|| !(rei->frameh = malloc(sizeof(rei_fh_t) * fileh.num_frames)))
		goto error;

	rei->num_frames = fileh.num_frames;

	for (i = 0; i < fileh.num_frames; i++) {
		rei_fh_t *frameh = &rei->frameh[i];

		if (read(rei->fd, frameh, sizeof(rei_fh_t)) != sizeof(rei_fh_t))
			goto error;

		frameh->width = ntohs(frameh->width);
		frameh->height = ntohs(frameh->height);
		frameh->pixel_format = ntohs(frameh->pixel_format);
		frameh->frame_size = ntohl(frameh->frame_size);
		frameh->crypto_mode = ntohs(frameh->crypto_mode);
		frameh->iv_len = ntohs(frameh->iv_len);

		rei->frame_pos[i] = lseek(rei->fd, 0, SEEK_CUR) - sizeof(rei_fh_t);
		//printf("frame %d pos: %d\n", i, rei->frame_pos[i]);

		lseek(rei->fd, frameh->frame_size - sizeof(rei_fh_t), SEEK_CUR);
	}

	return rei;

error:
	if (rei) {
		if (rei->frame_pos)
			free(rei->frame_pos);

		if (rei->frameh)
			free(rei->frameh);

		free(rei);
	}

	return NULL;
}

void rei_close(rei_t *rei) {
	if (!rei)
		return;

	close(rei->fd);

	if (rei->frame_pos)
		free(rei->frame_pos);

	if (rei->frameh)
		free(rei->frameh);

	free(rei);
}

int rei_get_num_frames(rei_t *rei) {
	if (!rei)
		return -1;

	return rei->num_frames;
}

int rei_get_frame_width(rei_t *rei, int idx) {
	if (!rei)
		return -1;

	if (idx >= rei->num_frames || idx < 0)
		return -1;

	return rei->frameh[idx].width;
}

int rei_get_frame_height(rei_t *rei, int idx) {
	if (!rei)
		return -1;

	if (idx >= rei->num_frames || idx < 0)
		return -1;

	return rei->frameh[idx].height;
}

ssize_t rei_get_frame_size(rei_t *rei, int idx) {
	if (!rei)
		return -1;

	if (idx >= rei->num_frames || idx < 0)
		return -1;

	return rei->frameh[idx].frame_size;
}

int rei_read_frame(rei_t *rei, int idx, void *buf, size_t buflen) {
	//printf("rei_read_frame: buflen: %u\n", buflen);
	ssize_t frame_size;
	int to_read;

	if (!buf)
		return -1;

	if ((frame_size = rei_get_frame_size(rei, idx)) < 0)
		return -1;

	if (buflen < frame_size)
		return -1;

	if (lseek(rei->fd, rei->frame_pos[idx] + sizeof(rei_fh_t), SEEK_SET) < 0)
		return -1;

	// copy ntoh-ed header.
	*((rei_fh_t*)buf) = rei->frameh[idx];

	// copy frame payload.
	to_read = frame_size - sizeof(rei_fh_t);
	
	if (read(rei->fd, buf + sizeof(rei_fh_t), to_read) != to_read)
		return -1;

	//printf("rei_read_frame: ok\n");

	return 0;
}

