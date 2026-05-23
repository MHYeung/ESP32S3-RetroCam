#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_lcd_gc9a01_init(void);
void ui_lcd_gc9a01_start_preview_task(void);

#ifdef __cplusplus
}
#endif
