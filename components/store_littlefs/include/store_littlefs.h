#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STORE_LITTLEFS_PART_LABEL "storage"
#define STORE_LITTLEFS_MOUNT "/littlefs"

esp_err_t store_littlefs_init(void);

#ifdef __cplusplus
}
#endif
