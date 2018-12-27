#ifndef DARKSKY_H
#define DARKSKY_H

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

// #define WEB_SERVER "api.openweathermap.org"
// #define WEB_PORT "443"
// #define CURRENT_WEATHER_WEB_URL "https://api.openweathermap.org/data/2.5/weather?id=" CONFIG_ESP_OPEN_WEATHER_MAP_CITY_ID "&appid=" CONFIG_ESP_OPEN_WEATHER_MAP_API_KEY "&lang=" CONFIG_ESP_OPEN_WEATHER_MAP_API_LANG "&units=metric"
// #define FORECAST_WEATHER_WEB_URL "https://api.openweathermap.org/data/2.5/forecast?id=" CONFIG_ESP_OPEN_WEATHER_MAP_CITY_ID "&appid=" CONFIG_ESP_OPEN_WEATHER_MAP_API_KEY "&lang=" CONFIG_ESP_OPEN_WEATHER_MAP_API_LANG "&units=metric"
#define WEB_SERVER "api.darksky.net"
#define WEB_PORT "443"
#define WEB_URL "https://api.darksky.net/forecast/d1e8cbd61876b713aec0eeed353e0181/52.234361,5.716846?lang=nl&exclude=hourly,flag,currently&units=auto"

static const char* REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
                             "Host: " WEB_SERVER "\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";

/* Root cert for api.openweathermap.org, taken from server_root_cert.pem
   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect api.openweathermap.com:443 </dev/null
   The CA root cert is the last cert given in the chain of certs.
   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");

typedef struct Forecasts {
    time_t time;
    char summary[50];
    char icon[20];
    double temperatureHigh;
    double temperatureLow;
    double humidity;
    int pressure;
} Forecast;

void get_current_weather_task(void* pvParameters);

#endif // DARKSKY_H