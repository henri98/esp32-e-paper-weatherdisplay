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

static int update_interval_seconds = 6 * 60 * 60;

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
            .bssid_set = false },
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
    struct tm timeinfo = { 0 };
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
    Forecast* forecast;

    printf("%-20s%-20s%-40s%-25s%-20s%-20s%-15s%-15s\n", "", "", "Summary", "Icon", "Temperature high", "Temperature low", "Humidity", "Pressure");

    for (size_t i = 0; i < (sizeof(forecasts) / sizeof(Forecast)); i++) {
        struct tm timeinfo;
        setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0", 1);
        tzset();
        localtime_r(&forecasts[i].time, &timeinfo);
        char date[20];
        strftime(date, sizeof(date), "%A %d - %m", &timeinfo);
        printf("%-20lu%-20s%-40s%-25s%-20.2f%-20.2f%-15.2f%-15d\n", forecasts[i].time, date, forecasts[i].summary, forecasts[i].icon, forecasts[i].temperatureHigh, forecasts[i].temperatureLow, forecasts[i].humidity, forecasts[i].pressure);
    }

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
    esp_log_level_set("wifi", ESP_LOG_WARN);
    static const char* TAG = "app_main";
    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi();

    xTaskCreate(&get_current_weather_task, "get_current_weather_task", 1024 * 14, &forecasts, 5, &get_current_weather_task_handler);

    xTaskCreate(&update_time_using_ntp_task, "update_time_using_ntp_task", 2048, NULL, 5, &update_time_using_ntp_task_handler);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    xTaskCreate(&update_display_task, "update_display_task", 8192, NULL, 5, NULL);

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    deinitialize_wifi();

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    const int deep_sleep_sec = update_interval_seconds - ((timeinfo.tm_sec + (timeinfo.tm_min * 60) + (timeinfo.tm_hour * 60 * 60)) % update_interval_seconds);
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
