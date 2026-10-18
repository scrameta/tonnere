/*
 * paddle_bringup.h — stage (i) paddle ADC bring-up diagnostic (target-only).
 *
 * Proves the 8-channel paddle ADC path alone, with NO FPGA involvement:
 *   phase POLL: every sweep, Start -> poll+read 8 ranks -> Stop, logging the
 *               raw 12-bit values plus rolling min/max per channel.
 *   phase RAM:  re-arm the same ADC2 as circular DMA into a RAM buffer
 *               (instead of the FPGA PADDLE_ADC registers) and keep logging,
 *               with DMA liveness (NDTR sampled twice) + overrun state.
 *
 * Mode selector TONNEREXL_PADBRING (override with -D):
 *   0 = off (normal paddle + audio streams run)
 *   1 = poll forever
 *   2 = DMA-to-RAM immediately
 *   3 = poll 30 s, then DMA-to-RAM  (default)
 *
 * While the diagnostic is on it OWNS ADC2: app_main must not start the
 * paddle/audio streams (see paddle_bringup_owns_adc2()).
 *
 * Host builds get no-op stubs so app_main.c keeps linking there.
 */
#ifndef TONNEREXL_PADDLE_BRINGUP_H
#define TONNEREXL_PADDLE_BRINGUP_H

#include <stdbool.h>
#include <stdint.h>

#if defined(FPGA_BUS_STM32)

#ifndef TONNEREXL_PADBRING
#define TONNEREXL_PADBRING 3
#endif

/* Create the bring-up thread. Call from app_main after app_threads_create()
 * succeeded (still pre-scheduler; the thread is TX_AUTO_START). Returns a
 * ThreadX status as int, 0 == success. No-op when TONNEREXL_PADBRING == 0. */
int paddle_bringup_start(void);

/* true when the diagnostic owns ADC2 and the normal paddle/audio stream
 * starts must be skipped. False iff TONNEREXL_PADBRING == 0. */
bool paddle_bringup_owns_adc2(void);

#else /* host / non-STM32: no-op stubs */

static inline int paddle_bringup_start(void) { return 0; }
static inline bool paddle_bringup_owns_adc2(void) { return false; }

#endif /* FPGA_BUS_STM32 */
#endif /* TONNEREXL_PADDLE_BRINGUP_H */
