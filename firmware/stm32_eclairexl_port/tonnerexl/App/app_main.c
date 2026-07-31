/*
 * app_main.c — shared port startup. Identical on board and host.
 *
 * This is the code that embodies "how the port starts up": bring the platform
 * seam up, init the logger, init the FPGA bus (verify identity), create the
 * filesystem lock, and start the application threads. Hardware bring-up (pools,
 * USBX, FileX, si5351) has already happened in the caller's target-specific
 * path before this runs.
 */
#include "app_main.h"
#include "app_threads.h"
#include "platform.h"
#include "logger.h"
#include "fpga_bus.h"
#include "simplefile_filex.h"

UINT app_main(const app_config_t *cfg) {
    if (!cfg || !cfg->thread_pool) return TX_PTR_ERROR;

    logger_init();
    if (platform_init() != 0) {
        log_puts("platform_init failed\r\n");
        return TX_NOT_DONE;
    }

    fpga_status_t fs = fpga_bus_init();
    if (fs == FPGA_ERR_MAGIC) {
        /* Identity mismatch: RTL/firmware contract skew. Continue in a limited
         * mode rather than trusting the bus — the menu can still report it. */
        log_puts("WARNING: FPGA identity mismatch (safe mode)\r\n");
    } else if (fs != FPGA_OK) {
        log_puts("fpga_bus_init failed\r\n");
    }

    simplefile_init_lock();

    /* Enable the interrupt sources the threads consume. */
    fpga_irq_enable((uint16_t)((1u << IRQ_SIO_CMD_BIT) |
                               (1u << IRQ_UART_RX_BIT) |
                               (1u << IRQ_UART_TX_BIT) |
                               (1u << IRQ_POTGO_BIT)));

    UINT st = app_threads_create(cfg->thread_pool);
    if (st != TX_SUCCESS) {
        log_printf("app_threads_create failed: %u\r\n", st);
        return st;
    }
    log_puts("TonnereXL port started\r\n");
    return TX_SUCCESS;
}
