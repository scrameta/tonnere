/*
 * paddle_bringup.c — stage (i) paddle ADC bring-up diagnostic (STM32 target).
 *
 * See paddle_bringup.h for the mode selector and phases. Design decisions:
 *
 * - Poll phase converts ONE channel per Start. MX_ADC2_Init runs 8 ranks with
 *   ContinuousConvMode=ENABLE, EOCS=1: once SWSTART fires, the sequencer
 *   free-runs at ~us per conversion, so polling rank-by-rank (with logging in
 *   between) loses the race — DR is overwritten, OVR sets, and on F4 an
 *   overrun with EOCS=1 aborts regular conversions, so every later poll
 *   times out. So the poll phase temporarily re-inits ADC2 as single-channel,
 *   no scan, no continuous, software trigger; selects each channel with
 *   HAL_ADC_ConfigChannel(rank 1); and does Start -> Poll -> Get -> Stop per
 *   channel. Logging between conversions is then harmless. The CubeMX config
 *   (Init struct + SQR/SMPR registers) is saved first and restored before the
 *   DMA phase, which depends on the 8-rank continuous scan.
 * - Polling is legal here because HAL_ADC_Start_DMA was never called on
 *   hadc2: CR2.DMA stays 0, so HAL_ADC_PollForConversion's EOCS+DMA guard
 *   cannot fire. The DMA handle stays linked (MSP) but idle.
 * - DMA phase re-uses adc_dma_paddle_start_to_ram(): same circular setup as
 *   production. With padbring_measure=1 (default) it fakes the FPGA: dump,
 *   release, and time each channel's threshold crossing from the live DMA
 *   buffer, logged in the same format as the poll phase so the two can be
 *   compared directly. Older note on the raw-buffer view (measure=0):
 *   same circular setup as
 *   production but the destination is this module's RAM buffer. NOTE on
 *   interpretation: with DMA FIFO at the 1/4 threshold and very-high
 *   priority, a lane can be torn between the ADC sample and our read (the
 *   DMA may be mid-burst). So in the RAM log, WHICH channels change is the
 *   signal; exact values are not. Compare min/max spans, not single reads.
 * - Paddles are RC: pot charges a cap. Like POKEY, each poll sweep first
 *   DUMPS all 8 caps (pins -> push-pull output low for padbring_dump_us),
 *   RELEASES them (pins -> analog), waits padbring_charge_us, then converts
 *   the 8 channels back-to-back. Reading at fixed T gives
 *   V/Vcc = 1 - exp(-T/RC), so higher R = lower reading; R is proportional
 *   to T / -ln(1 - V/Vcc). Channel r is sampled ~r*30 us after channel 0;
 *   the actual release->start time per channel is in padbring_diag.t_us[].
 *   If the thread is preempted between steps the sample is dropped and
 *   counted in padbring_diag.late. Both tunables are globals: change them
 *   live from GDB (`set var padbring_charge_us = 3000`).
 *   The DMA phase does NOT dump, so it reads the RC steady state only.
 * - No logging from ISR context: everything here runs in our own thread.
 *
 * Pin map must match MX_ADC2_Init ranks 1..8 (Core/Src/main.c) and the
 * PADDLE_ADC0..7 order the FPGA contract expects:
 *   rank1..4 = CH10..13 = PC0 PC1 PC2 PC3 ; rank5..8 = CH0,1,4,5 = PA0 PA1 PA4 PA5
 */
#include "paddle_bringup.h"

#if defined(FPGA_BUS_STM32) && (TONNEREXL_PADBRING != 0)

#include "tx_api.h"
#include "stm32f4xx_hal.h"
#include "adc_dma.h"
#include "logger.h"
#include <stdio.h>

extern ADC_HandleTypeDef hadc2;   /* CubeMX handle, Core/Src/main.c */

#define PADBRING_RANKS   8u
#define PADBRING_PRIO    25u      /* below PRIO_MENU(20): never preempt the UI */
#define PADBRING_STACK   2048u
#define PADBRING_LOG_TICKS  (TX_TIMER_TICKS_PER_SECOND/2u)  /* 500 ms */
#define PADBRING_POLL_TICKS (30u*TX_TIMER_TICKS_PER_SECOND) /* 30 s   */
#define PADBRING_DEADBAND   24u   /* 12-bit LSBs; span above this = "moving" */
#define PADBRING_POLL_MS    20u   /* per-channel EOC wait (one conv ~25 us) */
#define PADBRING_POLL_SMP   ADC_SAMPLETIME_480CYCLES  /* slow + stable for debug */
#define PADBRING_VERBOSE_SWEEPS 4u  /* log every conversion for the first N sweeps */
#define PADBRING_ERR_LOG_MAX    32u /* then log only the first N timeouts      */

/* RC (POKEY-style) timing. Defaults only: the live values are the globals
 * padbring_dump_us / padbring_charge_us below. Pick charge_us so the largest
 * pot resistance still reads well above 0 and the smallest isn't pinned at
 * 0xFFF; roughly T ~ 0.3 * R_max * C is a reasonable start. */
#define PADBRING_DUMP_US_DEFAULT    100u    /* caps held at GND            */
/* Default T = one full POKEY slow pot scan: the pot counter clocks at the
 * line rate (1.79 MHz / 114 = ~15.7 kHz) and stops at 228, so
 * 228 * 114 / 1.7897725 MHz = 14.52 ms (NTSC; PAL ~14.66 ms). A paddle at
 * the Atari's max count then reads ~ the POKEY threshold voltage. */
#define PADBRING_ATARI_SCAN_US      14520u
#define PADBRING_CHARGE_US_DEFAULT  PADBRING_ATARI_SCAN_US  /* release -> first conversion */
#define PADBRING_GAP_MAX_US         60u     /* step gap above this = preempted */

/* Threshold-timing mode (padbring_measure = 1): after release, convert the 8
 * channels round-robin as fast as possible and record the DWT time at which
 * each first reads >= padbring_thresh_mv. NOTE the CB3T bus switch on the pot
 * lines has an internal pull-up current source that kicks in at ~1.4 V and
 * yanks the node to VCC, so thresholds above ~1.3 V still give a monotonic
 * time but no longer a clean RC one. */
#define PADBRING_MEASURE_DEFAULT    1u      /* 0 = sample at fixed T, 1 = time to threshold */
#define PADBRING_THRESH_MV_DEFAULT  1200u
#define PADBRING_TMAX_US_DEFAULT    40000u  /* give up on a channel after this */
#define PADBRING_VREF_MV            3300u   /* ADC full scale (VDDA)           */
#define PADBRING_TGAP_LATE_US       200u    /* bigger gap before crossing = preempted */
#define PADBRING_T_NONE             0xFFFFFFFFu

/* Paddle pins (must match pad_map): PA0 PA1 PA4 PA5, PC0..PC3 */
#define PAD_PA_PINS ((1u<<0) | (1u<<1) | (1u<<4) | (1u<<5))
#define PAD_PC_PINS ((1u<<0) | (1u<<1) | (1u<<2) | (1u<<3))

/* phase codes for the diag block */
#define PHASE_POLL 1u
#define PHASE_RAM  2u

static TX_THREAD t_padbring;
static uint64_t padbring_stack[PADBRING_STACK/8u] __attribute__((aligned(8)));

/* Circular DMA destination. 32-byte aligned so it never shares a cache line
 * with anything else (F407 has no cache, but the habit is cheap). */
static uint16_t padbring_dma_buf[2u * PADBRING_RANKS] __attribute__((aligned(32)));  /* room for D,P,D,P... */

static const struct { uint8_t ch; const char *pin; } pad_map[PADBRING_RANKS] = {
    { 10, "PC0" }, { 11, "PC1" }, { 12, "PC2" }, { 13, "PC3" },
    {  0, "PA0" }, {  1, "PA1" }, {  4, "PA4" }, {  5, "PA5" },
};

/* GDB: `print padbring_diag` at any time; also mirrored to the log. */
volatile struct {
    uint32_t mode, phase;
    uint16_t last[PADBRING_RANKS];
    uint16_t mn[PADBRING_RANKS], mx[PADBRING_RANKS];
    uint32_t sweeps_ok, sweeps_fail, poll_timeouts, ovr_poll;
    uint32_t fault_count_seen;      /* last adc_dma_last_fault.count logged */
    uint32_t ndtr_a, ndtr_b;        /* DMA liveness samples                 */
    uint32_t dma_alive, ovr_dma;
    uint32_t late;                  /* RC samples dropped: thread preempted */
    uint32_t t_us[PADBRING_RANKS];  /* last sweep: release -> conv start, us */
    /* threshold-timing mode */
    uint32_t tx_us[PADBRING_RANKS];      /* last sweep: release -> crossing, us (T_NONE = never) */
    uint32_t tx_mn[PADBRING_RANKS], tx_mx[PADBRING_RANKS]; /* per log window */
    uint32_t tx_res_us;                  /* last sweep: avg time per round of 8 */
    uint32_t tx_late;                    /* crossings with a big sampling gap  */
    uint32_t scan_ns;                    /* RAM phase: measured full-sequence scan period */
    uint32_t nranks;                     /* RAM phase: ranks in the armed sequence (8 or 16) */
    uint16_t dummy_max;                  /* RAM phase: max dummy-slot reading, last window */
} padbring_diag;

/* GDB-tunable RC timing (see header comment). */
volatile uint32_t padbring_dump_us    = PADBRING_DUMP_US_DEFAULT;
volatile uint32_t padbring_charge_us  = PADBRING_CHARGE_US_DEFAULT;
volatile uint32_t padbring_measure    = PADBRING_MEASURE_DEFAULT;
volatile uint32_t padbring_thresh_mv  = PADBRING_THRESH_MV_DEFAULT;
volatile uint32_t padbring_tmax_us    = PADBRING_TMAX_US_DEFAULT;

/* RAM-phase ADC2 sequence (re-armed live when any of these change):
 *   padbring_dummy       0 = production 8 ranks (P0..P7)
 *                        1 = 16 ranks D,P0,D,P1,...,D,P7 with D = PC4 (ADC12_IN14,
 *                            unconnected on 0.51) so the S/H cap is reset to the
 *                            dummy's voltage before every pot, instead of carrying
 *                            the previous pot's voltage over.
 *   padbring_dummy_drive 1 = PC4 push-pull output low (no wiring needed; check the
 *                            "dummy max" log value is ~0x000)
 *                        0 = PC4 analog mode (use if you wire PC4 to GND / a divider)
 *   padbring_pot_smp / padbring_dummy_smp: SMPx codes, 0=3 1=15 2=28 3=56 4=84
 *                        5=112 6=144 7=480 cycles. CubeMX uses 3 for everything. */
volatile uint32_t padbring_dummy       = 1u;
volatile uint32_t padbring_dummy_drive = 0u;
volatile uint32_t padbring_pot_smp     = 0u;
volatile uint32_t padbring_dummy_smp   = 0u;
#define PADBRING_DUMMY_CH    14u          /* PC4 */
#define PADBRING_DUMMY_PIN   (1u << 4)    /* on GPIOC */

/* Mirror any new adc_dma fault record into the log (thread context). */
static void padbring_fault_watch(void)
{
    if (adc_dma_last_fault.count != padbring_diag.fault_count_seen) {
        padbring_diag.fault_count_seen = adc_dma_last_fault.count;
        log_printf("padbring: ADC/DMA FAULT #%lu err=0x%lX sr=0x%lX ndtr=%lu\r\n",
                   (unsigned long)adc_dma_last_fault.count,
                   (unsigned long)adc_dma_last_fault.error,
                   (unsigned long)adc_dma_last_fault.adc_sr,
                   (unsigned long)adc_dma_last_fault.dma_ndtr);
    }
}

/* ------------------------------------------------------------------ */
/* Poll phase                                                          */
/* ------------------------------------------------------------------ */
/* CubeMX ADC2 state, saved across the poll phase so the DMA phase sees the
 * original 8-rank continuous scan. HAL_ADC_Init only rewrites CR1/CR2, SQR1.L
 * and the common prescaler, so the rank (SQRx) and sample-time (SMPRx)
 * registers are saved raw and written back after re-init. */
static ADC_InitTypeDef pad_saved_init;
static uint32_t pad_saved_sqr1, pad_saved_sqr2, pad_saved_sqr3;
static uint32_t pad_saved_smpr1, pad_saved_smpr2;

static void pad_log_adc_regs(const char *tag)
{
    log_printf("padbring: %s CR1=%08lX CR2=%08lX SQR1=%08lX SQR3=%08lX SR=%08lX CCR=%08lX\r\n",
               tag,
               (unsigned long)hadc2.Instance->CR1, (unsigned long)hadc2.Instance->CR2,
               (unsigned long)hadc2.Instance->SQR1, (unsigned long)hadc2.Instance->SQR3,
               (unsigned long)hadc2.Instance->SR, (unsigned long)ADC123_COMMON->CCR);
}

/* Re-init ADC2 as: 1 conversion, no scan, no continuous, software trigger,
 * EOC per conversion. Returns false if HAL refuses. */
static bool pad_poll_config(void)
{
    (void)HAL_ADC_Stop(&hadc2);   /* harmless if never started */

    pad_saved_init  = hadc2.Init;
    pad_saved_sqr1  = hadc2.Instance->SQR1;
    pad_saved_sqr2  = hadc2.Instance->SQR2;
    pad_saved_sqr3  = hadc2.Instance->SQR3;
    pad_saved_smpr1 = hadc2.Instance->SMPR1;
    pad_saved_smpr2 = hadc2.Instance->SMPR2;
    pad_log_adc_regs("cubemx");

    hadc2.Init.ScanConvMode          = DISABLE;
    hadc2.Init.ContinuousConvMode    = DISABLE;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.NbrOfConversion       = 1;
    hadc2.Init.ExternalTrigConv      = ADC_SOFTWARE_START;  /* clears EXTEN/EXTSEL */
    hadc2.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc2.Init.DMAContinuousRequests = DISABLE;
    hadc2.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc2) != HAL_OK) {
        log_printf("padbring: poll-mode HAL_ADC_Init FAILED state=0x%lX err=0x%lX\r\n",
                   (unsigned long)hadc2.State, (unsigned long)hadc2.ErrorCode);
        return false;
    }
    pad_log_adc_regs("poll");

    /* HAL_ADC_Start only sets SWSTART on ADC2 when dual/triple mode is off. */
    if (ADC123_COMMON->CCR & ADC_CCR_MULTI)
        log_printf("padbring: WARNING ADC multimode enabled (CCR.MULTI) - ADC2 SWSTART will be skipped\r\n");
    return true;
}

/* Put the CubeMX 8-rank config back for the DMA phase. */
static void pad_restore_config(void)
{
    (void)HAL_ADC_Stop(&hadc2);
    hadc2.Init = pad_saved_init;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
        log_printf("padbring: restore HAL_ADC_Init FAILED state=0x%lX err=0x%lX\r\n",
                   (unsigned long)hadc2.State, (unsigned long)hadc2.ErrorCode);
    hadc2.Instance->SMPR1 = pad_saved_smpr1;
    hadc2.Instance->SMPR2 = pad_saved_smpr2;
    hadc2.Instance->SQR1  = pad_saved_sqr1;
    hadc2.Instance->SQR2  = pad_saved_sqr2;
    hadc2.Instance->SQR3  = pad_saved_sqr3;
    pad_log_adc_regs("restored");
}

/* ------------------------------------------------------------------ */
/* RC dump/release (POKEY-style)                                       */
/* ------------------------------------------------------------------ */
static bool     pad_rc_ok;         /* false -> sweep runs without dumping */
static uint32_t pad_cyc_per_us;

static inline uint32_t pad_cyc(void) { return DWT->CYCCNT; }

/* Write a 2-bit-per-pin field (MODER / PUPDR) for every pin in `pins`.
 * Caller holds interrupts off: other threads may RMW the same GPIO port. */
static void pad_set2(volatile uint32_t *reg, uint32_t pins, uint32_t val)
{
    uint32_t m = *reg;
    for (unsigned p = 0; p < 16u; ++p)
        if (pins & (1u << p)) m = (m & ~(3u << (2u*p))) | (val << (2u*p));
    *reg = m;
}

/* Pins in `pins` whose MODER is not analog (0b11). */
static uint32_t pad_not_analog(GPIO_TypeDef *g, uint32_t pins)
{
    uint32_t bad = 0;
    for (unsigned p = 0; p < 16u; ++p)
        if ((pins & (1u << p)) && ((g->MODER >> (2u*p)) & 3u) != 3u) bad |= 1u << p;
    return bad;
}

/* Busy-wait until `us` have elapsed since cycle stamp t0. Long waits sleep
 * the bulk first so we don't hog the CPU; tx_thread_sleep(n) can return up
 * to a tick early, never late, so (n-1) ticks never overshoots. Preemption
 * can still overshoot; the gap check in pad_sweep catches that. */
static void pad_wait_from(uint32_t t0, uint32_t us)
{
    const uint32_t tick_us = 1000000u / TX_TIMER_TICKS_PER_SECOND;
    if (us > 2u * tick_us) tx_thread_sleep(us / tick_us - 1u);
    const uint32_t cyc = us * pad_cyc_per_us;
    while ((pad_cyc() - t0) < cyc) { }
}

static void pad_dump(void)
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    GPIOA->BSRR = PAD_PA_PINS << 16;   /* ODR low BEFORE switching to output */
    GPIOC->BSRR = PAD_PC_PINS << 16;
    pad_set2(&GPIOA->MODER, PAD_PA_PINS, 1u);
    pad_set2(&GPIOC->MODER, PAD_PC_PINS, 1u);
    TX_RESTORE
}

static void pad_release(void)
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    pad_set2(&GPIOA->MODER, PAD_PA_PINS, 3u);
    pad_set2(&GPIOC->MODER, PAD_PC_PINS, 3u);
    TX_RESTORE
}

/* One-time setup: cycle counter, sanity-check pin ownership, push-pull/no-pull. */
static void pad_rc_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
    pad_cyc_per_us    = SystemCoreClock / 1000000u;
    pad_rc_ok         = false;

    const uint32_t bad_a = pad_not_analog(GPIOA, PAD_PA_PINS);
    const uint32_t bad_c = pad_not_analog(GPIOC, PAD_PC_PINS);
    if (bad_a || bad_c) {
        log_printf("padbring: pins not analog (PA %04lX PC %04lX) - another peripheral owns them, RC dump OFF\r\n",
                   (unsigned long)bad_a, (unsigned long)bad_c);
        return;
    }
#if defined(DAC)
    if (DAC->CR & (DAC_CR_EN1 | DAC_CR_EN2)) {
        log_printf("padbring: DAC enabled (drives PA4/PA5) - RC dump OFF\r\n");
        return;
    }
#endif
    {
        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        GPIOA->OTYPER &= ~PAD_PA_PINS;               /* push-pull */
        GPIOC->OTYPER &= ~PAD_PC_PINS;
        pad_set2(&GPIOA->PUPDR, PAD_PA_PINS, 0u);    /* no pulls */
        pad_set2(&GPIOC->PUPDR, PAD_PC_PINS, 0u);
        TX_RESTORE
    }
    pad_rc_ok = true;
    log_printf("padbring: RC mode ON dump=%luus charge=%luus (%lu cyc/us)\r\n",
               (unsigned long)padbring_dump_us, (unsigned long)padbring_charge_us,
               (unsigned long)pad_cyc_per_us);
}

/* ------------------------------------------------------------------ */
/* Sweep                                                               */
/* ------------------------------------------------------------------ */

/* per-channel result codes, for the post-sweep log */
enum { PS_OK = 0, PS_CFG, PS_START, PS_TIMEOUT, PS_LATE };

/* One sweep: dump all caps, release, wait charge_us, then 8 single
 * conversions back-to-back. NO logging until all 8 are done - a printf
 * between conversions would wreck the timing. On F4, ADC_CHANNEL_n == n, so
 * pad_map[].ch goes straight to ConfigChannel. */
static void pad_sweep(void)
{
    const bool verbose =
        (padbring_diag.sweeps_ok + padbring_diag.sweeps_fail) < PADBRING_VERBOSE_SWEEPS;
    const uint32_t charge_us = padbring_charge_us;   /* snapshot GDB tunables */
    const uint32_t dump_us   = padbring_dump_us;
    bool ok = true, any_err = false;
    uint16_t val[PADBRING_RANKS] = {0};
    uint32_t el[PADBRING_RANKS]  = {0};
    uint32_t sr[PADBRING_RANKS]  = {0};
    uint8_t  st[PADBRING_RANKS]  = {0};

    ADC_ChannelConfTypeDef c = {0};
    c.Rank         = 1;
    c.SamplingTime = PADBRING_POLL_SMP;

    uint32_t t_rel = 0;
    if (pad_rc_ok) {
        pad_dump();
        pad_wait_from(pad_cyc(), dump_us);
        pad_release();
        t_rel = pad_cyc();
        pad_wait_from(t_rel, charge_us);
    }
    uint32_t prev_us = charge_us;

    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        c.Channel = pad_map[r].ch;
        if (HAL_ADC_ConfigChannel(&hadc2, &c) != HAL_OK) { st[r] = PS_CFG;   ok = false; continue; }
        if (HAL_ADC_Start(&hadc2) != HAL_OK)            { st[r] = PS_START; ok = false; continue; }

        /* SWSTART is set at the end of HAL_ADC_Start, so this is (to a few
         * cycles) the start of the sample window. */
        el[r] = pad_rc_ok ? (pad_cyc() - t_rel) / pad_cyc_per_us : 0u;
        padbring_diag.t_us[r] = el[r];

        if (HAL_ADC_PollForConversion(&hadc2, PADBRING_POLL_MS) == HAL_OK) {
            val[r] = (uint16_t)(HAL_ADC_GetValue(&hadc2) & 0x0FFFu);
            if (pad_rc_ok && (el[r] - prev_us) > PADBRING_GAP_MAX_US) {
                st[r] = PS_LATE;              /* preempted: T is wrong, drop it */
                padbring_diag.late++;
            } else {
                padbring_diag.last[r] = val[r];
                if (val[r] < padbring_diag.mn[r]) padbring_diag.mn[r] = val[r];
                if (val[r] > padbring_diag.mx[r]) padbring_diag.mx[r] = val[r];
            }
        } else {
            st[r] = PS_TIMEOUT;
            sr[r] = hadc2.Instance->SR;
            ok = false;
            any_err = true;
            padbring_diag.poll_timeouts++;
        }
        prev_us = el[r];

        /* Check OVR before Stop: Stop doesn't clear it, but the next Start will. */
        if (__HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_OVR)) padbring_diag.ovr_poll++;
        (void)HAL_ADC_Stop(&hadc2);
    }
    if (ok) padbring_diag.sweeps_ok++; else padbring_diag.sweeps_fail++;

    /* ---- logging, after the timed part ---- */
    const bool log_errs = any_err && padbring_diag.poll_timeouts <= PADBRING_ERR_LOG_MAX;
    if (!verbose && !log_errs) return;
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        if (!verbose && st[r] == PS_OK) continue;
        switch (st[r]) {
        case PS_CFG:   log_printf("padbring: r%u %s ConfigChannel FAILED\r\n", r, pad_map[r].pin); break;
        case PS_START: log_printf("padbring: r%u %s Start FAILED\r\n", r, pad_map[r].pin); break;
        case PS_TIMEOUT:
            log_printf("padbring: r%u %s TIMEOUT SR=%08lX\r\n", r, pad_map[r].pin, (unsigned long)sr[r]);
            break;
        default:
            log_printf("padbring: r%u %s ch%u = %03X (%4u) t=%luus%s\r\n",
                       r, pad_map[r].pin, (unsigned)pad_map[r].ch, val[r], val[r],
                       (unsigned long)el[r], st[r] == PS_LATE ? " LATE(dropped)" : "");
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Threshold-timing sweep (padbring_measure = 1)                       */
/* ------------------------------------------------------------------ */

/* Sample time for our 8 channels only. The ADC sees the 47 nF node through
 * the ~5 ohm switch, so a short sample time is fine (4 pF S/H vs 47 nF). */
static void pad_set_smp(ADC_TypeDef *adc, uint32_t smp)
{
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        const uint32_t ch = pad_map[r].ch;
        if (ch >= 10u) {
            const uint32_t s = 3u * (ch - 10u);
            adc->SMPR1 = (adc->SMPR1 & ~(7u << s)) | (smp << s);
        } else {
            const uint32_t s = 3u * ch;
            adc->SMPR2 = (adc->SMPR2 & ~(7u << s)) | (smp << s);
        }
    }
}

/* Dump, release, then convert the channels still below threshold round-robin
 * (register level: HAL call overhead would cost ~10x in resolution) and log
 * the DWT time of each channel's first sample >= threshold. Every sample is
 * timestamped, so preemption costs resolution, not correctness; a crossing
 * whose previous sample was > PADBRING_TGAP_LATE_US earlier is counted in
 * tx_late. Busy-waits up to padbring_tmax_us per sweep. */
static void pad_sweep_thresh(void)
{
    ADC_TypeDef *const adc = hadc2.Instance;
    const uint32_t cpu    = pad_cyc_per_us;
    const uint32_t tmax   = padbring_tmax_us * cpu;
    uint32_t thr = (padbring_thresh_mv * 4095u) / PADBRING_VREF_MV;
    if (thr > 4095u) thr = 4095u;

    if (!pad_rc_ok) { padbring_diag.sweeps_fail++; return; }

    pad_set_smp(adc, 3u);                       /* 56 cycles */
    if (!(adc->CR2 & ADC_CR2_ADON)) {           /* HAL_ADC_Stop clears ADON */
        adc->CR2 |= ADC_CR2_ADON;
        const uint32_t t = pad_cyc();
        while (pad_cyc() - t < 3u * cpu) { }    /* tSTAB */
    }
    (void)adc->DR;
    adc->SR = 0;                                /* rc_w0: clear EOC/OVR/STRT */

    uint32_t tx[PADBRING_RANKS], prev[PADBRING_RANKS];
    uint32_t pending = (1u << PADBRING_RANKS) - 1u;
    uint32_t rounds = 0;
    bool hw_err = false;

    pad_dump();
    pad_wait_from(pad_cyc(), padbring_dump_us);
    pad_release();
    const uint32_t t_rel = pad_cyc();
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) { tx[r] = PADBRING_T_NONE; prev[r] = t_rel; }

    while (pending && !hw_err) {
        for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
            if (!(pending & (1u << r))) continue;
            adc->SQR3 = pad_map[r].ch;
            adc->CR2 |= ADC_CR2_SWSTART;
            const uint32_t ts = pad_cyc();      /* ~ start of the sample window */
            while (!(adc->SR & ADC_SR_EOC)) {
                if (pad_cyc() - ts > 100u * cpu) { hw_err = true; break; }
            }
            if (hw_err) break;
            const uint32_t v = adc->DR & 0x0FFFu;   /* DR read clears EOC */
            if (v >= thr) {
                tx[r] = (ts - t_rel) / cpu;
                if (ts - prev[r] > PADBRING_TGAP_LATE_US * cpu) padbring_diag.tx_late++;
                pending &= ~(1u << r);
            }
            prev[r] = ts;
        }
        rounds++;
        if (pad_cyc() - t_rel > tmax) break;    /* the rest never crossed */
    }

    padbring_diag.tx_res_us = rounds ? ((pad_cyc() - t_rel) / cpu) / rounds : 0u;
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        padbring_diag.tx_us[r] = tx[r];
        if (tx[r] < padbring_diag.tx_mn[r]) padbring_diag.tx_mn[r] = tx[r];
        if (tx[r] > padbring_diag.tx_mx[r]) padbring_diag.tx_mx[r] = tx[r];
    }
    if (hw_err) { padbring_diag.poll_timeouts++; padbring_diag.sweeps_fail++; }
    else          padbring_diag.sweeps_ok++;
}

static void pad_tx_window_reset(void)
{
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        padbring_diag.tx_mn[r] = PADBRING_T_NONE;
        padbring_diag.tx_mx[r] = 0u;
    }
}

static void pad_log_times(const char *tag, const volatile uint32_t *t)
{
    char buf[PADBRING_RANKS * 7u + 1u];
    unsigned n = 0;
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        if (t[r] == PADBRING_T_NONE)
            n += (unsigned)snprintf(buf + n, sizeof buf - n, "  -----");
        else
            n += (unsigned)snprintf(buf + n, sizeof buf - n, " %6lu", (unsigned long)t[r]);
    }
    log_printf("%s:%s\r\n", tag, buf);
}

static void pad_log_values(const char *tag)
{
    log_printf("%s: %03X %03X %03X %03X %03X %03X %03X %03X\r\n", tag,
               padbring_diag.last[0], padbring_diag.last[1],
               padbring_diag.last[2], padbring_diag.last[3],
               padbring_diag.last[4], padbring_diag.last[5],
               padbring_diag.last[6], padbring_diag.last[7]);
}

static void pad_log_minmax(void)
{
    log_printf("pad min: %03X %03X %03X %03X %03X %03X %03X %03X\r\n",
               padbring_diag.mn[0], padbring_diag.mn[1],
               padbring_diag.mn[2], padbring_diag.mn[3],
               padbring_diag.mn[4], padbring_diag.mn[5],
               padbring_diag.mn[6], padbring_diag.mn[7]);
    log_printf("pad max: %03X %03X %03X %03X %03X %03X %03X %03X\r\n",
               padbring_diag.mx[0], padbring_diag.mx[1],
               padbring_diag.mx[2], padbring_diag.mx[3],
               padbring_diag.mx[4], padbring_diag.mx[5],
               padbring_diag.mx[6], padbring_diag.mx[7]);
}

/* Bit r set when channel r's min..max span exceeds the deadband. */
static uint32_t pad_moving_mask(void)
{
    uint32_t m = 0;
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        if ((uint16_t)(padbring_diag.mx[r] - padbring_diag.mn[r]) > PADBRING_DEADBAND)
            m |= 1u << r;
    }
    return m;
}

static void pad_poll_phase(ULONG deadline_ticks /* 0 = forever */)
{
    const ULONG t0 = tx_time_get();
    ULONG t_log = t0;
    uint32_t moving_prev = 0xFFFFFFFFu;   /* force first print */

    if (!pad_poll_config()) {
        log_printf("padbring: poll phase skipped\r\n");
        pad_restore_config();
        return;
    }
    pad_rc_init();
    pad_tx_window_reset();

    padbring_diag.phase = PHASE_POLL;
    while (deadline_ticks == 0 || tx_time_get() - t0 < deadline_ticks) {
        const bool timing = (padbring_measure != 0u);
        if (timing) pad_sweep_thresh(); else pad_sweep();
        padbring_fault_watch();
        if (tx_time_get() - t_log >= PADBRING_LOG_TICKS && timing) {
            t_log = tx_time_get();
            const uint32_t thr_mv = padbring_thresh_mv;
            log_printf("pad thr=%lumV(0x%03lX) res~%luus ok=%lu fail=%lu late=%lu (us, release->cross; ----- = >%luus)\r\n",
                       (unsigned long)thr_mv,
                       (unsigned long)((thr_mv * 4095u) / PADBRING_VREF_MV),
                       (unsigned long)padbring_diag.tx_res_us,
                       (unsigned long)padbring_diag.sweeps_ok,
                       (unsigned long)padbring_diag.sweeps_fail,
                       (unsigned long)padbring_diag.tx_late,
                       (unsigned long)padbring_tmax_us);
            pad_log_times("pad t min", padbring_diag.tx_mn);
            pad_log_times("pad t max", padbring_diag.tx_mx);
            pad_log_times("pad t cur", padbring_diag.tx_us);
            pad_tx_window_reset();
        } else if (tx_time_get() - t_log >= PADBRING_LOG_TICKS) {
            t_log = tx_time_get();
            log_printf("pad ok=%lu fail=%lu timeouts=%lu ovr=%lu late=%lu T=%luus t7=%luus\r\n",
                       (unsigned long)padbring_diag.sweeps_ok,
                       (unsigned long)padbring_diag.sweeps_fail,
                       (unsigned long)padbring_diag.poll_timeouts,
                       (unsigned long)padbring_diag.ovr_poll,
                       (unsigned long)padbring_diag.late,
                       (unsigned long)padbring_charge_us,
                       (unsigned long)padbring_diag.t_us[PADBRING_RANKS - 1u]);
            pad_log_minmax();
            pad_log_values("pad cur");   /* last sample, taken T after release */
            const uint32_t mv = pad_moving_mask();
            if (mv != moving_prev) {
                char names[8*4+1];
                unsigned n = 0;
                moving_prev = mv;
                for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
                    if (mv & (1u << r)) {
                        names[n++] = ' '; names[n++] = pad_map[r].pin[0];
                        names[n++] = pad_map[r].pin[1]; names[n++] = pad_map[r].pin[2];
                    }
                }
                names[n] = '\0';
                log_printf("pad MOVING mask=0x%02lX:%s\r\n",
                           (unsigned long)mv, n ? names : " (none)");
            }
        }
        tx_thread_sleep(1);   /* ~100 sweeps/s; leaves the CPU to everything else */
    }

    /* Only reached with a deadline (mode 3): pins back to analog (they always
     * are between sweeps, but be explicit) and hand the CubeMX config back. */
    if (pad_rc_ok) pad_release();
    pad_restore_config();
}

/* ------------------------------------------------------------------ */
/* DMA-to-RAM phase                                                    */
/* ------------------------------------------------------------------ */
/* Armed sequence: buffer index of pot r, and whether slot i is a dummy. */
static uint32_t pad_ram_n = PADBRING_RANKS;
static uint32_t pad_ram_cfg_applied = 0xFFFFFFFFu;

static inline unsigned pad_ram_idx(unsigned r) { return (pad_ram_n > PADBRING_RANKS) ? 2u * r + 1u : r; }

static uint32_t pad_ram_cfg_word(void)
{
    return (padbring_dummy ? 1u : 0u) | ((padbring_dummy_drive ? 1u : 0u) << 1) |
           ((padbring_pot_smp & 7u) << 4) | ((padbring_dummy_smp & 7u) << 8);
}

static void pad_smp_set_ch(ADC_TypeDef *adc, uint32_t ch, uint32_t smp)
{
    if (ch >= 10u) { const uint32_t sh = 3u * (ch - 10u); adc->SMPR1 = (adc->SMPR1 & ~(7u << sh)) | (smp << sh); }
    else           { const uint32_t sh = 3u * ch;         adc->SMPR2 = (adc->SMPR2 & ~(7u << sh)) | (smp << sh); }
}

/* Write the regular sequence directly: SQR3 ranks 1-6, SQR2 7-12, SQR1 13-16 + L. */
static void pad_write_seq(ADC_TypeDef *adc, const uint8_t *seq, uint32_t n)
{
    uint32_t sqr1 = (n - 1u) << 20, sqr2 = 0, sqr3 = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t ch = seq[i] & 0x1Fu;
        if      (i < 6u)  sqr3 |= ch << (5u * i);
        else if (i < 12u) sqr2 |= ch << (5u * (i - 6u));
        else              sqr1 |= ch << (5u * (i - 12u));
    }
    adc->SQR3 = sqr3; adc->SQR2 = sqr2; adc->SQR1 = sqr1;
}

/* (Re)arm ADC2 + circular DMA to RAM with the sequence the tunables ask for.
 * Everything else (CONT=1, SCAN=1, DDS=1, DMA stream setup) stays CubeMX. */
static bool pad_ram_arm(bool was_armed)
{
    ADC_TypeDef *const adc = hadc2.Instance;
    const uint32_t cfg = pad_ram_cfg_word();
    const bool dummy = (cfg & 1u) != 0u;
    uint8_t seq[2u * PADBRING_RANKS];
    uint32_t n = 0;

    if (was_armed) (void)HAL_ADC_Stop_DMA(&hadc2);

    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        if (dummy) seq[n++] = (uint8_t)PADBRING_DUMMY_CH;
        seq[n++] = pad_map[r].ch;
        pad_smp_set_ch(adc, pad_map[r].ch, padbring_pot_smp & 7u);
    }
    if (dummy) pad_smp_set_ch(adc, PADBRING_DUMMY_CH, padbring_dummy_smp & 7u);
    pad_write_seq(adc, seq, n);
    hadc2.Init.NbrOfConversion = n;          /* bookkeeping only; Start_DMA doesn't rewrite L */

    /* Dummy pin: driven low, or analog (externally tied). Unused when dummy=0. */
    {
        TX_INTERRUPT_SAVE_AREA
        TX_DISABLE
        GPIOC->PUPDR &= ~(3u << (2u * 4u));
        if (dummy && (cfg & 2u)) {
            GPIOC->BSRR    = PADBRING_DUMMY_PIN << 16;     /* latch low first */
            GPIOC->OTYPER &= ~PADBRING_DUMMY_PIN;
            pad_set2(&GPIOC->MODER, PADBRING_DUMMY_PIN, 1u);
        } else {
            pad_set2(&GPIOC->MODER, PADBRING_DUMMY_PIN, 3u);
        }
        TX_RESTORE
    }

    for (unsigned i = 0; i < 2u * PADBRING_RANKS; ++i) padbring_dma_buf[i] = 0u;
    pad_ram_n = n;
    padbring_diag.nranks = n;
    pad_ram_cfg_applied = cfg;

    const bool ok = (adc_dma_paddle_start_to_ram(padbring_dma_buf, n) == FPGA_OK);
    static const uint16_t smp_cyc[8] = { 3, 15, 28, 56, 84, 112, 144, 480 };
    log_printf("padbring: RAM %s: %lu ranks%s pot_smp=%u cyc dummy_smp=%u cyc SQR1=%08lX SQR2=%08lX SQR3=%08lX\r\n",
               ok ? "armed" : "ARM FAILED", (unsigned long)n,
               dummy ? ((cfg & 2u) ? " (D=PC4 driven low)" : " (D=PC4 analog)") : " (no dummy)",
               smp_cyc[padbring_pot_smp & 7u], smp_cyc[padbring_dummy_smp & 7u],
               (unsigned long)adc->SQR1, (unsigned long)adc->SQR2, (unsigned long)adc->SQR3);
    return ok;
}

/* Fake-FPGA sweep: the stream (continuous scan, circular DMA) keeps running
 * untouched; we do what the FPGA does - request a dump, release, then watch
 * the DMA'd pot slots and note when each first reads >= threshold. The
 * DMA'd value can be up to one scan + one FIFO burst old when read, so the
 * resolution is ~one scan period. NDTR decrements are counted to measure the
 * real scan period; convs == 0 over a whole sweep means the DMA is stuck. */
static void pad_ram_sweep_thresh(void)
{
    const uint32_t cpu  = pad_cyc_per_us;
    const uint32_t tmax = padbring_tmax_us * cpu;
    const uint32_t n    = pad_ram_n;
    const bool     dummy = (n > PADBRING_RANKS);
    uint32_t thr = (padbring_thresh_mv * 4095u) / PADBRING_VREF_MV;
    if (thr > 4095u) thr = 4095u;
    const volatile uint16_t *const buf = (const volatile uint16_t *)padbring_dma_buf;
    DMA_Stream_TypeDef *const ds = hadc2.DMA_Handle->Instance;

    if (!pad_rc_ok) { padbring_diag.sweeps_fail++; return; }

    uint32_t tx[PADBRING_RANKS];
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) tx[r] = PADBRING_T_NONE;
    uint32_t pending = (1u << PADBRING_RANKS) - 1u;
    uint16_t dmax = padbring_diag.dummy_max;

    /* "FPGA requests a dump" */
    pad_dump();
    pad_wait_from(pad_cyc(), padbring_dump_us);
    pad_release();
    const uint32_t t_rel = pad_cyc();

    uint32_t nd_prev = ds->NDTR, convs = 0, now = t_rel;
    while (pending) {
        now = pad_cyc();
        const uint32_t nd = ds->NDTR;           /* n..1, reloads to n (circular) */
        convs += (nd_prev + n - nd) % n;
        nd_prev = nd;
        __DMB();
        for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
            if ((pending & (1u << r)) && (uint32_t)(buf[pad_ram_idx(r)] & 0x0FFFu) >= thr) {
                tx[r] = (now - t_rel) / cpu;
                pending &= ~(1u << r);
            }
        }
        if (dummy) {
            const uint16_t d = (uint16_t)(buf[0] & 0x0FFFu);   /* one dummy slot is plenty */
            if (d > dmax) dmax = d;
        }
        if (now - t_rel > tmax) break;
    }

    const uint32_t el_us = (now - t_rel) / cpu;
    padbring_diag.dma_alive = (convs != 0u);
    padbring_diag.dummy_max = dmax;
    /* full-sequence scan period in ns (= timing resolution); n<=16, el<=~40 ms: no overflow */
    padbring_diag.scan_ns = convs ? (el_us * 1000u * n) / convs : 0u;
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        padbring_diag.tx_us[r] = tx[r];
        if (tx[r] < padbring_diag.tx_mn[r]) padbring_diag.tx_mn[r] = tx[r];
        if (tx[r] > padbring_diag.tx_mx[r]) padbring_diag.tx_mx[r] = tx[r];
    }
    padbring_diag.ovr_dma = __HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_OVR) ? 1u : 0u;
    if (convs) padbring_diag.sweeps_ok++; else padbring_diag.sweeps_fail++;
}

static void pad_ram_phase(void)
{
    ULONG t_log = tx_time_get();

    padbring_diag.phase = PHASE_RAM;
    /* Mode 2 never ran the poll phase; mode 3 did, but re-checking is cheap. */
    pad_rc_init();
    bool armed = pad_ram_arm(false);
    log_printf("padbring: RAM phase%s\r\n", padbring_measure ? " - faking FPGA dump requests" : "");
    padbring_diag.sweeps_ok = padbring_diag.sweeps_fail = 0u;
    padbring_diag.dummy_max = 0u;
    pad_tx_window_reset();

    while (1) {
        /* Live A/B: change padbring_dummy / _drive / _pot_smp / _dummy_smp in GDB. */
        if (pad_ram_cfg_word() != pad_ram_cfg_applied) {
            armed = pad_ram_arm(armed);
            padbring_diag.sweeps_ok = padbring_diag.sweeps_fail = 0u;
            padbring_diag.dummy_max = 0u;
            pad_tx_window_reset();
            t_log = tx_time_get();
        }

        if (padbring_measure && armed) {
            /* ---- fake-FPGA timing, ~one dump per 20-40 ms like a frame ---- */
            pad_ram_sweep_thresh();
            padbring_fault_watch();
            if (tx_time_get() - t_log >= PADBRING_LOG_TICKS) {
                t_log = tx_time_get();
                const uint32_t thr_mv = padbring_thresh_mv;
                const uint32_t scan_ns = padbring_diag.scan_ns;
                log_printf("ram %lu ranks thr=%lumV(0x%03lX) scan=%lu.%02luus alive=%lu ovr=%lu ok=%lu fail=%lu (us, release->cross; ----- = >%luus)\r\n",
                           (unsigned long)padbring_diag.nranks,
                           (unsigned long)thr_mv,
                           (unsigned long)((thr_mv * 4095u) / PADBRING_VREF_MV),
                           (unsigned long)(scan_ns / 1000u),
                           (unsigned long)((scan_ns % 1000u) / 10u),
                           (unsigned long)padbring_diag.dma_alive,
                           (unsigned long)padbring_diag.ovr_dma,
                           (unsigned long)padbring_diag.sweeps_ok,
                           (unsigned long)padbring_diag.sweeps_fail,
                           (unsigned long)padbring_tmax_us);
                if (padbring_diag.nranks > PADBRING_RANKS)
                    log_printf("ram dummy max=0x%03X (expect ~000 driven / GND)\r\n",
                               (unsigned)padbring_diag.dummy_max);
                pad_log_times("ram t min", padbring_diag.tx_mn);
                pad_log_times("ram t max", padbring_diag.tx_mx);
                pad_log_times("ram t cur", padbring_diag.tx_us);
                pad_tx_window_reset();
                padbring_diag.dummy_max = 0u;
            }
            tx_thread_sleep(1);
            continue;
        }

        /* ---- padbring_measure = 0: original steady-state liveness view ---- */
        /* Liveness: NDTR counts down every conversion, so two samples 10 ms
         * apart differ iff the stream is actually moving. A stream stuck on a
         * FIFO/transfer error holds NDTR constant. */
        padbring_diag.ndtr_a = hadc2.DMA_Handle->Instance->NDTR;
        tx_thread_sleep(1);
        padbring_diag.ndtr_b = hadc2.DMA_Handle->Instance->NDTR;
        padbring_diag.dma_alive = (padbring_diag.ndtr_a != padbring_diag.ndtr_b) && armed;
        padbring_fault_watch();

        if (tx_time_get() - t_log >= PADBRING_LOG_TICKS) {
            t_log = tx_time_get();
            const volatile uint16_t *bv = (const volatile uint16_t *)padbring_dma_buf;
            __DMB();
            for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
                const uint16_t v = (uint16_t)(bv[pad_ram_idx(r)] & 0x0FFFu);
                padbring_diag.last[r] = v;
                if (v < padbring_diag.mn[r]) padbring_diag.mn[r] = v;
                if (v > padbring_diag.mx[r]) padbring_diag.mx[r] = v;
            }
            pad_log_values("ram cur");
            padbring_diag.ovr_dma = __HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_OVR) ? 1u : 0u;
            log_printf("ram ndtr %lu->%lu alive=%lu ovr=%lu ok=%lu fail=%lu\r\n",
                       (unsigned long)padbring_diag.ndtr_a,
                       (unsigned long)padbring_diag.ndtr_b,
                       (unsigned long)padbring_diag.dma_alive,
                       (unsigned long)padbring_diag.ovr_dma,
                       (unsigned long)padbring_diag.sweeps_ok,
                       (unsigned long)padbring_diag.sweeps_fail);
            pad_log_minmax();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Thread                                                              */
/* ------------------------------------------------------------------ */
static void padbring_entry(ULONG arg)
{
    (void)arg;
    padbring_diag.mode = TONNEREXL_PADBRING;
    for (unsigned r = 0; r < PADBRING_RANKS; ++r) {
        padbring_diag.mn[r] = 0x0FFFu;
        padbring_diag.mx[r] = 0u;
    }
    log_printf("padbring: mode=%d ADC2 diagnostic start (ranks: PC0-3 PA0 PA1 PA4 PA5)\r\n",
               (int)TONNEREXL_PADBRING);

    if (TONNEREXL_PADBRING == 1) {
        pad_poll_phase(0);
    } else if (TONNEREXL_PADBRING == 2) {
        pad_ram_phase();
    } else { /* 3: poll 30 s, then RAM */
        pad_poll_phase(PADBRING_POLL_TICKS);
        log_puts("padbring: poll window done, switching to DMA->RAM\r\n");
        pad_ram_phase();
    }
}

#endif /* FPGA_BUS_STM32 && TONNEREXL_PADBRING != 0 */

int paddle_bringup_start(void)
{
#if defined(FPGA_BUS_STM32) && (TONNEREXL_PADBRING != 0)
    const UINT st = tx_thread_create(&t_padbring, "padbring", padbring_entry, 0,
                                     padbring_stack, PADBRING_STACK,
                                     PADBRING_PRIO, PADBRING_PRIO,
                                     TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st != TX_SUCCESS) log_printf("padbring: thread create failed: %u\r\n", st);
    return (int)st;
#else
    return 0;
#endif
}

bool paddle_bringup_owns_adc2(void)
{
#if defined(FPGA_BUS_STM32) && (TONNEREXL_PADBRING != 0)
    return true;
#else
    return false;
#endif
}
