#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define BYTES_PER_PIXEL 2

struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
uint8_t *fbp;
int fb_fd = -1;
long screensize;


void init_fb() {
    fb_fd = open("/dev/graphics/fb0",O_RDWR);

    //Get variable screen information
    //ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    //vinfo.grayscale=0;
    //vinfo.bits_per_pixel=32;
    //ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    screensize = vinfo.yres_virtual * finfo.line_length;

	printf("finfo.line_length: %d, vinfo.yres_virtual: %d, bpp: %d\n",
			finfo.line_length, vinfo.yres_virtual, vinfo.bits_per_pixel);
	printf("vinfo.xoffset: %d, vinfo.yoffset: %d\n", vinfo.xoffset, vinfo.yoffset);

    fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

}

void deinit_fb() {
	munmap(fbp, screensize);
	close(fb_fd);
	fb_fd = -1;
}

int write_to_fb(int x_pos, int y_pos, int width, int height, void *buffer, size_t buflen) {
    int x, y;

	if (fb_fd < 0)
		init_fb();
    
	for (y = 0; y < height; y++) {
		ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
		long location = (vinfo.xoffset + x_pos) * (vinfo.bits_per_pixel / 8)
			+ (y + vinfo.yoffset + y_pos) * finfo.line_length;

		memcpy((void *)(fbp + location),
				buffer + y * width * BYTES_PER_PIXEL,
				width * BYTES_PER_PIXEL);
	}

    return 0;
}
