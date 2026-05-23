#pragma once

#include "esp_err.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Mount FAT on SD (1-bit SDMMC). Call after NVS; safe before Wi-Fi. */
esp_err_t storage_sd_mount(void);

/** Unmount VFS FAT (required before USB MSC takes the card). */
esp_err_t storage_sd_unmount(void);

/** SD access mutex — hold around fopen/fwrite on /sdcard. */
void storage_sd_lock(void);
void storage_sd_unlock(void);

/** Card handle for TinyUSB MSC after unmount (do not use with VFS mounted). */
sdmmc_card_t *storage_sd_get_card(void);

#ifdef __cplusplus
}
#endif
