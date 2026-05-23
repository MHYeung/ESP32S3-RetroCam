idf_component_register(
    SRCS "storage_sd.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs sdmmc esp_driver_sdmmc esp_driver_gpio freertos
)
