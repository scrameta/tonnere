/*
 * fpga_bus_map.h — TonnereXL FSMC/FPGA register map (native 16-bit).
 *
 * v0.2: flat 16-bit, purpose-named. Replaces the ZPU 32-bit-word map and the
 * v0.1 half-split scheme entirely. See docs/fpga_interface.md.
 *
 * Register index -> FSMC offset = FPGA_WIN_REGS + 2*index.
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

/* Region offsets within the FSMC window (bytes). TODO(mark): confirm. */
#define FPGA_WIN_REGS    0x00000000u   /* 16-bit register file            */
#define FPGA_WIN_ATARI   0x00010000u   /* 64 KB live Atari address space  */
#define FPGA_WIN_UART    0x00020000u   /* SIO UART FIFOs                   */

#define FPGA_WIN_ATARI_BYTES  0x10000u /* 64 KB */

/* A register's absolute uint16* address, given its logical index. */
#define FPGA_REG_ADDR(idx) \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_WIN_REGS + ((uint32_t)(idx) * 2u)))

/* Atari window base as uint16*. */
#define FPGA_ATARI_ADDR \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_WIN_ATARI))

/* SIO UART register absolute address, given its logical index. */
#define FPGA_UART_ADDR(idx) \
    ((volatile uint16_t *)(FPGA_BUS_BASE + FPGA_WIN_UART + ((uint32_t)(idx) * 2u)))

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
    REG_TURBO_DRIVE,
    REG_CART,
    REG_VIDEO,
    /* keyboard (W) + console */
    REG_KBD0,
    REG_KBD1,
    REG_KBD2,
    REG_KBD3,
    REG_CONSOLE_INJECT,   /* W */
    REG_CONSOLE_PHYS,     /* R */
    /* controllers (W) */
    REG_JOY01,
    REG_JOY23,
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
    REG_COUNT
};

/* ------------------------------------------------------------------ */
/* SIO UART register indices (separate region)                         */
/* ------------------------------------------------------------------ */
enum fpga_uart_index {
    UART_TX = 0,          /* W  bits 7:0 enqueue                        */
    UART_TX_FIFO,         /* R  full@9 empty@8 count7:0                 */
    UART_RX,              /* R  data7:0 div14:8; advances RX FIFO       */
    UART_RX_FIFO,         /* R  full@9 empty@8 count7:0                 */
    UART_DIVISOR,         /* RW tx div / rx div                         */
    UART_FRAMING_ERR,     /* R  serial@0 siocmd@1; read clears          */
    UART_COUNT
};

/* UART FIFO status bits. */
#define UART_FIFO_EMPTY   (1u << 8)
#define UART_FIFO_FULL    (1u << 9)
#define UART_FIFO_COUNT_M 0x00ffu

/* ------------------------------------------------------------------ */
/* Bitfields                                                           */
/* ------------------------------------------------------------------ */
/* CONTROL */
#define CTRL_PAUSE_BIT        0
#define CTRL_WARM_RESET_BIT   1
#define CTRL_COLD_RESET_BIT   2   /* strobe */
#define CTRL_FREEZER_EN_BIT   3
#define CTRL_ATARI800_BIT     4

/* RAMCONFIG */
#define RAMCFG_SEL_SHIFT      0
#define RAMCFG_SEL_MASK       0x07u

/* PERFORMANCE */
#define PERF_SPEED_SHIFT      0
#define PERF_SPEED_MASK       0x3fu  /* 6502 speed/turbo select */
#define PERF_VBL_RESTRICT_BIT 8

/* TURBO_DRIVE */
#define TURBO_DRIVE_SHIFT     0
#define TURBO_DRIVE_MASK      0x07u

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
#define CONSOLE_RESET_BIT     3

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

/* IRQ controller source bits */
#define IRQ_SIO_CMD_BIT       0
#define IRQ_UART_RX_BIT       1
#define IRQ_UART_TX_BIT       2
#define IRQ_POTGO_BIT         3
#define IRQ_DMA_DONE_BIT      4

/* ------------------------------------------------------------------ */
/* Identity                                                            */
/* ------------------------------------------------------------------ */
#define FPGA_IFACE_MAGIC    0x584Cu   /* 'XL' — TODO(mark): finalise */
#define FPGA_IFACE_VERSION  0x0001u

/*
 * Atari 800XL keyboard matrix: 64 switches, bit n = KBCODE n. Break and both
 * Shift keys occupy their real matrix positions. KBD0=bits15:0, KBD1=31:16,
 * KBD2=47:32, KBD3=63:48.
 * TODO(mark): confirm KBCODE->bit assignment against the RTL matrix scan;
 * firmware fills this from the standard 800XL KBCODE table.
 */

/* ------------------------------------------------------------------ */
/* Firmware-side GPIO the RTL does NOT own. TODO(mark): real pins.     */
/* ------------------------------------------------------------------ */
/* SD card-detect/write-protect are STM32-side (SD is not on the FPGA). */
#define FPGA_SD_CD_ACTIVE_LOW   1   /* TODO(mark): confirm */
#define FPGA_SD_WP_ACTIVE_HIGH  1   /* TODO(mark): confirm */
/* TODO(mark): FPGA_SD_CD_GPIO_PORT / _PIN / EXTI line */
/* TODO(mark): FPGA_SD_WP_GPIO_PORT / _PIN */
/* TODO(mark): FPGA_IRQ_GPIO_PORT / _PIN / EXTI line (contract §6) */

#endif /* TONNEREXL_FPGA_BUS_MAP_H */
