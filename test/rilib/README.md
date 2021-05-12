# RILib: Rushmore Image Library

dhkim09@kaist.ac.kr, 2019

A simple image library in C for the trusted computing.

```c
typedef _canvas_t {
	int width;
	int height;

	int format;

	uint8_t *buffer;
} canvas_t;

typedef _region_t {
	canvas_t *canvas;

	int x;
	int y;
	int width;
	int height;
} region_t;

/** Initialize a canvas.
 * [out] canvas
 * [in]  width
 * [in]  height
 * [in]  format: Either RGB888 or RGB565.
 * [in]  buffer
 * [ret] 0 on success, -1 on failure
 */
int img_init_canvas(canvas_t *canvas, int width, int height, int format, uint8_t *buffer);

/** Initialize a region.
 * [in]  canvas
 * [out] region
 * [in]  x
 * [in]  y
 * [in]  width
 * [in]  height
 * [ret] 0 on success, -1 on failure
 */
int img_init_region(canvas_t *canvas, region_t *region, int x, int y, int width, int height);

/** Initialize a region to cover an entire canvas.
 * [in]  canvas
 * [out] region
 * [ret] 0 on success, -1 on failure
 */
int img_canvas_to_region(canvas_t *canvas, region_t *region);
```

## Drawing
```c
/** Fill a region with a given color
 * [in]  region
 * [in]  r
 * [in]  g
 * [in]  b
 * [ret] 0 on success, -1 on failure
 */
int img_fill_region(region_t *region, uint8_t r, uint8_t g, uint8_t b);

/** Set the color of a pixel
 * [in]  region
 * [in]  x
 * [in]  y
 * [in]  r
 * [in]  g
 * [in]  b
 * [ret] 0 on success, -1 on failure
 */
int img_set_color(region_t *region, int x, int y, uint8_t r, uint8_t g, uint8_t b);
```

## Text Rendering
```c
/** Render text to 
 * [in]  region
 * [in]  scale
 * [in]  flags: TEXT_LEFT, TEXT_CENTER_HORIZONTAL, TEXT_RIGHT, TEXT_TOP, TEXT_BOTTOM, TEXT_CENTER_VERTICAL
 * [in]  format
 * [ret] the number of characters rendered on success, -1 on failure
 */
int img_printf(region_t *region, int scale, int flags, const char * restrict format, ...);
```

## Random Image Generation
```c

int img_randomize_region(region_t *region, int (*random)(int len));
```


## Exporting Images
```c
int img_write_ppm(region_t *region, FILE *fp, int scale);
```
