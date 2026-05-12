/**
 * @file camera_bsp_pins.h
 * Board support: OV2640 parallel bus on ESP32-S3-WROOM style camera PCB.
 *
 * Default: Espressif esp32-camera example BOARD_ESP32S3_WROOM
 * https://github.com/espressif/esp32-camera/blob/master/examples/camera_example/main/camera_pinout.h
 */
#pragma once

#define CAM_BSP_PIN_PWDN   38
#define CAM_BSP_PIN_RESET  -1
#define CAM_BSP_PIN_XCLK   15
#define CAM_BSP_PIN_SIOD    4
#define CAM_BSP_PIN_SIOC    5
#define CAM_BSP_PIN_D7     16
#define CAM_BSP_PIN_D6     17
#define CAM_BSP_PIN_D5     18
#define CAM_BSP_PIN_D4     12
#define CAM_BSP_PIN_D3     10
#define CAM_BSP_PIN_D2      8
#define CAM_BSP_PIN_D1      9
#define CAM_BSP_PIN_D0     11
#define CAM_BSP_PIN_VSYNC   6
#define CAM_BSP_PIN_HREF    7
#define CAM_BSP_PIN_PCLK   13
