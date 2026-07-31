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

/*
 * Board-provided hooks (implemented in the STM32 project, e.g. alongside the
 * existing si5351 code). The port declares only these abstract entry points;
 * it never sees si5351 mode enums or pca9546 channels.
 *
 * TODO(mark): implement these two in the board project. board_set_video_clock
 * maps the abstract mode to your si5351_apply_mode(...) on the right pca9546
 * channels; board_log_putc emits to your UART/ITM log transport. Declared weak
 * so the port links even before you wire them.
 */
__attribute__((weak)) void board_set_video_clock(int mode) { (void)mode; }
__attribute__((weak)) void board_log_putc(char c)          { (void)c; }

int platform_init(void) {
    /* si5351 already initialised in tx_application_define; nothing extra. */
    return 0;
}

void platform_set_clock_mode(platform_clock_mode_t mode) {
    board_set_video_clock((int)mode);
}

void platform_log_putc(char c) { board_log_putc(c); }

#endif
