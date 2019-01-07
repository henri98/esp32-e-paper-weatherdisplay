
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
#include "Ubuntu24.c"
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

extern Forecast forecasts[8];
extern char summary[50];
extern char icon[20];
extern double temperature;
extern double humidity;
extern int pressure;
extern double wind_speed;
extern double wind_bearing;
extern double precip_probability;

esp_err_t event_handler(void* ctx, system_event_t* event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        ESP_ERROR_CHECK(esp_wifi_connect());
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
            .bssid_set = false,
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_OPEN,
        }
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
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
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

static void update_display_task(void* pvParameters)
{
    static const char* TAG = "update_display_task";

    time_t now;
    struct tm timeinfo;

    char tmp_buff[30];

    if (epd4in2bInit() != 0) {
        ESP_LOGE(TAG, "e-Paper init failed");
        vTaskDelay(2000 / portTICK_RATE_MS);
        return;
    }
    ESP_LOGE(TAG, "e-Paper initialized");

    ClearFrame();

    unsigned char* frame_black = (unsigned char*)malloc(400 * 300 / 8);

    if (frame_black == NULL) {
        ESP_LOGE(TAG, "error");
    }

    paint(frame_black, 400, 300);

    clear(UNCOLORED);

    // Current weather
    const tImage* image = NULL;

    if (strcmp(icon, "clear-day") == 0) {
        image = &widaysunny;
    } else if (strcmp(icon, "clear-night") == 0) {
        image = &winightclear;
    } else if (strcmp(icon, "rain") == 0) {
        image = &wirain;
    } else if (strcmp(icon, "snow") == 0) {
        image = &wisnow;
    } else if (strcmp(icon, "sleet") == 0) {
        image = &wisleet;
    } else if (strcmp(icon, "wind") == 0) {
        image = &wistrongwind;
    } else if (strcmp(icon, "fog") == 0) {
        image = &wifog;
    } else if (strcmp(icon, "cloudy") == 0) {
        image = &wicloudy;
    } else if (strcmp(icon, "partly-cloudy-day") == 0) {
        image = &widaycloudy;
    } else if (strcmp(icon, "partly-cloudy-night") == 0) {
        image = &winightaltcloudy;
    }

    if (image != NULL) {
        draw_bitmap_mono_in_center(2, 0, 500, 40, image);
    }

    sprintf(tmp_buff, "%0.1f º", temperature);
    draw_string_in_center(3, 0, 400, 45, tmp_buff, &Ubuntu24);

    draw_string_in_center(2, 1, 400, 65, summary, &Ubuntu12);

    sprintf(tmp_buff, "Humidity: %d%%", (int)(humidity * 100));
    draw_string_in_center(2, 1, 400, 85, tmp_buff, &Ubuntu12);

    sprintf(tmp_buff, "Pressure:%d hPa", pressure);
    draw_string_in_center(2, 1, 400, 105, tmp_buff, &Ubuntu12);

    sprintf(tmp_buff, "Wind :%d m/s (%s)", (int)round(wind_speed), deg_to_compass(wind_bearing));
    draw_string_in_center(2, 1, 400, 125, tmp_buff, &Ubuntu12);

    sprintf(tmp_buff, "Chance of Precipitation : %d%%", (int)round(precip_probability * 100));
    draw_string_in_center(2, 1, 400, 145, tmp_buff, &Ubuntu12);

    for (size_t i = 0; i < (sizeof(forecasts) / sizeof(Forecast)); i++) {
        struct tm timeinfo;
        setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0", 1);
        tzset();
        localtime_r(&forecasts[i].time, &timeinfo);
        char day[20];
        char date[20];
        strftime(date, sizeof(date), "%d - %m", &timeinfo);
        strftime(day, sizeof(date), "%A", &timeinfo);

        if (i == 0) {
            sprintf(day, "Today");
        }

        if (i == 1) {
            sprintf(day, "Tomorrow");
        }

        draw_string_in_center(7, i, 400, 210, day, &Ubuntu10);

        draw_string_in_center(7, i, 400, 225, date, &Ubuntu10);

        sprintf(tmp_buff, "%d - %d º", (int)round(forecasts[i].temperatureMin), (int)round(forecasts[i].temperatureMax));
        draw_string_in_center(7, i, 400, 240, tmp_buff, &Ubuntu10);

        const tImage* forecast_image = NULL;

        if (strcmp(forecasts[i].icon, "clear-day") == 0) {
            forecast_image = &daysunny;
        } else if (strcmp(forecasts[i].icon, "clear-night") == 0) {
            forecast_image = &nightclear;
        } else if (strcmp(forecasts[i].icon, "rain") == 0) {
            forecast_image = &rain;
        } else if (strcmp(forecasts[i].icon, "snow") == 0) {
            forecast_image = &snow;
        } else if (strcmp(forecasts[i].icon, "sleet") == 0) {
            forecast_image = &sleet;
        } else if (strcmp(forecasts[i].icon, "wind") == 0) {
            forecast_image = &strongwind;
        } else if (strcmp(forecasts[i].icon, "fog") == 0) {
            forecast_image = &fog;
        } else if (strcmp(forecasts[i].icon, "cloudy") == 0) {
            forecast_image = &cloudy;
        } else if (strcmp(forecasts[i].icon, "partly-cloudy-day") == 0) {
            forecast_image = &daycloudy;
        } else if (strcmp(forecasts[i].icon, "partly-cloudy-night") == 0) {
            forecast_image = &nightaltcloudy;
        }

        if (forecast_image != NULL) {
            draw_bitmap_mono_in_center(7, i, 400, 255, forecast_image);
        }
    }

    draw_string("Garderen, The Netherlands", 240, 0, &Ubuntu12);

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
    draw_horizontal_line(0, 200, 400, COLORED);
    draw_horizontal_line(0, 0, 400, COLORED);
    draw_vertical_line(0, 0, 300, COLORED);
    draw_horizontal_line(0, 299, 400, COLORED);
    draw_vertical_line(399, 0, 300, COLORED);

    for (size_t i = 1; i < 7; i++) {
        draw_vertical_line((400 / 7 * i), 200, 138, COLORED);
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

    xTaskCreate(&get_current_weather_task, "get_current_weather_task", 1024 * 14, &forecasts, 5, &get_current_weather_task_handler);
    xTaskCreate(&update_time_using_ntp_task, "update_time_using_ntp_task", 2048, NULL, 5, &update_time_using_ntp_task_handler);

    initialise_wifi();

    vTaskDelay(10000 / portTICK_PERIOD_MS);

    xTaskCreate(&update_display_task, "update_display_task", 8192, NULL, 5, NULL);

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    // deinitialize_wifi();

    // Sleep();

    // time_t now;
    // struct tm timeinfo;

    // time(&now);
    // localtime_r(&now, &timeinfo);

    // const int deep_sleep_sec = update_interval_seconds - ((timeinfo.tm_sec + (timeinfo.tm_min * 60) + (timeinfo.tm_hour * 60 * 60)) % update_interval_seconds);
    // ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    // esp_deep_sleep(1000000LL * deep_sleep_sec);
}
