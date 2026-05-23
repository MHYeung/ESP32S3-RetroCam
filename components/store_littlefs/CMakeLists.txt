idf_component_register(
    SRCS "store_littlefs.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_partition vfs littlefs
)
