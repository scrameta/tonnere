/*
 * logger.h — shared logging for the port.
 *
 * Identical code on board and host. The only target-specific part is the byte
 * sink, provided by platform_log_putc(). Call sites use log_printf() /
 * log_puts(); on-board these match the existing project's logger API so ported
 * code and Mark's existing code share one logger.
 */
#ifndef TONNEREXL_LOGGER_H
#define TONNEREXL_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

void logger_init(void);
void log_puts(const char *s);
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_LOGGER_H */
