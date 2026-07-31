/*
 * platform.h — hardware abstraction seam.
 *
 * The ONE boundary between shared port code and target-specific hardware.
 * app_main() and everything above it is identical on board and host; only the
 * implementation of these functions differs:
 *   - platform_stm32.c : real board (si5351, UART log sink, HAL glue)
 *   - platform_host.c  : Linux host (stubs / stdout)
 *
 * Hardware BRING-UP (byte pools, USBX, FileX init) is NOT here — it stays in
 * each target's natural place (CubeMX's tx_application_define on board; the
 * test harness on host). This interface is only for things app_main() itself
 * needs to call that happen to be target-specific.
 */
#ifndef TONNEREXL_PLATFORM_H
#define TONNEREXL_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called early by app_main(), after hardware bring-up, before threads start.
 * Board: any port-visible board setup not already done by CubeMX (currently
 * none — si5351 is done in tx_application_define). Host: no-op.
 * Returns 0 on success. */
int platform_init(void);

/* Video output configuration. The port speaks in abstract (standard, refresh)
 * pairs; the board maps them to its actual clock generator (si5351) modes and
 * owns the table of which pairs are valid. Not every pair exists (e.g. ORIGINAL
 * has no 59.94 variant) — platform_video_supported() reports validity. Today
 * the board hardcodes the table; later it may read monitor EDID/HDMI caps
 * behind the same interface. */
typedef enum {
    STANDARD_ORIGINAL = 0,  /* authentic Atari clocks (PAL 28.375 / NTSC 28.636, with chroma) */
    STANDARD_ED,            /* enhanced-def progressive: 576p (PAL) / 480p (NTSC), 27.0 MHz */
    STANDARD_HD720,         /* 720p, 74.25 MHz */
    STANDARD_COUNT
} platform_video_standard_t;

typedef enum {
    REFRESH_PAL_50 = 0,     /* 50 Hz    */
    REFRESH_NTSC_60,        /* 60.000 Hz */
    REFRESH_NTSC_5994,      /* 59.940 Hz */
    REFRESH_COUNT
} platform_video_refresh_t;

/* Request a video mode. If the (standard, refresh) pair is unsupported, the
 * board leaves the current mode unchanged and returns non-zero. */
int platform_set_video(platform_video_standard_t standard,
                       platform_video_refresh_t refresh);

/* Query whether a (standard, refresh) pair is currently supported (for a
 * settings menu to grey out invalid combos). Returns 1 if valid, 0 if not. */
int platform_video_supported(platform_video_standard_t standard,
                             platform_video_refresh_t refresh);

/* Log sink: emit one byte to the target's console. The shared logger
 * (logger.c) calls this; it's the ONLY board-specific part of logging.
 * Board: UART/ITM. Host: stdout. */
void platform_log_putc(char c);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_PLATFORM_H */
