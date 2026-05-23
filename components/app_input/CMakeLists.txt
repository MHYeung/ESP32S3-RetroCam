idf_component_register(
    SRCS "app_input.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos esp32-camera storage_sd
)
