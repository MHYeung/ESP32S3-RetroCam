#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * During the first boot window: count low-active presses with debounce.
 * Returns true if three taps detected → USB MSC path.
 */
bool app_input_boot_triple_tap_msc(void);

/**
 * Register a callback invoked (from the shutter task) when GPIO1 is held for
 * APP_LONG_PRESS_MS.  Call before app_input_start_shutter_task().
 * A long-press does NOT trigger the shutter.
 */
void app_input_set_long_press_cb(void (*cb)(void));

/** Three quick taps while running (not long-press) — e.g. enter USB MSC. */
void app_input_set_triple_tap_cb(void (*cb)(void));

/** Start the background shutter + long-press task. */
void app_input_start_shutter_task(void);

/** Notify the status LED that a photo was just saved (called from shutter task). */
void app_input_notify_shutter_saved(void);

#ifdef __cplusplus
}
#endif
