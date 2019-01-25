#include "darksky.h"

extern QueueHandle_t msgQueue;
extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

static const char* TAG = "darksky";

static void https_get(const char* url, const char* request, char* buf)
{
    int ret, len;

    /* Wait for the callback to set the CONNECTED_BIT in the event group. */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
    esp_tls_cfg_t cfg = {
        .cacert_pem_buf = server_root_cert_pem_start,
        .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };

    struct esp_tls* tls = esp_tls_conn_http_new(url, &cfg);

    if (tls != NULL) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        goto exit;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls, request + written_bytes, strlen(request) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
            goto exit;
        }
    } while (written_bytes < strlen(request));

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
            buf[i + offset] = tmp_buf[i];
        }
        offset += len;

    } while (1);

    buf[offset] = 0;

exit:
    esp_tls_conn_delete(tls);
}

const char* deg_to_compass(int degrees)
{
    int val = floor((degrees / 22.5) + 0.5);
    const char* arr[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
    return arr[(val % 16)];
}

void get_current_weather_task(void* pvParameters)
{

    char buf[1024 * 15];

    https_get(WEB_URL, request, buf);

    // Data can be found after the HTTP header, get te offset to get the data
    char* data_offset = strstr(buf, "\r\n\r\n");

    cJSON* json = cJSON_Parse(data_offset);

    if (json == NULL) {
        ESP_LOGE(TAG, "Error in cJSON_Parse: [%s]\n", cJSON_GetErrorPtr());
    }

    // Get the current weather data from the current object
    cJSON* json_currently = cJSON_GetObjectItemCaseSensitive(json, "currently");

    cJSON* json_currently_summary = cJSON_GetObjectItemCaseSensitive(json_currently, "summary");
    if (cJSON_IsString(json_currently_summary) && (json_currently_summary->valuestring != NULL)) {
        sprintf(summary, "%s", json_currently_summary->valuestring);
    }

    cJSON* json_currently_icon = cJSON_GetObjectItemCaseSensitive(json_currently, "icon");
    if (cJSON_IsString(json_currently_icon) && (json_currently_icon->valuestring != NULL)) {
        sprintf(icon, "%s", json_currently_icon->valuestring);
    }

    cJSON* json_currently_temperature = cJSON_GetObjectItemCaseSensitive(json_currently, "temperature");
    if (cJSON_IsNumber(json_currently_temperature)) {
        temperature = json_currently_temperature->valuedouble;
    }

    cJSON* json_currently_humidity = cJSON_GetObjectItemCaseSensitive(json_currently, "humidity");
    if (cJSON_IsNumber(json_currently_humidity)) {
        humidity = json_currently_humidity->valuedouble;
    }

    cJSON* json_currently_pressure = cJSON_GetObjectItemCaseSensitive(json_currently, "pressure");
    if (cJSON_IsNumber(json_currently_pressure)) {
        pressure = json_currently_pressure->valuedouble;
    }

    cJSON* json_currently_wind_speed = cJSON_GetObjectItemCaseSensitive(json_currently, "windSpeed");
    if (cJSON_IsNumber(json_currently_wind_speed)) {
        wind_speed = json_currently_wind_speed->valuedouble;
    }

    cJSON* json_currently_wind_bearing = cJSON_GetObjectItemCaseSensitive(json_currently, "windBearing");
    if (cJSON_IsNumber(json_currently_wind_bearing)) {
        wind_bearing = json_currently_wind_bearing->valuedouble;
    }

    cJSON* json_currently_precip_probability = cJSON_GetObjectItemCaseSensitive(json_currently, "precipProbability");
    if (cJSON_IsNumber(json_currently_precip_probability)) {
        precip_probability = json_currently_precip_probability->valuedouble;
    }

    // Get the forecast weather data from the daily object
    cJSON* json_daily = cJSON_GetObjectItemCaseSensitive(json, "daily");
    cJSON* json_daily_data = cJSON_GetObjectItemCaseSensitive(json_daily, "data");
    cJSON* json_daily_data_x = NULL;
    uint8_t q = 0;
    cJSON_ArrayForEach(json_daily_data_x, json_daily_data)
    {
        cJSON* json_daily_data_x_time = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "time");

        if (cJSON_IsNumber(json_daily_data_x_time)) {
            forecasts[q].time = (long int)json_daily_data_x_time->valueint;
        }

        cJSON* json_daily_data_x_summary = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "summary");

        if (cJSON_IsString(json_daily_data_x_summary) && (json_daily_data_x_summary->valuestring != NULL)) {
            sprintf(forecasts[q].summary, "%s", json_daily_data_x_summary->valuestring);
        }

        cJSON* json_daily_data_x_icon = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "icon");

        if (cJSON_IsString(json_daily_data_x_icon) && (json_daily_data_x_icon->valuestring != NULL)) {
            sprintf(forecasts[q].icon, "%s", json_daily_data_x_icon->valuestring);
        }

        cJSON* json_daily_data_x_temperature_max = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "temperatureMax");
        if (cJSON_IsNumber(json_daily_data_x_temperature_max)) {
            forecasts[q].temperatureMax = json_daily_data_x_temperature_max->valuedouble;
        }

        cJSON* json_daily_data_x_temperature_min = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "temperatureMin");
        if (cJSON_IsNumber(json_daily_data_x_temperature_min)) {
            forecasts[q].temperatureMin = json_daily_data_x_temperature_min->valuedouble;
        }

        cJSON* json_daily_data_x_humidity = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "humidity");
        if (cJSON_IsNumber(json_daily_data_x_humidity)) {
            forecasts[q].humidity = json_daily_data_x_humidity->valuedouble;
        }

        cJSON* json_daily_data_x_pressure = cJSON_GetObjectItemCaseSensitive(json_daily_data_x, "pressure");
        if (cJSON_IsNumber(json_daily_data_x_pressure)) {
            forecasts[q].pressure = json_daily_data_x_pressure->valuedouble;
        }

        q++;
    }

    vTaskDelete(NULL);
}