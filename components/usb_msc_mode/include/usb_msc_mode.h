#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Init SD (no VFS), start TinyUSB MSC, block until host disconnects, then restart. */
void usb_msc_mode_run_blocking(void);

#ifdef __cplusplus
}
#endif
