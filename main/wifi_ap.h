#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One-time initialisation: netif, default event loop, wifi driver.
 * Radio is NOT started here. Call once from app_main before status_led_init.
 */
esp_err_t wifi_ap_init_once(void);

/**
 * Start SoftAP (SSID/password from Kconfig).
 * Safe to call repeatedly; no-op if already up.
 */
esp_err_t wifi_ap_start(void);

/**
 * Stop SoftAP and free the radio.
 * Safe to call when AP is not running.
 */
esp_err_t wifi_ap_stop(void);

/** True if at least one station is associated. Updated from Wi-Fi events. */
bool wifi_ap_client_connected(void);

/** True if SoftAP is currently started. */
bool wifi_ap_running(void);

#ifdef __cplusplus
}
#endif
