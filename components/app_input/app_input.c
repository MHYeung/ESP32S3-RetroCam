#include "app_input.h"
#include "sdkconfig.h"

#ifndef CONFIG_APP_TRIPLE_TAP_WINDOW_MS
#define CONFIG_APP_TRIPLE_TAP_WINDOW_MS 1200
#endif

#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage_sd.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "app_input";

#define BTN_GPIO        CONFIG_APP_BUTTON_GPIO
#define DEBOUNCE_US     40000
#define LONG_PRESS_US   ((int64_t)CONFIG_APP_LONG_PRESS_MS * 1000)

static int64_t s_last_edge_us;
static int     s_tap_count;

static void (*s_long_press_cb)(void);
static void (*s_triple_tap_cb)(void);

#define TRIPLE_TAP_WINDOW_US ((int64_t)CONFIG_APP_TRIPLE_TAP_WINDOW_MS * 1000)
/* Weak forward declaration satisfied by status_led if linked, otherwise nop */
__attribute__((weak)) void app_input_notify_shutter_saved(void) {}

static bool gpio_pressed(void)
{
    return gpio_get_level(BTN_GPIO) == 0;
}

/* ------------------------------------------------------------------ */
/* Boot-window triple-tap (unchanged)                                   */
/* ------------------------------------------------------------------ */

bool app_input_boot_triple_tap_msc(void)
{
    gpio_reset_pin(BTN_GPIO);
    gpio_set_direction(BTN_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_GPIO, GPIO_PULLUP_ONLY);

    s_tap_count   = 0;
    s_last_edge_us = esp_timer_get_time();
    bool prev = gpio_pressed();
    const int64_t window_us = (int64_t)CONFIG_APP_BOOT_MSC_WINDOW_MS * 1000;

    int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < window_us) {
        bool now = gpio_pressed();
        if (now != prev) {
            int64_t t = esp_timer_get_time();
            if (t - s_last_edge_us > DEBOUNCE_US) {
                if (now && !prev) {
                    s_tap_count++;
                    ESP_LOGI(TAG, "boot tap %d / 3", s_tap_count);
                }
                s_last_edge_us = t;
            }
            prev = now;
        }
        if (s_tap_count >= 3) {
            ESP_LOGW(TAG, "triple-tap: entering USB MSC mode");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "boot window done, taps=%d (normal mode)", s_tap_count);
    return false;
}

/* ------------------------------------------------------------------ */
/* Post-boot shutter + long-press task                                  */
/* ------------------------------------------------------------------ */

void app_input_set_long_press_cb(void (*cb)(void))
{
    s_long_press_cb = cb;
}

void app_input_set_triple_tap_cb(void (*cb)(void))
{
    s_triple_tap_cb = cb;
}

static bool note_short_tap(int64_t release_us)
{
    static int64_t taps[3];

    taps[0] = taps[1];
    taps[1] = taps[2];
    taps[2] = release_us;
    if (taps[0] != 0 && (release_us - taps[0]) <= TRIPLE_TAP_WINDOW_US) {
        taps[0] = taps[1] = taps[2] = 0;
        return true;
    }
    return false;
}

static bool ensure_dcim(void)
{
    struct stat st = {0};
    if (stat("/sdcard/DCIM", &st) == 0) {
        return true; /* already exists */
    }
    if (mkdir("/sdcard/DCIM", 0755) != 0) {
        ESP_LOGE(TAG, "mkdir /sdcard/DCIM failed: %s", strerror(errno));
        return false;
    }
    return true;
}

static void shutter_task(void *arg)
{
    (void)arg;

    /* Track press state separately from the debounce timestamp */
    bool pressed        = false;
    int64_t press_us    = 0;
    bool long_fired     = false;
    int64_t last_edge   = esp_timer_get_time();
    static uint32_t cap_num;   /* 8.3-safe counter: IMG00000.jpg .. IMG99999.jpg */

    while (1) {
        bool now = gpio_pressed();

        if (now && !pressed) {
            /* falling edge — button down */
            int64_t t = esp_timer_get_time();
            if (t - last_edge > DEBOUNCE_US) {
                pressed    = true;
                long_fired = false;
                press_us   = t;
                last_edge  = t;
            }
        } else if (!now && pressed) {
            /* rising edge — button released */
            int64_t t = esp_timer_get_time();
            if (t - last_edge > DEBOUNCE_US) {
                if (!long_fired) {
                    if (note_short_tap(t)) {
                        ESP_LOGW(TAG, "triple-tap: USB MSC");
                        if (s_triple_tap_cb) {
                            s_triple_tap_cb();
                        }
                    } else {
                        storage_sd_lock();
                        camera_fb_t *fb = esp_camera_fb_get();
                        if (fb && fb->format == PIXFORMAT_JPEG && fb->len > 2 &&
                            fb->buf[0] == 0xFF && fb->buf[1] == 0xD8) {
                            if (ensure_dcim()) {
                                char path[32];
                                snprintf(path, sizeof(path),
                                         "/sdcard/DCIM/IMG%05" PRIu32 ".jpg",
                                         cap_num++);
                                FILE *f = fopen(path, "wb");
                                if (f) {
                                    fwrite(fb->buf, 1, fb->len, f);
                                    fclose(f);
                                    ESP_LOGI(TAG, "saved %s (%zu bytes)", path, fb->len);
                                    app_input_notify_shutter_saved();
                                } else {
                                    ESP_LOGE(TAG, "fopen %s failed: %s", path, strerror(errno));
                                }
                            }
                        } else if (!fb) {
                            ESP_LOGW(TAG, "camera_fb_get returned NULL");
                        } else if (fb->format != PIXFORMAT_JPEG) {
                            ESP_LOGW(TAG, "unexpected pixel format %d", fb->format);
                        } else {
                            ESP_LOGW(TAG, "invalid JPEG (NO-SOI?) — not saved");
                        }
                        if (fb) {
                            esp_camera_fb_return(fb);
                        }
                        storage_sd_unlock();
                    }
                }
                pressed   = false;
                last_edge = t;
            }
        } else if (pressed && !long_fired) {
            /* Check for long-press while held */
            if (esp_timer_get_time() - press_us >= LONG_PRESS_US) {
                long_fired = true;
                ESP_LOGI(TAG, "long press: toggling AP");
                if (s_long_press_cb) s_long_press_cb();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_input_start_shutter_task(void)
{
    xTaskCreate(shutter_task, "shutter", 4096, NULL, 5, NULL);
}
