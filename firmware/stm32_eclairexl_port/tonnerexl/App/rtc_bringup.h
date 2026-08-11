/*
 * rtc_bringup.h — internal STM32 RTC on LSE (32.768kHz crystal) with VBAT
 * battery backup.
 *
 * KEY IDEA: the RTC + LSE live in the F407 backup domain, kept alive by the
 * VBAT coin cell across a main-power cycle. So on every boot AFTER the first,
 * the crystal is already running and the RTC is already ticking — we just read
 * it, with NO slow LSE start-up wait. The one-time cold init (start LSE, wait
 * for LSERDY, configure the RTC) happens in a background thread, never on the
 * boot path, and only when a backup-register sentinel shows the RTC is not yet
 * configured.
 *
 * Board-only (touches HAL_RTC / RCC backup domain).
 */
#ifndef RTC_BRINGUP_H
#define RTC_BRINGUP_H

#include <stdint.h>

typedef enum {
    RTC_STATE_UNKNOWN = 0,
    RTC_STATE_RUNNING,     /* already ticking (VBAT-backed from a prior boot) */
    RTC_STATE_COLD_INIT,   /* was cold, we started LSE + configured it now */
    RTC_STATE_UNAVAILABLE, /* LSE didn't come up — no RTC this boot */
} rtc_state_t;

typedef struct {
    uint16_t year;   /* full year, e.g. 2026 */
    uint8_t  month;  /* 1-12 */
    uint8_t  day;    /* 1-31 */
    uint8_t  hour;   /* 0-23 */
    uint8_t  minute; /* 0-59 */
    uint8_t  second; /* 0-59 */
} rtc_datetime_t;

/* Runs in a thread, NOT on the boot path. If the RTC is already running on VBAT
 * it returns almost immediately (no crystal wait); only a cold start pays the
 * LSE settle time here, off the critical path. Returns the resulting state. */
rtc_state_t rtc_bringup(void);

/* Current RTC state (valid after rtc_bringup has run). */
rtc_state_t rtc_get_state(void);

/* True if the RTC is readable this boot (RUNNING or COLD_INIT). */
int rtc_available(void);

/* Read/set wall-clock time. Return 0 on success, non-zero if RTC unavailable.
 * rtc_set_time also refreshes the backup sentinel. */
int rtc_get_time(rtc_datetime_t *out);
int rtc_set_time(const rtc_datetime_t *in);

#endif /* RTC_BRINGUP_H */
