/*
 * platform_host.c — host (Linux) implementation of the platform seam.
 * No si5351; records the last requested video mode for tests. Log sink = stdout.
 * Uses the same validity grid as the board so the port's mode logic is testable.
 */
#include "platform.h"
#include <stdio.h>

#if !defined(FPGA_BUS_STM32)   /* host build */

static platform_video_standard_t s_last_standard;
static platform_video_refresh_t  s_last_refresh;
static int s_last_supported;

/* Same validity grid as the board (see platform_stm32.c resolve_mode). */
int platform_video_supported(platform_video_standard_t s, platform_video_refresh_t r) {
    switch (s) {
    case STANDARD_ORIGINAL: return (r == REFRESH_PAL_50 || r == REFRESH_NTSC_60);
    case STANDARD_ED:       return (r < REFRESH_COUNT);   /* all three valid */
    case STANDARD_HD720:    return (r < REFRESH_COUNT);   /* all three valid */
    default:                return 0;
    }
}

int platform_set_video(platform_video_standard_t s, platform_video_refresh_t r) {
    if (!platform_video_supported(s, r)) return -1;
    s_last_standard = s; s_last_refresh = r; s_last_supported = 1;
    return 0;
}

int platform_init(void) { return 0; }
void platform_log_putc(char c) { fputc(c, stdout); }

/* test introspection */
platform_video_standard_t host_platform_last_standard(void) { return s_last_standard; }
platform_video_refresh_t  host_platform_last_refresh(void)  { return s_last_refresh; }

#endif
