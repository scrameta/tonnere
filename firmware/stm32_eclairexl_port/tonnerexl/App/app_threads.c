/*
 * app_threads.c — thread shells + sync object creation.
 *
 * The shells are deliberately tiny: block on the relevant event, run one
 * service step, loop. All substantive logic lives in the *_service_step()
 * functions (elsewhere), which are exercised on the host without threads.
 */
#include "app_threads.h"
#include "fpga_bus.h"

TX_EVENT_FLAGS_GROUP g_fpga_events;
TX_EVENT_FLAGS_GROUP g_sd_events;
TX_QUEUE             g_input_queue;

static TX_THREAD t_drive, t_usbin, t_sdlife, t_menu;

#define STACK_DRIVE  (2*1024)
#define STACK_USBIN  (2*1024)
#define STACK_SDLIFE (4*1024)   /* FileX calls can be stack-hungry */
#define STACK_MENU   (3*1024)
#define INPUT_QUEUE_MSGS 32

/* ---- thread shells ---- */
void drive_thread_entry(ULONG arg) {
    (void)arg;
    for (;;) {
        ULONG flags = 0;
        /* Highest priority: wake on SIO command or UART RX from the FPGA IRQ. */
        tx_event_flags_get(&g_fpga_events, EVT_SIO_CMD | EVT_UART_RX,
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

    st = tx_event_flags_create(&g_fpga_events, "fpga_evt"); if (st) return st;
    st = tx_event_flags_create(&g_sd_events,   "sd_evt");   if (st) return st;

    void *qmem;
    st = tx_byte_allocate(pool, &qmem, INPUT_QUEUE_MSGS * sizeof(ULONG), TX_NO_WAIT);
    if (st) return st;
    st = tx_queue_create(&g_input_queue, "input", TX_1_ULONG, qmem,
                         INPUT_QUEUE_MSGS * sizeof(ULONG));
    if (st) return st;

    st = tx_byte_allocate(pool, &sp, STACK_DRIVE, TX_NO_WAIT); if (st) return st;
    st = tx_thread_create(&t_drive, "drive", drive_thread_entry, 0, sp, STACK_DRIVE,
                          PRIO_DRIVE, PRIO_DRIVE, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) return st;

    st = tx_byte_allocate(pool, &sp, STACK_USBIN, TX_NO_WAIT); if (st) return st;
    st = tx_thread_create(&t_usbin, "usbin", usbin_thread_entry, 0, sp, STACK_USBIN,
                          PRIO_USBIN, PRIO_USBIN, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) return st;

    st = tx_byte_allocate(pool, &sp, STACK_SDLIFE, TX_NO_WAIT); if (st) return st;
    st = tx_thread_create(&t_sdlife, "sdlife", sdlife_thread_entry, 0, sp, STACK_SDLIFE,
                          PRIO_SDLIFE, PRIO_SDLIFE, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) return st;

    st = tx_byte_allocate(pool, &sp, STACK_MENU, TX_NO_WAIT); if (st) return st;
    st = tx_thread_create(&t_menu, "menu", menu_thread_entry, 0, sp, STACK_MENU,
                          PRIO_MENU, PRIO_MENU, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (st) return st;

    return TX_SUCCESS;
}
