idf_component_register(
    SRCS "ui_lcd_gc9a01.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_spi espressif__esp_lcd_gc9a01 esp_jpeg esp32-camera freertos heap
)
