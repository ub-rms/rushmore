#pragma once

#include <stdint.h>

// REI file header
typedef struct _reih_t {
	uint16_t magic;
	uint16_t hdr_len;
	uint8_t ver;
	uint8_t reserved;
	uint16_t fps;
	uint32_t num_frames;
	uint8_t data[];
} __attribute__((packed)) reih_t;

// REI frame header
typedef struct _rei_fh_t {
	uint16_t width;
	uint16_t height;
	uint16_t pixel_format;
	uint16_t reserved;
	uint32_t frame_size;
	uint16_t crypto_mode;
	uint16_t iv_len;
	uint8_t iv[];
} __attribute__((packed)) rei_fh_t;

typedef struct _rei_t {
	int fd;

	int num_frames;
	size_t *frame_pos;
	rei_fh_t *frameh;
} rei_t;

rei_t *rei_open(char const * const path);

void rei_close(rei_t *rei);

int rei_get_num_frames(rei_t *rei);

int rei_get_frame_width(rei_t *rei, int idx);

int rei_get_frame_height(rei_t *rei, int idx);

ssize_t rei_get_frame_size(rei_t *rei, int idx);

int rei_read_frame(rei_t *rei, int idx, void *buf, size_t buflen);

