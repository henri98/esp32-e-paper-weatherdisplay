#include "epd4in2b.h"
#include <stdlib.h>

gpio_num_t reset_pin;
gpio_num_t dc_pin;
gpio_num_t cs_pin;
gpio_num_t busy_pin;

unsigned int width;
unsigned int height;

int epd4in2b_init(void)
{
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;

    /* this calls the peripheral hardware interface, see epdif */
    if (ifinit() != 0) {
        return -1;
    }
    /* EPD hardware init start */
    reset();
    send_command(BOOSTER_SOFT_START);
    send_data(0x17);
    send_data(0x17);
    send_data(0x17); //07 0f 17 1f 27 2F 37 2f
    send_command(POWER_ON);
    wait_untile_idle();
    send_command(PANEL_SETTING);
    send_data(0x0F); // LUT from OTP
    /* EPD hardware init end */

    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void send_command(unsigned char command)
{
    digital_write(dc_pin, 0);
    spi_transfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void send_data(unsigned char data)
{
    digital_write(dc_pin, 1);
    spi_transfer(data);
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
void wait_untile_idle(void)
{
    while (digital_read(busy_pin) == 0) { //0: busy, 1: idle
        delay_ms(100);
    }
}

/**
 *  @brief: module reset. 
 *          often used to awaken the module in deep sleep, 
 *          see sleep();
 */
void reset(void)
{
    digital_write(reset_pin, 0);
    delay_ms(200);
    digital_write(reset_pin, 1);
    delay_ms(200);
}

/**
 * @brief Transmit partial data to the SRAM
 * 
 * @param char 
 * @param char 
 * @param x 
 * @param y 
 * @param w 
 * @param l 
 */
void set_partial_window(const unsigned char* buffer_black, const unsigned char* buffer_red, int x, int y, int w, int l)
{
    send_command(PARTIAL_IN);
    send_command(PARTIAL_WINDOW);
    send_data(x >> 8);
    send_data(x & 0xf8); // x should be the multiple of 8, the last 3 bit will always be ignored
    send_data(((x & 0xf8) + w - 1) >> 8);
    send_data(((x & 0xf8) + w - 1) | 0x07);
    send_data(y >> 8);
    send_data(y & 0xff);
    send_data((y + l - 1) >> 8);
    send_data((y + l - 1) & 0xff);
    send_data(0x01); // Gates scan both inside and outside of the partial window. (default)
    delay_ms(2);
    send_command(DATA_START_TRANSMISSION_1);
    if (buffer_black != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            send_data(buffer_black[i]);
        }
    }
    delay_ms(2);
    send_command(DATA_START_TRANSMISSION_2);
    if (buffer_red != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            send_data(buffer_red[i]);
        }
    }
    delay_ms(2);
    send_command(PARTIAL_OUT);
}

/**
 *  @brief: Transmit partial data to the black part of SRAM
 */
void set_partial_window_black(const unsigned char* buffer_black, int x, int y, int w, int l)
{
    send_command(PARTIAL_IN);
    send_command(PARTIAL_WINDOW);
    send_data(x >> 8);
    send_data(x & 0xf8); // x should be the multiple of 8, the last 3 bit will always be ignored
    send_data(((x & 0xf8) + w - 1) >> 8);
    send_data(((x & 0xf8) + w - 1) | 0x07);
    send_data(y >> 8);
    send_data(y & 0xff);
    send_data((y + l - 1) >> 8);
    send_data((y + l - 1) & 0xff);
    send_data(0x01); // Gates scan both inside and outside of the partial window. (default)
    delay_ms(2);
    send_command(DATA_START_TRANSMISSION_1);
    if (buffer_black != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            send_data(buffer_black[i]);
        }
    }
    delay_ms(2);
    send_command(PARTIAL_OUT);
}

/**
 * @brief Transmit partial data to the red part of SRAM
 * 
 * @param char 
 * @param x 
 * @param y 
 * @param w 
 * @param l 
 */
void set_partial_window_red(const unsigned char* buffer_red, int x, int y, int w, int l)
{
    send_command(PARTIAL_IN);
    send_command(PARTIAL_WINDOW);
    send_data(x >> 8);
    send_data(x & 0xf8); // x should be the multiple of 8, the last 3 bit will always be ignored
    send_data(((x & 0xf8) + w - 1) >> 8);
    send_data(((x & 0xf8) + w - 1) | 0x07);
    send_data(y >> 8);
    send_data(y & 0xff);
    send_data((y + l - 1) >> 8);
    send_data((y + l - 1) & 0xff);
    send_data(0x01); // Gates scan both inside and outside of the partial window. (default)
    delay_ms(2);
    send_command(DATA_START_TRANSMISSION_2);
    if (buffer_red != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            send_data(buffer_red[i]);
        }
    }
    delay_ms(2);
    send_command(PARTIAL_OUT);
}

/**
 * @brief Refresh and displays the frame
 * 
 * @param char Pointer to black frame buffer
 * @param char Pointer to red frame buffer
 */
void display_frame(const unsigned char* frame_black, const unsigned char* frame_red)
{
    if (frame_black != NULL) {
        send_command(DATA_START_TRANSMISSION_1);
        delay_ms(2);
        for (int i = 0; i < width / 8 * height; i++) {
            send_data(frame_black[i]);
        }
        delay_ms(2);
    }
    if (frame_red != NULL) {
        send_command(DATA_START_TRANSMISSION_2);
        delay_ms(2);
        for (int i = 0; i < width / 8 * height; i++) {
            send_data(frame_red[i]);
        }
        delay_ms(2);
    }
    send_command(DISPLAY_REFRESH);
    wait_untile_idle();
}

/**
 * @brief clear the frame data from the SRAM, this won't refresh the display
 * 
 */
void clear_frame(void)
{
    send_command(DATA_START_TRANSMISSION_1);
    delay_ms(2);
    for (int i = 0; i < width / 8 * height; i++) {
        send_data(0xFF);
    }
    delay_ms(2);
    send_command(DATA_START_TRANSMISSION_2);
    delay_ms(2);
    for (int i = 0; i < width / 8 * height; i++) {
        send_data(0xFF);
    }
    delay_ms(2);
}

/**
 * @brief: This displays the frame data from SRAM
 */
void refresh_display(void)
{
    send_command(DISPLAY_REFRESH);
    delay_ms(100);
    wait_untile_idle();
}

/**
 * @brief: After this command is transmitted, the chip would enter the deep-sleep mode to save power. 
 *         The deep sleep mode would return to standby by hardware reset. The only one parameter is a 
 *         check code, the command would be executed if check code = 0xA5. 
 *         You can use reset() to awaken and use Init() to initialize.
 */
void epd4in2_sleep()
{
    send_command(VCOM_AND_DATA_INTERVAL_SETTING);
    send_data(0xF7); // border floating
    send_command(POWER_OFF);
    wait_untile_idle();
    send_command(DEEP_SLEEP);
    send_data(0xA5); // check code
}

/* END OF FILE */
