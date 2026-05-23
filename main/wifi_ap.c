#include "wifi_ap.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "wifi_ap";

static bool s_running;
static bool s_client;
static bool s_netif_done;   /* esp_netif_init + event loop — must be early */
static bool s_wifi_done;    /* esp_wifi_init — deferred until first AP start */

static void on_ap_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_AP_STACONNECTED) {
            s_client = true;
            ESP_LOGI(TAG, "client connected");
        } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
            s_client = false;
            ESP_LOGI(TAG, "client disconnected");
        }
    }
}

esp_err_t wifi_ap_init_once(void)
{
    if (s_netif_done) {
        return ESP_OK;
    }
    /* Only the network stack and event loop — no radio, no DMA buffers yet. */
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif_init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event_loop");
    esp_netif_create_default_wifi_ap();
    s_netif_done = true;
    return ESP_OK;
}

esp_err_t wifi_ap_start(void)
{
    if (s_running) {
        return ESP_OK;
    }

    /* Lazy: allocate wifi DMA buffers only on first actual AP start. */
    if (!s_wifi_done) {
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&wcfg), TAG, "wifi_init");
        ESP_RETURN_ON_ERROR(
            esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_ap_event, NULL),
            TAG, "ev_reg");
        s_wifi_done = true;
    }

    wifi_config_t cfg = {
        .ap = {
            .channel        = CONFIG_APP_WIFI_AP_CHANNEL,
            .max_connection = CONFIG_APP_WIFI_AP_MAX_CONN,
            .authmode       = WIFI_AUTH_WPA2_PSK,
        },
    };
    strlcpy((char *)cfg.ap.ssid,     CONFIG_APP_WIFI_AP_SSID,     sizeof(cfg.ap.ssid));
    strlcpy((char *)cfg.ap.password, CONFIG_APP_WIFI_AP_PASSWORD, sizeof(cfg.ap.password));
    cfg.ap.ssid_len = (uint8_t)strlen(CONFIG_APP_WIFI_AP_SSID);

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &cfg), TAG, "config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");

    s_running = true;
    s_client  = false;
    ESP_LOGI(TAG, "SoftAP '%s' started -- http://192.168.4.1/", CONFIG_APP_WIFI_AP_SSID);
    return ESP_OK;
}

esp_err_t wifi_ap_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }
    esp_wifi_stop();
    s_running = false;
    s_client  = false;
    ESP_LOGI(TAG, "SoftAP stopped");
    return ESP_OK;
}

bool wifi_ap_client_connected(void)
{
    return s_client;
}

bool wifi_ap_running(void)
{
    return s_running;
}
