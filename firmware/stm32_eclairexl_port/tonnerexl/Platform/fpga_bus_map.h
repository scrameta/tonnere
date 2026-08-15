/*
 * fpga_bus_map.h — TonnereXL FSMC/FPGA register map (native 16-bit).
 *
 * v0.4: aperture-banked addressing. Two banked RAM apertures + a fixed region.
 * See docs/fpga_interface.md §2.
 *
 * FSMC A22..A19 select the region:
 *   A22=0                  -> Aperture 1 (8 MB window, A21..A0)
 *   A22=1, A22..A19!=1111  -> Aperture 2 (7 MB window)
 *   A22..A19 = 1111        -> Fixed region (1 MB, never banked)
 * Within the fixed region, offset bits A18..A16 pick the slot:
 *   000 -> Atari window,  110 -> SIO handler,  111 -> Register file.
 * Each aperture's high physical bits A29..A22 come from an 8-bit extension
 * register (APERTURE1_EXT / APERTURE2_EXT); phys = EXT[7:0] & FSMC_A21..A0.
 *
 * Values marked TODO(mark) are firmware defaults the RTL/board finalises.
 */
#ifndef TONNEREXL_FPGA_BUS_MAP_H
#define TONNEREXL_FPGA_BUS_MAP_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Physical base. FSMC Bank1 NE1.                                      */
/* ------------------------------------------------------------------ */
#ifndef FPGA_BUS_BASE
#define FPGA_BUS_BASE  0x60000000u
#endif

/* --- Banked RAM apertures (byte offsets within the FSMC window) ---- */
/* Aperture 1: A22=0,          byte 0x000_0000 .. 0x07F_FFFF (8 MB).    */
/* Aperture 2: A22=1 (!=1111), byte 0x080_0000 .. 0x0EF_FFFF (7 MB).    */
#define FPGA_APERTURE1_BASE  0x00000000u
#define FPGA_APERTURE2_BASE  0x00800000u

/* --- Fixed region slots (byte offsets; never banked) --------------- */
/* Fixed region = FSMC A22..A19=1111 (word 0x780000 = byte 0xF00000).  */
/* Slot = offset bits A18..A16: 000 Atari / 110 SIO / 111 Registers.   */
#define FPGA_WIN_ATARI   0x00F00000u   /* slot 000: 64 KB Atari window  */
#define FPGA_WIN_SIO     0x00FC0000u   /* slot 110: SIO handler         */
#define FPGA_WIN_REGS    0x00FE0000u   /* slot 111: 16-bit register file*/

#define FPGA_WIN_ATARI_BYTES  0x10000u /* 64 KB */

/* A register's absolute uint16* address, given its logical index. */
#define FPGA_REG_ADDR(idx) \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_WIN_REGS + ((uint32_t)(idx) * 2u)))

/* Atari window base as uint16*. */
#define FPGA_ATARI_ADDR \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_WIN_ATARI))

/* SIO handler register absolute address, given its logical index. */
#define FPGA_SIO_ADDR(idx) \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_WIN_SIO + ((uint32_t)(idx) * 2u)))

/* Aperture window base as uint16* (for streaming after setting EXT). */
#define FPGA_APERTURE1_ADDR \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_APERTURE1_BASE))
#define FPGA_APERTURE2_ADDR \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_APERTURE2_BASE))

/* ------------------------------------------------------------------ */
/* Register indices                                                    */
/* ------------------------------------------------------------------ */
enum fpga_reg_index {
    /* identity (R) */
    REG_IFACE_MAGIC = 0,
    REG_IFACE_VERSION,
    /* machine control (W) */
    REG_CONTROL,
    REG_RAMCONFIG,
    REG_PERFORMANCE,
    REG_CART,
    REG_VIDEO,
    /* keyboard (W) */
    REG_KBD0,
    REG_KBD1,
    REG_KBD2,
    REG_KBD3,
    REG_KBD_SPECIAL,      /* W: shift/ctrl/break (KR2 non-matrix keys) */
    /* console keys (Start/Select/Option) — inject/phys */
    REG_CONSOLE_INJECT,   /* W */
    REG_CONSOLE_PHYS,     /* R */
    /* joysticks — inject/phys (digital) */
    REG_JOY01,            /* W: inject joy 0+1 */
    REG_JOY23,            /* W: inject joy 2+3 */
    REG_JOY01_PHYS,       /* R: physical joy ports 0+1 */
    REG_JOY23_PHYS,       /* R: physical joy ports 2+3 */
    /* paddles (W, STM32 ADC values) */
    REG_PADDLE01,
    REG_PADDLE23,
    /* freezer debug (W) */
    REG_FREEZE_ADDR,
    REG_FREEZE_DATA_CTRL,
    /* interrupt controller */
    REG_IRQ_ENABLE,       /* RW */
    REG_IRQ_PENDING,      /* R  */
    REG_IRQ_CLEAR,        /* W1C */
    /* debug scratch (RW) */
    REG_DEBUG0,
    REG_DEBUG1,
    REG_DEBUG2,
    REG_DEBUG3,
    /* Aperture extension registers: 8-bit high physical address (A29..A22)
     * for each banked RAM aperture. phys = EXT[7:0] & FSMC_A21..A0. Write-only;
     * firmware shadows the values. See docs/fpga_interface.md §2.2. */
    REG_APERTURE1_EXT,   /* aperture 1 (A22=0, 8 MB window) */
    REG_APERTURE2_EXT,   /* aperture 2 (A22=1 !=1111, 7 MB window) */

    /* Physical paddle ADC stream (W; ADC2->DMA2 circular target).
     * Eight CONSECUTIVE 16-bit write-only regs; the register index is the
     * ADC rank/channel identity. data[11:0] = right-aligned 12-bit sample,
     * bits 15:12 ignored by the FPGA. DMA streams rank 0..7 -> ADC0..ADC7
     * with MINC=1/NDTR=8/CIRC=1. Contiguity is load-bearing — do not reorder.
     * See docs/fpga_interface.md §3 "Physical paddle ADC stream". */
    REG_PADDLE_ADC0,     /* index 31 */
    REG_PADDLE_ADC1,
    REG_PADDLE_ADC2,
    REG_PADDLE_ADC3,
    REG_PADDLE_ADC4,
    REG_PADDLE_ADC5,
    REG_PADDLE_ADC6,
    REG_PADDLE_ADC7,     /* index 38 */

    /* Audio ADC stream (W; second ADC->DMA2 circular target).
     * Four CONSECUTIVE 16-bit write-only regs, same pattern as the paddle
     * stream. Timer-triggered 4-rank scan at ~44.1 kHz/channel (CONT=0).
     * See docs/fpga_interface.md §3 "Audio ADC inputs". */
    REG_AUDIO_ADC0,      /* index 39 */
    REG_AUDIO_ADC1,
    REG_AUDIO_ADC2,
    REG_AUDIO_ADC3,      /* index 42 */

    REG_COUNT
};

/* Compile-time guarantee of the contiguity the DMA contract relies on. */
_Static_assert(REG_PADDLE_ADC7 - REG_PADDLE_ADC0 == 7, "paddle ADC regs must be contiguous");
_Static_assert(REG_AUDIO_ADC3  - REG_AUDIO_ADC0  == 3, "audio ADC regs must be contiguous");
_Static_assert(REG_PADDLE_ADC0 == 31, "paddle ADC base index must match fpga_interface.md");
_Static_assert(REG_AUDIO_ADC0  == 39, "audio ADC base index must match fpga_interface.md");

/* Absolute FSMC halfword addresses of the two DMA destinations.
 * These are the exact byte addresses quoted in fpga_interface.md §3:
 *   PADDLE_ADC0 = 0x60FE_003E   AUDIO_ADC0 = 0x60FE_004E
 * Kept as macros so the ADC/DMA driver can hand them straight to the DMA
 * memory-address field without recomputing the aperture math. */
#define FPGA_PADDLE_ADC_ADDR  FPGA_REG_ADDR(REG_PADDLE_ADC0)
#define FPGA_AUDIO_ADC_ADDR   FPGA_REG_ADDR(REG_AUDIO_ADC0)
#define FPGA_PADDLE_ADC_COUNT 8u
#define FPGA_AUDIO_ADC_COUNT  4u

/* ------------------------------------------------------------------ */
/* SIO UART register indices (separate region)                         */
/* ------------------------------------------------------------------ */
/* SIO handler register map — matches sio_handler.vhdl (addr 0-5). */
enum fpga_sio_index {
    SIO_TX = 0,           /* W  bits 7:0 transmit byte                    */
    SIO_TX_FIFO,          /* R  full@9 empty@8 count7:0                   */
    SIO_RX,               /* R  data 14:0; reading advances RX FIFO       */
    SIO_RX_FIFO,          /* R  full@9 empty@8 count7:0                   */
    SIO_DIVISOR,          /* W  divisor (applied after tx done); R rx div */
    SIO_FRAMING_ERR,      /* R  serial@0 command@1; auto-clear on read    */
    SIO_COUNT
};

/* SIO FIFO status bits. */
#define SIO_FIFO_EMPTY   (1u << 8)
#define SIO_FIFO_FULL    (1u << 9)
#define SIO_FIFO_COUNT_M 0x00ffu

/* ------------------------------------------------------------------ */
/* Bitfields                                                           */
/* ------------------------------------------------------------------ */
/* CONTROL */
#define CTRL_RESET_BIT        0   /* 6502 reset, LEVEL (SW toggles low/high) */
#define CTRL_PAUSE_BIT        1
#define CTRL_FREEZER_EN_BIT   2
#define CTRL_ATARI800_BIT     3
/* No cold/warm distinction: RAM clear is done via FSMC->RAM DMA writes. */

/* RAMCONFIG */
#define RAMCFG_SEL_SHIFT      0
#define RAMCFG_SEL_MASK       0x07u

/* PERFORMANCE */
#define PERF_SPEED_SHIFT      0
#define PERF_SPEED_MASK       0x3fu  /* 6502 speed/turbo select */
#define PERF_VBL_RESTRICT_BIT 8

/* CART */
#define CART_SEL_SHIFT        0
#define CART_SEL_MASK         0x3fu

/* VIDEO */
#define VIDEO_MODE_SHIFT      0
#define VIDEO_MODE_MASK       0x07u
#define VIDEO_PAL_BIT         4
#define VIDEO_SCANLINES_BIT   5
#define VIDEO_CSYNC_BIT       6

/* CONSOLE_INJECT / CONSOLE_PHYS (same bit layout) */
#define CONSOLE_START_BIT     0
#define CONSOLE_SELECT_BIT    1
#define CONSOLE_OPTION_BIT    2
/* (reset is CTRL_RESET_BIT in CONTROL, not a console key) */

/* KBD_SPECIAL: the KR2 non-matrix keys (shift/ctrl/break). */
#define KBD_SPECIAL_SHIFT_BIT   0
#define KBD_SPECIAL_CTRL_BIT    1
#define KBD_SPECIAL_BREAK_BIT   2

/* JOYxx: joystick A in bits 4:0, joystick B in bits 12:8 */
#define JOY_A_SHIFT           0
#define JOY_B_SHIFT           8
#define JOY_DIR_MASK          0x0fu   /* 4 direction bits */
#define JOY_TRIGGER_BIT       4       /* within each 5-bit field */
#define JOY_FIELD_MASK        0x1fu

/* PADDLExx: axis A in bits 7:0, axis B in bits 15:8 */
#define PADDLE_A_SHIFT        0
#define PADDLE_B_SHIFT        8
#define PADDLE_AXIS_MASK      0xffu

/* FREEZE_DATA_CTRL */
#define FREEZE_DATA_SHIFT     0
#define FREEZE_DATA_MASK      0xffu
#define FREEZE_READ_BIT       8
#define FREEZE_WRITE_BIT      9
#define FREEZE_MATCH_BIT      10

/* IRQ controller source bits (edge-triggered; see docs/fpga_interface.md §6).
 * Each source latches on the SPECIFIC edge noted — one edge, not a level.
 * Most are rising; POT_RESET is deliberately FALLING. */
#define IRQ_SIO_CMD_BIT       0   /* rising:  new SIO command frame           */
#define IRQ_SIO_RX_BIT        1   /* rising:  RX FIFO empty->non-empty        */
#define IRQ_SIO_TX_BIT        2   /* rising:  TX FIFO drained (drain->empty)  */
#define IRQ_POT_RESET_LH_BIT  3   /* rising:  POT_RESET — enter paddle discharge  */
#define IRQ_POT_RESET_HL_BIT  4   /* FALLING: POT_RESET 1->0 — release paddle */
                                  /*          pins back to analogue mode      */

/* ------------------------------------------------------------------ */
/* Identity                                                            */
/* ------------------------------------------------------------------ */
#define FPGA_IFACE_MAGIC    0x584Cu   /* 'XL' — TODO(mark): finalise */
#define FPGA_IFACE_VERSION  0x0003u   /* fpga_interface.md v0.6 (paddle+audio ADC) */

/* ------------------------------------------------------------------ */
/* Firmware-side GPIO (from the board netlist / CubeMX pinlist).        */
/* These are STM32-side signals, not FPGA registers. Guarded to the     */
/* on-target build since they reference STM32 HAL GPIO symbols.         */
/* ------------------------------------------------------------------ */
#if defined(FPGA_BUS_STM32)

/* FPGA -> STM32 interrupt line: PG12 (net FPGA.IRQ). EXTI12.          */
#define FPGA_IRQ_GPIO_PORT      GPIOG
#define FPGA_IRQ_GPIO_PIN       GPIO_PIN_12
#define FPGA_IRQ_EXTI_IRQn      EXTI15_10_IRQn

/* SD card-detect: PC13 (net SD_DETECT).                               */
#define FPGA_SD_CD_GPIO_PORT    GPIOC
#define FPGA_SD_CD_GPIO_PIN     GPIO_PIN_13

/* SD write-protect: PD3 (schematic net SD.WP; the pinlist .md mislabelled
 * this pin as a generic net, so an earlier revision wrongly assumed no WP). */
#define FPGA_SD_WP_GPIO_PORT    GPIOD
#define FPGA_SD_WP_GPIO_PIN     GPIO_PIN_3

/* SD write-protect: PD3 (SD.WP via RN7; pinlist net label was generic  */
/* Net-(RN7-R2.2) but the schematic shows it as SD.WP).                 */
#define FPGA_SD_WP_GPIO_PORT    GPIOD
#define FPGA_SD_WP_GPIO_PIN     GPIO_PIN_3

/* Paddle ADC input pins, in DMA rank order (rank n -> PADDLE_ADCn).
 * From the CubeMX pinlist with board 0.51 errata applied. The rank order
 * here is the channel identity the FPGA decodes from the register address,
 * so it MUST match the ADC1 regular-sequence rank order in adc_dma.c.
 *
 * These are needed for the POTGO/POT_RESET clamp: on POTGO the ISR switches
 * these pins analogue->output-low (discharge); on POT_RESET falling it
 * switches them back to analogue. Rank order (see adc_dma.c):
 *   rank 0..3 : PC0 PC1 PC2 PC3   (JOY.POT0-3,  ADC_IN10..13)
 *   rank 4..7 : PA0 PA1 PA4 PA5   (JOY2.POT4-7, ADC_IN0,1,4,5)
 * Note the paddle pins are split across GPIOA and GPIOC, so the "one MODER
 * write" optimisation in the design doc is not available here — the clamp
 * touches two ports. See adc_dma.c: paddle_pins_clamp()/release(). */
#define FPGA_PADDLE_PC_PORT     GPIOC
#define FPGA_PADDLE_PC_PINS     (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)
#define FPGA_PADDLE_PA_PORT     GPIOA
#define FPGA_PADDLE_PA_PINS     (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5)

/* Other FPGA config/status lines from the netlist, for later use:     */
/*   PG11 FPGA.PS_N, PG13 FPGA.CONFIG_N, PG14 FPGA.STATUS_N,           */
/*   PG15 FPGA.CONF_DONE — FPGA (re)configuration control.             */
#define FPGA_CONFIG_N_PORT      GPIOG
#define FPGA_CONFIG_N_PIN       GPIO_PIN_13
#define FPGA_STATUS_N_PORT      GPIOG
#define FPGA_STATUS_N_PIN       GPIO_PIN_14
#define FPGA_CONF_DONE_PORT     GPIOG
#define FPGA_CONF_DONE_PIN      GPIO_PIN_15
#define FPGA_PS_N_PORT          GPIOG
#define FPGA_PS_N_PIN           GPIO_PIN_11

#endif /* FPGA_BUS_STM32 */

/* Board characteristics (target-independent, safe on host). */
#define FPGA_SD_CD_ACTIVE_LOW   1   /* TODO(mark): confirm CD polarity via RN7 */
#define FPGA_SD_HAS_WP          1   /* write-protect on PD3 (via RN7) */
#define FPGA_SD_WP_ACTIVE_HIGH  1   /* TODO(mark): confirm WP polarity */

#endif /* TONNEREXL_FPGA_BUS_MAP_H */
