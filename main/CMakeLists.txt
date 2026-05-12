idf_component_register(
    SRCS "main.c" "wifi_prov.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_http_server camera_bsp web_stream
)
