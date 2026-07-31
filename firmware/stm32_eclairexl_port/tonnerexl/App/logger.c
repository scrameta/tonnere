/*
 * logger.c — shared logger. Formats to a small stack buffer and emits via
 * platform_log_putc(). No heap, bounded output. Same code on both targets.
 */
#include "logger.h"
#include "platform.h"
#include <stdarg.h>
#include <stdio.h>

void logger_init(void) { /* nothing to do; sink is set up by platform */ }

void log_puts(const char *s) {
    if (!s) return;
    while (*s) platform_log_putc(*s++);
}

void log_printf(const char *fmt, ...) {
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;  /* truncated */
    for (int i = 0; i < n; i++) platform_log_putc(buf[i]);
}
