/*
 * si5351_modes.h
 *
 * Si5351A clock configuration — Tonnere board.
 *
 * Per-device outputs:
 *   CLK0  HDMI pixel clock (ED/HD modes) OR Atari master clock (original modes)
 *   CLK1  Atari clock = PHI2 x 4, frequency-locked so the Atari's cycles/frame
 *         produce the target vsync exactly (FPGA multiplies x4 for the master clock)
 *   CLK2  real PAL/NTSC colour subcarrier — original (analogue RGB) modes only
 *
 * Reference crystal: 25 MHz.  Divider values per AN619 rev 0.8.
 * All frequencies are exact rationals — every mode resolves to 0.000 ppm.
 *
 * Atari cycles per frame:  PAL = 624*228*4 = 569088   NTSC = 524*228*4 = 477888
 * (Atari is progressive: both fields identical, so 524 lines not 525.)
 *
 * Usage:
 *   si5351_init();                          // once after power-on
 *   si5351_apply_mode(SI5351_MODE_720_PAL); // program a mode (resets PLL)
 *
 * ---------------------------------------------------------------------------
 * Mode            Enum                           CLK0(MHz)   CLK1(MHz)   CLK2(MHz)  Vsync(Hz)
 * --------------- ---------------------------- ----------- ----------- ----------- ----------
 * 576p PAL@50     SI5351_MODE_576_PAL           27.0000000   7.1136000           —  50.000000
 * 480p NTSC@59.94 SI5351_MODE_480_NTSC          27.0000000   7.1611588           —  59.940060
 * 480p NTSC@60    SI5351_MODE_480_NTSC_60       27.0000000   7.1683200           —  60.000000
 * 720p PAL@50     SI5351_MODE_720_PAL           74.2500000   7.1136000           —  50.000000
 * 720p NTSC@60    SI5351_MODE_720_NTSC          74.2500000   7.1683200           —  60.000000
 * 720p NTSC@59.94 SI5351_MODE_720_NTSC_5994     74.1758242   7.1611588           —  59.940060
 * PAL original    SI5351_MODE_PAL_ORIGINAL      28.3750000   7.0937500   4.4336187  49.860479
 * NTSC original   SI5351_MODE_NTSC_ORIGINAL     28.6363636   7.1590909   3.5795455  59.922751
 * ---------------------------------------------------------------------------
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
#define SI5351_REG_OE           3u
#define SI5351_REG_INT_MASK     2u
#define SI5351_REG_CLK0_CTRL   16u
#define SI5351_REG_CLK1_CTRL   17u
#define SI5351_REG_CLK2_CTRL   18u
#define SI5351_REG_MSNA        26u   /* PLLA feedback MS (26-33) */
#define SI5351_REG_MS0         42u   /* MS0 output divider (42-49) */
#define SI5351_REG_MS1         50u   /* MS1 output divider (50-57) */
#define SI5351_REG_MS2         58u   /* MS2 output divider (58-65) */
#define SI5351_REG_PLL_RESET  177u
#define SI5351_REG_XTAL_CL    183u
#define SI5351_REG_FANOUT     187u

#define SI5351_CLK_PDN        0x80u  /* Power down                */
#define SI5351_CLK_INT        0x40u  /* Integer mode (low jitter) */
#define SI5351_CLK_PLL_A      0x00u  /* Source = PLLA             */
#define SI5351_CLK_SRC_MS     0x0Cu  /* Input = own multisynth   */
#define SI5351_CLK_IDRV_8MA   0x03u

/* ---------------------------------------------------------------------------
 * Mode selection
 * ------------------------------------------------------------------------- */
typedef enum {
    SI5351_MODE_576_PAL         , /* 576p PAL@50 */
    SI5351_MODE_480_NTSC        , /* 480p NTSC@59.94 */
    SI5351_MODE_480_NTSC_60     , /* 480p NTSC@60 */
    SI5351_MODE_720_PAL         , /* 720p PAL@50 */
    SI5351_MODE_720_NTSC        , /* 720p NTSC@60 */
    SI5351_MODE_720_NTSC_5994   , /* 720p NTSC@59.94 */
    SI5351_MODE_PAL_ORIGINAL    , /* PAL original */
    SI5351_MODE_NTSC_ORIGINAL     /* NTSC original */
} si5351_mode_t;

/* ---------------------------------------------------------------------------
 * One-time initialisation
 * ------------------------------------------------------------------------- */
static inline void si5351_init(void)
{
    si5351_write(SI5351_REG_OE,        0xFFu);
    si5351_write(SI5351_REG_CLK0_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK1_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_INT_MASK,  0xF0u);
    si5351_write(SI5351_REG_XTAL_CL,   0xD2u);  /* 10 pF load */
    si5351_write(SI5351_REG_FANOUT,    0x90u);  /* MS + XTAL fanout enable */
}

/* ---------------------------------------------------------------------------
 * Apply a mode — programs PLLA, the multisynths, and the CLK controls,
 * then resets the PLL. The FPGA PLL loses lock briefly and recovers.
 * ------------------------------------------------------------------------- */
static inline void si5351_apply_mode(si5351_mode_t mode)
{
    si5351_write(SI5351_REG_CLK0_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK1_CTRL, SI5351_CLK_PDN);
    si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);

    switch (mode) {
    case SI5351_MODE_576_PAL:
        /* ── 576p PAL@50
         * VCO  = 702.0000000 MHz  PLLA: 28+2/25
         * CLK0 = 27.0000000 MHz HDMI pixel clock  (MS0: 26+0/1)
         * CLK1 = 7.1136000 MHz  Atari PHI2x4  (MS1: 98+13/19)  -> vsync 50.000000 Hz
         * CLK2 = unused
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x19u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Cu);  /* reg  29 */
        si5351_write(0x1Eu, 0x0Au);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x06u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x0Bu);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x13u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x2Fu);  /* reg  53 */
        si5351_write(0x36u, 0x57u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x0Bu);  /* reg  57 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);  /* unused */
        break;

    case SI5351_MODE_480_NTSC:
        /* ── 480p NTSC@59.94
         * VCO  = 648.0000000 MHz  PLLA: 25+23/25
         * CLK0 = 27.0000000 MHz HDMI pixel clock  (MS0: 24+0/1)
         * CLK1 = 7.1611588 MHz  Atari PHI2x4  (MS1: 90+1215/2489)  -> vsync 59.940060 Hz
         * CLK2 = unused
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x19u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Au);  /* reg  29 */
        si5351_write(0x1Eu, 0xF5u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x13u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x0Au);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x09u);  /* reg  50 */
        si5351_write(0x33u, 0xB9u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x2Bu);  /* reg  53 */
        si5351_write(0x36u, 0x3Eu);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x04u);  /* reg  56 */
        si5351_write(0x39u, 0xB2u);  /* reg  57 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);  /* unused */
        break;

    case SI5351_MODE_480_NTSC_60:
        /* ── 480p NTSC@60
         * VCO  = 675.0000000 MHz  PLLA: 27+0/1
         * CLK0 = 27.0000000 MHz HDMI pixel clock  (MS0: 25+0/1)
         * CLK1 = 7.1683200 MHz  Atari PHI2x4  (MS1: 94+409/2489)  -> vsync 60.000000 Hz
         * CLK2 = unused
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x01u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Bu);  /* reg  29 */
        si5351_write(0x1Eu, 0x80u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x00u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x0Au);  /* reg  45 */
        si5351_write(0x2Eu, 0x80u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x09u);  /* reg  50 */
        si5351_write(0x33u, 0xB9u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x2Du);  /* reg  53 */
        si5351_write(0x36u, 0x15u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x53u);  /* reg  57 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);  /* unused */
        break;

    case SI5351_MODE_720_PAL:
        /* ── 720p PAL@50
         * VCO  = 891.0000000 MHz  PLLA: 35+16/25
         * CLK0 = 74.2500000 MHz HDMI pixel clock  (MS0: 12+0/1)
         * CLK1 = 7.1136000 MHz  Atari PHI2x4  (MS1: 125+125/494)  -> vsync 50.000000 Hz
         * CLK2 = unused
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x19u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Fu);  /* reg  29 */
        si5351_write(0x1Eu, 0xD1u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x17u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x04u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x01u);  /* reg  50 */
        si5351_write(0x33u, 0xEEu);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x3Cu);  /* reg  53 */
        si5351_write(0x36u, 0xA0u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0xC0u);  /* reg  57 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);  /* unused */
        break;

    case SI5351_MODE_720_NTSC:
        /* ── 720p NTSC@60
         * VCO  = 891.0000000 MHz  PLLA: 35+16/25
         * CLK0 = 74.2500000 MHz HDMI pixel clock  (MS0: 12+0/1)
         * CLK1 = 7.1683200 MHz  Atari PHI2x4  (MS1: 124+739/2489)  -> vsync 60.000000 Hz
         * CLK2 = unused
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x19u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Fu);  /* reg  29 */
        si5351_write(0x1Eu, 0xD1u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x17u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x04u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x09u);  /* reg  50 */
        si5351_write(0x33u, 0xB9u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x3Cu);  /* reg  53 */
        si5351_write(0x36u, 0x26u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x0Au);  /* reg  57 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);  /* unused */
        break;

    case SI5351_MODE_720_NTSC_5994:
        /* ── 720p NTSC@59.94
         * VCO  = 890.1098901 MHz  PLLA: 35+55/91
         * CLK0 = 74.1758242 MHz HDMI pixel clock  (MS0: 12+0/1)
         * CLK1 = 7.1611588 MHz  Atari PHI2x4  (MS1: 124+739/2489)  -> vsync 59.940060 Hz
         * CLK2 = unused
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x5Bu);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Fu);  /* reg  29 */
        si5351_write(0x1Eu, 0xCDu);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x21u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x04u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x09u);  /* reg  50 */
        si5351_write(0x33u, 0xB9u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x3Cu);  /* reg  53 */
        si5351_write(0x36u, 0x26u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x0Au);  /* reg  57 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x0Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, SI5351_CLK_PDN);  /* unused */
        break;

    case SI5351_MODE_PAL_ORIGINAL:
        /* ── PAL original
         * VCO  = 624.2500000 MHz  PLLA: 24+97/100
         * CLK0 = 28.3750000 MHz Atari master clock  (MS0: 22+0/1)
         * CLK1 = 7.0937500 MHz  Atari PHI2x4  (MS1: 88+0/1)  -> vsync 49.860479 Hz
         * CLK2 = 4.4336187 MHz colour subcarrier  (MS2: 140+51540/64489)
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x64u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Au);  /* reg  29 */
        si5351_write(0x1Eu, 0x7Cu);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x10u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x09u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x01u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x2Au);  /* reg  53 */
        si5351_write(0x36u, 0x00u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x00u);  /* reg  57 */
        /* MS2: CLK2 (colour subcarrier) */
        si5351_write(0x3Au, 0xFBu);  /* reg  58 */
        si5351_write(0x3Bu, 0xE9u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x44u);  /* reg  61 */
        si5351_write(0x3Eu, 0x66u);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x4Bu);  /* reg  64 */
        si5351_write(0x41u, 0x2Au);  /* reg  65 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);
        break;

    case SI5351_MODE_NTSC_ORIGINAL:
        /* ── NTSC original
         * VCO  = 630.0000000 MHz  PLLA: 25+1/5
         * CLK0 = 28.6363636 MHz Atari master clock  (MS0: 22+0/1)
         * CLK1 = 7.1590909 MHz  Atari PHI2x4  (MS1: 88+0/1)  -> vsync 59.922751 Hz
         * CLK2 = 3.5795455 MHz colour subcarrier  (MS2: 176+0/1)
         */
        /* PLLA */
        si5351_write(0x1Au, 0x00u);  /* reg  26 */
        si5351_write(0x1Bu, 0x05u);  /* reg  27 */
        si5351_write(0x1Cu, 0x00u);  /* reg  28 */
        si5351_write(0x1Du, 0x0Au);  /* reg  29 */
        si5351_write(0x1Eu, 0x99u);  /* reg  30 */
        si5351_write(0x1Fu, 0x00u);  /* reg  31 */
        si5351_write(0x20u, 0x00u);  /* reg  32 */
        si5351_write(0x21u, 0x03u);  /* reg  33 */
        /* MS0: CLK0 */
        si5351_write(0x2Au, 0x00u);  /* reg  42 */
        si5351_write(0x2Bu, 0x01u);  /* reg  43 */
        si5351_write(0x2Cu, 0x00u);  /* reg  44 */
        si5351_write(0x2Du, 0x09u);  /* reg  45 */
        si5351_write(0x2Eu, 0x00u);  /* reg  46 */
        si5351_write(0x2Fu, 0x00u);  /* reg  47 */
        si5351_write(0x30u, 0x00u);  /* reg  48 */
        si5351_write(0x31u, 0x00u);  /* reg  49 */
        /* MS1: CLK1 (Atari PHI2 x4) */
        si5351_write(0x32u, 0x00u);  /* reg  50 */
        si5351_write(0x33u, 0x01u);  /* reg  51 */
        si5351_write(0x34u, 0x00u);  /* reg  52 */
        si5351_write(0x35u, 0x2Au);  /* reg  53 */
        si5351_write(0x36u, 0x00u);  /* reg  54 */
        si5351_write(0x37u, 0x00u);  /* reg  55 */
        si5351_write(0x38u, 0x00u);  /* reg  56 */
        si5351_write(0x39u, 0x00u);  /* reg  57 */
        /* MS2: CLK2 (colour subcarrier) */
        si5351_write(0x3Au, 0x00u);  /* reg  58 */
        si5351_write(0x3Bu, 0x01u);  /* reg  59 */
        si5351_write(0x3Cu, 0x00u);  /* reg  60 */
        si5351_write(0x3Du, 0x56u);  /* reg  61 */
        si5351_write(0x3Eu, 0x00u);  /* reg  62 */
        si5351_write(0x3Fu, 0x00u);  /* reg  63 */
        si5351_write(0x40u, 0x00u);  /* reg  64 */
        si5351_write(0x41u, 0x00u);  /* reg  65 */
        si5351_write(SI5351_REG_CLK0_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK1_CTRL, 0x4Fu);
        si5351_write(SI5351_REG_CLK2_CTRL, 0x0Fu);
        break;

    }

    si5351_write(SI5351_REG_PLL_RESET, 0x20u);  /* reset PLLA, re-lock */
    si5351_write(SI5351_REG_OE,        0xF8u);  /* enable CLK0/1/2 */
}
