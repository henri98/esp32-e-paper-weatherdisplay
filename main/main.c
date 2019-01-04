
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

#include "cJSON.h"

#include "darksky.h"

#include "epd4in2b.h"

#include "epdpaint.h"

#include "icons.h"

#include "Ubuntu10.c"
#include "Ubuntu12.c"
#include "Ubuntu14.c"
#include "Ubuntu16.c"
#include "Ubuntu18.c"
#include "Ubuntu20.c"
#include "Ubuntu22.c"
#include "Ubuntu8.c"

#define COLORED 1
#define UNCOLORED 0

QueueHandle_t msgQueue;

TaskHandle_t get_current_weather_task_handler;
TaskHandle_t update_time_using_ntp_task_handler;

/* The project use simple WiFi configuration that you can set via 'make menuconfig'.*/

/* FreeRTOS event group to signal when we are connected & ready to make a CURRENT_WEATHER_REQUEST */
EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Variable holding number of times ESP32 restarted since first boot.
* It is placed into RTC memory using RTC_DATA_ATTR and
* maintains its value when ESP32 wakes from deep sleep.
*/
RTC_DATA_ATTR static int boot_count = 0;

static int update_interval_seconds = 1 * 60 * 60;

Forecast forecasts[8];

esp_err_t event_handler(void* ctx, system_event_t* event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    static const char* TAG = "initialise_wifi";
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .bssid_set = false }
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, CONFIG_ESP_DNS_NAME));
}

static void deinitialize_wifi()
{
    ESP_ERROR_CHECK(esp_wifi_stop());
}

static void initialize_sntp(void)
{
    static const char* TAG = "initialize_sntp";
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    static const char* TAG = "obtain_time";
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

#define forecastcount 5

static void update_display_task(void* pvParameters)
{
    static const char* TAG = "update_display_task";

    time_t now;
    struct tm timeinfo;

    if (epd4in2bInit() != 0) {
        ESP_LOGE(TAG, "e-Paper init failed");
        vTaskDelay(2000 / portTICK_RATE_MS);
        return;
    }
    ESP_LOGE(TAG, "e-Paper initialized");

    ClearFrame();

    //DisplayFrame1(image_data_imgditherlicious, image_data_imgditherlicious);

    unsigned char* frame_black = (unsigned char*)malloc(400 * 300 / 8);

    if (frame_black == NULL) {
        ESP_LOGE(TAG, "error");
    }

    paint(frame_black, 400, 300);

    clear(UNCOLORED);

    Forecast* forecast;

    printf("%-20s%-20s%-60s%-25s%-20s%-20s%-15s%-15s\n", "", "", "Summary", "Icon", "Temperature high", "Temperature low", "Humidity", "Pressure");

    for (size_t i = 0; i < (sizeof(forecasts) / sizeof(Forecast)); i++) {
        struct tm timeinfo;
        setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0", 1);
        tzset();
        localtime_r(&forecasts[i].time, &timeinfo);
        char date[20];
        strftime(date, sizeof(date), "%a %d - %m", &timeinfo);

        char tmp_buff[30];

        if (i == 0) {
            // Today

            int textmaxwidthondisplay = calculate_width("max:", &Ubuntu10);
            draw_string("max:", (400 - 20 - textmaxwidthondisplay), 30, &Ubuntu10);
            sprintf(tmp_buff, "%0.1f ºC", forecasts[i].temperatureHigh);
            int temphighwidthondisplay = calculate_width(tmp_buff, &Ubuntu20);
            draw_string(tmp_buff, (400 - 20 - temphighwidthondisplay), 40, &Ubuntu20);

            draw_string("min:", 20, 30, &Ubuntu10);
            int templowewidthondisplay = calculate_width(tmp_buff, &Ubuntu20);
            sprintf(tmp_buff, "%0.1f ºC", forecasts[i].temperatureLow);
            draw_string(tmp_buff, 20, 40, &Ubuntu20);

            if (strcmp(forecasts[i].icon, "clear-day") == 0) {
                draw_bitmap_mono(120, 0, &widaysunny);
            } else if (strcmp(forecasts[i].icon, "clear-night") == 0) {
                draw_bitmap_mono(120, 10, &winightclear);
            } else if (strcmp(forecasts[i].icon, "rain") == 0) {
                draw_bitmap_mono(120, -10, &wirain);
            } else if (strcmp(forecasts[i].icon, "snow") == 0) {
                draw_bitmap_mono(120, -10, &wisnow);
            } else if (strcmp(forecasts[i].icon, "sleet") == 0) {
                draw_bitmap_mono(120, -10, &wisleet);
            } else if (strcmp(forecasts[i].icon, "wind") == 0) {
                draw_bitmap_mono(120, -10, &wistrongwind);
            } else if (strcmp(forecasts[i].icon, "fog") == 0) {
                draw_bitmap_mono(120, -10, &wifog);
            } else if (strcmp(forecasts[i].icon, "cloudy") == 0) {
                draw_bitmap_mono(120, 10, &wicloudy);
            } else if (strcmp(forecasts[i].icon, "partly-cloudy-day") == 0) {
                draw_bitmap_mono(120, 15, &widaycloudy);
            } else if (strcmp(forecasts[i].icon, "partly-cloudy-night") == 0) {
                draw_bitmap_mono(120, 15, &winightaltcloudy);
            }

            int summarywidthondisplay = calculate_width(forecasts[i].summary, &Ubuntu12);
            draw_string(forecasts[i].summary, (400 - summarywidthondisplay) / 2, 140, &Ubuntu12);

            sprintf(tmp_buff, "Humidity: %d%%", (int)(forecasts[i].humidity * 100));
            int humiditywidthondisplay = calculate_width(tmp_buff, &Ubuntu12);
            draw_string(tmp_buff, (400 - humiditywidthondisplay) / 2, 160, &Ubuntu12);

            sprintf(tmp_buff, "Pressure:%d hPa", forecasts[i].pressure);
            int pressurewidthondisplay = calculate_width(tmp_buff, &Ubuntu12);
            draw_string(tmp_buff, (400 - pressurewidthondisplay) / 2, 180, &Ubuntu12);

        } else {
            sprintf(tmp_buff, "%0.1f - %0.1f ºC", forecasts[i].temperatureLow, forecasts[i].temperatureHigh);

            int forecastswidthondisplay = calculate_width(tmp_buff, &Ubuntu10);
            draw_string(tmp_buff, ((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - forecastswidthondisplay) / 2), 240, &Ubuntu10);

            int datewidthondisplay = calculate_width(date, &Ubuntu10);
            draw_string(date, ((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - datewidthondisplay) / 2), 230, &Ubuntu10);

            if (strcmp(forecasts[i].icon, "clear-day") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &daysunny);
            } else if (strcmp(forecasts[i].icon, "clear-night") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &nightclear);
            } else if (strcmp(forecasts[i].icon, "rain") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &rain);
            } else if (strcmp(forecasts[i].icon, "snow") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &snow);
            } else if (strcmp(forecasts[i].icon, "sleet") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &sleet);
            } else if (strcmp(forecasts[i].icon, "wind") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &strongwind);
            } else if (strcmp(forecasts[i].icon, "fog") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &fog);
            } else if (strcmp(forecasts[i].icon, "cloudy") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &cloudy);
            } else if (strcmp(forecasts[i].icon, "partly-cloudy-day") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &daycloudy);
            } else if (strcmp(forecasts[i].icon, "partly-cloudy-night") == 0) {
                draw_bitmap_mono(((400 / forecastcount)) * (i - 1) + (((400 / forecastcount) - 40) / 2), 255, &nightaltcloudy);
            }
        }

        printf("%-20lu%-20s%-60s%-25s%-20.2f%-20.2f%-15.2f%-15d\n", forecasts[i].time, date, forecasts[i].summary, forecasts[i].icon, forecasts[i].temperatureHigh, forecasts[i].temperatureLow, forecasts[i].humidity, forecasts[i].pressure);
    }

    draw_string("Garderen", 345, 0, &Ubuntu12);

    time(&now);
    char strftime_buf[64];
    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "Last updated: %H:%M", &timeinfo);

    // DrawStringAt(2, 2, strftime_buf, &Font12, COLORED);
    draw_string(strftime_buf, 2, 0, &Ubuntu12);

    draw_horizontal_line(0, 14, 400, COLORED);
    draw_horizontal_line(0, 220, 400, COLORED);
    draw_horizontal_line(0, 0, 400, COLORED);
    draw_vertical_line(0, 0, 300, COLORED);
    draw_horizontal_line(0, 299, 400, COLORED);
    draw_vertical_line(399, 0, 300, COLORED);

    for (size_t i = 1; i < forecastcount; i++) {
        draw_vertical_line((400 / forecastcount * i), 220, 138, COLORED);
    }

    // /* Display the frame buffer */
    DisplayFrame1(NULL, frame_black);

    vTaskDelete(NULL);
}

static void update_time_using_ntp_task(void* pvParameters)
{
    static const char* TAG = "update_time_using_ntp_task";

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (2016 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }

    char strftime_buf[64];
    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Europe/Amsterdam is: %s", strftime_buf);

    vTaskDelete(NULL);
}

void app_main(void)
{
    static const char* TAG = "app_main";

    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi();

    xTaskCreate(&get_current_weather_task, "get_current_weather_task", 1024 * 14, &forecasts, 5, &get_current_weather_task_handler);

    xTaskCreate(&update_time_using_ntp_task, "update_time_using_ntp_task", 2048, NULL, 5, &update_time_using_ntp_task_handler);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    xTaskCreate(&update_display_task, "update_display_task", 8192, NULL, 5, NULL);

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    deinitialize_wifi();

    Sleep();

    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    const int deep_sleep_sec = update_interval_seconds - ((timeinfo.tm_sec + (timeinfo.tm_min * 60) + (timeinfo.tm_hour * 60 * 60)) % update_interval_seconds);
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
