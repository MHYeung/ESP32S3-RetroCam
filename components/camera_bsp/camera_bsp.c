#include "camera_bsp.h"
#include "camera_bsp_pins.h"
#include "esp_log.h"

static const char *TAG = "camera_bsp";

void camera_bsp_default_config(camera_config_t *cfg)
{
    *cfg = (camera_config_t){
        .pin_pwdn = CAM_BSP_PIN_PWDN,
        .pin_reset = CAM_BSP_PIN_RESET,
        .pin_xclk = CAM_BSP_PIN_XCLK,
        .pin_sccb_sda = CAM_BSP_PIN_SIOD,
        .pin_sccb_scl = CAM_BSP_PIN_SIOC,
        .pin_d7 = CAM_BSP_PIN_D7,
        .pin_d6 = CAM_BSP_PIN_D6,
        .pin_d5 = CAM_BSP_PIN_D5,
        .pin_d4 = CAM_BSP_PIN_D4,
        .pin_d3 = CAM_BSP_PIN_D3,
        .pin_d2 = CAM_BSP_PIN_D2,
        .pin_d1 = CAM_BSP_PIN_D1,
        .pin_d0 = CAM_BSP_PIN_D0,
        .pin_vsync = CAM_BSP_PIN_VSYNC,
        .pin_href = CAM_BSP_PIN_HREF,
        .pin_pclk = CAM_BSP_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };
}

esp_err_t camera_bsp_init(void)
{
    camera_config_t cfg;
    camera_bsp_default_config(&cfg);
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    }
    return err;
}
