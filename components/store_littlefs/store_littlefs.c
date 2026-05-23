#include "store_littlefs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "esp_littlefs.h"

static const char *TAG = "store_littlefs";

esp_err_t store_littlefs_init(void)
{
    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, STORE_LITTLEFS_PART_LABEL);
    if (part == NULL) {
        ESP_LOGE(TAG, "partition '%s' not found", STORE_LITTLEFS_PART_LABEL);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "partition '%s' @0x%x size %u", part->label, (unsigned)part->address, (unsigned)part->size);

    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORE_LITTLEFS_MOUNT,
        .partition_label = STORE_LITTLEFS_PART_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "littlefs register failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info(conf.partition_label, &total, &used);
    ESP_LOGI(TAG, "LittleFS mounted at %s (%zu / %zu bytes used)", STORE_LITTLEFS_MOUNT, used, total);
    return ESP_OK;
}
