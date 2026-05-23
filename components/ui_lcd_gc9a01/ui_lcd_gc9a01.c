#include "ui_lcd_gc9a01.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_camera.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/lcd_types.h"
#include "jpeg_decoder.h"
#include <string.h>

static const char *TAG = "ui_lcd";

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;

#define LCD_SZ 240

static esp_err_t lcd_bl_on(void)
{
#if CONFIG_APP_LCD_PIN_BL >= 0
    gpio_reset_pin(CONFIG_APP_LCD_PIN_BL);
    gpio_set_direction(CONFIG_APP_LCD_PIN_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_APP_LCD_PIN_BL, 1);
#endif
    return ESP_OK;
}

esp_err_t ui_lcd_gc9a01_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = CONFIG_APP_LCD_PIN_SCLK,
        .mosi_io_num = CONFIG_APP_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_SZ * LCD_SZ * sizeof(uint16_t),
    };
    spi_host_device_t host = (spi_host_device_t)CONFIG_APP_LCD_SPI_HOST;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(host, &bus, SPI_DMA_CH_AUTO), TAG, "spi_bus");

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = CONFIG_APP_LCD_PIN_DC,
        .cs_gpio_num = CONFIG_APP_LCD_PIN_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)host, &io_cfg, &s_io), TAG, "panel_io");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_APP_LCD_PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(s_io, &panel_cfg, &s_panel), TAG, "new_panel");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "on");
    ESP_RETURN_ON_ERROR(lcd_bl_on(), TAG, "bl");

    ESP_LOGI(TAG, "GC9A01 init OK (SPI host %d)", CONFIG_APP_LCD_SPI_HOST);
    return ESP_OK;
}

static void crop_center_rgb565(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh)
{
    int ox = (sw - dw) / 2;
    int oy = (sh - dh) / 2;
    if (ox < 0) {
        ox = 0;
    }
    if (oy < 0) {
        oy = 0;
    }
    for (int y = 0; y < dh; y++) {
        memcpy(dst + y * dw, src + (oy + y) * sw + ox, dw * sizeof(uint16_t));
    }
}

static void preview_task(void *arg)
{
    (void)arg;
    const int period_ms = 1000 / CONFIG_APP_LCD_PREVIEW_FPS;
    static uint8_t jpeg_work[8192];

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb || fb->format != PIXFORMAT_JPEG) {
            if (fb) {
                esp_camera_fb_return(fb);
            }
            vTaskDelay(pdMS_TO_TICKS(period_ms));
            continue;
        }

        esp_jpeg_image_cfg_t jcfg = {
            .indata = fb->buf,
            .indata_size = fb->len,
            .out_format = JPEG_IMAGE_FORMAT_RGB565,
            .out_scale = JPEG_IMAGE_SCALE_0,
            .flags.swap_color_bytes = 0,
            .advanced.working_buffer = jpeg_work,
            .advanced.working_buffer_size = sizeof(jpeg_work),
        };
        esp_jpeg_image_output_t info = {};
        if (esp_jpeg_get_image_info(&jcfg, &info) != ESP_OK || info.output_len == 0) {
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(period_ms));
            continue;
        }

        size_t out_bytes = info.output_len;
        uint16_t *rgb = (uint16_t *)heap_caps_malloc(out_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!rgb) {
            rgb = (uint16_t *)heap_caps_malloc(out_bytes, MALLOC_CAP_8BIT);
        }
        if (!rgb) {
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(period_ms));
            continue;
        }

        jcfg.outbuf = (uint8_t *)rgb;
        jcfg.outbuf_size = out_bytes;
        esp_jpeg_image_output_t out = {};
        esp_err_t dec = esp_jpeg_decode(&jcfg, &out);
        esp_camera_fb_return(fb);

        if (dec == ESP_OK && out.width > 0 && out.height > 0) {
            uint16_t *panel_buf =
                (uint16_t *)heap_caps_malloc(LCD_SZ * LCD_SZ * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            if (!panel_buf) {
                panel_buf = (uint16_t *)heap_caps_malloc(LCD_SZ * LCD_SZ * 2, MALLOC_CAP_INTERNAL);
            }
            if (panel_buf) {
                crop_center_rgb565(rgb, out.width, out.height, panel_buf, LCD_SZ, LCD_SZ);
                esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_SZ, LCD_SZ, panel_buf);
                heap_caps_free(panel_buf);
            }
        }
        heap_caps_free(rgb);
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

void ui_lcd_gc9a01_start_preview_task(void)
{
    const uint32_t stack = 8192;
    xTaskCreatePinnedToCore(preview_task, "lcd_prev", stack, NULL, 3, NULL, 1);
}
