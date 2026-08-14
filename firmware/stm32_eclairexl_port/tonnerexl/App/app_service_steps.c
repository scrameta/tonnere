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
void pot_reset_service_step(void) {
    if (fpga_irq_pending() & (1u << IRQ_POT_RESET_LH_BIT)) {
        adc_dma_paddle_pins_clamp();
        fpga_irq_clear((uint16_t)(1u << IRQ_POT_RESET_LH_BIT));
    }
    if (fpga_irq_pending() & (1u << IRQ_POT_RESET_HL_BIT)) {
        adc_dma_paddle_pins_release();
        fpga_irq_clear((uint16_t)(1u << IRQ_POT_RESET_HL_BIT));
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
    /* TODO(port): observe physical console keys (Start/Select/Option) for menu
     * navigation. Physical Reset is no longer a console key — the RESET key
     * drives the 6502 reset line directly; if the menu needs to react to it,
     * expose it via a dedicated signal/IRQ from the RTL. */
    /* TODO(port): menu state machine, drawing to Atari memory window. */
}
