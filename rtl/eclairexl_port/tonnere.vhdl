-- thunder_u30_pins.vhd
--
-- Top-level entity pin declaration for the Cyclone 10 LP (U30)
-- on the Tonnere board.
--
-- Derived from the KiCad netlist (tonnere.net, 2026-03-29).
--
-- Direction conventions:
--   in     = FPGA receives only (clocks, STM32-driven bus control, config)
--   out    = FPGA drives only  (memory address/control, DAC, video)
--   inout  = truly bidirectional, or direction is design-dependent
--
-- Bus switches (SN74CB3T16211):
--   U27 — joystick ports 1 & 2 (directions + triggers)
--   U29 — PBI address bus, PBI data D0-D1, FPGA_GPIO
--   U31 — SIO, console keys, JOY2_DIR[15], JOY2_TRIG[3]
--   U33 — PBI data D2-D7, PBI control signals
--   All four are FET pass-gates (NOT level-shifters), powered from +3V3,
--   OEs tied to GND (always enabled).
--
-- SPI / active-serial configuration pins (not in this entity):
--   DATA1/ASDO (D1), DATA0 (K1), DCLK (K2), FLASH_nCE/nCSO (E2) are
--   dedicated config pins handled by the Cyclone 10 hard config IP.
--   Post-config flash access uses the SFL IP (sfl instance below), which
--   owns the dedicated config pins after configuration. The ZPU SPI master
--   reaches the EPCQ through it (no top-level flash pins needed).
--   The STM32 (PB4-7) shares this bus and can program the flash directly.
--   RN8 provides pull-ups to +3V3 on MOSI, MISO, SCK, CS1.
--   J16 (SPI SEL, 1x03 header) selects which flash chip receives CS2:
--     pin 1 -> U26, pin 3 -> U23, centre = CS2 line.
--   J14 (SPI CFG, 2x03 header) breaks out MISO/MOSI/SCK/CS2 for debug.
--   CS1 goes to SD card and STM32 PB6 only; it does not reach the FPGA.
--
-- STM32-to-ESP32 direct wiring (none pass through the FPGA):
--   UART:  PC6/PC7 <-> TXD0/RXD0  (also on J18 ESP PGM header)
--   BOOT:  PG6/PG7 <-> IO0/EN     (also on J18)
--   GPIO:  PG8-10  ->  SGPI0-2 (IO36/39/34, input-only on ESP)
--          PA9     <-  SGPIO (IO2)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use IEEE.STD_LOGIC_MISC.all;    -- and_reduce (ANTIC_LIGHTPEN)

library work;

entity tonnere is
  port (
    ---------------------------------------------------------------------------
    -- PLL / clock inputs
    -- From Si5351 oscillators (Y4, Y7, Y5) via series resistors to
    -- Cyclone 10 dedicated clock pins.
    ---------------------------------------------------------------------------
    PLL1            : in    std_logic_vector(2 downto 0);   -- [0]=CLK3(T1)  [1]=CLK2(T2)  [2]=CLK14(AB11) from Y4
    PLL2            : in    std_logic_vector(2 downto 0);   -- [0]=CLK4(G21) [1]=CLK5(G22) [2]=CLK6(T21)   from Y7
    CLK27_A12       : in    std_logic;                      -- 27 MHz on CLK8 (A12) from Y5

    ---------------------------------------------------------------------------
    -- FSMC bus  (direct to STM32 U19 FMC/FSMC, no bus switch)
    ---------------------------------------------------------------------------
    FSMC_A          : in    std_logic_vector(22 downto 0);  -- address from STM32
    FSMC_D          : inout std_logic_vector(15 downto 0);  -- bidirectional data
    FSMC_NBL        : in    std_logic_vector(1 downto 0);   -- byte-lane selects from STM32
    FSMC_NE         : in    std_logic_vector(1 downto 1);   -- chip-enable from STM32 (active low)
    FSMC_NOE        : in    std_logic;                      -- output-enable from STM32
    FSMC_NWE        : in    std_logic;                      -- write-enable from STM32
    FSMC_NWAIT      : out   std_logic;                      -- wait/ready back to STM32

    ---------------------------------------------------------------------------
    -- FPGA <-> STM32 control / status  (direct, no bus switch)
    ---------------------------------------------------------------------------
    FPGA_IRQ        : out   std_logic;                      -- interrupt request -> STM32 PG12
    --FPGA_CONFIG_N   : in    std_logic;                      -- nCONFIG (K5) <- STM32 PG13; pull-up to +3V3 (R65)
    --FPGA_STATUS_N   : out   std_logic;                      -- nSTATUS (K6) -> STM32 PG14; pull-up to +3V3 (R64)
    --FPGA_CONF_DONE  : out   std_logic;                      -- CONF_DONE (M18) -> STM32 PG15; pull-up to +3V3 (R97)
    --FPGA_PS_N       : in    std_logic;                      -- MSEL1 (L18) <- STM32 PG11; pull-down to +2V5 (R79)

    ---------------------------------------------------------------------------
    -- FPGA GPIO  (via U29 bus switch to PBI edge connector)
    ---------------------------------------------------------------------------
    -- FPGA_GPIO       : inout std_logic_vector(4 downto 0);
    FPGA_GPIO       : inout std_logic_vector(4 downto 4);

    ---------------------------------------------------------------------------
    -- ESP32 SPI slave bus  (direct to ESP32-WROVER-E U34, 3V3, no bus switch)
    ---------------------------------------------------------------------------
    ESP_SCK         : in   std_logic;                      -- -> ESP32 IO18
    ESP_MOSI        : in   std_logic;                      -- -> ESP32 IO23
    ESP_MISO        : out    std_logic;                      -- <- ESP32 IO19
    ESP_CS          : in   std_logic;                      -- -> ESP32 IO5; pull-up to +3V3 (R94)

    ---------------------------------------------------------------------------
    -- I2S audio  (direct to PCM5102A DAC U17, no bus switch)
    ---------------------------------------------------------------------------
    FPGAAUD_BCK     : out   std_logic;                      -- bit clock   -> U17.BCK
    FPGAAUD_LR      : out   std_logic;                      -- L/R clock   -> U17.LRCK
    FPGAAUD_DATA    : out   std_logic;                      -- serial data -> U17.DIN

    ---------------------------------------------------------------------------
    -- Video DAC  (via series resistors into THS7316 video buffer U21)
    ---------------------------------------------------------------------------
    VDAC_RH          : out   std_logic;                      -- red   via R75 -> U21 ch3
    VDAC_GH          : out   std_logic;                      -- green via R74 -> U21 ch2
    VDAC_BH          : out   std_logic;                      -- blue  via R76 -> U21 ch1
    VDAC_RL          : out   std_logic;                      -- red   low bits
    VDAC_GL          : out   std_logic;                      -- green low bits
    VDAC_BL          : out   std_logic;                      -- blue  low bits
    VDAC_HSYNC      : out   std_logic;                      -- hsync via R39 -> connector
    VDAC_VSYNC      : out   std_logic;                      -- vsync via R38 -> connector

    ---------------------------------------------------------------------------
    -- HDMI TMDS  (via 270R series resistor networks RN1/RN2 to connector)
    ---------------------------------------------------------------------------
    HDMI_D0P        : out   std_logic;                      -- data0+  (V2)
    HDMI_D0N        : out   std_logic;                      -- data0-  (V1)
    HDMI_D1P        : out   std_logic;                      -- data1+  (W2)
    HDMI_D1N        : out   std_logic;                      -- data1-  (W1)
    HDMI_D2P        : out   std_logic;                      -- data2+  (Y2)
    HDMI_D2N        : out   std_logic;                      -- data2-  (Y1)
    HDMI_CKP        : out   std_logic;                      -- clock+  (AA3)
    HDMI_CKN        : out   std_logic;                      -- clock-  (AB3)

    ---------------------------------------------------------------------------
    -- SDRAM  (AS4C16M16SA U35, FPGA-mastered, direct)
    ---------------------------------------------------------------------------
    SDRAM1_A        : out   std_logic_vector(12 downto 0);  -- address
    SDRAM1_BA       : out   std_logic_vector(1 downto 0);   -- bank address
    SDRAM1_DQ       : inout std_logic_vector(15 downto 0);  -- bidirectional data
    SDRAM1_CAS_N    : out   std_logic;                      -- column address strobe
    SDRAM1_RAS_N    : out   std_logic;                      -- row address strobe
    SDRAM1_WE_N     : out   std_logic;                      -- write enable
    SDRAM1_CS_N     : out   std_logic;                      -- chip select
    SDRAM1_CKE      : out   std_logic;                      -- clock enable
    SDRAM1_CLK      : out   std_logic;                      -- clock via series R93 to U35.CLK
    SDRAM1_LDQM     : out   std_logic;                      -- lower byte data mask
    SDRAM1_UDQM     : out   std_logic;                      -- upper byte data mask

    ---------------------------------------------------------------------------
    -- SRAM 1  (IS61WV204816BLL U28, FPGA-mastered, direct)
    ---------------------------------------------------------------------------
    --SRAM1_A         : out   std_logic_vector(20 downto 0);  -- address
    SRAM1_A         : out   std_logic_vector(19 downto 0);  -- address
    SRAM1_D         : inout std_logic_vector(15 downto 0);  -- bidirectional data
    SRAM1_CE_N      : out   std_logic;
    SRAM1_OE_N      : out   std_logic;
    SRAM1_W_N       : out   std_logic;
    SRAM1_LB_N      : out   std_logic;
    SRAM1_UB_N      : out   std_logic;

    ---------------------------------------------------------------------------
    -- SRAM 2  (IS61WV204816BLL U32, FPGA-mastered, direct)
    ---------------------------------------------------------------------------
    --SRAM2_A         : out   std_logic_vector(20 downto 0);  -- address
    SRAM2_A         : out   std_logic_vector(19 downto 0);  -- address
    SRAM2_D         : inout std_logic_vector(15 downto 0);  -- bidirectional data
    SRAM2_CE_N      : out   std_logic;
    SRAM2_OE_N      : out   std_logic;
    SRAM2_W_N       : out   std_logic;
    SRAM2_LB_N      : out   std_logic;
    SRAM2_UB_N      : out   std_logic;

    ---------------------------------------------------------------------------
    -- PBI (Parallel Bus Interface)
    -- Via bus switches U29 (address, GPIO, data D0-D1) and U33 (data D2-D7,
    -- control) to the PBI edge connector.
    -- Direction depends on Atari bus cycle.
    -- External-side pull-ups to +5V: IRQ (R11), MPD (R12), REF (R13),
    --   EXTSEL (R15).
    -- External-side pull-downs to GND: RD4 (R20), RD5 (R24).
    ---------------------------------------------------------------------------
    PBI_A           : inout std_logic_vector(15 downto 0);  -- address bus (via U29)
    PBI_D           : inout std_logic_vector(7 downto 0);   -- data bus (via U29/U33)
    PBI_PHI2        : inout std_logic;                      -- PHI2 clock (via U33)
    PBI_RW_N        : inout std_logic;                      -- read/write (via U33)
    PBI_RD          : inout std_logic_vector(5 downto 4);   -- RD4/RD5 (via U33); ext pull-down to GND
    PBI_HALT        : inout std_logic;                      -- (via U33)
    PBI_IRQ         : inout std_logic;                      -- (via U33); ext pull-up to +5V
    PBI_RST         : inout std_logic;                      -- (via U33)
    PBI_RDY         : inout std_logic;                      -- (via U33)
    PBI_REF         : inout std_logic;                      -- (via U33); ext pull-up to +5V
    PBI_RAS         : inout std_logic;                      -- (via U33)
    PBI_CAS         : inout std_logic;                      -- (via U33)
    PBI_MPD         : inout std_logic;                      -- (via U33); ext pull-up to +5V
    PBI_S4_N        : inout std_logic;                      -- (via U33)
    PBI_S5_N        : inout std_logic;                      -- (via U33)
    PBI_CCTL        : inout std_logic;                      -- (via U33)
    PBI_D1XX        : inout std_logic;                      -- (via U33)
    PBI_EXTENB      : inout std_logic;                      -- (via U33)
    PBI_EXTSEL      : inout std_logic;                      -- (via U33); ext pull-up to +5V

    ---------------------------------------------------------------------------
    -- SIO (Serial I/O)
    -- Via bus switch U31 to the SIO connector (active-low accent on ext side).
    -- Several signals are also routed to ESP32 U34, allowing the ESP32 to
    -- participate in SIO bus traffic independently of the FPGA.
    -- External-side pull-ups to +5V via RN3: COMMAND, INTERRUPT, PROCEED.
    -- External-side pull-ups to +5V via RN4: DATA_IN, DATA_OUT, CLOCK_IN,
    --   CLOCK_OUT.
    ---------------------------------------------------------------------------
    SIO_DATA_IN     : inout std_logic;                      -- via U31; also to ESP32 IO25; ext pull-up +5V
    SIO_DATA_OUT    : inout std_logic;                      -- via U31; also to ESP32 IO22; ext pull-up +5V
    SIO_CLOCK_IN    : inout std_logic;                      -- via U31; also to ESP32 IO21; ext pull-up +5V
    SIO_CLOCK_OUT   : inout std_logic;                      -- via U31; also to ESP32 IO4;  ext pull-up +5V
    SIO_COMMAND     : inout std_logic;                      -- via U31; also to ESP32 IO33; ext pull-up +5V
    SIO_PROCEED     : inout std_logic;                      -- via U31; also to ESP32 IO27; ext pull-up +5V
    SIO_INTERRUPT   : inout std_logic;                      -- via U31; also to ESP32 IO32; ext pull-up +5V
    SIO_MOTOR       : inout std_logic;                      -- via R29 to Q1 (BCX51 driver); pull-up to +5V (R30); also to ESP32 IO35

    ---------------------------------------------------------------------------
    -- Joystick ports
    -- Via bus switches U27 (most signals) and U31 (JOY2_DIR[15], JOY2_TRIG[3]).
    -- All direction lines have external pull-ups to +5V:
    --   port 1 via RN5 (DIR0-3) and RN6 (DIR4-7)
    --   port 2 via RN9 (DIR8-11) and RN11 (DIR12-15)
    -- All trigger lines have external pull-ups to +5V:
    --   TRIG0 (R32), TRIG1 (R49), TRIG2 (R63), TRIG3 (R85)
    ---------------------------------------------------------------------------
    JOY_DIR         : inout std_logic_vector(7 downto 0);   -- port 1 directions (via U27); ext pull-up +5V
    --JOY_TRIG        : in std_logic_vector(1 downto 0);   -- port 1 triggers   (via U27); ext pull-up +5V
    JOY_TRIG        : inout std_logic_vector(1 downto 0);   -- port 1 triggers   (via U27); ext pull-up +5V	 
    JOY2_DIR        : inout std_logic_vector(15 downto 8);  -- port 2 directions (via U27/U31); ext pull-up +5V
    --JOY2_TRIG       : in std_logic_vector(3 downto 2);   -- port 2 triggers   (via U27/U31); ext pull-up +5V
    JOY2_TRIG       : inout std_logic_vector(3 downto 2);   -- port 2 triggers   (via U27/U31); ext pull-up +5V	 

    ---------------------------------------------------------------------------
    -- Console keys  (via bus switch U31)
    ---------------------------------------------------------------------------
    CONSOL_START    : in std_logic; --TODO weak pull up/down
    CONSOL_SELECT   : in std_logic;
    CONSOL_OPTION   : in std_logic;
    CONSOL_RESET    : in std_logic
  );
end entity tonnere;

architecture vhdl of tonnere is

  ---------------------------------------------------------------------------
  -- NOTE ON THIS PORT (Eclaire XL -> Tonnere, Cyclone V -> Cyclone 10 LP)
  --
  -- This is a port of atari800core_eclaireXLv3.vhd. The full atari800core
  -- (with ZPU config/SD, PBI/6502 bus master and DMA) is instantiated, but
  -- the board-external SD, USB and runtime-PLL-reconfig hardware present on
  -- Eclaire does NOT exist on Tonnere. Those paths are kept as internal
  -- signals and tied off / left open, marked "TODO(tonnere)".
  --
  -- Differences from Eclaire, by subsystem:
  --   CLOCKS : Eclaire uses a reconfigurable fractional PLL (pll_acore +
  --            pll_acore_reconfig + FIFO + PLL state machine) that the ZPU
  --            retunes at runtime for exact PAL/NTSC colour clocks. Tonnere
  --            has fixed integer PLLs. pll_atari replaces acore; the reconfig
  --            machinery is removed and ZPU_PLL_* are left open.
  --            CONSEQUENCE: no runtime PAL/NTSC retune; svideo colour clock is
  --            not exact. TODO(tonnere): revisit if svideo/composite needed.
  --   AUDIO  : Eclaire drives two hq_dac 1-bit outputs. Tonnere has I2S ->
  --            PCM5102A. The core PCM feeds the existing i2smaster; hq_dac is
  --            not ported.
  --   VIDEO  : scandoubler + svideo_gtia + scandoubler_hdmi are ported from
  --            Eclaire. The HDMI serializer is Tonnere's OWN altddio_out1
  --            instance (kept as-is), fed from scandoubler_hdmi's H/L TMDS
  --            words and clocked by clk_hdmi. Tonnere additionally has
  --            sdm_dac_video (analogue video DAC), fed from the same
  --            scandoubled video_r/g/b via the video_mode mux.
  --   SD/SPI : ZPU SPI master reaches the EPCQ config flash via the SFL IP
  --            (sfl instance). SD card is not wired (SELECT0 open).
  --   USB    : zpucore still generated with usb=>2; USBWire* tied off internally,
  --            no top-level USB pins.
  --   GPIO   : Eclaire's gpio_debug used a 24-bit GPIO; Tonnere exposes one pin
  --            (FPGA_GPIO(4)). gpio_debug dropped, FPGA_GPIO tied off.
  ---------------------------------------------------------------------------

  constant GENERIC_INTERNAL_ROM : integer := 1;      -- 16k os+basic (block RAM)
  constant GENERIC_INTERNAL_RAM : integer := 0;      -- RAM is external SRAM2 now
  constant GENERIC_SID          : integer := 0;

  ---------------------------------------------------------------------------
  -- Components  (only those still used on Tonnere)
  ---------------------------------------------------------------------------

  -- SYSTEM CLOCKS ----------------------------------------------------------
  -- pll_atari : replaces Eclaire's reconfigurable pll_acore. Integer ratios.
  --   c1 = CLK56  (2x ~28MHz "CLK" main core clock in Eclaire terms)
  --   c2 = CLK112 (4x, SDRAM clock)
  --   c3 = CLK112_N (4x 180deg, SDRAM output clock)
  component pll_atari is
    port (
      inclk0 : in  std_logic := '0';
      c0     : out std_logic;   -- PHI2 (unused here)
      c1     : out std_logic;   -- CLK56
      c2     : out std_logic;   -- CLK112
      c3     : out std_logic;   -- CLK112_N
      locked : out std_logic
    );
  end component;

  component pll_aud is
    port (
      inclk0 : in  std_logic := '0';
      c0     : out std_logic;   -- CLK1_536 (I2S master clock)
      locked : out std_logic
    );
  end component;

  component pll_vdac is
    port (
      inclk0 : in  std_logic := '0';
      c0     : out std_logic;   -- CLK_PATTERN (sdm dac)
      locked : out std_logic
    );
  end component;

  component pll_hdmi is
    port (
      inclk0 : in  std_logic := '0';
      c0     : out std_logic;   -- pixel clock (CLK_PIXEL_IN)
      c1     : out std_logic;   -- 5x pixel  (CLK_HDMI_IN)
      locked : out std_logic
    );
  end component;

  -- Serial Flash Loader / ASMI bridge for post-config EPCQ access.
  -- Cyclone 10 LP signature (same as Cyclone V here). TODO(tonnere): confirm
  -- the regenerated IP's port list matches; regenerate for the 10 LP device
  -- and its EPCQ part if it differs.
  component sfl is
    port (
      asmi_access_granted : in  std_logic := 'X'; -- asmi_access_granted
      asmi_access_request : out std_logic;        -- asmi_access_request
      dclk_in             : in  std_logic := 'X'; -- dclkin
      ncso_in             : in  std_logic := 'X'; -- scein
      noe_in              : in  std_logic := 'X'; -- noe
      asdo_in             : in  std_logic := 'X'; -- sdoin
      data0_out           : out std_logic         -- data0out
    );
  end component;

  ---------------------------------------------------------------------------
  -- SIGNALS  (ported from Eclaire; board-specific names kept where they exist)
  ---------------------------------------------------------------------------

  -- SYSTEM
  SIGNAL CLK        : STD_LOGIC;   -- main core clock (Eclaire "CLK"); = CLK56
  SIGNAL CLK_SDRAM  : STD_LOGIC;   -- = CLK112
  SIGNAL RESET_N    : STD_LOGIC;
  signal SDRAM_RESET_N : std_logic;

  SIGNAL CLK_PIXEL_IN : STD_LOGIC;
  SIGNAL CLK_HDMI_IN  : STD_LOGIC;
  SIGNAL CLK_PIXEL    : STD_LOGIC;   -- Tonnere pll_hdmi c0 (pixel clock)
  SIGNAL CLK_HDMI     : STD_LOGIC;   -- Tonnere pll_hdmi c1 (5x, DDR/TMDS clock)

  -- raw/undivided clocks used by the video mux for VGA-style outputs.
  -- On Tonnere there is no separate gated "raw" clock tree; alias to CLK.
  SIGNAL CLK_raw     : STD_LOGIC;
  SIGNAL CLK_114     : STD_LOGIC;  -- Eclaire 4x clock; = CLK112

  -- PLL lock flags
  signal atari_lock : std_logic;
  signal aud_lock   : std_logic;
  signal vdac_lock  : std_logic;
  signal hdmi_lock  : std_logic;
  signal MASTER_RESET_N : std_logic;

  -- Tonnere-specific extra clocks
  signal CLK112     : std_logic;
  signal CLK112_N   : std_logic;
  signal CLK_PATTERN: std_logic;
  signal CLK1_536   : std_logic;

  -- PIA
  SIGNAL CA1_IN : STD_LOGIC;
  SIGNAL CB1_IN : STD_LOGIC;
  SIGNAL CA2_OUT : STD_LOGIC;
  SIGNAL CA2_DIR_OUT : STD_LOGIC;
  SIGNAL CB2_OUT : STD_LOGIC;
  SIGNAL CB2_DIR_OUT : STD_LOGIC;
  SIGNAL CA2_IN : STD_LOGIC;
  SIGNAL CB2_IN : STD_LOGIC;
  SIGNAL PORTA_IN : STD_LOGIC_VECTOR(7 DOWNTO 0);
  SIGNAL PORTA_OUT : STD_LOGIC_VECTOR(7 DOWNTO 0);
  SIGNAL PORTA_DIR_OUT : STD_LOGIC_VECTOR(7 DOWNTO 0);
  SIGNAL PORTB_IN : STD_LOGIC_VECTOR(7 DOWNTO 0);
  SIGNAL PORTB_OUT : STD_LOGIC_VECTOR(7 DOWNTO 0);
  SIGNAL PORTB_DIR_OUT : STD_LOGIC_VECTOR(7 DOWNTO 0);

  -- GTIA
  signal GTIA_TRIG : std_logic_vector(3 downto 0);

  -- ANTIC
  signal ANTIC_LIGHTPEN : std_logic;
  signal ANTIC_TURBO : std_logic;

  -- PBI
  SIGNAL PBI_WRITE_DATA : std_logic_vector(31 downto 0);
  SIGNAL PBI_WIDTH_32BIT_ACCESS : std_logic;
  SIGNAL PBI_WIDTH_16BIT_ACCESS : std_logic;
  SIGNAL PBI_WIDTH_8BIT_ACCESS : std_logic;

  -- INTERNAL ROM/RAM
  SIGNAL RAM_ADDR : STD_LOGIC_VECTOR(18 DOWNTO 0);
  SIGNAL RAM_DO   : STD_LOGIC_VECTOR(15 DOWNTO 0);
  SIGNAL RAM_REQUEST : STD_LOGIC;
  SIGNAL RAM_REQUEST_COMPLETE : STD_LOGIC;
  SIGNAL RAM_WRITE_ENABLE : STD_LOGIC;

  SIGNAL ROM_ADDR : STD_LOGIC_VECTOR(21 DOWNTO 0);
  SIGNAL ROM_DO   : STD_LOGIC_VECTOR(7 DOWNTO 0);
  SIGNAL ROM_REQUEST : STD_LOGIC;
  SIGNAL ROM_REQUEST_COMPLETE : STD_LOGIC;
  SIGNAL ROM_WRITE_ENABLE : STD_LOGIC;

  -- SDRAM
  signal SDRAM_REQUEST : std_logic;
  signal SDRAM_REQUEST_COMPLETE : std_logic;
  signal SDRAM_READ_ENABLE  : STD_LOGIC;
  signal SDRAM_WRITE_ENABLE : std_logic;
  signal SDRAM_ADDR : STD_LOGIC_VECTOR(22 DOWNTO 0);
  signal SDRAM_DO   : STD_LOGIC_VECTOR(31 DOWNTO 0);

  signal ANTIC_REFRESH : std_logic;

  -- pokey keyboard
  SIGNAL KEYBOARD_SCAN : std_logic_vector(5 downto 0);
  SIGNAL KEYBOARD_RESPONSE : std_logic_vector(1 downto 0);

  signal KEYBOARD_MATRIX : std_logic_vector(63 downto 0);
  signal KEYBOARD_SHIFT : std_logic;
  signal KEYBOARD_CONTROL : std_logic;
  signal KEYBOARD_BREAK : std_logic;

  -- gtia consol keys
  SIGNAL CONSOL_START_INT : std_logic;
  SIGNAL CONSOL_SELECT_INT : std_logic;
  SIGNAL CONSOL_OPTION_INT : std_logic;

  -- SIO
  SIGNAL ASIO_RXD : std_logic;
  SIGNAL ASIO_TXD : std_logic;
  SIGNAL ASIO_CLOCKOUT : std_logic;
  SIGNAL ASIO_CLOCKIN_IN : std_logic;
  SIGNAL ASIO_CLOCKIN_OUT : std_logic;
  SIGNAL ASIO_CLOCKIN_OE : std_logic;

  -- VIDEO (from core)
  signal VIDEO_VS : std_logic;
  signal VIDEO_HS : std_logic;
  signal VIDEO_CS : std_logic;
  signal VIDEO_BLANK : std_logic;
  signal VIDEO_BURST : std_logic;
  signal VIDEO_ODD_LINE : std_logic;
  -- post scandoubler (scandoubler outputs)
  signal VIDEO_R : std_logic_vector(7 downto 0);
  signal VIDEO_G : std_logic_vector(7 downto 0);
  signal VIDEO_B : std_logic_vector(7 downto 0);
  signal VIDEO_VSYNC : std_logic;
  signal VIDEO_HSYNC : std_logic;
  -- post video_mode mux (what actually feeds the analogue VDAC)
  -- Distinct names from VIDEO_* above: the scandoubler drives VIDEO_*, the mux
  -- drives VGA_*. Reusing VIDEO_* here would give those signals two drivers.
  signal VGA_R : std_logic_vector(7 downto 0);
  signal VGA_G : std_logic_vector(7 downto 0);
  signal VGA_B : std_logic_vector(7 downto 0);
  signal VGA_BLANK : std_logic;
  signal VGA_VSYNC : std_logic;
  signal VGA_HSYNC : std_logic;

  -- AUDIO
  signal AUDIO_L_PCM : std_logic_vector(15 downto 0);
  signal AUDIO_R_PCM : std_logic_vector(15 downto 0);
  signal AUDIO_L_PCM_SIGNED : signed(15 downto 0);
  signal AUDIO_R_PCM_SIGNED : signed(15 downto 0);

  -- dma/virtual drive
  signal DMA_ADDR_FETCH : std_logic_vector(23 downto 0);
  signal DMA_WRITE_DATA : std_logic_vector(31 downto 0);
  signal DMA_FETCH : std_logic;
  signal DMA_32BIT_WRITE_ENABLE : std_logic;
  signal DMA_16BIT_WRITE_ENABLE : std_logic;
  signal DMA_8BIT_WRITE_ENABLE : std_logic;
  signal DMA_READ_ENABLE : std_logic;
  signal DMA_MEMORY_READY : std_logic;
  signal SNOOP_DATA : std_logic_vector(31 downto 0);
  signal SNOOP_DATA_READY : std_logic;

  signal stm_pokey_enable : std_logic;
  signal stm_sio_txd : std_logic;
  signal stm_sio_rxd : std_logic;
  signal stm_sio_command : std_logic;

  -- STM32/FSMC adaptor control and status registers
  signal CONTROL : std_logic_vector(3 downto 0);
  signal RAMCONFIG : std_logic_vector(2 downto 0);
  signal PERFORMANCE : std_logic_vector(8 downto 0);
  signal CART : std_logic_vector(5 downto 0);
  signal VIDEO : std_logic_vector(6 downto 0);

  signal CONSOLE_INJECT : std_logic_vector(2 downto 0);
  signal CONSOLE_PHYS : std_logic_vector(2 downto 0);

  signal JOY0_INJECT : std_logic_vector(4 downto 0);
  signal JOY1_INJECT : std_logic_vector(4 downto 0);
  signal JOY2_INJECT : std_logic_vector(4 downto 0);
  signal JOY3_INJECT : std_logic_vector(4 downto 0);

  signal JOY_DIR_INT : std_logic_vector(7 downto 0);
  signal JOY2_DIR_INT : std_logic_vector(15 downto 8);
  signal JOY_TRIG_INT : std_logic_vector(1 downto 0);
  signal JOY2_TRIG_INT : std_logic_vector(3 downto 2);

  signal FREEZE_ADDR : std_logic_vector(15 downto 0);
  signal FREEZE_DATA_CTRL : std_logic_vector(15 downto 0);

  signal FSMC_D_OUT : std_logic_vector(15 downto 0);
  signal FSMC_D_OE : std_logic;
  signal FPGA_IRQ_INT : std_logic;

  -- system control from zpu
  signal ram_select : std_logic_vector(2 downto 0);
  signal reset_atari_request : std_logic;
  signal reset_atari : std_logic;
  signal antic_rnmi_n : std_logic;
  signal pause_atari : std_logic;
  SIGNAL speed_6502 : std_logic_vector(5 downto 0);
  signal turbo_vblank_only : std_logic;
  signal emulated_cartridge_select : std_logic_vector(5 downto 0);
  signal atari800mode : std_logic;

  -- GPIO / paddles
  signal POT_RESET : std_logic;
  signal POT_IN : std_logic_vector(7 downto 0);

  signal PBI_WRITE_ENABLE : std_logic;
  signal PBI_ADDRESS : std_logic_vector(15 downto 0);

  signal pbi_disable : std_logic;
  signal pbi_external : std_logic;
  signal pbi_takeover : std_logic;
  signal pbi_release : std_logic;
  signal pbi_request : std_logic;
  signal pbi_request_complete : std_logic;
  signal pbi_data : std_logic_vector(7 downto 0);
  signal pbi_addr : std_logic_vector(15 downto 0);

  signal enable_179_early : std_logic;

  -- scandoubler
  signal half_scandouble_enable_reg : std_logic;
  signal half_scandouble_enable_next : std_logic;
  signal ATARI_COLOUR : std_logic_vector(7 downto 0);

  -- freezer
  signal freezer_enable : std_logic;
  signal freezer_activate : std_logic;
  signal freezer_state : std_logic_vector(2 downto 0);

  -- CONFIG
  SIGNAL ROM_IN_RAM : STD_LOGIC;

  -- svideo
  signal svideo_c : std_logic_vector(7 downto 0);
  signal svideo_yout : std_logic_vector(7 downto 0);
  signal svideo_sync_n : std_logic;
  signal comp_n : std_logic;

  -- pbi bus
  signal bus_data_out : std_logic_vector(7 downto 0);
  signal bus_data_oe : std_logic;
  signal bus_addr_out : std_logic_vector(15 downto 0);
  signal bus_addr_oe : std_logic;
  signal bus_write_n : std_logic;
  signal bus_s4_n : std_logic;
  signal bus_s5_n : std_logic;
  signal bus_cctl_n : std_logic;
  signal bus_control_oe : std_logic;
  signal bus_refresh_oe : std_logic;
  signal bus_phi2 : std_logic;
  signal bus_cas_n : std_logic;
  signal bus_ras_n : std_logic;
  signal bus_casinh_n : std_logic;
  signal bus_casinh_oe : std_logic;
  signal mpd_n : std_logic;
  signal pbi_debug_data : std_logic_vector(31 downto 0);
  signal pbi_debug_ready : std_logic;
  signal nmi_n : std_logic;
  signal irq_n : std_logic;
  signal rdy : std_logic;
  signal an : std_logic_vector(2 downto 0);

  -- video settings (from STM32 VIDEO register)
  signal pal : std_logic;
  signal scandouble : std_logic;
  signal scanlines : std_logic;
  signal csync : std_logic;
  signal video_mode : std_logic_vector(2 downto 0);
  signal scandoubler_format : std_logic_vector(1 downto 0);

  -- vga exact-mode (from scandoubler_hdmi)
  signal adj_hsync : std_logic;
  signal adj_vsync : std_logic;
  signal adj_blank : std_logic;
  signal adj_red : std_logic_vector(7 downto 0);
  signal adj_green : std_logic_vector(7 downto 0);
  signal adj_blue : std_logic_vector(7 downto 0);

  -- hdtv tmds
  signal tmds_h : std_logic_vector(7 downto 0);
  signal tmds_l : std_logic_vector(7 downto 0);

  -- video via ddr out (to HDMI pins)
  signal DDIO_OUT : std_logic_vector(7 downto 0);

  signal state_reg_out : std_logic_vector(1 downto 0);
  signal memory_ready_antic_out : STD_LOGIC;
  signal memory_ready_cpu_out : STD_LOGIC;
  signal shared_enable_out : STD_LOGIC;

  -- ZPU SPI (TODO(tonnere): SD/flash not wired; left open)
  signal spi_do  : std_logic;
  signal spi_clk : std_logic;
  signal spi_flash_select : std_logic;
  signal spi_flash_di : std_logic;   -- flash MISO from SFL/EPCQ
  signal sfl_data_out : std_logic;  -- SFL data_out 

  -- SDRAM data-in width bridge: Eclaire's sdram_statemachine takes DATA_IN
  -- from PBI_WRITE_DATA; keep that wiring.
  -- (SDRAM_DI naming from the simple_sdram bring-up is dropped.)

begin

  ---------------------------------------------------------------------------
  -- CLOCKS
  -- pll_atari replaces Eclaire's reconfigurable pll_acore.
  --   CLK   (core)  = c1 = CLK56
  --   CLK112(4x)    = c2
  --   CLK112_N      = c3  (SDRAM output clock)
  -- No clkctrl gating, no acore reconfig FIFO/state-machine (removed).
  ---------------------------------------------------------------------------
  pll_atari1 : pll_atari
    port map (
      inclk0 => PLL1(1),
      c0     => open,        -- PHI2 (unused)
      c1     => CLK,         -- CLK56
      c2     => CLK112,
      c3     => CLK112_N,
      locked => atari_lock
    );

  CLK_SDRAM <= CLK112;
  CLK_114   <= CLK112;      -- Eclaire "CLK_114" 4x alias
  CLK_raw   <= CLK;        -- no separate raw/gated tree on Tonnere

  pll_aud1 : pll_aud
    port map (
      inclk0 => CLK27_A12,
      c0     => CLK1_536,
      locked => aud_lock
    );

  pll_vdac1 : pll_vdac
    port map (
      inclk0 => CLK27_A12,
      c0     => CLK_PATTERN,
      locked => vdac_lock     -- TODO(tonnere): sync video DAC clock to Atari video
    );

  pll_hdmi1 : pll_hdmi
    port map (
      inclk0 => PLL1(0),
      c0     => CLK_PIXEL,     -- pixel clock (27MHz)
      c1     => CLK_HDMI,      -- 5x pixel (TMDS/DDR clock)
      locked => hdmi_lock
    );
  -- scandoubler_hdmi expects ports named CLK_PIXEL_IN / CLK_HDMI_IN:
  CLK_PIXEL_IN <= CLK_PIXEL;
  CLK_HDMI_IN  <= CLK_HDMI;

  ---------------------------------------------------------------------------
  -- MASTER RESET  (Tonnere reset_gen; replaces Eclaire's pll-lock/reset FSM)
  -- Held until Si5351 refs + all four FPGA PLLs are locked and stable.
  ---------------------------------------------------------------------------
  reset_gen1 : entity work.reset_gen
    generic map ( STABLE_CYCLES => 65536 )   -- ~1.17 ms at 56 MHz
    port map (
      clk            => CLK,
      pll_atari_lock => atari_lock,
      pll_aud_lock   => aud_lock,
      pll_vdac_lock  => vdac_lock,
      pll_hdmi_lock  => hdmi_lock,
      reset_n        => MASTER_RESET_N
    );

  RESET_N <= MASTER_RESET_N;

  ---------------------------------------------------------------------------
  -- UNUSED / TIED-OFF TOP-LEVEL PINS
  ---------------------------------------------------------------------------
  -- FPGA GPIO: only one usable pin on Tonnere; gpio_debug dropped.
  FPGA_GPIO <= (others=>'Z');           -- TODO(tonnere): expose debug if wanted
  ESP_MISO  <= 'Z';                      -- TODO(tonnere): ESP32 SPI slave

  -- Second SRAM chip and (for now) SRAM1: the ported core uses SDRAM only.
  -- SRAM1 unused (atari RAM is on SRAM2). SRAM2 is driven by the sram instance.
  SRAM1_A <= (others=>'0'); SRAM1_D <= (others=>'Z');
  SRAM1_CE_N<='1'; SRAM1_OE_N<='1'; SRAM1_W_N<='1'; SRAM1_LB_N<='1'; SRAM1_UB_N<='1';
  -- TODO(tonnere): map SRAM1 as additional/expanded RAM per the memory-map comments.

  ---------------------------------------------------------------------------
  -- SDRAM  (AS4C16M16SA). Same statemachine/generics as Eclaire.
  -- DATA_IN comes from PBI_WRITE_DATA (as Eclaire), not the simple_sdram
  -- SDRAM_DI signal (removed).
  ---------------------------------------------------------------------------
  sdram_adaptor : entity work.sdram_statemachine
    GENERIC MAP(ADDRESS_WIDTH => 22, AP_BIT => 10, COLUMN_WIDTH => 8, ROW_WIDTH => 12)
    PORT MAP(
      CLK_SYSTEM => CLK,
      CLK_SDRAM  => CLK_SDRAM,
      RESET_N    => RESET_N,
      READ_EN    => SDRAM_READ_ENABLE,
      WRITE_EN   => SDRAM_WRITE_ENABLE,
      REQUEST    => SDRAM_REQUEST,
      BYTE_ACCESS     => PBI_WIDTH_8BIT_ACCESS,
      WORD_ACCESS     => PBI_WIDTH_16BIT_ACCESS,
      LONGWORD_ACCESS => PBI_WIDTH_32BIT_ACCESS,
      REFRESH    => ANTIC_REFRESH,
      ADDRESS_IN => SDRAM_ADDR,
      DATA_IN    => PBI_WRITE_DATA(31 downto 0),
      SDRAM_DQ   => SDRAM1_DQ,
      COMPLETE   => SDRAM_REQUEST_COMPLETE,
      SDRAM_BA0  => SDRAM1_BA(0),
      SDRAM_BA1  => SDRAM1_BA(1),
      SDRAM_CKE  => SDRAM1_CKE,
      SDRAM_CS_N => SDRAM1_CS_N,
      SDRAM_RAS_N=> SDRAM1_RAS_N,
      SDRAM_CAS_N=> SDRAM1_CAS_N,
      SDRAM_WE_N => SDRAM1_WE_N,
      SDRAM_ldqm => SDRAM1_LDQM,
      SDRAM_udqm => SDRAM1_UDQM,
      DATA_OUT   => SDRAM_DO,
      SDRAM_ADDR => SDRAM1_A(11 downto 0),
      reset_client_n => SDRAM_RESET_N
    );

  SDRAM1_A(12) <= '0';
  SDRAM1_CLK   <= CLK112_N;   -- Tonnere: dedicated 180deg output clock pin

  ---------------------------------------------------------------------------
  -- INTERNAL ROM (block RAM) + RAM request generation
  -- internal_ram=0: the RAM data path is external SRAM2 (sram1 instance below).
  -- internalromram still provides the OS/BASIC ROM and generates the
  -- RAM_REQUEST / RAM_WRITE_ENABLE / RAM_ADDR signals; its RAM data/complete
  -- outputs are unused (the external sram drives RAM_DO / RAM_REQUEST_COMPLETE).
  ---------------------------------------------------------------------------
  internalromram1 : entity work.internalromram
    GENERIC MAP ( internal_rom => GENERIC_INTERNAL_ROM,
                  internal_ram => GENERIC_INTERNAL_RAM )
    PORT MAP (
      clock   => CLK,
      reset_n => RESET_N,
      ROM_ADDR => ROM_ADDR,
      ROM_WR_ENABLE => ROM_WRITE_ENABLE,
      ROM_DATA_IN => PBI_WRITE_DATA(7 downto 0),
      ROM_REQUEST_COMPLETE => ROM_REQUEST_COMPLETE,
      ROM_REQUEST => ROM_REQUEST,
      ROM_DATA => ROM_DO,
      RAM_ADDR => RAM_ADDR,
      RAM_WR_ENABLE => RAM_WRITE_ENABLE,
      RAM_DATA_IN => PBI_WRITE_DATA(7 downto 0),
      RAM_REQUEST_COMPLETE => open,        -- external SRAM drives this
      RAM_REQUEST => RAM_REQUEST,
      RAM_DATA => open                     -- external SRAM drives RAM_DO
    );

  ---------------------------------------------------------------------------
  -- EXTERNAL RAM : atari main RAM -> SRAM2 (leave SRAM1 tied off)
  -- SRAM2 is a 512K x 16 (1MB) part: 19-bit word address SRAM2_A(18:0).
  -- BUT the core's RAM_ADDR is 19-bit BYTE address (18:0) = 256K words = 512KB,
  -- so only the low half of the chip is used. The wrapper emits an 18-bit word
  -- address (RAM_ADDR(18:1)); SRAM2_A(18) is held low, SRAM2_A(19) is unused.
  -- TODO(tonnere): to use the full 1MB, the core RAM port would need widening.
  ---------------------------------------------------------------------------
  sram1 : entity work.sram
    PORT MAP (
      WREN        => RAM_WRITE_ENABLE,
      clk         => CLK,
      reset_n     => RESET_N,
      request     => RAM_REQUEST,
      width_16bit => PBI_WIDTH_16BIT_ACCESS,
      ADDRESS     => RAM_ADDR,
      DIN         => PBI_WRITE_DATA(15 DOWNTO 0),
      SRAM_DQ     => SRAM2_D,
      SRAM_CE_N   => SRAM2_CE_N,
      SRAM_OE_N   => SRAM2_OE_N,
      SRAM_WE_N   => SRAM2_W_N,
      SRAM_LB_N   => SRAM2_LB_N,
      SRAM_UB_N   => SRAM2_UB_N,
      complete    => RAM_REQUEST_COMPLETE,
      DOUT        => RAM_DO,
      SRAM_ADDR   => SRAM2_A(17 downto 0)
    );
  SRAM2_A(18) <= '0';                      -- core addresses only 256K words
  SRAM2_A(19) <= '0';                      -- spare (512Kx16 part)

  ---------------------------------------------------------------------------
  -- JOYSTICK / PADDLES
  -- Eclaire used PORTA/PORTB and POTIN/TRIG top-level pins directly. Tonnere
  -- brings the joysticks in via JOY_DIR/JOY_TRIG/JOY2_* bus-switch pins.
  -- TODO(tonnere): PORTA/PORTB are the PIA ports; on Tonnere the joystick
  -- direction lines map to PIA PORTA. Wire real mapping once confirmed.
  -- For now: drive PIA PORTA from JOY_DIR, PORTB left internal.
  ---------------------------------------------------------------------------
PORTA_gen:
  for I in 0 to 7 generate
    JOY_DIR(I) <= '0' when PORTA_DIR_OUT(I)='1' and PORTA_OUT(I)='0' else 'Z';
  end generate;
  -- Combine both joystick ports into PORTA_IN (UDLR etc.) - TODO(tonnere) verify bit order
  PORTA_IN <= JOY_DIR_INT;

  -- PORTB is XL/XE memory-control / not on joystick pins; keep internal.
  PORTB_IN <= PORTB_OUT;
  -- TODO, what happened to JOY2?

  -- Triggers
  GTIA_TRIG <= JOY2_TRIG_INT(3 downto 2) & JOY_TRIG_INT(1 downto 0);  -- TODO(tonnere) verify order
  ANTIC_LIGHTPEN <= and_reduce(GTIA_TRIG);

  ---------------------------------------------------------------------------
  -- SIO
  -- Eclaire drove SIO_* pins directly. Tonnere uses SIO_DATA_IN/OUT/CLOCK_*,
  -- SIO_COMMAND/PROCEED/INTERRUPT/MOTOR via bus switch U31 (active-low ext).
  ---------------------------------------------------------------------------
  ASIO_CLOCKIN_IN <= SIO_CLOCK_IN;
  SIO_CLOCK_IN    <= ASIO_CLOCKIN_OUT when ASIO_CLOCKIN_OE='1' else 'Z';
  SIO_CLOCK_OUT   <= '0' when ASIO_CLOCKOUT='0' else 'Z';

  -- CB1=SIO_IRQ(INTERRUPT), CB2=SIO_COMMAND, CA1=SIO_PROCEED, CA2=SIO_MOTOR
  CB1_in <= SIO_INTERRUPT;
  SIO_COMMAND <= '0' when (CB2_DIR_OUT and NOT(CB2_OUT))='1' else 'Z';
  CB2_in <= SIO_COMMAND;
  CA1_in <= SIO_PROCEED;
  SIO_MOTOR <= '0' when (CA2_DIR_OUT and NOT(CA2_OUT))='1' else 'Z';
  CA2_in <= SIO_MOTOR;

  ASIO_RXD <= SIO_DATA_IN;
  SIO_DATA_IN <= '0' when (stm_sio_txd)='0' else 'Z';
  stm_sio_rxd <= ASIO_TXD;
  stm_sio_command <= CB2_OUT when CB2_DIR_OUT='1' else '1';
  SIO_DATA_OUT <= ASIO_TXD when ASIO_TXD='0' else 'Z';

  -- Unused SIO on Tonnere
  SIO_PROCEED <= 'Z';

  ---------------------------------------------------------------------------
  -- CARTRIDGE / PBI  (6502 bus master)
  ---------------------------------------------------------------------------
  pbi_disable <= antic_turbo when speed_6502="000001" else '1';

  bus_adaptor : entity work.pbi6502
    PORT MAP (
      CLK => CLK,
      RESET_N => RESET_N and SDRAM_RESET_N and not(reset_atari),
      ENABLE_179_EARLY => enable_179_early,
      REQUEST => pbi_request,
      ADDR_IN => pbi_addr,
      DATA_IN => pbi_write_data(7 downto 0),
      WRITE_IN => pbi_write_enable,
      PORTB => portb_out,
      ANTIC_REFRESH => antic_refresh,
      EXTERNAL_ACCESS => pbi_external,
      TAKEOVER => pbi_takeover,
      RELEASE => pbi_release,
      DISABLE => pbi_disable,
      COMPLETE => pbi_request_complete,
      MPD_N => MPD_N,
      SNOOP_DATA_IN => snoop_data(7 downto 0),
      SNOOP_DATA_READY => snoop_data_ready,
      DATA_OUT => pbi_data,
      DEBUG => pbi_debug_data(24 downto 0),
      DEBUG_READY => pbi_debug_ready,
      -- 6502 side
      BUS_DATA_IN => PBI_D,
      BUS_PHI1 => open,
      BUS_PHI2 => bus_phi2,
      BUS_ADDR_OUT => bus_addr_out,
      BUS_ADDR_OE => bus_addr_oe,
      BUS_DATA_OUT => bus_data_out,
      BUS_DATA_OE => bus_data_oe,
      BUS_WRITE_N => bus_write_n,
      BUS_S4_N => bus_s4_n,
      BUS_S5_N => bus_s5_n,
      BUS_CCTL_N => bus_cctl_n,
      BUS_D1XX_N => open,             -- TODO(tonnere): PBI_D1XX pin available if ECI needed
      BUS_CONTROL_OE => bus_control_oe,
      BUS_REFRESH_OE => bus_refresh_oe,
      BUS_CASINH_N => bus_casinh_n,
      BUS_CASINH_OE => bus_casinh_oe,
      BUS_CAS_N => bus_cas_n,
      BUS_RAS_N => bus_ras_n,
      BUS_RD4 => PBI_RD(4),
      BUS_RD5 => PBI_RD(5),
      PBI_MPD_N => PBI_MPD,
      PBI_REF_N => PBI_REF,
      PBI_EXTSEL_N => PBI_EXTSEL
    );

  pbi_debug_data(25) <= NMI_N;
  pbi_debug_data(27) <= IRQ_N;
  pbi_debug_data(28) <= RDY;
  pbi_debug_data(29) <= VIDEO_HS;
  pbi_debug_data(30) <= VIDEO_VS;
  pbi_debug_data(31) <= ANTIC_REFRESH;

  -- PBI/cart pin drive. On Tonnere these are the PBI_* inout bus-switch pins.
  PBI_CCTL <= bus_cctl_n when bus_control_oe='1' else 'Z';
  PBI_S4_N <= bus_s4_n   when bus_control_oe='1' else 'Z';
  PBI_S5_N <= bus_s5_n   when bus_control_oe='1' else 'Z';
  PBI_CAS  <= bus_cas_n  when bus_control_oe='1' else 'Z';
  PBI_RAS  <= bus_ras_n  when bus_control_oe='1' else 'Z';
  -- CASINH: no dedicated Tonnere pin in this entity. TODO(tonnere).
  PBI_A    <= bus_addr_out when bus_addr_oe='1' else (others=>'Z');
  PBI_D    <= bus_data_out when bus_data_oe='1' else (others=>'Z');
  PBI_PHI2 <= bus_phi2;
  PBI_RW_N <= bus_write_n;
  PBI_REF  <= '0' when bus_refresh_oe='1' else 'Z';
  PBI_RST  <= '0' when (RESET_N and not(reset_atari))='0' else '1';

  -- Remaining PBI pins present on Tonnere but not driven by this pass:
  PBI_HALT   <= 'Z';   -- TODO(tonnere)
  PBI_IRQ    <= 'Z';   -- input (PBI_IRQ_N on core) - see core map below
  PBI_RDY    <= 'Z';   -- TODO(tonnere)
  PBI_D1XX   <= 'Z';   -- TODO(tonnere)
  PBI_EXTENB <= 'Z';   -- TODO(tonnere)

  ---------------------------------------------------------------------------
  -- SCANDOUBLER  (colour phase toggle + scandoubler, same as Eclaire)
  ---------------------------------------------------------------------------
  process(clk,RESET_N,SDRAM_RESET_N,reset_atari)
  begin
    if ((RESET_N and SDRAM_RESET_N and not(reset_atari))='0') then
      half_scandouble_enable_reg <= '0';
    elsif (clk'event and clk='1') then
      half_scandouble_enable_reg <= half_scandouble_enable_next;
    end if;
  end process;
  half_scandouble_enable_next <= not(half_scandouble_enable_reg);

  scandoubler_inst : entity work.scandoubler
    GENERIC MAP ( video_bits => 8 )
    PORT MAP(
      CLK => CLK,
      RESET_N => RESET_N and SDRAM_RESET_N and not(reset_atari),
      VGA => scandouble,
      COMPOSITE_ON_HSYNC => csync,
      colour_enable => half_scandouble_enable_reg,
      doubled_enable => '1',
      scanlines_on => scanlines,
      vsync_in => VIDEO_VS,
      hsync_in => VIDEO_HS,
      csync_in => VIDEO_CS,
      pal => PAL,
      colour_in => ATARI_COLOUR,
      VSYNC => VIDEO_VSYNC,
      HSYNC => VIDEO_HSYNC,
      B => VIDEO_B,
      G => VIDEO_G,
      R => VIDEO_R
    );

    ---------------------------------------------------------------------------
  -- STM32 keyboard matrix -> POKEY keyboard response
  ---------------------------------------------------------------------------
  -- Provide results as if the STM32 keyboard matrix were a POKEY key grid.
  process(KEYBOARD_SCAN, KEYBOARD_MATRIX, KEYBOARD_CONTROL, KEYBOARD_SHIFT, KEYBOARD_BREAK)
  begin
    KEYBOARD_RESPONSE <= (others => '1');

    if KEYBOARD_MATRIX(to_integer(unsigned(not KEYBOARD_SCAN))) = '1' then
      KEYBOARD_RESPONSE(0) <= '0';
    end if;

    if KEYBOARD_SCAN(5 downto 4) = "00" and KEYBOARD_BREAK = '1' then
      KEYBOARD_RESPONSE(1) <= '0';
    end if;

    if KEYBOARD_SCAN(5 downto 4) = "10" and KEYBOARD_SHIFT = '1' then
      KEYBOARD_RESPONSE(1) <= '0';
    end if;

    if KEYBOARD_SCAN(5 downto 4) = "11" and KEYBOARD_CONTROL = '1' then
      KEYBOARD_RESPONSE(1) <= '0';
    end if;
  end process;

  ---------------------------------------------------------------------------
  -- MACHINE RESET request handling (from STM32), as Eclaire
  ---------------------------------------------------------------------------
  process(reset_atari_request,atari800mode)
  begin
    reset_atari <= '0';
    antic_rnmi_n <= '1';
    if (atari800mode='1') then -- TODO: Why was this gated on fkeys(9)?
      antic_rnmi_n <= not(reset_atari_request);
    else
      reset_atari <= reset_atari_request;
    end if;
  end process;

  ---------------------------------------------------------------------------
  -- FULL ATARI CORE
  ---------------------------------------------------------------------------
  atari800 : entity work.atari800core
    GENERIC MAP (
      cycle_length => 32,
      video_bits => 8,
      palette => 0,
      internal_ram => GENERIC_INTERNAL_RAM,
      freezer_debug => 1,
      sid => GENERIC_SID
    )
    PORT MAP (
      CLK => CLK,
      RESET_N => RESET_N and SDRAM_RESET_N and not(reset_atari),
      VIDEO_VS => VIDEO_VS,
      VIDEO_HS => VIDEO_HS,
      VIDEO_CS => VIDEO_CS,
      VIDEO_B => ATARI_COLOUR,
      VIDEO_G => open,
      VIDEO_R => open,
      VIDEO_BLANK => VIDEO_BLANK,
      VIDEO_BURST => VIDEO_BURST,
      VIDEO_START_OF_FIELD => open,
      VIDEO_ODD_LINE => VIDEO_ODD_LINE,
      AUDIO_L => AUDIO_L_PCM,
      AUDIO_R => AUDIO_R_PCM,
      SIO_AUDIO => (others=>'0'),
      CA1_IN => CA1_IN,
      CB1_IN => CB1_IN,
      CA2_IN => CA2_IN,
      CA2_OUT => CA2_OUT,
      CA2_DIR_OUT => CA2_DIR_OUT,
      CB2_IN => CB2_IN,
      CB2_OUT => CB2_OUT,
      CB2_DIR_OUT => CB2_DIR_OUT,
      PORTA_IN => PORTA_IN,
      PORTA_DIR_OUT => PORTA_DIR_OUT,
      PORTA_OUT => PORTA_OUT,
      PORTB_IN => PORTB_IN,
      PORTB_DIR_OUT => PORTB_DIR_OUT,
      PORTB_OUT => PORTB_OUT,
      NMI_N_OUT => NMI_N,
      IRQ_N_OUT => IRQ_N,
      RDY_OUT => RDY,
      AN_OUT => AN,
      KEYBOARD_RESPONSE => KEYBOARD_RESPONSE,
      KEYBOARD_SCAN => KEYBOARD_SCAN,
      POT_IN => POT_IN,
      POT_RESET => POT_RESET,
      ENABLE_179_EARLY => ENABLE_179_EARLY,
      PBI_ADDR => PBI_ADDR,
      PBI_WRITE_ENABLE => PBI_WRITE_ENABLE,
      PBI_SNOOP_DATA => SNOOP_DATA,
      PBI_SNOOP_READY => SNOOP_DATA_READY,
      PBI_WRITE_DATA => PBI_WRITE_DATA,
      PBI_WIDTH_8bit_ACCESS => PBI_WIDTH_8bit_ACCESS,
      PBI_WIDTH_16bit_ACCESS => PBI_WIDTH_16bit_ACCESS,
      PBI_WIDTH_32bit_ACCESS => PBI_WIDTH_32bit_ACCESS,
      PBI_ROM_DO => pbi_data,
      PBI_REQUEST => pbi_request,
      PBI_REQUEST_COMPLETE => pbi_request_complete,
      PBI_TAKEOVER => pbi_takeover,
      PBI_RELEASE => pbi_release,
      PBI_DISABLE => pbi_disable,
      CART_RD5 => PBI_RD(5),
      PBI_MPD_N => MPD_N,
      PBI_IRQ_N => PBI_IRQ,          -- Tonnere PBI_IRQ inout used as input
      SIO_RXD => ASIO_RXD,
      SIO_TXD => ASIO_TXD,
      SIO_CLOCKIN_IN => ASIO_CLOCKIN_IN,
      SIO_CLOCKIN_OUT => ASIO_CLOCKIN_OUT,
      SIO_CLOCKIN_OE => ASIO_CLOCKIN_OE,
      SIO_CLOCKOUT => ASIO_CLOCKOUT,
      CONSOL_OPTION => CONSOL_OPTION_INT,
      CONSOL_SELECT => CONSOL_SELECT_INT,
      CONSOL_START => CONSOL_START_INT,
      GTIA_TRIG => GTIA_TRIG,
      ANTIC_LIGHTPEN => ANTIC_LIGHTPEN,
      SDRAM_REQUEST => SDRAM_REQUEST,
      SDRAM_REQUEST_COMPLETE => SDRAM_REQUEST_COMPLETE,
      SDRAM_READ_ENABLE => SDRAM_READ_ENABLE,
      SDRAM_WRITE_ENABLE => SDRAM_WRITE_ENABLE,
      SDRAM_ADDR => SDRAM_ADDR,
      SDRAM_DO => SDRAM_DO,
      ANTIC_REFRESH => ANTIC_REFRESH,
      ANTIC_TURBO => ANTIC_TURBO,
      ANTIC_RNMI_N => ANTIC_RNMI_N,
      RAM_ADDR => RAM_ADDR,
      RAM_DO => RAM_DO,
      RAM_REQUEST => RAM_REQUEST,
      RAM_REQUEST_COMPLETE => RAM_REQUEST_COMPLETE,
      RAM_WRITE_ENABLE => RAM_WRITE_ENABLE,
      ROM_ADDR => ROM_ADDR,
      ROM_DO => ROM_DO,
      ROM_REQUEST => ROM_REQUEST,
      ROM_REQUEST_COMPLETE => ROM_REQUEST_COMPLETE,
      ROM_WRITE_ENABLE => ROM_WRITE_ENABLE,
      DMA_FETCH => dma_fetch,
      DMA_READ_ENABLE => dma_read_enable,
      DMA_32BIT_WRITE_ENABLE => dma_32bit_write_enable,
      DMA_16BIT_WRITE_ENABLE => dma_16bit_write_enable,
      DMA_8BIT_WRITE_ENABLE => dma_8bit_write_enable,
      DMA_ADDR => dma_addr_fetch,
      DMA_WRITE_DATA => dma_write_data,
      MEMORY_READY_DMA => dma_memory_ready,
      RAM_SELECT => ram_select,
      CART_EMULATION_SELECT => emulated_cartridge_select,
      PAL => PAL,
      ROM_IN_RAM => ROM_IN_RAM,
      THROTTLE_COUNT_6502 => speed_6502,
      TURBO_VBLANK_ONLY => turbo_vblank_only,
      HALT => pause_atari,
      ATARI800MODE => atari800mode,
      freezer_enable => freezer_enable,
      freezer_activate => freezer_activate,
      freezer_state_out => freezer_state,
      state_reg_out => state_reg_out,
      memory_ready_antic_out => memory_ready_antic_out,
      memory_ready_cpu_out => memory_ready_cpu_out,
      shared_enable_out => shared_enable_out,
      freezer_debug_addr => FREEZE_ADDR,
      freezer_debug_data => FREEZE_DATA_CTRL(7 downto 0),
      freezer_debug_read => FREEZE_DATA_CTRL(8),
      freezer_debug_write => FREEZE_DATA_CTRL(9),
      freezer_debug_data_match => FREEZE_DATA_CTRL(10)
    );

  ROM_IN_RAM <= '1' when GENERIC_INTERNAL_ROM=0 else '0';

  -- FSMC (STM32) bus: not used by the ported core yet.
  dma_32bit_write_enable <= '0'; -- 16-bit FSMC bus, no 32-bit access

      -- SPI master : SD not present on Tonnere; SELECT1 drives the EPCQ via SFL.
--      ZPU_SPI_DI => spi_flash_di,          -- flash MISO (via SFL). (was sd_dat0 AND flash_di on Eclaire)
--      ZPU_SPI_CLK => spi_clk,
--      ZPU_SPI_DO => spi_do,
--      ZPU_SPI_SELECT0 => open,             -- TODO(tonnere): SD card select if ever wired
--      ZPU_SPI_SELECT1 => spi_flash_select,
  spi_flash_select <= '0'; -- Tie off for now since we will use STM32 flash. Potentially we may use it so left sfl for now. In theory STM can directly write to the flash ic too...
  dma_write_data(31 downto 16) <= (others=>'0');

  fsmc_adapt1 : entity work.fsmc_adaptor
    GENERIC MAP (
      version => 1
    )
    PORT MAP (
      CLK => CLK,
      CLK_FAST  => CLK_SDRAM,
      RESET_N => RESET_N and sdram_reset_n,

      DMA_ADDR_FETCH => dma_addr_fetch,
      DMA_DATA_OUT => dma_write_data(15 downto 0),
      DMA_FETCH => dma_fetch,
      DMA_16BIT_WIDTH => dma_16bit_write_enable, -- width, no write enable!!
      DMA_8BIT_WIDTH => dma_8bit_write_enable,
      DMA_READ_ENABLE => dma_read_enable,
      DMA_MEMORY_READY => dma_memory_ready,
      DMA_MEMORY_DATA => snoop_data(15 downto 0),

      -- built-in Pokey / SIO to Atari -> potentially handled by Fujinet-alike on ESP32 instead later? Wire up STM so it can anyway.
      SIO_POKEY_ENABLE => stm_pokey_enable,
      SIO_TXD => stm_sio_txd,
      SIO_RXD => stm_sio_rxd,
      SIO_COMMAND => stm_sio_command,
      SIO_CLK => ASIO_CLOCKOUT,

      -- Sit on STM FSMC bus
      -- NB: async to fpga clocks
      FSMC_A     => FSMC_A,
      FSMC_D_IN  => FSMC_D,
      FSMC_D_OUT => FSMC_D_OUT,
      FSMC_D_OE  => FSMC_D_OE,
      FSMC_NBL   => FSMC_NBL,
      FSMC_NE    => FSMC_NE,
      FSMC_NOE   => FSMC_NOE,
      FSMC_NWE   => FSMC_NWE,
      FSMC_NWAIT => FSMC_NWAIT,
      FSMC_IRQ   => FPGA_IRQ_INT,

      -- external control
      -- dedicated regs
      CONTROL => CONTROL,
      RAMCONFIG => RAMCONFIG,
      PERFORMANCE => PERFORMANCE,
      CART => CART,
      VIDEO => VIDEO,

      KEYBOARD_MATRIX => KEYBOARD_MATRIX,
      KEYBOARD_SHIFT => KEYBOARD_SHIFT,
      KEYBOARD_CONTROL => KEYBOARD_CONTROL,
      KEYBOARD_BREAK => KEYBOARD_BREAK,
      CONSOLE_INJECT => CONSOLE_INJECT,
      CONSOLE_PHYS => CONSOLE_PHYS,
      JOY0_INJECT => JOY0_INJECT,
      JOY1_INJECT => JOY1_INJECT,
      JOY2_INJECT => JOY2_INJECT,
      JOY3_INJECT => JOY3_INJECT,
      JOY0_PHYS => JOY_TRIG(0)&JOY_DIR(3 downto 0),
      JOY1_PHYS => JOY_TRIG(1)&JOY_DIR(7 downto 4),
      JOY2_PHYS => JOY2_TRIG(2) & JOY2_DIR(11 downto 8),
      JOY3_PHYS => JOY2_TRIG(3) & JOY2_DIR(15 downto 12),

      POT_TRIGGER => POT_IN, --ADC on STM drives paddles
      POT_RESET => POT_RESET, -- To allow an IRQ to reset the pots

      FREEZE_ADDR => FREEZE_ADDR,
      FREEZE_DATA_CTRL => FREEZE_DATA_CTRL,

      DEBUG0 => (others=>'0'),
      DEBUG1 => (others=>'0'),
      DEBUG2 => (others=>'0'),
      DEBUG3 => (others=>'0')
    );

  FSMC_D <= FSMC_D_OUT when FSMC_D_OE='1' else (others=>'Z');
  FPGA_IRQ <= '0' when FPGA_IRQ_INT='1' else 'Z'; -- Active low open drain externally (pull up elsewhere), active high internally

  -- From STM
  reset_atari_request <= CONTROL(0) or not(CONSOL_RESET);
  pause_atari <= CONTROL(1);
  freezer_enable <= CONTROL(2);
  atari800mode <= CONTROL(3);

  ram_select <= RAMCONFIG;

  speed_6502 <= PERFORMANCE(5 downto 0);
  turbo_vblank_only <= PERFORMANCE(8);

  emulated_cartridge_select <= CART;

  video_mode <= "001"; --VIDEO(2 downto 0) TODO - put back FIXME!
  PAL <= video(4);
  scanlines <= VIDEO(5);
  csync <= VIDEO(6);

  -- To STM
  CONSOLE_PHYS(0) <= CONSOL_START;
  CONSOLE_PHYS(1) <= CONSOL_SELECT;
  CONSOLE_PHYS(2) <= CONSOL_OPTION;

  -- Merge STM and FPGA lines
  CONSOL_START_INT <= CONSOLE_INJECT(0) or CONSOL_START;
  CONSOL_SELECT_INT <= CONSOLE_INJECT(1) or CONSOL_SELECT;
  CONSOL_OPTION_INT <= CONSOLE_INJECT(2) or CONSOL_OPTION;

  JOY_DIR_INT <= JOY_DIR and (JOY1_INJECT(3 downto 0) & JOY0_INJECT(3 downto 0));
  JOY2_DIR_INT <= JOY2_DIR and (JOY3_INJECT(3 downto 0) & JOY2_INJECT(3 downto 0));

  JOY_TRIG_INT <= JOY_TRIG and (JOY1_INJECT(4) & JOY0_INJECT(4));
  JOY2_TRIG_INT <= JOY2_TRIG and (JOY3_INJECT(4) & JOY2_INJECT(4));

  enable_179_clock_div_stm_pokey : entity work.enable_divider
    generic map ( COUNT => 32 )
    port map ( clk=>clk, reset_n=>reset_n, enable_in=>'1', enable_out=>stm_pokey_enable );

  ---------------------------------------------------------------------------
  -- SERIAL FLASH LOADER (EPCQ) - post-config access to config flash via ASMI.
  -- The SFL IP owns the dedicated config pins (DCLK / nCSO / ASDO / DATA0),
  -- so there are NO top-level flash pins to map here. The ZPU SPI master talks
  -- to the flash through this bridge:
  --   spi_do  (MOSI) -> data_in(0)   (data_oe(0)='1', driven)
  --   spi_flash_di    <- data_out(1) (MISO, data_oe(1)='0', input)
  --   spi_clk         -> dclk_in
  --   spi_flash_select-> ncso_in
  -- asmi_access_granted tied '0' / request left open: no runtime arbitration
  -- with a second ASMI master (matches Eclaire).
  ---------------------------------------------------------------------------
  sfl_spi : sfl
    port map (
      asmi_access_granted => '0',
      asmi_access_request => open,
      asdo_in             => 'Z', --spi_do,   -- bit0 = MOSI; bits 1-3 unused
      --data_oe             => "0001",            -- only bit0 driven
      data0_out            => sfl_data_out,
      dclk_in             => 'Z', --spi_clk,
      ncso_in             => 'Z', --spi_flash_select,
      noe_in              => 'Z' --'0' -- TODO
    );
  spi_flash_di <= sfl_data_out;              -- flash MISO

  ---------------------------------------------------------------------------
  -- AUDIO : core PCM -> I2S -> PCM5102A 
  ---------------------------------------------------------------------------
  AUDIO_L_PCM_SIGNED <= signed(not(AUDIO_L_PCM(15))&AUDIO_L_PCM(14 downto 0));
  AUDIO_R_PCM_SIGNED <= signed(not(AUDIO_R_PCM(15))&AUDIO_R_PCM(14 downto 0));

  audio_codec_data : entity work.i2smaster
    PORT MAP(
      CLK => CLK1_536,
      RESET_N => MASTER_RESET_N,
      BCLK => FPGAAUD_BCK,
      DACLRC => FPGAAUD_LR,
      LEFT_IN => std_logic_vector(AUDIO_L_PCM_SIGNED),
      RIGHT_IN => std_logic_vector(AUDIO_R_PCM_SIGNED),
      DACDAT => FPGAAUD_DATA
    );

  ---------------------------------------------------------------------------
  -- VIDEO OUTPUT
  -- (1) Tonnere analogue video DAC (sdm_dac_video) - fed from scandoubled RGB
  --     via the video_mode mux below.
  -- (2) svideo_gtia generator (ported from Eclaire).
  -- (3) scandoubler_hdmi (Eclaire) -> Tonnere altddio_out1 -> HDMI pins.
  --     The altddio serializer is Tonnere's own instance, kept unchanged.
  ---------------------------------------------------------------------------

  -- video_mode mux: selects what the analogue VDAC sees. (Same intent as the
  -- Eclaire VGA mux, but the target here is sdm_dac_video, and HDMI is always
  -- generated in parallel by scandoubler_hdmi.)
  process(video_mode,adj_red,adj_green,adj_blue,adj_vsync,adj_hsync,adj_blank,
          VIDEO_R,VIDEO_G,VIDEO_B,VIDEO_BLANK,VIDEO_VSYNC,VIDEO_HSYNC,
          svideo_yout,svideo_c,svideo_sync_n)
  begin
    -- defaults: scandoubled RGB
    VGA_R <= VIDEO_R;
    VGA_G <= VIDEO_G;
    VGA_B <= VIDEO_B;
    VGA_BLANK  <= VIDEO_BLANK;
    VGA_VSYNC  <= VIDEO_VSYNC;
    VGA_HSYNC  <= VIDEO_HSYNC;
    scandouble <= '0';
    scandoubler_format <= "00";
    comp_n <= '1';

    case video_mode is
      when "000" =>          -- original RGB
        null;
      when "001" =>          -- scandoubled
        scandouble <= '1';
		  VGA_BLANK <= '0';
      when "010" =>          -- svideo
        VGA_G <= svideo_yout;
        VGA_B <= svideo_c;
        VGA_R <= svideo_c;
      when "011" =>          -- 480p/576p exact (HDMI + analogue)
        VGA_R <= adj_red;  VGA_G <= adj_green; VGA_B <= adj_blue;
        VGA_BLANK <= adj_blank; VGA_VSYNC <= adj_vsync; VGA_HSYNC <= adj_hsync;
        scandoubler_format <= "10";
      when "100" =>          -- 480p/576p DVI
        VGA_R <= adj_red;  VGA_G <= adj_green; VGA_B <= adj_blue;
        VGA_BLANK <= adj_blank; VGA_VSYNC <= adj_vsync; VGA_HSYNC <= adj_hsync;
        scandoubler_format <= "01";
      when "101" =>          -- 480p/576p VGA-exact
        VGA_R <= adj_red;  VGA_G <= adj_green; VGA_B <= adj_blue;
        VGA_BLANK <= adj_blank; VGA_VSYNC <= adj_vsync; VGA_HSYNC <= adj_hsync;
        scandoubler_format <= "00";
      when "110" =>          -- composite
        VGA_R <= (others=>'0');
        VGA_G <= svideo_yout;
        VGA_B <= (others=>'0');
        comp_n <= '0';
      when others => null;
    end case;
  end process;

  VDAC_HSYNC <= VGA_HSYNC;
  VDAC_VSYNC <= VGA_VSYNC;

  vdac : entity work.sdm_dac_video
    port map (
      clk_pattern => CLK_PATTERN,
      rst_n => RESET_N,
      clk_pixel => CLK,
      in_r => VGA_R,
      in_g => VGA_G,
      in_b => VGA_B,
      in_blank_n => not(VGA_BLANK),
      dac_r(1) => VDAC_RH, dac_g(1) => VDAC_GH, dac_b(1) => VDAC_BH,
      dac_r(0) => VDAC_RL, dac_g(0) => VDAC_GL, dac_b(0) => VDAC_BL
    );

  -- SVIDEO generator (ported from Eclaire)
  svideo_imple : entity work.svideo_gtia
    PORT MAP (
      CLK => clk,
      RESET_N => reset_n,
      brightness => ATARI_COLOUR(3 downto 0),
      hue => ATARI_COLOUR(7 downto 4),
      burst => VIDEO_BURST,
      blank => VIDEO_BLANK,
      sof => VIDEO_VS,
      csync_n => not(VIDEO_CS),
      vpos_lsb => VIDEO_ODD_LINE,
      pal => pal,
      composite => not(COMP_N),
      chroma => svideo_c,
      luma => svideo_yout,
      luma_sync_n => svideo_sync_n
    );

  -- HDMI scandoubler (ported from Eclaire) -> Tonnere altddio_out1 -> HDMI pins
  scandoubler_hdmi_int : entity work.scandoubler_hdmi
    PORT MAP (
      CLK_ATARI_IN => CLK,
      RESET_N => RESET_N,
      audio_left => audio_l_pcm,
      audio_right => audio_r_pcm,
      pal => pal,
      scanlines_on => scanlines,
      csync_on => csync,
      format => scandoubler_format,
      colour_enable => half_scandouble_enable_reg,
      colour_in => atari_colour,
      vsync_in => VIDEO_VS,
      hsync_in => VIDEO_HS,
      CLK_HDMI_IN => CLK_HDMI_IN,
      CLK_PIXEL_IN => CLK_PIXEL_IN,
      O_hsync => adj_hsync,
      O_vsync => adj_vsync,
      O_blank => adj_blank,
      O_red => adj_red,
      O_green => adj_green,
      O_blue => adj_blue,
      O_TMDS_H => tmds_h,
      O_TMDS_L => tmds_l
    );

  -- HDMI serializer: Tonnere's OWN altddio_out1 (kept), fed from the Eclaire
  -- scandoubler_hdmi H/L TMDS words. scandoubler_hdmi already packs the clock
  -- lane (shift_clk) into O_TMDS_H/L, so H/L map straight onto the 8 DDR lanes.
  ddio_inst : entity work.altddio_out1
    port map (
      datain_h => TMDS_H,
      datain_l => TMDS_L,
      outclock => CLK_HDMI,
      dataout  => DDIO_OUT
    );

  HDMI_D2P <= DDIO_OUT(7);
  HDMI_D2N <= DDIO_OUT(6);
  HDMI_D1P <= DDIO_OUT(5);
  HDMI_D1N <= DDIO_OUT(4);
  HDMI_D0P <= DDIO_OUT(3);
  HDMI_D0N <= DDIO_OUT(2);
  HDMI_CKP <= DDIO_OUT(1);
  HDMI_CKN <= DDIO_OUT(0);

end vhdl;
