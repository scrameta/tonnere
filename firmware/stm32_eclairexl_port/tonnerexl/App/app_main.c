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

/* ------------------------------------------------------------------ *
 * Simple FPGA RAM smoke test.
 *
 * Reaches physical RAM through aperture 1: set the 8-bit extension
 * (phys A29..A22), then read/write 16-bit words in the aperture window
 * (FPGA_APERTURE1_ADDR + word offset). Writes a few patterns to a small
 * block, reads them back, reports pass/fail. Non-exhaustive by design.
 *
 * ext = 0x00 targets physical word 0 = base of SDRAM (spec §2.1).
 * Change TEST_EXT to point at SRAM1 (ext 0x20), SRAM2 (0x24), etc.
 * ------------------------------------------------------------------ */
__attribute__((unused)) static void fpga_mem_test(uint8_t bank)
{
    volatile uint16_t *win = FPGA_APERTURE1_ADDR;   /* aperture 1 window base */
    const uint32_t TEST_WORDS = 256;                /* small block: 512 bytes */
    //const uint32_t TEST_WORDS = 2;                /* small block: 512 bytes */

    static const uint16_t patterns[] = { 0x0000u, 0xFFFFu, 0xA5A5u, 0x5A5Au };
    const uint32_t NPAT = sizeof(patterns) / sizeof(patterns[0]);
    uint32_t errors = 0;

    log_printf("memory test: aperture1 ext=0x%02X, %lu words...\r\n",
               bank, (unsigned long)TEST_WORDS);

/*
    while (1)
    {
        for (uint16_t i=0;i!=4;++i)
	    win[i] = i;
//        log_printf("read1 0x%04X 0x%04X 0x%04X 0x%04X\r\n",win[0], win[1], win[2], win[3]);
        tx_thread_sleep(100);
        for (uint16_t i=0;i!=4;++i)
        {
            uint16_t val = win[i];
//	    if (val != i)
//                log_printf("i:%04x win[i]:%04x\r\n",i,val);
        }
        tx_thread_sleep(100);
    } */

    fpga_aperture_set_ext(1, bank);

    /* constant patterns */
    for (uint32_t p = 0; p < NPAT; p++) {
        uint16_t val = patterns[p];
        for (uint32_t i = 0; i < TEST_WORDS; i++) win[i] = val;
        for (uint32_t i = 0; i < TEST_WORDS; i++) {
            uint16_t got = win[i];
            if (got != val) {
                if (errors < 8)
                    log_printf("  MISMATCH @%lu: wrote 0x%04X read 0x%04X\r\n",
                               (unsigned long)i, val, got);
                errors++;
            }
        }
    }

    /* address-in-data (each cell = its own index) */
    for (uint32_t i = 0; i < TEST_WORDS; i++) win[i] = (uint16_t)i;
    for (uint32_t i = 0; i < TEST_WORDS; i++) {
        uint16_t got = win[i];
        if (got != (uint16_t)i) {
            if (errors < 8)
                log_printf("  ADDR MISMATCH @%lu: read 0x%04X\r\n",
                           (unsigned long)i, got);
            errors++;
        }
    }

    if (errors == 0) log_puts("  memory test PASSED\r\n");
    else             log_printf("  memory test FAILED (%lu errors)\r\n",
                                (unsigned long)errors);
}

UINT app_main(const app_config_t *cfg) {
    if (!cfg) return TX_PTR_ERROR;   /* thread_pool may be TX_NULL (use own pool) */

    logger_init();
    log_puts("\r\n=== TonnereXL: entered app_main ===\r\n");

    if (platform_init() != 0) {
        log_puts("platform_init failed\r\n");
        return TX_NOT_DONE;
    }

    log_puts("checking FPGA interface...\r\n");
    while (1)
    {
        fpga_status_t fs = fpga_bus_init();
        if (fs == FPGA_ERR_MAGIC) {
            /* Identity mismatch: RTL/firmware contract skew, or FPGA not yet
             * configured/responding. Continue in a limited mode rather than
             * trusting the bus — hello-world still completes. */
             log_printf("  FPGA identity = 0x%04X ver 0x%04X (expected magic 0x%04X)\r\n",
                        fpga_iface_magic(), fpga_iface_version(), FPGA_IFACE_MAGIC);
             log_puts("  -> FPGA not matched; waiting\r\n");
             tx_thread_sleep(100);
        } else if (fs != FPGA_OK) {
            log_puts("  fpga_bus_init failed\r\n");
        } else {
            log_puts("  FPGA interface OK\r\n");
            break;
        }
    }

    ////const uint8_t  TEST_EXT   = 0x00;               /* -> SDRAM word 0 */
    ////const uint8_t  TEST_EXT   = 0x20;               /* -> SRAM word 0 */
    //const uint8_t  TEST_EXT   = 0x28;               /* -> BLOCK RAM ? */
/*    while (1)
    {
        fpga_mem_test(0x00);
        tx_thread_sleep(100);
        fpga_mem_test(0x20);
        tx_thread_sleep(100);
        fpga_mem_test(0x28);
        tx_thread_sleep(100000);
        log_puts("\r\n\r\n\r\n\r\n");
    }*/

    simplefile_init_lock();

    /* Enable the interrupt sources the threads consume. */
    fpga_irq_enable((uint16_t)((1u << IRQ_SIO_CMD_BIT) |
                               (1u << IRQ_SIO_RX_BIT) |
                               (1u << IRQ_SIO_TX_BIT) |
                               (1u << IRQ_POT_RESET_LH_BIT) |
                               (1u << IRQ_POT_RESET_HL_BIT)));

    /* Create the application threads and return so tx_kernel_enter can finish
     * and the scheduler goes live. The core bring-up (6502 reset, RAM clear,
     * release) and the keyboard-matrix walk run in the "boot" thread — they
     * MUST be in a thread, not here: app_main runs at the end of
     * tx_application_define, before the scheduler starts, so tx_thread_sleep
     * does not block in this context. */
    UINT st = app_threads_create(cfg->thread_pool);
    if (st != TX_SUCCESS) {
        log_printf("app_threads_create failed: %u\r\n", st);
        return st;
    }

    /* ADCs - for paddle and audio */
    UINT pad_st = adc_dma_paddle_start();
    if (pad_st != TX_SUCCESS) {
        log_printf("paddle dma start failed: %u\r\n", st);
        return st;
    }
    UINT aud_st = adc_dma_audio_start();
    if (aud_st != TX_SUCCESS) {
        log_printf("adc dma start failed: %u\r\n", st);
        return st;
    }

    log_puts("TonnereXL port started\r\n");
    return TX_SUCCESS;
}
