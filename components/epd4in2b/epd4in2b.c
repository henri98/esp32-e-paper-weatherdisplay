#include "epd4in2b.h"
#include <stdlib.h>

gpio_num_t reset_pin;
gpio_num_t dc_pin;
gpio_num_t cs_pin;
gpio_num_t busy_pin;

unsigned int width;
unsigned int height;

int epd4in2bInit(void)
{
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;

    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        return -1;
    }
    /* EPD hardware init start */
    Reset();
    SendCommand(BOOSTER_SOFT_START);
    SendData(0x17);
    SendData(0x17);
    SendData(0x17); //07 0f 17 1f 27 2F 37 2f
    SendCommand(POWER_ON);
    WaitUntilIdle();
    SendCommand(PANEL_SETTING);
    SendData(0x0F); // LUT from OTP
    /* EPD hardware init end */

    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void SendCommand(unsigned char command)
{
    DigitalWrite(dc_pin, 0);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void SendData(unsigned char data)
{
    DigitalWrite(dc_pin, 1);
    SpiTransfer(data);
}

/**
 *  @brief: Wait until the busy_pin goes HIGH
 */
void WaitUntilIdle(void)
{
    while (DigitalRead(busy_pin) == 0) { //0: busy, 1: idle
        DelayMs(100);
    }
}

/**
 *  @brief: module reset. 
 *          often used to awaken the module in deep sleep, 
 *          see Sleep();
 */
void Reset(void)
{
    DigitalWrite(reset_pin, 0);
    DelayMs(200);
    DigitalWrite(reset_pin, 1);
    DelayMs(200);
}

/**
 *  @brief: transmit partial data to the SRAM
 */
void SetPartialWindow(const unsigned char* buffer_black, const unsigned char* buffer_red, int x, int y, int w, int l)
{
    SendCommand(PARTIAL_IN);
    SendCommand(PARTIAL_WINDOW);
    SendData(x >> 8);
    SendData(x & 0xf8); // x should be the multiple of 8, the last 3 bit will always be ignored
    SendData(((x & 0xf8) + w - 1) >> 8);
    SendData(((x & 0xf8) + w - 1) | 0x07);
    SendData(y >> 8);
    SendData(y & 0xff);
    SendData((y + l - 1) >> 8);
    SendData((y + l - 1) & 0xff);
    SendData(0x01); // Gates scan both inside and outside of the partial window. (default)
    DelayMs(2);
    SendCommand(DATA_START_TRANSMISSION_1);
    if (buffer_black != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            SendData(buffer_black[i]);
        }
    }
    DelayMs(2);
    SendCommand(DATA_START_TRANSMISSION_2);
    if (buffer_red != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            SendData(buffer_red[i]);
        }
    }
    DelayMs(2);
    SendCommand(PARTIAL_OUT);
}

/**
 *  @brief: transmit partial data to the black part of SRAM
 */
void SetPartialWindowBlack(const unsigned char* buffer_black, int x, int y, int w, int l)
{
    SendCommand(PARTIAL_IN);
    SendCommand(PARTIAL_WINDOW);
    SendData(x >> 8);
    SendData(x & 0xf8); // x should be the multiple of 8, the last 3 bit will always be ignored
    SendData(((x & 0xf8) + w - 1) >> 8);
    SendData(((x & 0xf8) + w - 1) | 0x07);
    SendData(y >> 8);
    SendData(y & 0xff);
    SendData((y + l - 1) >> 8);
    SendData((y + l - 1) & 0xff);
    SendData(0x01); // Gates scan both inside and outside of the partial window. (default)
    DelayMs(2);
    SendCommand(DATA_START_TRANSMISSION_1);
    if (buffer_black != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            SendData(buffer_black[i]);
        }
    }
    DelayMs(2);
    SendCommand(PARTIAL_OUT);
}

/**
 *  @brief: transmit partial data to the red part of SRAM
 */
void SetPartialWindowRed(const unsigned char* buffer_red, int x, int y, int w, int l)
{
    SendCommand(PARTIAL_IN);
    SendCommand(PARTIAL_WINDOW);
    SendData(x >> 8);
    SendData(x & 0xf8); // x should be the multiple of 8, the last 3 bit will always be ignored
    SendData(((x & 0xf8) + w - 1) >> 8);
    SendData(((x & 0xf8) + w - 1) | 0x07);
    SendData(y >> 8);
    SendData(y & 0xff);
    SendData((y + l - 1) >> 8);
    SendData((y + l - 1) & 0xff);
    SendData(0x01); // Gates scan both inside and outside of the partial window. (default)
    DelayMs(2);
    SendCommand(DATA_START_TRANSMISSION_2);
    if (buffer_red != NULL) {
        for (int i = 0; i < w / 8 * l; i++) {
            SendData(buffer_red[i]);
        }
    }
    DelayMs(2);
    SendCommand(PARTIAL_OUT);
}

/**
 * @brief: refresh and displays the frame
 */
void DisplayFrame1(const unsigned char* frame_black, const unsigned char* frame_red)
{
    if (frame_black != NULL) {
        SendCommand(DATA_START_TRANSMISSION_1);
        DelayMs(2);
        for (int i = 0; i < width / 8 * height; i++) {
            SendData(frame_black[i]);
        }
        DelayMs(2);
    }
    if (frame_red != NULL) {
        SendCommand(DATA_START_TRANSMISSION_2);
        DelayMs(2);
        for (int i = 0; i < width / 8 * height; i++) {
            SendData(frame_red[i]);
        }
        DelayMs(2);
    }
    SendCommand(DISPLAY_REFRESH);
    WaitUntilIdle();
}

/**
 * @brief: clear the frame data from the SRAM, this won't refresh the display
 */
void ClearFrame(void)
{
    SendCommand(DATA_START_TRANSMISSION_1);
    DelayMs(2);
    for (int i = 0; i < width / 8 * height; i++) {
        SendData(0xFF);
    }
    DelayMs(2);
    SendCommand(DATA_START_TRANSMISSION_2);
    DelayMs(2);
    for (int i = 0; i < width / 8 * height; i++) {
        SendData(0xFF);
    }
    DelayMs(2);
}

/**
 * @brief: This displays the frame data from SRAM
 */
void DisplayFrame(void)
{
    SendCommand(DISPLAY_REFRESH);
    DelayMs(100);
    WaitUntilIdle();
}

/**
 * @brief: After this command is transmitted, the chip would enter the deep-sleep mode to save power. 
 *         The deep sleep mode would return to standby by hardware reset. The only one parameter is a 
 *         check code, the command would be executed if check code = 0xA5. 
 *         You can use Reset() to awaken and use Init() to initialize.
 */
void Sleep()
{
    SendCommand(VCOM_AND_DATA_INTERVAL_SETTING);
    SendData(0xF7); // border floating
    SendCommand(POWER_OFF);
    WaitUntilIdle();
    SendCommand(DEEP_SLEEP);
    SendData(0xA5); // check code
}

/* END OF FILE */
