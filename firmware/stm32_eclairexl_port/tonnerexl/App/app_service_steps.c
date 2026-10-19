/*
 * app_service_steps.c — portable per-iteration logic for each thread (v0.2).
 *
 * Seams where ported EclaireXL logic lands. No RTOS primitives as args; host
 * unit tests call these directly. Each has a TODO marking where reference logic
 * slots in.
 */
#include "app_threads.h"
#include "fpga_bus.h"
#include "adc_dma.h"
#include "logger.h"
#if defined(FPGA_BUS_STM32)
#include "main.h"   /* FPGA_IRQ_GPIO_Port / FPGA_IRQ_Pin */
#endif

/* Drive/SIO: on an IRQ, demux via the interrupt controller, then drain SIO.
 * Real ATR/ATX protocol SM ports here.
 *
 * Edge-IRQ ordering matters: clear the pending bits BEFORE draining the RX FIFO.
 * The RX IRQ is a single rising edge on empty->non-empty (contract §6). If we
 * cleared after draining, a byte arriving between the drain finishing and the
 * clear would set a fresh edge that the clear then wipes — and since the FIFO
 * stays non-empty, no new edge ever comes, stranding that byte. Clearing first
 * means such a byte's edge stays latched for the next wake. We then drain until
 * the FIFO reports empty (read-until-empty rule, §6) to catch bytes already
 * queued. */
void drive_service_step(void) {
    uint16_t pending = fpga_irq_pending();

    /* clear first (edge-IRQ safe) */
    fpga_irq_clear((uint16_t)((1u<<IRQ_SIO_CMD_BIT) | (1u<<IRQ_SIO_RX_BIT) | (1u<<IRQ_SIO_TX_BIT)));

    if (pending & (1u << IRQ_SIO_CMD_BIT)) {
        /* TODO(port): a new SIO command frame is starting. */
    }
    if (pending & ((1u << IRQ_SIO_RX_BIT) | (1u << IRQ_SIO_CMD_BIT))) {
        /* Drain until empty regardless of which bit woke us — a command frame
         * also brings bytes, and one edge may cover several bytes. */
        uint8_t b;
        while (fpga_sio_getc(&b)) {
            /* TODO(port): feed b into the SIO command/frame state machine. */
        }
    }
    if (pending & (1u << IRQ_SIO_TX_BIT)) {
        /* TODO(port): transmit of the queued frame completed. */
    }
}

/* Paddle IRQ:  -> POT_RESET: dump on, NOT(POT_RESET): adc dma on */

/* GDB-readable IRQ counters (print pot_irq_diag any time; no logging from ISR
 * context). entries=EXTI edges seen; clamps/releases=bits actually handled;
 * loops>entries means the re-check loop caught a mid-handling event;
 * raw_pending shows what the FPGA reported (SIO bits there = enable mask or
 * clear is wrong; clamps==0 with entries>0 = bits 3/4 never reaching us). */
volatile struct {
    uint32_t entries;
    uint32_t clamps;
    uint32_t releases;
    uint32_t loops;
    uint16_t last_pending;   /* masked pending of the last handled iteration */
    uint16_t raw_pending;    /* pending & enabled at entry, unmasked */
} pot_irq_diag;

/* 1 = printer thread logs pot_irq_diag once a second while it changes. */
volatile uint32_t pot_irq_log = 1u;

void pot_reset_service_step(void) {
    int first = 1;
    pot_irq_diag.entries++;
    for (;;) {
        uint16_t p = fpga_irq_pending() & fpga_irq_enabled();
        if (first) { pot_irq_diag.raw_pending = p; first = 0; }
        p &= (1u << IRQ_POT_RESET_LH_BIT) | (1u << IRQ_POT_RESET_HL_BIT); // Only these are ours to handle
        if (!p) break;
        pot_irq_diag.loops++;
        pot_irq_diag.last_pending = p;
        if (p & (1u << IRQ_POT_RESET_LH_BIT)) { adc_dma_paddle_pins_clamp(); pot_irq_diag.clamps++; }
        if (p & (1u << IRQ_POT_RESET_HL_BIT)) { adc_dma_paddle_pins_release(); pot_irq_diag.releases++; }
        fpga_irq_clear(p);
    }
}

/* USB input: pull a decoded key event off the queue and set it in the matrix
 * shadow, then flush. Real HID->KBCODE mapping ports here in Phase 2. */
void usbin_service_step(void) {
    ULONG msg;
    if (tx_queue_receive(&g_input_queue, &msg, TX_NO_WAIT) == TX_SUCCESS) {
        /* msg encodes: bit31 = pressed, bits7:0 = kbcode (placeholder scheme). */
        uint8_t kbcode = (uint8_t)(msg & 0x3fu);
        int pressed = (msg >> 31) & 1u;
        fpga_kbd_set(kbcode, pressed);
        fpga_kbd_flush();
    }
}

/* SD lifecycle: SD is entirely STM32-side (SDIO). React to card insert/remove
 * detected via the STM32 CD pin. Real fx_media_open/close + simplefile bind
 * ports here in Phase 3. */
void sdlife_service_step(void) {
    /* TODO(port): on insert -> MX_SDIO_SD_Init, fx_media_open, simplefile_bind;
       on remove -> simplefile_unbind, fx_media_close. Card-detect is a STM32
       GPIO/EXTI, not an FPGA register. */
}

/* Menu: one UI tick. Also observe the physical console/Reset switches so a
 * hardware Reset can drive a menu action. Real menu ports here in Phase 4. */
void menu_service_step(void) {
    uint16_t phys = fpga_console_phys_read();
    (void)phys;

    /* Debug printer for the paddle IRQ counters (thread context; the ISR must
     * never log). Now a 1 s heartbeat with LIVE pend/lvl, not just counter
     * changes: a pending that latches but never edges the line was invisible
     * before. pot_irq_log=0 to silence. */
    static uint32_t tick;
    static int armed;
    if (!armed) { armed = 1; log_printf("potirq: printer armed\r\n"); }
    if (++tick >= 50u) {            /* menu thread ticks every 20 ms */
        tick = 0;
        if (pot_irq_log) {
            uint16_t pend = fpga_irq_pending();
            /* Diagnostic clear removed: the EXTI path is proven. A forced
             * clear here bypasses clamp/release and can strand one pot scan. */
            uint16_t ppot = pend & ((1u << IRQ_POT_RESET_LH_BIT) | (1u << IRQ_POT_RESET_HL_BIT));
            (void)ppot;
#if defined(FPGA_BUS_STM32)
            unsigned lvl = (unsigned)HAL_GPIO_ReadPin(FPGA_IRQ_GPIO_Port, FPGA_IRQ_Pin);
#else
            unsigned lvl = 0u;
#endif
            log_printf("potirq: ent=%lu cl=%lu rel=%lu lp=%lu raw=%04X last=%04X pend=%04X lvl=%u\r\n",
                       (unsigned long)pot_irq_diag.entries,
                       (unsigned long)pot_irq_diag.clamps,
                       (unsigned long)pot_irq_diag.releases,
                       (unsigned long)pot_irq_diag.loops,
                       pot_irq_diag.raw_pending, pot_irq_diag.last_pending,
                       pend, lvl);
        }
    }

    /* TODO(port): observe physical console keys (Start/Select/Option) for menu
     * navigation. Physical Reset is no longer a console key — the RESET key
     * drives the 6502 reset line directly; if the menu needs to react to it,
     * expose it via a dedicated signal/IRQ from the RTL. */
    /* TODO(port): menu state machine, drawing to Atari memory window. */
}
