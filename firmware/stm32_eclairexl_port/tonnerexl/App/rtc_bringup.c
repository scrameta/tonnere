/*
 * rtc_bringup.c — see rtc_bringup.h.
 *
 * Boot-cost strategy:
 *   - The RTC + LSE are in the VBAT-backed backup domain. A sentinel value in
 *     backup register BKP0 marks "RTC already configured". On a warm boot the
 *     sentinel is present and LSE is already running, so we do NOTHING but note
 *     the clock is live — no LSE wait, no re-init (re-init would reset the time).
 *   - Only on a cold boot (no sentinel: fresh cell, or backup domain was reset)
 *     do we start LSE, wait for LSERDY *in this thread*, select it as the RTC
 *     source, configure the RTC, seed a default time, and write the sentinel.
 *   - We NEVER call Error_Handler (that hangs the board). On any failure we mark
 *     the RTC unavailable for this boot and return, exactly like the SD "absent
 *     this boot" pattern.
 */
#include "rtc_bringup.h"
#include "main.h"     /* hrtc, HAL */
#include "logger.h"

#if defined(FPGA_BUS_STM32)

extern RTC_HandleTypeDef hrtc;

/* Sentinel: "RTC has been configured and is running on VBAT". Arbitrary magic
 * in BKP0 that survives on the coin cell but not a cold backup-domain reset. */
#define RTC_SENTINEL_REG   RTC_BKP_DR0
#define RTC_SENTINEL_MAGIC 0x32C0FFEEu

/* Bound the cold-start LSE wait so a missing/dead crystal can't wedge the
 * thread forever. ~5s is generous; a healthy 32.768kHz crystal settles in a
 * few hundred ms. This wait is in the RTC THREAD, never on the boot path. */
#ifndef RTC_LSE_TIMEOUT_MS
#define RTC_LSE_TIMEOUT_MS 5000u
#endif

static rtc_state_t s_state = RTC_STATE_UNKNOWN;

/* ---- helpers ------------------------------------------------------------- */

/* Start LSE and wait (in-thread) for it to stabilise. Returns 1 on ready. */
static int lse_start_and_wait(void)
{
    /* Enable LSE via the backup-domain control register. */
    __HAL_RCC_LSE_CONFIG(RCC_LSE_ON);

    uint32_t start = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET) {
        if (HAL_GetTick() - start > RTC_LSE_TIMEOUT_MS) return 0;
        HAL_Delay(5);   /* real sleep — we're in a thread, scheduler is up */
    }
    return 1;
}

/* Select LSE as the RTC clock source and enable the RTC peripheral clock. */
static int select_lse_rtc_clock(void)
{
    RCC_PeriphCLKInitTypeDef p = {0};
    p.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    p.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK) return 0;
    __HAL_RCC_RTC_ENABLE();
    return 1;
}

/* Configure the RTC registers (prediv for 32768Hz, 24h). Does NOT call
 * Error_Handler; returns HAL status via the caller's check. */
static int rtc_configure(void)
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;   /* 128 * 256 = 32768 */
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    return (HAL_RTC_Init(&hrtc) == HAL_OK);
}

/* Parse the compiler's __TIME__ ("HH:MM:SS") and __DATE__ ("Mmm DD YYYY") so a
 * cold clock starts at a real, recognisable time (the build time) rather than a
 * bland 2000-01-01. Good enough to eyeball persistence/progress; the menu can
 * set exact time later. */
static uint8_t month_from_str(const char *m)
{
    static const char *names = "JanFebMarAprMayJunJulAugSepOctNovDec";
    for (int i = 0; i < 12; i++)
        if (m[0]==names[i*3] && m[1]==names[i*3+1] && m[2]==names[i*3+2])
            return (uint8_t)(i + 1);
    return 1;
}

static void seed_default_time(void)
{
    const char *bd = __DATE__;   /* "Mmm DD YYYY" */
    const char *bt = __TIME__;   /* "HH:MM:SS" */

    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    t.Hours   = (uint8_t)((bt[0]-'0')*10 + (bt[1]-'0'));
    t.Minutes = (uint8_t)((bt[3]-'0')*10 + (bt[4]-'0'));
    t.Seconds = (uint8_t)((bt[6]-'0')*10 + (bt[7]-'0'));
    d.Month   = month_from_str(bd);
    d.Date    = (uint8_t)((bd[4]==' '?0:(bd[4]-'0'))*10 + (bd[5]-'0'));
    d.Year    = (uint8_t)(((bd[9]-'0')*10 + (bd[10]-'0')));  /* last two digits */
    d.WeekDay = RTC_WEEKDAY_MONDAY;   /* not computed; not load-bearing */
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
}

/* ---- public API ---------------------------------------------------------- */

/* Read and log the current RTC time — call on every boot so persistence
 * (survives power cycle) and progress (ticking) are both visible in the log. */
static void log_current_time(void)
{
    rtc_datetime_t now;
    if (rtc_get_time(&now) == 0) {
        log_printf("RTC: now %04u-%02u-%02u %02u:%02u:%02u\r\n",
                   now.year, now.month, now.day,
                   now.hour, now.minute, now.second);
    }
}

rtc_state_t rtc_bringup(void)
{
    /* Backup-domain access is needed to read BKP regs and touch LSE/RTC. */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* hrtc.Instance must be set before the BKUP read macro/HAL is used. */
    hrtc.Instance = RTC;

    /* WARM PATH: sentinel present => RTC was configured on a prior boot and is
     * still running on VBAT. Do nothing else — no LSE wait, no re-init. */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_SENTINEL_REG) == RTC_SENTINEL_MAGIC) {
        s_state = RTC_STATE_RUNNING;
        log_puts("RTC: already running (VBAT-backed), no crystal wait\r\n");
        log_current_time();
        return s_state;
    }

    /* COLD PATH: no sentinel. Start the crystal (in this thread), select it,
     * configure the RTC, seed a default time, write the sentinel. */
    log_puts("RTC: cold start — bringing up LSE...\r\n");
    if (!lse_start_and_wait()) {
        log_puts("RTC: LSE did not stabilise — no RTC this boot\r\n");
        s_state = RTC_STATE_UNAVAILABLE;
        return s_state;
    }
    if (!select_lse_rtc_clock() || !rtc_configure()) {
        log_puts("RTC: configure failed — no RTC this boot\r\n");
        s_state = RTC_STATE_UNAVAILABLE;
        return s_state;
    }

    seed_default_time();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_SENTINEL_REG, RTC_SENTINEL_MAGIC);
    s_state = RTC_STATE_COLD_INIT;
    log_puts("RTC: cold init complete (LSE running, time set from build)\r\n");
    log_current_time();
    return s_state;
}

rtc_state_t rtc_get_state(void) { return s_state; }

int rtc_available(void)
{
    return (s_state == RTC_STATE_RUNNING || s_state == RTC_STATE_COLD_INIT);
}

int rtc_get_time(rtc_datetime_t *out)
{
    if (!out || !rtc_available()) return -1;

    /* Per the F4 RM: read TIME then DATE (reading time latches date until date
     * is read — must read both, time first, to unlock the shadow registers). */
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    if (HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return -1;
    if (HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) return -1;

    out->year   = (uint16_t)(2000 + d.Year);
    out->month  = d.Month;
    out->day    = d.Date;
    out->hour   = t.Hours;
    out->minute = t.Minutes;
    out->second = t.Seconds;
    return 0;
}

int rtc_set_time(const rtc_datetime_t *in)
{
    if (!in || !rtc_available()) return -1;

    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    t.Hours = in->hour; t.Minutes = in->minute; t.Seconds = in->second;
    d.Year = (uint8_t)(in->year >= 2000 ? in->year - 2000 : 0);
    d.Month = in->month; d.Date = in->day; d.WeekDay = RTC_WEEKDAY_MONDAY;

    if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return -1;
    if (HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) return -1;

    /* Refresh the sentinel so subsequent boots take the warm path. */
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_SENTINEL_REG, RTC_SENTINEL_MAGIC);
    return 0;
}

#endif /* FPGA_BUS_STM32 */
