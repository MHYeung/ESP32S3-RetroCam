#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage_sd.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "storage_sd";

static sdmmc_card_t *s_card;
static SemaphoreHandle_t s_sd_mutex;
static const char *s_mount_point = "/sdcard";

esp_err_t storage_sd_mount(void)
{
    if (s_sd_mutex == NULL) {
        s_sd_mutex = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_sd_mutex != NULL, ESP_ERR_NO_MEM, TAG, "mutex");
    }

    if (s_card != NULL) {
        ESP_LOGW(TAG, "already mounted");
        return ESP_OK;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = CONFIG_APP_SDMMC_PIN_CLK;
    slot_config.cmd = CONFIG_APP_SDMMC_PIN_CMD;
    slot_config.d0 = CONFIG_APP_SDMMC_PIN_D0;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "SDMMC 1-bit CLK=%d CMD=%d D0=%d", slot_config.clk, slot_config.cmd, slot_config.d0);

    esp_err_t err = esp_vfs_fat_sdmmc_mount(s_mount_point, &host, &slot_config, &mount_config, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(err));
        s_card = NULL;
        return err;
    }

    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "FAT mounted at %s", s_mount_point);
    return ESP_OK;
}

esp_err_t storage_sd_unmount(void)
{
    if (s_card == NULL) {
        return ESP_OK;
    }
    esp_err_t err = esp_vfs_fat_sdcard_unmount(s_mount_point, s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "unmount: %s", esp_err_to_name(err));
    }
    s_card = NULL;
    ESP_LOGI(TAG, "FAT unmounted from %s", s_mount_point);
    return err;
}

void storage_sd_lock(void)
{
    if (s_sd_mutex) {
        xSemaphoreTake(s_sd_mutex, portMAX_DELAY);
    }
}

void storage_sd_unlock(void)
{
    if (s_sd_mutex) {
        xSemaphoreGive(s_sd_mutex);
    }
}

sdmmc_card_t *storage_sd_get_card(void)
{
    return s_card;
}
