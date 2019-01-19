#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void init_ota_button(void);
uint8_t check_if_ota_button_pressed(void);
void ota_task(void* pvParameter);

#ifdef __cplusplus
}
#endif