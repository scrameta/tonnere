/*
 * platform_stm32.c — board implementation of the platform seam.
 *
 * Built only on-target (-DFPGA_BUS_STM32). Owns the mapping from the port's
 * abstract (standard, refresh) video pairs to the board's si5351 modes, and the
 * table of which pairs are valid. The port never sees SI5351_MODE_* enums.
 *
 * TODAY: the valid-pair table and the si5351 mode ids are hardcoded here.
 * LATER: platform_video_supported() can instead consult monitor EDID/HDMI caps
 * read over I2C — same signature, port unchanged.
 */
#include "platform.h"

#if defined(FPGA_BUS_STM32)

/*
 * Board clock code, provided by the STM32 project (not the port). We use the
 * project's real si5351 mode ids. To keep the port's -Werror posture independent
 * of the board headers' internals, we declare only what we call. The integer
 * values MUST match the project's SI5351_MODE_* enum.
 * TODO(mark): confirm these values match si5351_modes.h, or include that header
 * here and relax warnings for this TU.
 */
extern void board_si5351_apply_mode(int si5351_mode);   /* wraps si5351_apply_mode on the right pca9546 channels */

/* Must match the project's SI5351_MODE_* enum values. */
enum {
    BSI_576_PAL = 0,
    BSI_480_NTSC,           /* 59.94 */
    BSI_480_NTSC_60,
    BSI_720_PAL,
    BSI_720_NTSC,           /* 60 */
    BSI_720_NTSC_5994,
    BSI_PAL_ORIGINAL,
    BSI_NTSC_ORIGINAL,
    BSI_INVALID = -1
};

/* Resolve (standard, refresh) -> board si5351 mode, or BSI_INVALID. This is the
 * validity table AND the mapping in one place. */
static int resolve_mode(platform_video_standard_t s, platform_video_refresh_t r) {
    switch (s) {
    case STANDARD_ORIGINAL:
        if (r == REFRESH_PAL_50)  return BSI_PAL_ORIGINAL;
        if (r == REFRESH_NTSC_60) return BSI_NTSC_ORIGINAL;
        return BSI_INVALID;                 /* no 59.94 original */
    case STANDARD_ED:
        if (r == REFRESH_PAL_50)    return BSI_576_PAL;     /* PAL ED = 576p */
        if (r == REFRESH_NTSC_60)   return BSI_480_NTSC_60; /* NTSC ED = 480p */
        if (r == REFRESH_NTSC_5994) return BSI_480_NTSC;
        return BSI_INVALID;
    case STANDARD_HD720:
        if (r == REFRESH_PAL_50)    return BSI_720_PAL;
        if (r == REFRESH_NTSC_60)   return BSI_720_NTSC;
        if (r == REFRESH_NTSC_5994) return BSI_720_NTSC_5994;
        return BSI_INVALID;
    default:
        return BSI_INVALID;
    }
}

int platform_video_supported(platform_video_standard_t s, platform_video_refresh_t r) {
    return resolve_mode(s, r) != BSI_INVALID;
}

int platform_set_video(platform_video_standard_t s, platform_video_refresh_t r) {
    int mode = resolve_mode(s, r);
    if (mode == BSI_INVALID) return -1;     /* unsupported: leave current mode */
    board_si5351_apply_mode(mode);
    return 0;
}

/* Log sink transport (UART, wired up by Mark). Weak so the port links even if
 * absent; the board overrides it. */
__attribute__((weak)) void board_log_putc(char c) { (void)c; }

int platform_init(void) {
    /* si5351 powered up + initial mode set in tx_application_define. */
    return 0;
}

void platform_log_putc(char c) { board_log_putc(c); }

#endif
