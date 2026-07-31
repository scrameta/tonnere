/*
 * platform.h — hardware abstraction seam.
 *
 * The ONE boundary between shared port code and target-specific hardware.
 * app_main() and everything above it is identical on board and host; only the
 * implementation of these functions differs:
 *   - platform_stm32.c : real board (si5351, UART log sink, HAL glue)
 *   - platform_host.c  : Linux host (stubs / stdout)
 *
 * Hardware BRING-UP (byte pools, USBX, FileX init) is NOT here — it stays in
 * each target's natural place (CubeMX's tx_application_define on board; the
 * test harness on host). This interface is only for things app_main() itself
 * needs to call that happen to be target-specific.
 */
#ifndef TONNEREXL_PLATFORM_H
#define TONNEREXL_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called early by app_main(), after hardware bring-up, before threads start.
 * Board: any port-visible board setup not already done by CubeMX (currently
 * none — si5351 is done in tx_application_define). Host: no-op.
 * Returns 0 on success. */
int platform_init(void);

/* Video clock mode change request from the port (e.g. menu picks PAL/NTSC).
 * Board: drives si5351 via the board's clock code. Host: no-op/records last. */
typedef enum { PLAT_CLK_NTSC, PLAT_CLK_PAL, PLAT_CLK_480P, PLAT_CLK_720P } platform_clock_mode_t;
void platform_set_clock_mode(platform_clock_mode_t mode);

/* Log sink: emit one byte to the target's console. The shared logger
 * (logger.c) calls this; it's the ONLY board-specific part of logging.
 * Board: UART/ITM. Host: stdout. */
void platform_log_putc(char c);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_PLATFORM_H */
