/*
 * logger.c — shared logger. Formats to a small stack buffer and emits via
 * platform_log_putc(). No heap, bounded output. Same code on both targets.
 */
#include "logger.h"
#include "platform.h"
#include <stdarg.h>
#include <stdio.h>
#include "tx_api.h"

void logger_init(void) { /* nothing to do; sink is set up by platform */ }

static void log_puts_no_prefix(const char *s) {
    if (!s) return;
    while (*s) platform_log_putc(*s++);
}

static void prefix_time()
{
    ULONG ticks = tx_time_get();
    char buf[32];
    const char *s;
    
    uint32_t ms = (uint32_t)(((uint64_t)ticks * 1000U) / TX_TIMER_TICKS_PER_SECOND);

    uint32_t seconds = ms / 1000U;
    uint32_t millis  = ms % 1000U;

    snprintf(&buf[0], 31, "%ld.%03ld: ", seconds, millis); 
    s = &buf[0];
    log_puts_no_prefix(s);
}

void log_puts(const char *s) {
    prefix_time();
    log_puts_no_prefix(s);
}

void log_printf(const char *fmt, ...) {
    prefix_time();
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;  /* truncated */
    for (int i = 0; i < n; i++) platform_log_putc(buf[i]);
}

