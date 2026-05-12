#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Registers URI handlers on @p server for a simple camera check:
 *  GET /      — HTML viewer
 *  GET /stream — MJPEG (multipart JPEG)
 */
esp_err_t web_stream_register(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
