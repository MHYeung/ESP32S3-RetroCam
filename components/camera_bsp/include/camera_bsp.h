#pragma once

#include "esp_camera.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fills @p cfg with BSP defaults; override fields after the call if needed. */
void camera_bsp_default_config(camera_config_t *cfg);

/** Init camera using BSP defaults (JPEG QVGA, PSRAM framebuffer). */
esp_err_t camera_bsp_init(void);

#ifdef __cplusplus
}
#endif
