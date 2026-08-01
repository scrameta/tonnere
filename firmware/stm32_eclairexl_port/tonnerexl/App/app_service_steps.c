/*
 * app_service_steps.c — portable per-iteration logic for each thread (v0.2).
 *
 * Seams where ported EclaireXL logic lands. No RTOS primitives as args; host
 * unit tests call these directly. Each has a TODO marking where reference logic
 * slots in.
 */
#include "app_threads.h"
#include "fpga_bus.h"

/* Drive/SIO: on an IRQ, demux via the interrupt controller, drain SIO bytes,
 * then clear the handled sources (W1C). Real ATR/ATX protocol SM ports here. */
void drive_service_step(void) {
    uint16_t pending = fpga_irq_pending();
    if (pending & (1u << IRQ_SIO_RX_BIT)) {
        uint8_t b;
        while (fpga_sio_getc(&b)) {
            /* TODO(port): feed b into the SIO command/frame state machine. */
        }
    }
    /* clear the SIO-related sources we handled */
    fpga_irq_clear((uint16_t)((1u<<IRQ_SIO_CMD_BIT) | (1u<<IRQ_SIO_RX_BIT) | (1u<<IRQ_SIO_TX_BIT)));
}

/* Paddle poll, paced by POTGO IRQ: on POTGO, read STM32 ADCs and write the
 * paddle registers, then clear POTGO. Real ADC reads port here in Phase 6. */
void potgo_service_step(void) {
    if (fpga_irq_pending() & (1u << IRQ_POTGO_BIT)) {
        /* TODO(port): read 4 ADC axes, fpga_paddle_write(0,a0,a1); (1,a2,a3). */
        fpga_irq_clear((uint16_t)(1u << IRQ_POTGO_BIT));
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
