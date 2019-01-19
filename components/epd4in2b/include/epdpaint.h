#ifndef EPDPAINT_H
#define EPDPAINT_H

#include "esp_log.h"
#include "image.h"
#include <stdio.h>
#include <string.h>

// Display orientation
#define ROTATE_0 0
#define ROTATE_90 1
#define ROTATE_180 2
#define ROTATE_270 3

// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR 0

void paint(unsigned char* image, int width, int height);
void clear(int colored);
void draw_absolute_pixel(int x, int y, int colored);
void draw_pixel(int x, int y, int colored);
void draw_line(int x0, int y0, int x1, int y1, int colored);
void draw_horizontal_line(int x, int y, int width, int colored);
void draw_vertical_line(int x, int y, int height, int colored);
void draw_rectangle(int x0, int y0, int x1, int y1, int colored);
void draw_filled_rectangle(int x0, int y0, int x1, int y1, int colored);
void draw_circle(int x, int y, int radius, int colored);
void draw_filled_circle(int x, int y, int radius, int colored);
void draw_bitmap_mono(int x, int y, const tImage* image);
void draw_bitmap_mono_in_center(int x_dev, int x_number, int width, int y, const tImage* image);
const tChar* find_char_by_code(int code, const tFont* font);
int utf8_next_char(const char* str, int start, int* resultCode, int* nextIndex);
void draw_string(const char* str, int x, int y, const tFont* font);
void draw_string_in_grid_align_center(int x_dev, int x_number, int width, int y, const char* str, const tFont* font);
void draw_string_in_grid_align_left(int x_dev, int x_number, int offset, int width, int y, const char* str, const tFont* font);
void draw_string_in_grid_align_right(int x_dev, int x_number, int offset, int width, int y, const char* str, const tFont* font);
int calculate_width(const char* str, const tFont* font);

#endif