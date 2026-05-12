#include "web_stream.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "web_stream";

static esp_err_t root_get(httpd_req_t *req)
{
    static const char html[] =
        "<!DOCTYPE html><html><head><meta charset=utf-8><title>ESP32-S3 CAM</title></head>"
        "<body><h1>Live view</h1>"
        "<img src=\"/stream\" style=\"max-width:100%;height:auto;\" />"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_get(httpd_req_t *req)
{
    esp_err_t res = ESP_OK;
    char hdr[128];
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (res == ESP_OK) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "fb_get failed");
            res = ESP_FAIL;
            break;
        }
        int hlen = snprintf(hdr, sizeof(hdr),
                            "\r\n--frame\r\n"
                            "Content-Type: image/jpeg\r\n"
                            "Content-Length: %zu\r\n\r\n",
                            fb->len);
        if (hlen <= 0 || hlen >= (int)sizeof(hdr)) {
            esp_camera_fb_return(fb);
            res = ESP_FAIL;
            break;
        }
        res = httpd_resp_send_chunk(req, hdr, (size_t)hlen);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);
        if (res != ESP_OK) {
            break;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    return res;
}

esp_err_t web_stream_register(httpd_handle_t server)
{
    httpd_uri_t u_root = {.uri = "/", .method = HTTP_GET, .handler = root_get, .user_ctx = NULL};
    httpd_uri_t u_stream = {.uri = "/stream", .method = HTTP_GET, .handler = stream_get, .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u_root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u_stream));
    ESP_LOGI(TAG, "HTTP: GET / and GET /stream registered");
    return ESP_OK;
}
