/*
 * si5351_modes.h
 *
 * Si5351A clock configuration — Tonnere board HDMI video modes.
 *
 * Three outputs per Si5351A:
 *   CLK0  pixel clock          27.000 MHz  (576p/480p)  or  74.250 MHz  (720p/1080i)
 *   CLK1  Atari system clock   ~7.094 MHz PAL  /  ~7.159 MHz NTSC
 *   CLK2  colour subcarrier    ~4.434 MHz PAL  /  ~3.580 MHz NTSC
 *
 * Reference crystal: 25 MHz.  Divider values per AN619 rev 0.8.
 * No external library required — just provide si5351_write() for your platform.
 *
 * Usage:
 *   si5351_init();                          // once after power-on
 *   si5351_apply_mode(SI5351_MODE_720_PAL); // call whenever mode changes
 *
 * Frequency accuracy:
 *   576p/i PAL@50                 CLK1 +0.1156 ppm   CLK2 +0.0277 ppm
 *   480p/i NTSC@59.94             CLK1 +0.1270 ppm   CLK2 +0.1270 ppm
 *   720p/1080i PAL@50             CLK1 -0.2094 ppm   CLK2 -0.0073 ppm
 *   720p/1080i PAL@50 Hi          CLK1 -0.0004 ppm   CLK2 -0.0073 ppm
 *   720p/1080i NTSC@60            CLK1 +0.1270 ppm   CLK2 +0.1270 ppm
 *   720p/1080i NTSC@60 Hi         CLK1 -0.0109 ppm   CLK2 +0.1270 ppm
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <stdint.h>

/* Implement this for your platform (STM32 HAL, ESP-IDF i2c_master, etc.) */
extern void si5351_write(uint8_t reg, uint8_t val);

/* ---------------------------------------------------------------------------
 * Register map (Si5351A, AN619 rev 0.8)
 * ------------------------------------------------------------------------- */
#define SI5351_REG_OE           3u   /* Output Enable Control (active low)  */
#define SI5351_REG_INT_MASK     2u   /* Interrupt mask                      */
#define SI5351_REG_CLK0_CTRL   16u   /* CLK0 control                        */
#define SI5351_REG_CLK1_CTRL   17u   /* CLK1 control                        */
#define SI5351_REG_CLK2_CTRL   18u   /* CLK2 control                        */
#define SI5351_REG_MSNA        26u   /* PLLA feedback MS parameters (26-33) */
#define SI5351_REG_MS0         42u   /* MS0 output divider (42-49)          */
#define SI5351_REG_MS1         50u   /* MS1 output divider (50-57)          */
#define SI5351_REG_MS2         58u   /* MS2 output divider (58-65)          */
#define SI5351_REG_PLL_RESET  177u   /* PLL soft reset                      */
#define SI5351_REG_XTAL_CL    183u   /* Crystal load capacitance            */
#define SI5351_REG_FANOUT     187u   /* Internal fanout enable              */

/* CLK control register bits */
#define SI5351_CLK_PDN        0x80u  /* Power down                          */
#define SI5351_CLK_INT        0x40u  /* Integer mode (lower jitter, b=0)    */
#define SI5351_CLK_PLL_A      0x00u  /* Source = PLLA                       */
#define SI5351_CLK_PLL_B      0x20u  /* Source = PLLB                       */
#define SI5351_CLK_SRC_MS     0x0Cu  /* Input = own multisynth              */
#define SI5351_CLK_IDRV_2MA   0x00u
#define SI5351_CLK_IDRV_4MA   0x01u
#define SI5351_CLK_IDRV_6MA   0x02u
#define SI5351_CLK_IDRV_8MA   0x03u

/* ---------------------------------------------------------------------------
 * Mode selection
 * ------------------------------------------------------------------------- */
typedef enum {
    SI5351_MODE_576_PAL           , /* 576p/i PAL@50 */
    SI5351_MODE_480_NTSC          , /* 480p/i NTSC@59.94 */
    SI5351_MODE_720_PAL           , /* 720p/1080i PAL@50 */
    SI5351_MODE_720_PAL_HI        , /* 720p/1080i PAL@50 Hi */
    SI5351_MODE_720_NTSC          , /* 720p/1080i NTSC@60 */
    SI5351_MODE_720_NTSC_HI         /* 720p/1080i NTSC@60 Hi */
} si5351_mode_t;

/* ---------------------------------------------------------------------------
 * One-time initialisation — call once after power-on / reset
 * ------------------------------------------------------------------------- */
static inline void si5351_init(void)
{
    /* Disable all outputs while we configure */
    si5351_write(SI5351_REG_OE,        0xFFu);
    /* Power down all CLK output drivers */
    si5351_write(SI5351_REG_CLK0_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK1_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);
    /* Mask all interrupts (INTR pin stays inactive) */
    si5351_write(SI5351_REG_INT_MASK,  0xF0u);
    /* Crystal load capacitance: 10 pF (bits[7:6]=0b11), preserve lower bits */
    si5351_write(SI5351_REG_XTAL_CL,   0xD2u);
    /* Enable MS and XTAL fanout buffers (required for multisynth routing) */
    si5351_write(SI5351_REG_FANOUT,    0x90u);
}

/* ---------------------------------------------------------------------------
 * Apply a video mode — programs PLLA, MS0/1/2, CLK0/1/2 and resets the PLL.
 * Safe to call while running; outputs are powered down during reconfiguration.
 * ------------------------------------------------------------------------- */
static inline void si5351_apply_mode(si5351_mode_t mode)
{
    /* Power down outputs during reconfiguration */
    si5351_write(SI5351_REG_CLK0_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK1_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);

    switch (mode) {
    case SI5351_MODE_576_PAL:
        /* ── 576p/i PAL@50 ──
         * VCO  = 810.0 MHz      PLLA: 32+2/5    P1=3635 P2=1 P3=5
         * CLK0 = 27.00000 MHz  pixel clock  MS0 integer div=30
         * CLK1 = 7093788.82 Hz  Atari clk    MS1: 114+26/141  P1=14103 P2=85 P3=141  (+0.1156 ppm)
         * CLK2 = 4433618.87 Hz  fsc          MS2: 182+139/200  P1=22872 P2=192 P3=200  (+0.0277 ppm)
         */
        /* PLLA feedback multisynth (regs 26-33) */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x05u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Eu);  /* reg  29 */
        si5351_write(0x1Eu, 0x33u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x01u);  /* reg  33 */
        /* MS0: 27.000 MHz pixel clock — integer divide, INT bit set (regs 42-49) */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x0Du);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: 7093788.82 Hz Atari system clock (regs 50-57) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x8Du);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x37u);  /* reg  53 */
        si5351_write(0x36u, 0x17u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x55u);  /* reg  57 */
        /* MS2: 4433618.87 Hz colour subcarrier (regs 58-65) */
        si5351_write(0x3Au, 0x00u);  /* reg  58 */
        si5351_write(0x3Bu, 0xC8u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x59u);  /* reg  61 */
        si5351_write(0x3Eu, 0x58u);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0xC0u);  /* reg  65 */
        /* CLK control: PLLA, MS_OWN, 8 mA; CLK0 INT bit set */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);  /* INT|PLLA|MS_OWN|8mA */
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        break;

    case SI5351_MODE_480_NTSC:
        /* ── 480p/i NTSC@59.94 ──
         * VCO  = 810.0 MHz      PLLA: 32+2/5    P1=3635 P2=1 P3=5
         * CLK0 = 27.00000 MHz  pixel clock  MS0 integer div=30
         * CLK1 = 7159090.91 Hz  Atari clk    MS1: 113+1/7  P1=13970 P2=2 P3=7  (+0.1270 ppm)
         * CLK2 = 3579545.45 Hz  fsc          MS2: 226+2/7  P1=28452 P2=4 P3=7  (+0.1270 ppm)
         */
        /* PLLA feedback multisynth (regs 26-33) */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x05u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Eu);  /* reg  29 */
        si5351_write(0x1Eu, 0x33u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x01u);  /* reg  33 */
        /* MS0: 27.000 MHz pixel clock — integer divide, INT bit set (regs 42-49) */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x0Du);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: 7159090.91 Hz Atari system clock (regs 50-57) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x07u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x36u);  /* reg  53 */
        si5351_write(0x36u, 0x92u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x02u);  /* reg  57 */
        /* MS2: 3579545.45 Hz colour subcarrier (regs 58-65) */
        si5351_write(0x3Au, 0x00u);  /* reg  58 */
        si5351_write(0x3Bu, 0x07u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x6Fu);  /* reg  61 */
        si5351_write(0x3Eu, 0x24u);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0x04u);  /* reg  65 */
        /* CLK control: PLLA, MS_OWN, 8 mA; CLK0 INT bit set */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);  /* INT|PLLA|MS_OWN|8mA */
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        break;

    case SI5351_MODE_720_PAL:
        /* ── 720p/1080i PAL@50 ──
         * VCO  = 742.5 MHz      PLLA: 29+7/10    P1=3289 P2=6 P3=10
         * CLK0 = 74.25000 MHz  pixel clock  MS0 integer div=10
         * CLK1 = 7093786.51 Hz  Atari clk    MS1: 104+93/139  P1=12885 P2=89 P3=139  (-0.2094 ppm)
         * CLK2 = 4433618.72 Hz  fsc          MS2: 167+167/355  P1=20924 P2=76 P3=355  (-0.0073 ppm)
         */
        /* PLLA feedback multisynth (regs 26-33) */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x0Au);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Cu);  /* reg  29 */
        si5351_write(0x1Eu, 0xD9u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x06u);  /* reg  33 */
        /* MS0: 74.250 MHz pixel clock — integer divide, INT bit set (regs 42-49) */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x03u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: 7093786.51 Hz Atari system clock (regs 50-57) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x8Bu);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x32u);  /* reg  53 */
        si5351_write(0x36u, 0x55u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x59u);  /* reg  57 */
        /* MS2: 4433618.72 Hz colour subcarrier (regs 58-65) */
        si5351_write(0x3Au, 0x01u);  /* reg  58 */
        si5351_write(0x3Bu, 0x63u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x51u);  /* reg  61 */
        si5351_write(0x3Eu, 0xBCu);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0x4Cu);  /* reg  65 */
        /* CLK control: PLLA, MS_OWN, 8 mA; CLK0 INT bit set */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);  /* INT|PLLA|MS_OWN|8mA */
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        break;

    case SI5351_MODE_720_PAL_HI:
        /* ── 720p/1080i PAL@50 Hi ──
         * VCO  = 742.5 MHz      PLLA: 29+7/10    P1=3289 P2=6 P3=10
         * CLK0 = 74.25000 MHz  pixel clock  MS0 integer div=10
         * CLK1 = 7093788.00 Hz  Atari clk    MS1: 104+6382/9539  P1=12885 P2=6081 P3=9539  (-0.0004 ppm)
         * CLK2 = 4433618.72 Hz  fsc          MS2: 167+167/355  P1=20924 P2=76 P3=355  (-0.0073 ppm)
         */
        /* PLLA feedback multisynth (regs 26-33) */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x0Au);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Cu);  /* reg  29 */
        si5351_write(0x1Eu, 0xD9u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x06u);  /* reg  33 */
        /* MS0: 74.250 MHz pixel clock — integer divide, INT bit set (regs 42-49) */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x03u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: 7093788.00 Hz Atari system clock (regs 50-57) */
        si5351_write(0x32u, 0x25u);  /* reg  50 */
        si5351_write(0x33u, 0x43u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x32u);  /* reg  53 */
        si5351_write(0x36u, 0x55u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x17u);  /* reg  56 */
        si5351_write(0x39u, 0xC1u);  /* reg  57 */
        /* MS2: 4433618.72 Hz colour subcarrier (regs 58-65) */
        si5351_write(0x3Au, 0x01u);  /* reg  58 */
        si5351_write(0x3Bu, 0x63u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x51u);  /* reg  61 */
        si5351_write(0x3Eu, 0xBCu);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0x4Cu);  /* reg  65 */
        /* CLK control: PLLA, MS_OWN, 8 mA; CLK0 INT bit set */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);  /* INT|PLLA|MS_OWN|8mA */
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        break;

    case SI5351_MODE_720_NTSC:
        /* ── 720p/1080i NTSC@60 ──
         * VCO  = 742.5 MHz      PLLA: 29+7/10    P1=3289 P2=6 P3=10
         * CLK0 = 74.25000 MHz  pixel clock  MS0 integer div=10
         * CLK1 = 7159090.91 Hz  Atari clk    MS1: 103+5/7  P1=12763 P2=3 P3=7  (+0.1270 ppm)
         * CLK2 = 3579545.45 Hz  fsc          MS2: 207+3/7  P1=26038 P2=6 P3=7  (+0.1270 ppm)
         */
        /* PLLA feedback multisynth (regs 26-33) */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x0Au);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Cu);  /* reg  29 */
        si5351_write(0x1Eu, 0xD9u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x06u);  /* reg  33 */
        /* MS0: 74.250 MHz pixel clock — integer divide, INT bit set (regs 42-49) */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x03u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: 7159090.91 Hz Atari system clock (regs 50-57) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x07u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x31u);  /* reg  53 */
        si5351_write(0x36u, 0xDBu);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x03u);  /* reg  57 */
        /* MS2: 3579545.45 Hz colour subcarrier (regs 58-65) */
        si5351_write(0x3Au, 0x00u);  /* reg  58 */
        si5351_write(0x3Bu, 0x07u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x65u);  /* reg  61 */
        si5351_write(0x3Eu, 0xB6u);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0x06u);  /* reg  65 */
        /* CLK control: PLLA, MS_OWN, 8 mA; CLK0 INT bit set */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);  /* INT|PLLA|MS_OWN|8mA */
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        break;

    case SI5351_MODE_720_NTSC_HI:
        /* ── 720p/1080i NTSC@60 Hi ──
         * VCO  = 742.5 MHz      PLLA: 29+7/10    P1=3289 P2=6 P3=10
         * CLK0 = 74.25000 MHz  pixel clock  MS0 integer div=10
         * CLK1 = 7159089.92 Hz  Atari clk    MS1: 103+7138/9993  P1=12763 P2=4301 P3=9993  (-0.0109 ppm)
         * CLK2 = 3579545.45 Hz  fsc          MS2: 207+3/7  P1=26038 P2=6 P3=7  (+0.1270 ppm)
         */
        /* PLLA feedback multisynth (regs 26-33) */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x0Au);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Cu);  /* reg  29 */
        si5351_write(0x1Eu, 0xD9u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x06u);  /* reg  33 */
        /* MS0: 74.250 MHz pixel clock — integer divide, INT bit set (regs 42-49) */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x03u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: 7159089.92 Hz Atari system clock (regs 50-57) */
        si5351_write(0x32u, 0x27u);  /* reg  50 */
        si5351_write(0x33u, 0x09u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x31u);  /* reg  53 */
        si5351_write(0x36u, 0xDBu);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x10u);  /* reg  56 */
        si5351_write(0x39u, 0xCDu);  /* reg  57 */
        /* MS2: 3579545.45 Hz colour subcarrier (regs 58-65) */
        si5351_write(0x3Au, 0x00u);  /* reg  58 */
        si5351_write(0x3Bu, 0x07u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x65u);  /* reg  61 */
        si5351_write(0x3Eu, 0xB6u);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0x06u);  /* reg  65 */
        /* CLK control: PLLA, MS_OWN, 8 mA; CLK0 INT bit set */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);  /* INT|PLLA|MS_OWN|8mA */
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);  /* PLLA|MS_OWN|8mA    */
        break;

    }

    /* Soft-reset PLLA — forces VCO to re-lock at the new frequency.
     * Outputs glitch briefly; re-enable only after this write. */
    si5351_write(SI5351_REG_PLL_RESET, 0x20u);

    /* Enable CLK0, CLK1, CLK2 outputs (register is active-low, bits 7:3 stay high) */
    si5351_write(SI5351_REG_OE, 0xF8u);
}
