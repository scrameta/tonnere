/*
 * platform_host.c — host (Linux) implementation of the platform seam.
 * si5351 is a no-op; log sink is stdout. Built only in the host test build.
 */
#include "platform.h"
#include <stdio.h>

#if !defined(FPGA_BUS_STM32)   /* host build */

static platform_clock_mode_t s_last_mode;

int platform_init(void) { return 0; }

void platform_set_clock_mode(platform_clock_mode_t mode) { s_last_mode = mode; }

void platform_log_putc(char c) { fputc(c, stdout); }

/* test introspection */
platform_clock_mode_t host_platform_last_clock_mode(void) { return s_last_mode; }

#endif
