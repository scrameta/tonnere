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
--   Post-config flash access would require the SFL IP or ALTASMI_PARALLEL.
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
    signal CLK27 : std_logic;
	 signal CLK54 : std_logic;
	 signal CLK56 : std_logic;
	 signal CLK108 : std_logic;
	 signal CLK108_N : std_logic;
	 signal CLK74_25 : std_logic;
	 signal CLK_PATTERN : std_logic;
	 signal CLK1_536 : std_logic;
	 
	 signal AUD_RESET_N : std_logic;
	 signal VDAC_RESET_N : std_logic;
	 signal VIDEO_RESET_N : std_logic;
	 signal HDMI_RESET_N : std_logic;
	 
	signal clk_pixel_in : std_logic;
	signal clk_pixel : std_logic;
	signal clk_hdmi : std_logic;
	
        signal AUDIO_L_PCM : std_logic_vector(15 downto 0);
        signal AUDIO_R_PCM : std_logic_vector(15 downto 0);
	signal AUDIO_L_PCM_SIGNED : signed(15 downto 0);
	signal AUDIO_R_PCM_SIGNED : signed(15 downto 0);

	signal video_vsync : std_logic;
	signal video_hsync : std_logic;
	signal video_csync : std_logic;
	signal video_b : std_logic_vector(7 downto 0);
	signal video_g : std_logic_vector(7 downto 0);
	signal video_r : std_logic_vector(7 downto 0);
	signal video_blank : std_logic;
	signal video_burst : std_logic;
	
	signal test_pbi_toggle_reg : std_logic_vector(41 downto 0);
	signal test_joy_toggle_reg : std_logic_vector(19 downto 0);
	signal test_sio_toggle_reg : std_logic_vector(13 downto 0);

	signal ddio_out : std_logic_vector(7 downto 0);
	
	signal SDRAM_DO : std_logic_vector(31 downto 0);
	signal SDRAM_READ_ENABLE : std_logic;
	signal SDRAM_WRITE_ENABLE : std_logic;
	signal SDRAM_REQUEST : std_logic;
	signal SDRAM_REQUEST_COMPLETE : std_logic;

-- Function to replace '1' with 'Z' in a std_logic_vector
function open_drain(vec : std_logic_vector) return std_logic_vector is
    variable result : std_logic_vector(vec'range);
begin
    for i in vec'range loop
        if vec(i) = '1' then
            result(i) := 'Z';
        else
            result(i) := vec(i);
        end if;
    end loop;
    return result;
end function;
	
begin
    -- FSMC bus from STM (we are SRAM!)
    --FSMC_D <= (others=>'Z'); --Do not drive
    --FSMC_NWAIT <= '1';       --Do not wait

    -- IRQ to STM, is this active low or high?
    FPGA_IRQ <= 'Z';

    -- FPGA_GPIO
    FPGA_GPIO <= (others=>'Z');

    -- ESP32 SPI slave bus  (ESP is master, to DMA to ram)
    ESP_MISO <= 'Z';

    -- I2S audio to PCM5102A DAC
    --FPGAAUD_BCK <= 'Z';
    --FPGAAUD_LR  <= 'Z';
    --FPGAAUD_DATA <= 'Z';
	 
    pll_aud1 : ENTITY work.pll_aud
	 PORT MAP
    (
        inclk0 => CLK27_A12,
        c0		=> CLK1_536,
        c1		=> CLK56,
        locked => AUD_RESET_N
    );	 

audio_codec_data : entity work.i2smaster
PORT MAP(CLK => CLK1_536,
		RESET_N => AUD_RESET_N,
		 BCLK => FPGAAUD_BCK,
		 DACLRC => FPGAAUD_LR,
		 LEFT_IN => std_logic_vector(AUDIO_L_PCM_SIGNED),
		 RIGHT_IN => std_logic_vector(AUDIO_R_PCM_SIGNED),
		 DACDAT => FPGAAUD_DATA);

    pll_video1 : ENTITY work.pll_video
	 PORT MAP
    (
        inclk0 => CLK27_A12,
        c0		=> CLK27,
        c1		=> CLK74_25,
		  c2		=> CLK54,
		  c3		=> CLK108,
		  c4		=> CLK108_N,
        locked => VIDEO_RESET_N
    );

    pll_vdac1 : ENTITY work.pll_vdac
	 PORT MAP
    (
        inclk0 => CLK27_A12,
        c0		=> CLK_PATTERN,
        locked => VDAC_RESET_N
    );
	 
    ----VDAC_HSYNC <= test_hsync; -- and test_vsync;
    --VDAC_HSYNC <= not(video_hsync or video_vsync);
    ----VDAC_HSYNC <= test_hsync and test_vsync;
    --VDAC_VSYNC <= video_vsync;

    --VDAC_HSYNC <= not(video_hsync);
    --VDAC_VSYNC <= not(video_vsync);
    VDAC_HSYNC <= video_hsync;
    VDAC_VSYNC <= video_vsync;

--	VDAC_HSYNC <= not(video_csync);
--	VDAC_VSYNC <= '1';

--                        if (composite_on_hsync = '1') then
--                                --hsync_next <= not(hsync_in xor vsync_in);
--                                hsync_next <= not(csync_in);
--                                vsync_next <='1';
--                        else
--                                hsync_next <= not(hsync_in);
--                                vsync_next <= not(vsync_in);
--                        end if;
    
	 
    vdac : entity work.sdm_dac_video
    port map (
        clk_pattern  => CLK_PATTERN,
        rst_n    => AUD_RESET_N,

        -- 8-bit unsigned pixel inputs, in the clk_pix domain.
        -- Internally synchronised to clk_sdm via two-stage CDC.
        clk_pixel => CLK56,
        in_r     => video_r,
        in_g     => video_g,
        in_b     => video_b,
        in_blank_n => not(video_blank),
        --in_r     => std_logic_vector(to_unsigned(120,8)),
        --in_g     => std_logic_vector(to_unsigned(120,8)),
        --in_b     => std_logic_vector(to_unsigned(120,8)),  

        -- 1-bit DDR outputs — one pin each, 540 Mbps
        dac_r(1)    => VDAC_RH,
        dac_g(1)    => VDAC_GH,
        dac_b(1)    => VDAC_BH,
        dac_r(0)    => VDAC_RL,
        dac_g(0)    => VDAC_GL,
        dac_b(0)    => VDAC_BL
    ); 

    -- HDMI TMDS
    -- HDMI_D2P <= 'Z';
    -- HDMI_D2N <= 'Z';
    -- HDMI_D1P <= 'Z';
    -- HDMI_D1N <= 'Z';
    -- HDMI_D0P <= 'Z';
    -- HDMI_D0N <= 'Z';
    -- HDMI_CKP <= 'Z';
    -- HDMI_CKN <= 'Z';

    clk_pixel_in <= CLK27;
    pll_hdmi1 : ENTITY work.pll_hdmi
    PORT MAP
    (
        inclk0 => clk_pixel_in,
        c0     => clk_pixel,
        c1     => clk_hdmi,
        locked => HDMI_RESET_N
    );

    hdmi_inst : entity work.hdmi
    port map (
    	I_CLK_PIXEL	=> clk_pixel,
    	I_CLK_TMDS	=> clk_hdmi,	-- pixelclock*5
    	I_HSYNC		=> video_hsync,
    	I_VSYNC		=> video_vsync,
    	I_BLANK		=> video_blank,
    	I_RED		=> video_r,
    	I_GREEN		=> video_g,
    	I_BLUE		=> video_b,
    	O_TMDS		=> ddio_out
    );
    HDMI_D2P <= DDIO_OUT(7); -- D2P
    HDMI_D2N <= DDIO_OUT(6); -- D2N
    HDMI_D1P <= DDIO_OUT(5); -- D1P
    HDMI_D1N <= DDIO_OUT(4); -- D1N
    HDMI_D0P <= DDIO_OUT(3); -- D0P
    HDMI_D0N <= DDIO_OUT(2); -- D0N
    HDMI_CKP <= DDIO_OUT(1); -- C P
    HDMI_CKN <= DDIO_OUT(0); -- C N

    -- SDRAM  (AS4C16M16SA U35, FPGA-mastered, direct)
--    SDRAM1_A        <= (others=>'Z');
--    SDRAM1_BA       <= (others=>'Z');
--    SDRAM1_DQ       <= (others=>'Z');
--    SDRAM1_CAS_N    <= 'Z';
--    SDRAM1_RAS_N    <= 'Z';
--    SDRAM1_WE_N     <= 'Z';
--    SDRAM1_CS_N     <= '1';
--    SDRAM1_CKE      <= 'Z';
--    SDRAM1_CLK      <= 'Z';
--    SDRAM1_LDQM     <= 'Z';
--    SDRAM1_UDQM     <= 'Z';

sdram_adaptor : entity work.sdram_statemachine
GENERIC MAP(ADDRESS_WIDTH => 22,
			AP_BIT => 10,
			COLUMN_WIDTH => 8,
			ROW_WIDTH => 12
			)
PORT MAP(CLK_SYSTEM => CLK54,
		 CLK_SDRAM => CLK108,
		 RESET_N =>  VIDEO_RESET_N,

		 READ_EN => SDRAM_READ_ENABLE,
		 WRITE_EN => SDRAM_WRITE_ENABLE,
		 REQUEST => SDRAM_REQUEST,
		 COMPLETE => SDRAM_REQUEST_COMPLETE,
		 BYTE_ACCESS => '0',
		 WORD_ACCESS => '1',
		 LONGWORD_ACCESS => '0',
		 REFRESH => '0',
		 ADDRESS_IN => FSMC_A(21 downto 0)&'0',
		 DATA_IN => FSMC_D&FSMC_D,
		 DATA_OUT => SDRAM_DO,

		 SDRAM_DQ => SDRAM1_DQ,
		 SDRAM_BA0 => SDRAM1_BA(0),
		 SDRAM_BA1 => SDRAM1_BA(1),
		 SDRAM_CKE => SDRAM1_CKE,
		 SDRAM_CS_N => SDRAM1_CS_N,
		 SDRAM_RAS_N => SDRAM1_RAS_N,
		 SDRAM_CAS_N => SDRAM1_CAS_N,
		 SDRAM_WE_N => SDRAM1_WE_N,
		 SDRAM_ldqm => SDRAM1_LDQM,
		 SDRAM_udqm => SDRAM1_UDQM,
		 SDRAM_ADDR => SDRAM1_A(11 downto 0),
		 reset_client_n => open
		 );

SDRAM1_A(12) <= '0';
SDRAM1_CLK <= CLK108_N;

    SDRAM_REQUEST <= not(FSMC_NE(1));
    SDRAM_READ_ENABLE <= FSMC_NWE;
    SDRAM_WRITE_ENABLE <= not(FSMC_NWE);

    FSMC_D <= SDRAM_DO(15 downto 0) when FSMC_NOE='0' else (others=>'Z');
    FSMC_NWAIT <= not(SDRAM_REQUEST_COMPLETE);

    -- SRAM 1  (IS61WV204816BLL U28, FPGA-mastered, direct)
    -- 512KB
    SRAM1_A         <= (others=>'Z');
    SRAM1_D         <= (others=>'Z');
    SRAM1_CE_N      <= '1';
    SRAM1_OE_N      <= '1';
    SRAM1_W_N       <= 'Z';
    SRAM1_LB_N      <= 'Z';
    SRAM1_UB_N      <= 'Z';

--    SRAM1_A <= FSMC_A(19 downto 0);
--    SRAM1_D <= FSMC_D when FSMC_NOE='1' else (others=>'Z');
--    SRAM1_CE_N <= FSMC_NE(1);
--    SRAM1_OE_N <= FSMC_NOE;
--    SRAM1_W_N <= FSMC_NWE;
--    SRAM1_LB_N <= FSMC_NBL(0);
--    SRAM1_UB_N <= FSMC_NBL(1);
--
--    FSMC_D <= SRAM1_D when FSMC_NOE='0' else (others=>'Z');
--    FSMC_NWAIT <= '1';       --Do not wait

    -- SRAM 2  (IS61WV204816BLL U32, FPGA-mastered, direct)
    SRAM2_A         <= (others=>'Z');
    SRAM2_D         <= (others=>'Z');
    SRAM2_CE_N      <= '1';
    SRAM2_OE_N      <= '1';
    SRAM2_W_N       <= 'Z';
    SRAM2_LB_N      <= 'Z';
    SRAM2_UB_N      <= 'Z';

--    SRAM2_A <= FSMC_A(19 downto 0);
--    SRAM2_D <= FSMC_D when FSMC_NOE='1' else (others=>'Z');
--    SRAM2_CE_N <= FSMC_NE(1);
--    SRAM2_OE_N <= FSMC_NOE;
--    SRAM2_W_N <= FSMC_NWE;
--    SRAM2_LB_N <= FSMC_NBL(0);
--    SRAM2_UB_N <= FSMC_NBL(1);
--
--    FSMC_D <= SRAM2_D when FSMC_NOE='0' else (others=>'Z');
--    FSMC_NWAIT <= '1';       --Do not wait

    -- PBI (Parallel Bus Interface)
	 pbi_test : entity work.io_square_test
    generic map(
        G_CLK_HZ        => 27_000_000,
        G_NUM_PINS      => 42,
        G_BASE_FREQ_HZ  => 100_000,
        G_STEP_FREQ_HZ  => 1000
    )
    port map (
        clk     => CLK27_A12,
        rst     => not(AUD_RESET_N),
        io_out  => test_pbi_toggle_reg
    );
    PBI_A           <= test_pbi_toggle_reg(15 downto 0);
    PBI_D           <= test_pbi_toggle_reg(23 downto 16);
    PBI_PHI2        <= test_pbi_toggle_reg(24);
    PBI_RW_N        <= test_pbi_toggle_reg(25);
    PBI_RD          <= test_pbi_toggle_reg(27 downto 26);
    PBI_HALT        <= test_pbi_toggle_reg(28);
    PBI_IRQ         <= test_pbi_toggle_reg(29);
    PBI_RST         <= test_pbi_toggle_reg(30);
    PBI_RDY         <= test_pbi_toggle_reg(31);
    PBI_REF         <= test_pbi_toggle_reg(32);
    PBI_RAS         <= test_pbi_toggle_reg(33);
    PBI_CAS         <= test_pbi_toggle_reg(34);
    PBI_MPD         <= test_pbi_toggle_reg(35);
    PBI_S4_N        <= test_pbi_toggle_reg(36);
    PBI_S5_N        <= test_pbi_toggle_reg(37);
    PBI_CCTL        <= test_pbi_toggle_reg(38);
    PBI_D1XX        <= test_pbi_toggle_reg(39);
    PBI_EXTENB      <= test_pbi_toggle_reg(40);
    PBI_EXTSEL      <= test_pbi_toggle_reg(41);

    -- SIO (Serial I/O)
    --SIO_DATA_IN     <= 'Z';
    --SIO_DATA_OUT    <= 'Z';
    --SIO_CLOCK_IN    <= 'Z';
    --SIO_CLOCK_OUT   <= 'Z';
    --SIO_COMMAND     <= 'Z';
    --SIO_PROCEED     <= 'Z';
    --SIO_INTERRUPT   <= 'Z';
    --SIO_MOTOR       <= 'Z';

    sio_test : entity work.io_square_test
    generic map(
        G_CLK_HZ        => 27_000_000,
        G_NUM_PINS      => 14,
        G_BASE_FREQ_HZ  => 1000,
        G_STEP_FREQ_HZ  => 10
    )
    port map (
        clk     => CLK27_A12,
        rst     => not(AUD_RESET_N),
        io_out  => test_sio_toggle_reg
    );
    SIO_DATA_IN     <= test_sio_toggle_reg(3); --ok
    SIO_DATA_OUT    <= test_sio_toggle_reg(5); --ok
    SIO_CLOCK_IN    <= test_sio_toggle_reg(1); --ok
--    SIO_CLOCK_OUT   <= test_sio_toggle_reg(2);
	SIO_CLOCK_OUT <= 'Z';-- currently the ESP is writing to it!
    SIO_COMMAND     <= test_sio_toggle_reg(7); --ok
    SIO_PROCEED     <= test_sio_toggle_reg(9); --ok
    SIO_INTERRUPT   <= test_sio_toggle_reg(13);--ok
    SIO_MOTOR       <= '0' when test_sio_toggle_reg(8)='1' else 'Z'; --input ok, output FAIL!

    -- Joystick ports
    --JOY_DIR  <= (others=>'Z');
    --JOY_TRIG  <= (others=>'Z');
    --JOY2_DIR <= (others=>'Z');
    --JOY2_TRIG <= (others=>'Z');

    joy_test : entity work.io_square_test
    generic map(
        G_CLK_HZ        => 27_000_000,
        G_NUM_PINS      => 20,
        G_BASE_FREQ_HZ  => 200_000,
        G_STEP_FREQ_HZ  => 1000
    )
    port map (
        clk     => CLK27_A12,
        rst     => not(AUD_RESET_N),
        io_out  => test_joy_toggle_reg
    );


    JOY_DIR  <= open_drain(test_joy_toggle_reg(7 downto 0)); -- 1)top right, 2)bottom right - UDLR (3 downto 0)
    JOY2_DIR <= open_drain(test_joy_toggle_reg(15 downto 8));-- 3)top left,  4)bottom left
    JOY_TRIG  <= open_drain(test_joy_toggle_reg(17 downto 16)); -- same
    JOY2_TRIG <= open_drain(test_joy_toggle_reg(19 downto 18));

hello_world : ENTITY work.atari800core_simple_sdram
	GENERIC MAP
	(
		cycle_length => 32,
		internal_rom => 1,
		internal_ram =>16384
	)
	PORT MAP
	(
		CLK => CLK56,
		RESET_N => AUD_RESET_N,

		-- VIDEO OUT - PAL/NTSC, original Atari timings approx (may be higher res)
		VIDEO_VS => video_vsync,
		VIDEO_HS => video_hsync,
		VIDEO_CS => video_csync,
		VIDEO_B => video_b,
		VIDEO_G => video_g,
		VIDEO_R => video_r,
			-- These ones are probably only needed for e.g. svideo
		VIDEO_BLANK => video_blank,
		VIDEO_BURST => video_burst,
		VIDEO_START_OF_FIELD => open,
		VIDEO_ODD_LINE => open,

		-- AUDIO OUT - Pokey/GTIA 1-bit and Covox all mixed
		-- TODO - choose stereo/mono pokey
		AUDIO_L => AUDIO_L_PCM,
		AUDIO_R => AUDIO_R_PCM,

		-- JOYSTICK
		JOY1_n => (others=>'1'),
		JOY2_n => (others=>'1'),

		-- Pokey keyboard matrix
		-- Standard component available to connect this to PS2
		KEYBOARD_RESPONSE => "11",
		KEYBOARD_SCAN => open,

		-- SIO
		SIO_COMMAND => open,
		SIO_RXD => '1',
		SIO_TXD => open,
		SIO_CLOCKOUT => open,

		-- GTIA consol
		CONSOL_OPTION => '0',
		CONSOL_SELECT => '0',
		CONSOL_START => '0',

		-----------------------
		-- After here all FPGA implementation specific
		-- e.g. need to write up RAM/ROM
		-- we can dma from memory space
		-- etc.

		-- External RAM/ROM - adhere to standard memory map
		-- TODO - lower/upper memory split defined by generic
		-- (TODO SRAM lower ram, SDRAM upper ram - no overlap?)
		---- SDRAM memory map (8MB) (lower 512k if USE_SDRAM=1)
		---- base 64k RAM  - banks 0-3    "000 0000 1111 1111 1111 1111" (TOP)
		---- to 512k RAM   - banks 4-31   "000 0111 1111 1111 1111 1111" (TOP) 
		---- to 4MB RAM    - banks 32-255 "011 1111 1111 1111 1111 1111" (TOP)
		---- +64k          - banks 256-259"100 0000 0000 1111 1111 1111" (TOP)
		---- SCRATCH       - 4MB+64k-5MB
		---- CARTS         -              "101 YYYY YYY0 0000 0000 0000" (BOT) - 2MB! 8kb banks
		--SDRAM_CART_ADDR      <= "101"&cart_select& "0000000000000";
		---- BASIC/OS ROM  -              "111 XXXX XX00 0000 0000 0000" (BOT) (BASIC IN SLOT 0!), 2nd to last 512K				
		--SDRAM_BASIC_ROM_ADDR <= "111"&"000000"   &"00000000000000";
		--SDRAM_OS_ROM_ADDR    <= "111"&rom_select &"00000000000000";
		---- SYSTEM        -              "111 1000 0000 0000 0000 0000" (BOT) - LAST 512K
		-- TODO - review if we need to pass out so many of these
		-- Perhaps we can simplify address decoder and have an external layer?
		SDRAM_REQUEST => open,
		SDRAM_REQUEST_COMPLETE => '1',
		SDRAM_READ_ENABLE => open,
		SDRAM_WRITE_ENABLE => open,
		SDRAM_ADDR => open,
		SDRAM_DO => (others=>'0'),
		SDRAM_DI => open,
		SDRAM_32BIT_WRITE_ENABLE => open,
		SDRAM_16BIT_WRITE_ENABLE => open,
		SDRAM_8BIT_WRITE_ENABLE => open,
		SDRAM_REFRESH => open,

		-- DMA memory map differs
		-- e.g. some special addresses to read behind hardware registers
		-- 0x0000-0xffff: Atari registers + 3 mirrors (bit 16/17)
		-- 23 downto 21:
		-- 001 : SRAM,512k
		-- 010|011 : ROM, 4MB
		-- 10xx : SDRAM, 8MB (If you have more, its unmapped for now... Can bank switch! Atari can't access this much anyway...)
		DMA_FETCH => '0',
		DMA_READ_ENABLE => '0',
		DMA_32BIT_WRITE_ENABLE => '0',
		DMA_16BIT_WRITE_ENABLE => '0',
		DMA_8BIT_WRITE_ENABLE => '0',
		DMA_ADDR => (others=>'0'),
		DMA_WRITE_DATA => (others=>'0'),
		MEMORY_READY_DMA => open,
		DMA_MEMORY_DATA => open,

		-- Special config params
   		RAM_SELECT => (others=>'0'), -- 64K,128K,320KB Compy, 320KB Rambo, 576K Compy, 576K Rambo, 1088K, 4MB
		PAL => '1',
		HALT => '0',
		TURBO_VBLANK_ONLY => '0',
		THROTTLE_COUNT_6502 => std_logic_vector(to_unsigned(32-1,6)),
		emulated_cartridge_select => (others=>'0')
	);
	AUDIO_L_PCM_SIGNED <= signed(not(AUDIO_L_PCM(15))&AUDIO_L_PCM(14 downto 0));
	AUDIO_R_PCM_SIGNED <= signed(not(AUDIO_R_PCM(15))&AUDIO_R_PCM(14 downto 0));

end vhdl;
