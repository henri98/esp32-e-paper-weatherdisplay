/**
 *  @filename   :   epdif.cpp
 *  @brief      :   Implements EPD interface functions
 *                  Users have to implement all the functions in epdif.cpp
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "epdif.h"
#include "esp_log.h"

void digital_write(gpio_num_t pin, int value)
{
    // ESP_LOGI("EPDIF", "Set Pin %i: %i", pin, value);
    gpio_set_level(pin, value);
}

int digital_read(gpio_num_t pin)
{
    int level = gpio_get_level(pin);
    return level;
}

void delay_ms(unsigned int delaytime)
{
    vTaskDelay(delaytime / portTICK_RATE_MS);
}

void spi_transfer(unsigned char data)
{
    esp_err_t ret;
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8, // transaction length is in bits
        .tx_data[0] = data,
        .tx_data[1] = data,
        .tx_data[2] = data,
        .tx_data[3] = data
    };

    ret = spi_device_transmit(spi, &t); //Transmit!
    assert(ret == ESP_OK); //Should have had no issues.
}

int ifinit(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((uint64_t)1 << (uint64_t)DC_PIN) | ((uint64_t)1 << (uint64_t)RST_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_config_t i_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((uint64_t)1 << (uint64_t)BUSY_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&i_conf));

    // gpio_set_direction(DC_PIN, GPIO_MODE_OUTPUT);
    // gpio_set_direction(RST_PIN, GPIO_MODE_OUTPUT);
    // gpio_set_direction(BUSY_PIN, GPIO_MODE_INPUT);

    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = MOSI_PIN,
        .sclk_io_num = CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    //Initialize the SPI bus
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, 0);
    switch (ret) {
    case ESP_ERR_INVALID_ARG:
        ESP_LOGE("EPDIF", "INVALID ARG");
        break;
    case ESP_ERR_INVALID_STATE:
        ESP_LOGE("EPDIF", "INVALID STATE");
        break;
    case ESP_ERR_NO_MEM:
        ESP_LOGE("EPDIF", "INVALID NO MEMORY");
        break;
    case ESP_OK:
        ESP_LOGE("EPDIF", "All OK");
    }
    assert(ret == ESP_OK);

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = CS_PIN,
        .queue_size = 1
    };

    //Attach the EPD to the SPI bus
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    assert(ret == ESP_OK);

    return 0;
}
