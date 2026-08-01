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

static TX_THREAD t_drive, t_usbin, t_sdlife, t_menu;

#define STACK_DRIVE  (2*1024)
#define STACK_USBIN  (2*1024)
#define STACK_SDLIFE (4*1024)   /* FileX calls can be stack-hungry */
#define STACK_MENU   (3*1024)
#define INPUT_QUEUE_MSGS 32

/*
 * The port owns its thread-stack memory rather than borrowing the caller's
 * byte pool, so it can't be starved by a too-small CubeMX TX_APP_MEM_POOL_SIZE
 * (which defaults to 1 KB). Sized for the four stacks + queue + ThreadX
 * per-block overhead, with headroom.
 */
#define PORT_POOL_SIZE ( STACK_DRIVE + STACK_USBIN + STACK_SDLIFE + STACK_MENU \
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
    for (;;) {
        ULONG flags = 0;
        tx_event_flags_get(&g_sd_events, EVT_SD_INSERTED | EVT_SD_REMOVED,
                           TX_OR_CLEAR, &flags, TX_WAIT_FOREVER);
        sdlife_service_step();
    }
}

void menu_thread_entry(ULONG arg) {
    (void)arg;
    for (;;) {
        menu_service_step();
        tx_thread_sleep(2);
    }
}

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

    return TX_SUCCESS;
}
