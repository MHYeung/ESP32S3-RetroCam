idf_component_register(
    SRCS "web_stream.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp32-camera
)
