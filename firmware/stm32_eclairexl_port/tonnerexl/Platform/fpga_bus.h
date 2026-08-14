/*
 * fpga_bus.h — typed HAL over the FSMC/FPGA interface (v0.2, native 16-bit).
 *
 * No code above this layer touches a raw FSMC pointer. Every register is a
 * plain 16-bit access — no half composition, no cross-boundary RMW.
 *
 * Two backends implement this header:
 *   - fpga_bus_stm32.c : real FSMC accesses (on-target, -DFPGA_BUS_STM32)
 *   - fpga_bus_fake.c  : in-memory model (host tests, -DFPGA_BUS_FAKE)
 */
#ifndef TONNEREXL_FPGA_BUS_H
#define TONNEREXL_FPGA_BUS_H

#include <stdint.h>
#include <stddef.h>
#include "fpga_bus_map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FPGA_OK = 0,
    FPGA_ERR_RANGE,
    FPGA_ERR_MAGIC,
    FPGA_ERR_TIMEOUT,
    FPGA_ERR_BUSY,
    FPGA_ERR_STATE,
} fpga_status_t;

/* ---- lifecycle ---- */
fpga_status_t fpga_bus_init(void);        /* init mutex, verify identity */
uint16_t      fpga_iface_magic(void);
uint16_t      fpga_iface_version(void);

/* ---- raw 16-bit register access (by logical index) ---- */
uint16_t fpga_reg_read(enum fpga_reg_index idx);
void     fpga_reg_write(enum fpga_reg_index idx, uint16_t value);
void     fpga_reg_rmw(enum fpga_reg_index idx, uint16_t mask, uint16_t value);

/* ---- machine control (typed) ---- */
void fpga_core_set_reset(int on);           /* 6502 reset, level — toggle low/high */
void fpga_core_set_pause(int on);
void fpga_core_set_freezer(int on);
void fpga_core_set_atari800(int on);
void fpga_set_ramconfig(uint16_t sel);
void fpga_set_performance(uint16_t speed, int vbl_restrict);
void fpga_set_cart(uint16_t cart_type);
void fpga_set_video(uint16_t mode, int pal, int scanlines, int csync);

/* ---- keyboard matrix (firmware is the source; write-only) ---- */
/* Write the full 64-bit matrix (bit n = KBCODE n) as four 16-bit regs. */
void fpga_kbd_matrix_write(uint16_t kbd0, uint16_t kbd1, uint16_t kbd2, uint16_t kbd3);
/* Convenience: set/clear a single matrix bit (0..63) in a shadow, then flush. */
void fpga_kbd_set(uint8_t kbcode, int pressed);
void fpga_kbd_clear_all(void);
void fpga_kbd_flush(void);                  /* push shadow to KBD0..3 */
/* Shift/Ctrl/Break — the KR2 non-matrix keys (KBD_SPECIAL bits). */
void fpga_kbd_special(int shift, int ctrl, int brk);

/* ---- console keys ---- */
void     fpga_console_inject(uint16_t bits);   /* CONSOLE_START_BIT etc. */
uint16_t fpga_console_phys_read(void);         /* physical switch state */

/* ---- joysticks (digital, inject/phys like console) ---- */
void     fpga_joy_write(int pair /*0=JOY01,1=JOY23*/, uint16_t a_field, uint16_t b_field);
uint16_t fpga_joy_phys_read(int pair /*0=JOY01,1=JOY23*/);   /* physical ports */

/* ---- paddles (analog, from STM32 ADCs) ---- */
void fpga_paddle_write(int pair /*0=PADDLE01,1=PADDLE23*/, uint8_t axis_a, uint8_t axis_b);

/* ---- physical ADC streams (normally written by DMA; these are for manual
 *      bring-up / test only) ----------------------------------------------
 * In normal operation the paddle and audio ADC registers are the destination
 * of a circular ADC->DMA2 stream and firmware never touches them. These
 * helpers let a test write a single raw 12-bit sample to one channel so the
 * FPGA threshold/latch path can be exercised without the ADC running.
 * chan is 0..7 (paddle) or 0..3 (audio); value is masked to 12 bits.
 * Out-of-range chan is ignored. */
void fpga_paddle_adc_write(unsigned chan, uint16_t value12);
void fpga_audio_adc_write(unsigned chan, uint16_t value12);

/* ---- freezer debug ---- */
void fpga_freeze_addr(uint16_t addr);
void fpga_freeze_data_ctrl(uint8_t data, int rd, int wr, int match);

/* ---- RAM apertures (banking windows into the big physical RAMs) ---- */
/* Two banked apertures reach the 2 GiB physical space through 8 MB / 7 MB FSMC
 * windows. Each aperture's high physical bits (A29..A22) come from an 8-bit
 * extension register; the physical word address of a window access is
 * (ext << 22) | FSMC_word_offset. Set the extension, then stream the window
 * (FPGA_APERTURE1_ADDR / FPGA_APERTURE2_ADDR). aperture is 1 or 2. */
void    fpga_aperture_set_ext(int aperture, uint8_t ext);
uint8_t fpga_aperture_get_ext(int aperture);
/* Convenience: physical word address currently reachable at window offset 0. */
uint32_t fpga_aperture_phys_base(int aperture);

/* ---- Atari memory window (bounded, direct FSMC indexing) ---- */
fpga_status_t fpga_atari_write(uint32_t off, const void *src, size_t len);
fpga_status_t fpga_atari_read(uint32_t off, void *dst, size_t len);

/* ---- interrupt controller ---- */
void     fpga_irq_enable(uint16_t mask);        /* set the enable mask       */
uint16_t fpga_irq_enabled(void);
uint16_t fpga_irq_pending(void);                /* read pending (enabled) bits */
void     fpga_irq_clear(uint16_t bits);         /* write-1-to-clear           */

/* ---- SIO UART ---- */
int      fpga_sio_rx_empty(void);
int      fpga_sio_tx_full(void);
uint16_t fpga_sio_tx_count(void);
int      fpga_sio_getc(uint8_t *out);
int      fpga_sio_putc(uint8_t c);
void     fpga_sio_set_divisor(uint8_t div);
uint16_t fpga_sio_framing_errors(void);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_FPGA_BUS_H */
