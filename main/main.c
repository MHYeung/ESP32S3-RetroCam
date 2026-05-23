#include "app_input.h"
#include "camera_bsp.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "status_led.h"
#include "storage_sd.h"
#include "store_littlefs.h"
#include "usb_msc_mode.h"
#include "web_stream.h"
#include "wifi_ap.h"

static const char *TAG = "main";

/* HTTP server handle — NULL when AP is off */
static httpd_handle_t s_server;

static void enter_msc_mode(void)
{
    ESP_LOGW(TAG, "USB MSC — unmounting SD and releasing camera");
    if (wifi_ap_running()) {
        web_gallery_stop(&s_server);
        wifi_ap_stop();
        status_led_set_ap_running(false);
        status_led_set_client_connected(false);
    }
    esp_camera_deinit();
    storage_sd_unmount();
    usb_msc_mode_run_blocking();
}

static void on_long_press(void)
{
    if (wifi_ap_running()) {
        /* AP on → turn it off */
        web_gallery_stop(&s_server);
        wifi_ap_stop();
        status_led_set_ap_running(false);
        status_led_set_client_connected(false);
        ESP_LOGI(TAG, "SoftAP off");
    } else {
        /* AP off → turn it on */
        wifi_ap_start();
        web_gallery_start(&s_server);
        status_led_set_ap_running(true);
        ESP_LOGI(TAG, "SoftAP on — connect to '%s', open http://192.168.4.1/",
                 CONFIG_APP_WIFI_AP_SSID);
    }
}

void app_main(void)
{
    /* NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Boot-window triple-tap before slow subsystems (LittleFS, camera, Wi-Fi). */
    if (app_input_boot_triple_tap_msc()) {
        usb_msc_mode_run_blocking();
    }

    err = store_littlefs_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS init skipped: %s", esp_err_to_name(err));
    }

    /* Camera */
    ESP_ERROR_CHECK(camera_bsp_init());

    /* SD card (FAT) */
    err = storage_sd_mount();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed: %s (captures need card)", esp_err_to_name(err));
    }

    /* Wi-Fi driver — radio stays off until long-press */
    ESP_ERROR_CHECK(wifi_ap_init_once());

    /* WS2812 status LED — starts its task */
    err = status_led_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "status_led init failed: %s", esp_err_to_name(err));
    }

    /* Shutter + long-press task */
    app_input_set_long_press_cb(on_long_press);
    app_input_set_triple_tap_cb(enter_msc_mode);
    app_input_start_shutter_task();

    ESP_LOGI(TAG, "ready — tap: photo | long-press: AP | triple-tap: MSC (also first %d ms after reset)",
             CONFIG_APP_BOOT_MSC_WINDOW_MS);

    while (1) {
        /* Sync AP client-connected state to the LED every second */
        status_led_set_client_connected(wifi_ap_client_connected());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
