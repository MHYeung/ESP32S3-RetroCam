#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the HTTP server and register the DCIM gallery routes:
 *   GET /              - HTML page listing JPEG files in /sdcard/DCIM
 *   GET /photo?name=X  - Serve one JPEG (no path traversal)
 *
 * Called when SoftAP is toggled on.  Safe to call multiple times (no-op if
 * already running).  Returns ESP_OK and sets *out_server on first call.
 */
esp_err_t web_gallery_start(httpd_handle_t *out_server);

/**
 * Stop the HTTP server started by web_gallery_start.
 * No-op if server was not running.
 */
void web_gallery_stop(httpd_handle_t *server);

#ifdef __cplusplus
}
#endif
