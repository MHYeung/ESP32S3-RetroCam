idf_component_register(
    SRCS "web_gallery.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server storage_sd json esp_rom
)
