/*
 * app_threads.h — TonnereXL application thread model.
 *
 * Four threads, each a thin shell: wait on its event/queue, call a
 * *_service_step(), loop. The step functions hold the portable logic and are
 * unit-tested on the host without the RTOS. Priorities per the port plan
 * (lower number = higher priority in ThreadX).
 */
#ifndef TONNEREXL_APP_THREADS_H
#define TONNEREXL_APP_THREADS_H

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Priorities (ThreadX: 0 = highest). */
#define PRIO_DRIVE   5    /* SIO/drive emulation — time critical           */
#define PRIO_USBIN   6   /* USB HID input                                 */
#define PRIO_SDLIFE  12   /* SD card lifecycle + filesystem                */
#define PRIO_MENU    20   /* menu / UI                                     */
#define PRIO_BOOT    15   /* one-shot core bring-up + kbd walk (bring-up)  */

/* Event flag groups / queues shared between ISR/callbacks and threads. */
extern TX_EVENT_FLAGS_GROUP g_fpga_events;   /* set from FPGA IRQ demux    */
extern TX_EVENT_FLAGS_GROUP g_sd_events;     /* card insert/remove         */
extern TX_QUEUE             g_input_queue;   /* decoded key/joy events     */

/* FPGA event flag bits (mirror contract §6 IRQ sources). */
#define EVT_SIO_CMD    (1u << 0)
#define EVT_SIO_RX    (1u << 1)
#define EVT_SIO_TX    (1u << 2)
#define EVT_POTGO      (1u << 3)
#define EVT_DMA_DONE   (1u << 4)

/* SD event flag bits. */
#define EVT_SD_INSERTED (1u << 0)
#define EVT_SD_REMOVED  (1u << 1)

/* Thread entry points. */
void drive_thread_entry(ULONG arg);
void usbin_thread_entry(ULONG arg);
void sdlife_thread_entry(ULONG arg);
void menu_thread_entry(ULONG arg);
void boot_thread_entry(ULONG arg);   /* core bring-up + keyboard walk       */

/* Core bring-up sequence (reset, RAM clear, release 6502). Runs in the boot
 * thread so its tx_thread_sleep hold-times actually wait. Non-portable (touches
 * FPGA HAL), so not a host-tested service step. */
void tonnere_boot_core(void);

#if defined(FPGA_BUS_STM32)
/* SD step-(i) bring-up: poll card-detect, init + read block 0 on insert, log on
 * remove. Board-only (touches HAL_SD). Called from the sdlife thread. */
void sd_bringup_poll(void);
#endif

/* Portable per-iteration service steps (unit-tested on host). Each returns
 * after doing at most one unit of work, so they compose into the wait-loop and
 * can be driven directly by tests. */
void drive_service_step(void);
void potgo_service_step(void);
void usbin_service_step(void);
void sdlife_service_step(void);
void menu_service_step(void);

/* One-time creation of threads + sync objects from a byte pool. */
UINT app_threads_create(TX_BYTE_POOL *pool);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_APP_THREADS_H */
