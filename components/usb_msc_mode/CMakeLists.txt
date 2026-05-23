idf_component_register(
    SRCS "usb_msc_mode.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_tinyusb fatfs sdmmc esp_driver_sdmmc freertos esp_driver_gpio
)
