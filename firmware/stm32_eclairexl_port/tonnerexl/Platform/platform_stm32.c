/*
 * platform_stm32.c — board implementation of the platform seam.
 *
 * Built only on-target (-DFPGA_BUS_STM32). The port stays free of si5351
 * specifics: it requests an abstract clock mode, and the BOARD maps that to
 * si5351 in board code. This wrapper just forwards to board-provided hooks.
 *
 * si5351 power-on init + initial mode are done by CubeMX's
 * tx_application_define before app_main runs (as the project already does).
 */
#include "platform.h"

#if defined(FPGA_BUS_STM32)

int platform_init(void) {
    /* si5351 already initialised in tx_application_define; nothing extra. */
    return 0;
}

void platform_set_clock_mode(platform_clock_mode_t mode) {
    board_set_video_clock((int)mode);
}

void platform_log_putc(char c) { board_log_putc(c); }

#endif
