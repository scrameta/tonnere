/*
 * app_threads.c — thread shells + sync object creation.
 *
 * The shells are deliberately tiny: block on the relevant event, run one
 * service step, loop. All substantive logic lives in the *_service_step()
 * functions (elsewhere), which are exercised on the host without threads.
 */
#include "app_threads.h"
#include "fpga_bus.h"
#include "logger.h"

TX_EVENT_FLAGS_GROUP g_fpga_events;
TX_EVENT_FLAGS_GROUP g_sd_events;
TX_QUEUE             g_input_queue;

static TX_THREAD t_drive, t_usbin, t_sdlife, t_menu, t_boot;

#define STACK_DRIVE  (2*1024)
#define STACK_USBIN  (2*1024)
#define STACK_SDLIFE (4*1024)   /* FileX calls can be stack-hungry */
#define STACK_MENU   (3*1024)
#define STACK_BOOT   (2*1024)
#define INPUT_QUEUE_MSGS 32

/*
 * The port owns its thread-stack memory rather than borrowing the caller's
 * byte pool, so it can't be starved by a too-small CubeMX TX_APP_MEM_POOL_SIZE
 * (which defaults to 1 KB). Sized for the four stacks + queue + ThreadX
 * per-block overhead, with headroom.
 */
#define PORT_POOL_SIZE ( STACK_DRIVE + STACK_USBIN + STACK_SDLIFE + STACK_MENU \
                       + STACK_BOOT \
                       + (INPUT_QUEUE_MSGS * sizeof(ULONG)) + 2048 /* overhead */ )
static UCHAR        s_port_pool_mem[PORT_POOL_SIZE];
static TX_BYTE_POOL s_port_pool;

/* ---- thread shells ---- */
void drive_thread_entry(ULONG arg) {
    (void)arg;
    for (;;) {
        ULONG flags = 0;
        /* Highest priority: wake on SIO command or UART RX from the FPGA IRQ. */
        tx_event_flags_get(&g_fpga_events, EVT_SIO_CMD | EVT_SIO_RX,
                           TX_OR_CLEAR, &flags, TX_WAIT_FOREVER);
        drive_service_step();
    }
}

void usbin_thread_entry(ULONG arg) {
    (void)arg;
    for (;;) {
        /* USB input arrives via the USBX HID client publishing to the queue;
         * the step consumes and injects through the FPGA HAL. A short wait keeps
         * the thread responsive without spinning. */
        usbin_service_step();
        tx_thread_sleep(1);
    }
}

void sdlife_thread_entry(ULONG arg) {
    (void)arg;
#if defined(FPGA_BUS_STM32)
    /* Card-detect is a plain GPIO input (PC13), not EXTI, so poll it. The poll
     * runs the detect/init/read/remove lifecycle in sd_bringup.c. */
    for (;;) {
        sd_bringup_poll();
        tx_thread_sleep(10);   /* ~100 ms poll interval */
    }
#else
    /* Host: no SDIO hardware. Keep the event-driven seam for the simulated
     * lifecycle (host_interactive drives fx_media directly). */
    for (;;) {
        ULONG flags = 0;
        tx_event_flags_get(&g_sd_events, EVT_SD_INSERTED | EVT_SD_REMOVED,
                           TX_OR_CLEAR, &flags, TX_WAIT_FOREVER);
        sdlife_service_step();
    }
#endif
}

void menu_thread_entry(ULONG arg) {
    (void)arg;
    for (;;) {
        menu_service_step();
        tx_thread_sleep(2);
    }
}

/* ---- core bring-up (runs in the boot thread, so sleeps really wait) ---- */
#if defined(FPGA_BUS_STM32)
void tonnere_boot_core(void) {
    /* Atari starts paused, reset deasserted while we set config. */
    fpga_core_set_pause(1);
    fpga_core_set_reset(0);
    fpga_set_performance(1, 0);
    fpga_core_set_freezer(0);
    fpga_set_ramconfig(0);          /* 0:64k, 1:128k */
    fpga_core_set_atari800(0);
    fpga_set_video(1, 1, 0, 0);
    fpga_set_cart(0);

    /* Clear the RAM the 6502 boots from. ext 0x20 = SRAM here — confirm that's
     * the chip the core maps the Atari 64k onto (if it's SDRAM, use ext 0x00,
     * else you clear the wrong chip and boot from garbage).
     * Board-only: the aperture window is a raw FSMC pointer (0x60000000) that
     * doesn't exist on host, so guard it. */
#if defined(FPGA_BUS_STM32)
    {
        log_puts("Cleaning SRAM\r\n");
        fpga_aperture_set_ext(1, 0x20);
        volatile uint16_t *win = FPGA_APERTURE1_ADDR;
        for (int addr = 0; addr < 65536; ++addr) win[addr] = 0;
        log_puts("Verifying SRAM\r\n");
        for (int addr = 0; addr < 65536; ++addr) 
        {
            uint16_t val = win[addr];
            if (val!=0) log_printf("Read of sram %x was non-zero %x\n",addr,val);
        }

        log_puts("Cleaning SDRAM\r\n");
        fpga_aperture_set_ext(1, 0x0);
        for (int addr = 0; addr < 65536; ++addr) win[addr] = 0;
        log_puts("Verifying SDRAM\r\n");
        for (int addr = 0; addr < 65536; ++addr) 
        {
            uint16_t val = win[addr];
            if (val!=0) log_printf("Read of sdram %x was non-zero %x\n",addr,val);
        }
    }
#endif

    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND/2); // Wait a little for USB to connect, so we can hold option? Better way?!

    /* Pulse 6502 reset — these sleeps now really wait (thread context). */
    log_puts("Booting 6502\r\n");
    fpga_core_set_reset(1);
    tx_thread_sleep(10);
    fpga_core_set_reset(0);
    tx_thread_sleep(10);
    fpga_core_set_pause(0);         /* release — Atari runs */

    fpga_kbd_special(0, 0, 0);
    log_puts("core released; walking keyboard matrix\r\n");
}

/* ---- boot / keyboard-walk thread ---- *
 * One-shot core bring-up, then walk a single set bit through the 64-bit key
 * matrix (kbd0..kbd3 = bits 0-15, 16-31, 32-47, 48-63), one key per second, so
 * you can map each bit position to a physical key. When the USB keyboard is
 * wired up, this walk is replaced by the HID->matrix mapping. */
void boot_thread_entry(ULONG arg) {
    (void)arg;
    tonnere_boot_core();

#if defined(FPGA_BUS_STM32)
    /* Bring up the internal SPI microSD "hard drive" once (no detect, no retry).
     * Runs after core bring-up; logs + optionally lists its root as a self-test. */
    spi_sd_bringup();
#endif

/*    uint64_t x = 1;
    for (;;) {
        log_printf("kbd matrix = %08lx%08lx\r\n",
                   (unsigned long)(x >> 32),
                   (unsigned long)(x & 0xFFFFFFFFu));
        fpga_kbd_matrix_write((uint16_t)(x        & 0xFFFF),
                              (uint16_t)((x >> 16) & 0xFFFF),
                              (uint16_t)((x >> 32) & 0xFFFF),
                              (uint16_t)((x >> 48) & 0xFFFF));
        x <<= 1;
        if (x == 0) x = 1;
        tx_thread_sleep(100);       // 1 s at 100 Hz tick — real delay 
    }*/
}

#endif /* FPGA_BUS_STM32 */

/* ---- creation ---- */
UINT app_threads_create(TX_BYTE_POOL *pool) {
    void *sp; UINT st;

    /* Use the port's own pool by default so a small CubeMX TX_APP_MEM_POOL_SIZE
     * can't starve us. If the caller explicitly passes a pool, honour it (lets
     * an integrator override), otherwise create ours. */
    if (pool == TX_NULL) {
        st = tx_byte_pool_create(&s_port_pool, "tonnerexl", s_port_pool_mem, PORT_POOL_SIZE);
        if (st) { log_printf("port pool create failed: %u\r\n", st); return st; }
        pool = &s_port_pool;
    }

    st = tx_event_flags_create(&g_fpga_events, "fpga_evt"); if (st) return st;
    st = tx_event_flags_create(&g_sd_events,   "sd_evt");   if (st) return st;

    void *qmem;
    st = tx_byte_allocate(pool, &qmem, INPUT_QUEUE_MSGS * sizeof(ULONG), TX_NO_WAIT);
    if (st) { log_printf("alloc queue (%u B) failed: %u\r\n",
                         (unsigned)(INPUT_QUEUE_MSGS*sizeof(ULONG)), st); return st; }
    st = tx_queue_create(&g_input_queue, "input", TX_1_ULONG, qmem,
                         INPUT_QUEUE_MSGS * sizeof(ULONG));
    if (st) { log_printf("queue_create failed: %u\r\n", st); return st; }

    st = tx_byte_allocate(pool, &sp, STACK_DRIVE, TX_NO_WAIT);
    if (st) { log_printf("alloc drive stack (%u B) failed: %u\r\n", STACK_DRIVE, st); return st; }
    st = tx_thread_create(&t_drive, "drive", drive_thread_entry, 0, sp, STACK_DRIVE,
                          PRIO_DRIVE, PRIO_DRIVE, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) { log_printf("create drive failed: %u\r\n", st); return st; }

    st = tx_byte_allocate(pool, &sp, STACK_USBIN, TX_NO_WAIT);
    if (st) { log_printf("alloc usbin stack failed: %u\r\n", st); return st; }
    st = tx_thread_create(&t_usbin, "usbin", usbin_thread_entry, 0, sp, STACK_USBIN,
                          PRIO_USBIN, PRIO_USBIN, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) { log_printf("create usbin failed: %u\r\n", st); return st; }

    st = tx_byte_allocate(pool, &sp, STACK_SDLIFE, TX_NO_WAIT);
    if (st) { log_printf("alloc sdlife stack failed: %u\r\n", st); return st; }
    st = tx_thread_create(&t_sdlife, "sdlife", sdlife_thread_entry, 0, sp, STACK_SDLIFE,
                          PRIO_SDLIFE, PRIO_SDLIFE, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) { log_printf("create sdlife failed: %u\r\n", st); return st; }

    st = tx_byte_allocate(pool, &sp, STACK_MENU, TX_NO_WAIT);
    if (st) { log_printf("alloc menu stack failed: %u\r\n", st); return st; }
    st = tx_thread_create(&t_menu, "menu", menu_thread_entry, 0, sp, STACK_MENU,
                          PRIO_MENU, PRIO_MENU, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) { log_printf("create menu failed: %u\r\n", st); return st; }

    /* The boot thread does board bring-up (core reset, RAM clear over the FSMC
     * aperture) then the keyboard-matrix walk — all hardware-only scaffolding.
     * Don't create it on host, where there's no FSMC and the walk would spin
     * forever in the test harness. */
#if defined(FPGA_BUS_STM32)
    st = tx_byte_allocate(pool, &sp, STACK_BOOT, TX_NO_WAIT);
    if (st) { log_printf("alloc boot stack failed: %u\r\n", st); return st; }
    st = tx_thread_create(&t_boot, "boot", boot_thread_entry, 0, sp, STACK_BOOT,
                          PRIO_BOOT, PRIO_BOOT, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) { log_printf("create boot failed: %u\r\n", st); return st; }
#else
    (void)t_boot;
#endif

    return TX_SUCCESS;
}
