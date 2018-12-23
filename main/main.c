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

struct WeatherMessage {
    char id;
    char name[20];
    double temperature;
    int pressure;
    int humidity;
    char main[20];
    char description[30];
    char icon[5];
    double wind_speed;
    int wind_deg;
    int clouds;
};

QueueHandle_t msgQueue;

TaskHandle_t get_current_weather_task_handler;
TaskHandle_t ntp_task_handler;

/* The project use simple WiFi configuration that you can set via 'make menuconfig'.*/

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

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

static void
obtain_time(void);
static void initialize_sntp(void);
static void initialise_wifi(void);
static esp_err_t event_handler(void* ctx, system_event_t* event);

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "api.openweathermap.org"
#define WEB_PORT "443"
#define WEB_URL "https://api.openweathermap.org/data/2.5/weather?id=" CONFIG_ESP_OPEN_WEATHER_MAP_CITY_ID "&appid=" CONFIG_ESP_OPEN_WEATHER_MAP_API_KEY "&lang=" CONFIG_ESP_OPEN_WEATHER_MAP_API_LANG ""
//#define WEB_URL "https://api.openweathermap.org/data/2.5/forecast?id=" CONFIG_ESP_OPEN_WEATHER_MAP_CITY_ID "&appid=" CONFIG_ESP_OPEN_WEATHER_MAP_API_KEY ""

static const char* REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
                             "Host: " WEB_SERVER "\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";

/* Root cert for howsmyssl.com, taken from server_root_cert.pem
   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect api.openweathermap.com:443 </dev/null
   The CA root cert is the last cert given in the chain of certs.
   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");

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

static void initialize_sntp(void)
{
    static const char* TAG = "initialize_sntp";
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void parse_current_weather_json(char* data_ptr, struct WeatherMessage* msg)
{
    static const char* TAG = "parse_current_weather_json";
    cJSON* json;

    json = cJSON_Parse(data_ptr);

    if (json == NULL) {
        ESP_LOGE(TAG, "Error in cJSON_Parse: [%s]\n", cJSON_GetErrorPtr());
    }

    cJSON* json_name = cJSON_GetObjectItemCaseSensitive(json, "name");
    if (cJSON_IsString(json_name) && (json_name->valuestring != NULL)) {
        sprintf((*msg).name, "%s", json_name->valuestring);
    }

    cJSON* json_main = cJSON_GetObjectItemCaseSensitive(json, "main");
    cJSON* json_main_temp = cJSON_GetObjectItemCaseSensitive(json_main, "temp");
    cJSON* json_main_pressure = cJSON_GetObjectItemCaseSensitive(json_main, "pressure");
    cJSON* json_main_humidity = cJSON_GetObjectItemCaseSensitive(json_main, "humidity");

    if (cJSON_IsNumber(json_main_temp)) {
        (*msg).temperature = json_main_temp->valuedouble - (double)273.15;
    }

    if (cJSON_IsNumber(json_main_pressure)) {
        msg->pressure = json_main_pressure->valueint;
    }

    if (cJSON_IsNumber(json_main_humidity)) {
        msg->humidity = json_main_humidity->valueint;
    }

    cJSON* json_weather = cJSON_GetObjectItemCaseSensitive(json, "weather");
    cJSON* tmp = NULL;
    cJSON_ArrayForEach(tmp, json_weather)
    {
        cJSON* json_weather_main = cJSON_GetObjectItemCaseSensitive(tmp, "main");
        cJSON* json_weather_description = cJSON_GetObjectItemCaseSensitive(tmp, "description");
        cJSON* json_weather_icon = cJSON_GetObjectItemCaseSensitive(tmp, "icon");

        if (cJSON_IsString(json_weather_main) && (json_weather_main->valuestring != NULL)) {
            sprintf(msg->main, "%s", json_weather_main->valuestring);
        }

        if (cJSON_IsString(json_weather_description) && (json_weather_description->valuestring != NULL)) {
            sprintf(msg->description, "%s", json_weather_description->valuestring);
        }

        if (cJSON_IsString(json_weather_icon) && (json_weather_icon->valuestring != NULL)) {
            sprintf(msg->icon, "%s", json_weather_icon->valuestring);
        }
    }

    cJSON* json_wind = cJSON_GetObjectItemCaseSensitive(json, "wind");
    cJSON* json_main_speed = cJSON_GetObjectItemCaseSensitive(json_wind, "speed");
    cJSON* json_main_deg = cJSON_GetObjectItemCaseSensitive(json_wind, "deg");

    if (cJSON_IsNumber(json_main_speed)) {

        msg->wind_speed = json_main_speed->valuedouble;
    }

    if (cJSON_IsNumber(json_main_deg)) {
        msg->wind_deg = json_main_deg->valuedouble;
    }

    cJSON* json_clouds = cJSON_GetObjectItemCaseSensitive(json, "clouds");
    cJSON* json_clouds_all = cJSON_GetObjectItemCaseSensitive(json_clouds, "all");

    if (cJSON_IsNumber(json_clouds_all)) {
        msg->clouds = json_clouds_all->valuedouble;
    }

    msg->id = 0;
}

static void get_current_weather_task(void* pvParameters)
{
    static const char* TAG = "get_current_weather_task";
    char buf[1024];
    struct WeatherMessage* msg = malloc(sizeof(struct WeatherMessage*));

    int ret, len;

    /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
    esp_tls_cfg_t cfg = {
        .cacert_pem_buf = server_root_cert_pem_start,
        .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };

    struct esp_tls* tls = esp_tls_conn_http_new(WEB_URL, &cfg);

    if (tls != NULL) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        goto exit;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
            REQUEST + written_bytes,
            strlen(REQUEST) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
            goto exit;
        }
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");

    int offset = 0;

    do {
        char tmp_buf[64];

        len = sizeof(tmp_buf) - 1;
        bzero(tmp_buf, sizeof(tmp_buf));
        ret = esp_tls_conn_read(tls, (char*)tmp_buf, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ)
            continue;

        if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
            break;
        }

        if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGD(TAG, "%d bytes read", len);

        for (int i = 0; i < len; i++) {
            //putchar();
            buf[i + offset] = tmp_buf[i];
        }
        offset += len;

    } while (1);

    buf[offset] = 0;

exit:
    esp_tls_conn_delete(tls);

    // Data can be found after the HTTP header, get te offset to get the data
    char* data_offset = strstr(buf, "\r\n\r\n");

    parse_current_weather_json(data_offset, msg);

    ESP_LOGI(TAG, "Sending msg in msgQueue!");
    xQueueSend(msgQueue, (void*)&msg, portMAX_DELAY);

    vTaskDelete(NULL);
}

static void update_display_task(void* pvParameters)
{
    static const char* TAG = "update_display_task";
    struct WeatherMessage* msg;

    // Receive a message on the queue
    if (xQueueReceive(msgQueue, &(msg), portMAX_DELAY)) {
        ESP_LOGI(TAG, "Message received in msgQueue!");

        ESP_LOGI(TAG, "\nPlace: %s,\nTemperature: %0.2f,\nPressure: %d,\nHumidity: %d,\nMain: %s,\nDescription: %s,\nIcon: %s,\nWind speed: %0.2f,\nWind deg: %d,\nClouds: %d\n", msg->name, msg->temperature, msg->pressure, msg->humidity, msg->main, msg->description, msg->icon, msg->wind_speed, msg->wind_deg, msg->clouds);
    }

    vTaskDelete(NULL);
}

static void ntp_task(void* pvParameters)
{
    static const char* TAG = "ntp_task";

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

    msgQueue = xQueueCreate(1, sizeof(struct WeatherMessage*));

    if (msgQueue == NULL) {
        ESP_LOGE(TAG, "msgQueue was not created and must not be used.");
    } else {
        xTaskCreate(&get_current_weather_task, "get_current_weather_task", 8192, NULL, 5, &get_current_weather_task_handler);
        xTaskCreate(&update_display_task, "update_display_task", 8192, NULL, 5, NULL);
    }

    xTaskCreate(&ntp_task, "ntp_task", 2048, NULL, 5, &ntp_task_handler);
    vTaskDelay(20000 / portTICK_PERIOD_MS);

    deinitialize_wifi();

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    const int deep_sleep_sec = update_interval_seconds - ((timeinfo.tm_sec + (timeinfo.tm_min * 60) + (timeinfo.tm_hour * 60 * 60)) % update_interval_seconds);
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
