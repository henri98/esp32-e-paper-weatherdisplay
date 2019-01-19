#include "epdpaint.h"

unsigned char* image;
int width;
int height;
int rotate;

void paint(unsigned char* image1, int width, int height)
{
    rotate = ROTATE_0;
    image = image1;
    /* 1 byte = 8 pixels, so the width should be the multiple of 8 */
    width = width % 8 ? width + 8 - (width % 8) : width;
    height = height;
}

/**
 *  @brief: clear the image
 */
void clear(int colored)
{
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            draw_absolute_pixel(x, y, colored);
        }
    }
}

/**
 *  @brief: this draws a pixel by absolute coordinates.
 *          this function won't be affected by the rotate parameter.
 */
void draw_absolute_pixel(int x, int y, int colored)
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }
    if (IF_INVERT_COLOR) {
        if (colored) {
            image[(x + y * width) / 8] |= 0x80 >> (x % 8);
        } else {
            image[(x + y * width) / 8] &= ~(0x80 >> (x % 8));
        }
    } else {
        if (colored) {
            image[(x + y * width) / 8] &= ~(0x80 >> (x % 8));
        } else {
            image[(x + y * width) / 8] |= 0x80 >> (x % 8);
        }
    }
}

/**
 *  @brief: this draws a pixel by the coordinates
 */
void draw_pixel(int x, int y, int colored)
{
    int point_temp;
    if (rotate == ROTATE_0) {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return;
        }
        draw_absolute_pixel(x, y, colored);
    } else if (rotate == ROTATE_90) {
        if (x < 0 || x >= height || y < 0 || y >= width) {
            return;
        }
        point_temp = x;
        x = width - y;
        y = point_temp;
        draw_absolute_pixel(x, y, colored);
    } else if (rotate == ROTATE_180) {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return;
        }
        x = width - x;
        y = height - y;
        draw_absolute_pixel(x, y, colored);
    } else if (rotate == ROTATE_270) {
        if (x < 0 || x >= height || y < 0 || y >= width) {
            return;
        }
        point_temp = x;
        x = y;
        y = height - point_temp;
        draw_absolute_pixel(x, y, colored);
    }
}

const tChar* find_char_by_code(int code, const tFont* font)
{
    int count = font->length;
    int first = 0;
    int last = count - 1;
    int mid = 0;

    if (count > 0) {
        if ((code >= font->chars[0].code) && (code <= font->chars[count - 1].code)) {
            while (last >= first) {
                mid = first + ((last - first) / 2);

                if (font->chars[mid].code < code)
                    first = mid + 1;
                else if (font->chars[mid].code > code)
                    last = mid - 1;
                else
                    break;
            }

            if (font->chars[mid].code == code)
                return (&font->chars[mid]);
        }
    }

    return (0);
}

int utf8_next_char(const char* str, int start, int* resultCode, int* nextIndex)
{
    int len = 0;
    int index = 0;
    *resultCode = 0;

    while (*(str + index) != 0) {
        len++;
        index++;
    }

    unsigned char c;
    unsigned int code = 0;
    int result = 0;
    int skip = 0;

    *resultCode = 0;
    *nextIndex = -1;

    if (start >= 0 && start < len) {
        index = start;

        while (index < len) {
            c = *(str + index);
            index++;

            // msb
            if (skip == 0) {
                // if range 0x00010000-0x001fffff
                if ((c & 0xf8) == 0xf0) {
                    skip = 3;
                    code = c;
                }
                // if range 0x00000800-0x0000ffff
                else if ((c & 0xf0) == 0xe0) {
                    skip = 2;
                    code = c;
                }
                // if range 0x00000080-0x000007ff
                else if ((c & 0xe0) == 0xc0) {
                    skip = 1;
                    code = c;
                }
                // if range 0x00-0x7f
                else //if ((c & 0x80) == 0x00)
                {
                    skip = 0;
                    code = c;
                }
            } else // not msb
            {
                code = code << 8;
                code |= c;
                skip--;
            }
            if (skip == 0) {
                // completed
                *resultCode = code;
                *nextIndex = index;
                result = 1;
                break;
            }
        }
    }
    return (result);
}

void draw_string(const char* str, int x, int y, const tFont* font)
{
    int len = strlen(str);
    int index = 0;
    int code = 0;
    int x1 = x;
    int nextIndex;
    while (index < len) {
        if (utf8_next_char(str, index, &code, &nextIndex) != 0) {
            const tChar* ch = find_char_by_code(code, font);
            if (ch != 0) {
                draw_bitmap_mono(x1, y, ch->image);
                x1 += ch->image->width;
            }
        }
        index = nextIndex;
        if (nextIndex < 0)
            break;
    }
}

void draw_bitmap_mono_in_center(int x_dev, int x_number, int width, int y, const tImage* image)
{
    draw_bitmap_mono(((width / x_dev)) * (x_number) + (((width / x_dev) - image->width) / 2), y, image);
}

void draw_string_in_grid_align_center(int x_dev, int x_number, int width, int y, const char* str, const tFont* font)
{
    int str_width_on_display = calculate_width(str, font);
    draw_string(str, ((width / x_dev)) * (x_number) + (((width / x_dev) - str_width_on_display) / 2), y, font);
}

void draw_string_in_grid_align_left(int x_dev, int x_number, int offset, int width, int y, const char* str, const tFont* font)
{
    int str_width_on_display = calculate_width(str, font);
    draw_string(str, ((width / x_dev)) * (x_number) + (((width / x_dev) - str_width_on_display) - offset), y, font);
}

void draw_string_in_grid_align_right(int x_dev, int x_number, int offset, int width, int y, const char* str, const tFont* font)
{
    draw_string(str, ((width / x_dev)) * (x_number) + offset, y, font);
}

int calculate_width(const char* str, const tFont* font)
{
    int width = 0;

    int len = strlen(str);
    int index = 0;
    int code = 0;
    int nextIndex;
    while (index < len) {
        if (utf8_next_char(str, index, &code, &nextIndex) != 0) {
            const tChar* ch = find_char_by_code(code, font);
            if (ch != 0) {
                width += ch->image->width;
            }
        }
        index = nextIndex;
        if (nextIndex < 0)
            break;
    }

    return width;
}

void draw_bitmap_mono(int x, int y, const tImage* image)
{
    uint8_t value = 0;
    int x0, y0;
    int counter = 0;
    const uint8_t* pdata = (const uint8_t*)image->data;

    // rows
    for (y0 = 0; y0 < image->height; y0++) {
        // columns
        counter = 0;
        for (x0 = 0; x0 < image->width; x0++) {
            // load new data
            if (counter == 0) {
                value = *pdata++;
                counter = image->dataSize;
            }
            counter--;

            if (x0 < image->width) {
                // set pixel
                if ((value & 0x80) != 0)
                    draw_pixel(x + x0, y + y0, 0);
                else
                    draw_pixel(x + x0, y + y0, 1);
            }

            value = value << 1;
        }
    }
}

/**
*  @brief: this draws a line on the frame buffer
*/
void draw_line(int x0, int y0, int x1, int y1, int colored)
{
    /* Bresenham algorithm */
    int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while ((x0 != x1) && (y0 != y1)) {
        draw_pixel(x0, y0, colored);
        if (2 * err >= dy) {
            err += dy;
            x0 += sx;
        }
        if (2 * err <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
*  @brief: this draws a horizontal line on the frame buffer
*/
void draw_horizontal_line(int x, int y, int line_width, int colored)
{
    int i;
    for (i = x; i < x + line_width; i++) {
        draw_pixel(i, y, colored);
    }
}

/**
*  @brief: this draws a vertical line on the frame buffer
*/
void draw_vertical_line(int x, int y, int line_height, int colored)
{
    int i;
    for (i = y; i < y + line_height; i++) {
        draw_pixel(x, i, colored);
    }
}

/**
*  @brief: this draws a rectangle
*/
void draw_rectangle(int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    draw_horizontal_line(min_x, min_y, max_x - min_x + 1, colored);
    draw_horizontal_line(min_x, max_y, max_x - min_x + 1, colored);
    draw_vertical_line(min_x, min_y, max_y - min_y + 1, colored);
    draw_vertical_line(max_x, min_y, max_y - min_y + 1, colored);
}

/**
*  @brief: this draws a filled rectangle
*/
void draw_filled_rectangle(int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    int i;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    for (i = min_x; i <= max_x; i++) {
        draw_vertical_line(i, min_y, max_y - min_y + 1, colored);
    }
}

/**
*  @brief: this draws a circle
*/
void draw_circle(int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        draw_pixel(x - x_pos, y + y_pos, colored);
        draw_pixel(x + x_pos, y + y_pos, colored);
        draw_pixel(x + x_pos, y - y_pos, colored);
        draw_pixel(x - x_pos, y - y_pos, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

/**
*  @brief: this draws a filled circle
*/
void draw_filled_circle(int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        draw_pixel(x - x_pos, y + y_pos, colored);
        draw_pixel(x + x_pos, y + y_pos, colored);
        draw_pixel(x + x_pos, y - y_pos, colored);
        draw_pixel(x - x_pos, y - y_pos, colored);
        draw_horizontal_line(x + x_pos, y + y_pos, 2 * (-x_pos) + 1, colored);
        draw_horizontal_line(x + x_pos, y - y_pos, 2 * (-x_pos) + 1, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

/* END OF FILE */
