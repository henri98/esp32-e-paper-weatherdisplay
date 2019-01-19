#include "ota.h"

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

#include "lwip/apps/sntp.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "esp_tls.h"

#include "esp_http_ota.h"

static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
{
    static const char* TAG = "_http_event_handler";
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void init_ota_button(void)
{
    gpio_config_t io_conf;
    // disable interruptrs
    io_conf.intr_type = GPIO_INTR_DISABLE;

    // bit mask of the pins, use GPIO4 here
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_4);

    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;

    // enable pull-up mode
    io_conf.pull_up_en = 1;

    // disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
}

uint8_t check_if_ota_button_pressed(void)
{
    return (!gpio_get_level(GPIO_NUM_4));
}

void ota_task(void* pvParameter)
{
    static const char* TAG = "simple_ota_example_task";
    ESP_LOGI(TAG, "Starting OTA example...");

    esp_http_client_config_t config = {
        .url = CONFIG_OTA_URL,
        .event_handler = _http_event_handler,
    };

    esp_err_t ret = esp_http_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware Upgrades Failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}