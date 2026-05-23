#include "usb_msc_mode.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "tusb.h"
#include <stdlib.h>

static const char *TAG = "usb_msc";

static tinyusb_msc_storage_handle_t s_storage;

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_MSC_OUT = 0x01,
    EDPT_MSC_IN = 0x81,
};

static tusb_desc_device_t s_dev = {
    .bLength = sizeof(s_dev),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4003,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static uint8_t const s_fs_cfg[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

static char const *s_str[] = {
    (const char[]){0x09, 0x04},
    "Espressif",
    "ESP32-S3 CAM Storage",
    "001",
};

static esp_err_t sd_init(sdmmc_card_t **out_card)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = CONFIG_APP_SDMMC_PIN_CLK;
    slot.cmd = CONFIG_APP_SDMMC_PIN_CMD;
    slot.d0 = CONFIG_APP_SDMMC_PIN_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    sdmmc_card_t *card = (sdmmc_card_t *)calloc(1, sizeof(sdmmc_card_t));
    ESP_RETURN_ON_FALSE(card, ESP_ERR_NO_MEM, TAG, "card");

    ESP_RETURN_ON_ERROR((*host.init)(), TAG, "host.init");
    ESP_RETURN_ON_ERROR(sdmmc_host_init_slot(host.slot, &slot), TAG, "init_slot");

    while (sdmmc_card_init(&host, card) != ESP_OK) {
        ESP_LOGW(TAG, "Insert SD card…");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    sdmmc_card_print_info(stdout, card);
    *out_card = card;
    return ESP_OK;
}

void usb_msc_mode_run_blocking(void)
{
    sdmmc_card_t *card = NULL;
    ESP_ERROR_CHECK(sd_init(&card));

    tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .fat_fs =
            {
                .base_path = NULL,
                .config.max_files = 5,
                .format_flags = 0,
            },
        .medium.card = card,
    };
    ESP_ERROR_CHECK(tinyusb_msc_new_storage_sdmmc(&storage_cfg, &s_storage));

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &s_dev;
    tusb_cfg.descriptor.full_speed_config = s_fs_cfg;
    tusb_cfg.descriptor.string = s_str;
    tusb_cfg.descriptor.string_count = sizeof(s_str) / sizeof(s_str[0]);

    ESP_LOGI(TAG, "TinyUSB MSC install…");
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGW(TAG, "Waiting for PC — use USB-OTG data port (S3: DM=GPIO19, DP=GPIO20),");
    ESP_LOGW(TAG, "not the UART/serial USB port you use for idf.py monitor.");

    bool seen_usb = false;
    int wait_log = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!seen_usb && ++wait_log >= 50) {
            wait_log = 0;
            ESP_LOGW(TAG, "Still waiting for USB host (tud_mounted=0)…");
        }
        if (tud_mounted()) {
            ESP_LOGI(TAG, "USB host mounted MSC — drive should appear on PC");
            seen_usb = true;
        } else if (seen_usb) {
            vTaskDelay(pdMS_TO_TICKS(500));
            if (!tud_mounted()) {
                break;
            }
        }
    }

    ESP_LOGI(TAG, "USB disconnected — restarting");
    tinyusb_msc_delete_storage(s_storage);
    s_storage = NULL;
    tinyusb_driver_uninstall();
    esp_restart();
}
