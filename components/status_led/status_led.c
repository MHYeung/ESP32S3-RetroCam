#include "status_led.h"
#include "app_input.h"
#include "sdkconfig.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <stdatomic.h>

static const char *TAG = "status_led";

/* ------------------------------------------------------------------ */
/* Shared state (atomic so no mutex needed for single-byte flags)      */
/* ------------------------------------------------------------------ */
static atomic_bool s_ap_running;
static atomic_bool s_client;
static atomic_bool s_shutter_pulse;   /* one-shot; task clears it */

/* ------------------------------------------------------------------ */
/* LED helpers                                                          */
/* ------------------------------------------------------------------ */

static led_strip_handle_t s_strip;

static void set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

/* Dim a colour channel to avoid blinding the user */
#define DIM(x) ((x) >> 2)

/* ------------------------------------------------------------------ */
/* Status task                                                          */
/* ------------------------------------------------------------------ */

static void status_task(void *arg)
{
    (void)arg;

    /* Camera health: probe once every ~2 s */
    bool cam_ok        = false;
    uint32_t probe_cnt = 0;
    uint32_t blink_cnt = 0;

    while (1) {
        /* Shutter pulse takes priority — brief white flash */
        if (atomic_exchange(&s_shutter_pulse, false)) {
            set_rgb(80, 80, 80);
            vTaskDelay(pdMS_TO_TICKS(120));
            /* fall through to re-evaluate state on the next loop */
        }

        bool ap  = atomic_load(&s_ap_running);
        bool cli = atomic_load(&s_client);

        if (ap) {
            /* AP on + client → bright cyan */
            /* AP on, no client → blue */
            if (cli) {
                set_rgb(0, DIM(200), DIM(200));
            } else {
                set_rgb(0, 0, DIM(200));
            }
        } else {
            /* Offline — probe camera every 4 cycles (~2 s) */
            if (++probe_cnt >= 4) {
                probe_cnt = 0;
                camera_fb_t *fb = esp_camera_fb_get();
                if (fb) {
                    cam_ok = (fb->len > 0);
                    esp_camera_fb_return(fb);
                } else {
                    cam_ok = false;
                }
            }

            if (cam_ok) {
                /* Dim green — offline, camera healthy */
                set_rgb(0, DIM(80), 0);
            } else {
                /* Red slow blink — camera fault */
                blink_cnt++;
                if (blink_cnt & 1) {
                    set_rgb(DIM(200), 0, 0);
                } else {
                    led_strip_clear(s_strip);
                    led_strip_refresh(s_strip);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ------------------------------------------------------------------ */
/* Public implementation of app_input weak hook                        */
/* ------------------------------------------------------------------ */

/* Override the weak stub in app_input.c */
void app_input_notify_shutter_saved(void)
{
    status_led_notify_shutter();
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

esp_err_t status_led_init(void)
{
    led_strip_config_t cfg = {
        .strip_gpio_num          = CONFIG_APP_LED_GPIO,
        .max_leds                = 1,
        .led_model               = LED_MODEL_WS2812,
        .color_component_format  = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out        = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = 10 * 1000 * 1000, /* 10 MHz */
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return err;
    }
    led_strip_clear(s_strip);
    led_strip_refresh(s_strip);

    xTaskCreate(status_task, "status_led", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "WS2812 on GPIO%d ready", CONFIG_APP_LED_GPIO);
    return ESP_OK;
}

void status_led_notify_shutter(void)
{
    atomic_store(&s_shutter_pulse, true);
}

void status_led_set_ap_running(bool running)
{
    atomic_store(&s_ap_running, running);
}

void status_led_set_client_connected(bool connected)
{
    atomic_store(&s_client, connected);
}
