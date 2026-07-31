/*
 * app_main.h — the shared port entry point.
 *
 * Both targets call app_main() after their own hardware/RTOS bring-up:
 *   - Board: from tx_application_define()'s USER CODE, after pools/USBX/si5351.
 *   - Host:  from the test harness, after tx_kernel_enter starts a thread.
 *
 * app_main() is identical source on both, so the port's real startup sequence
 * is exercised by the host tests.
 */
#ifndef TONNEREXL_APP_MAIN_H
#define TONNEREXL_APP_MAIN_H

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TX_BYTE_POOL *thread_pool;   /* pool app threads allocate stacks from */
} app_config_t;

/* Initialise the port and start its threads. Returns TX_SUCCESS or an error.
 * Does: platform_init, logger, fpga_bus_init, simplefile_init_lock,
 * app_threads_create. Shared by board and host. */
UINT app_main(const app_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_APP_MAIN_H */
