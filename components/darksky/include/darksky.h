#ifndef DARKSKY_H
#define DARKSKY_H

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <math.h>
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

#define WEB_SERVER "api.darksky.net"
#define WEB_PORT "443"
#define WEB_URL "https://api.darksky.net/forecast/" CONFIG_DARKSKY_API_KEY "/" CONFIG_LATITUDE "," CONFIG_LONGITUDE "?lang=en&exclude=hourly,flag,minutely&units=auto"
// #define WEB_URL "https://api.darksky.net/forecast/d1e8cbd61876b713aec0eeed353e0181/40.653,-73.968?lang=en&exclude=hourly,flag,minutely&units=auto"

static const char* request = "GET " WEB_URL " HTTP/1.0\r\n"
                             "Host: " WEB_SERVER "\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";

/* Root cert for api.darksky.org, taken from server_root_cert.pem
   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect api.darksky.com:443 </dev/null
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
    double temperatureMax;
    double temperatureMin;
    double humidity;
    int pressure;
} Forecast;

char summary[50];
char icon[20];
double temperature;
double humidity;
int pressure;
double wind_speed;
double wind_bearing;
double precip_probability;

Forecast forecasts[8];

const char* deg_to_compass(int degrees);
void get_current_weather_task(void* pvParameters);

#endif // DARKSKY_H