library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_misc.all;   -- and_reduce

entity fsmc_adaptor is
  generic (
    version : integer := 1
  );
  port (
    CLK                     : in    std_logic;
    CLK_FAST                : in    std_logic;
    RESET_N                 : in    std_logic;

    -- Atari core DMA/memory interface
    DMA_ADDR_FETCH          : out   std_logic_vector(23 downto 0);
    DMA_DATA_OUT            : out   std_logic_vector(15 downto 0);
    DMA_FETCH               : out   std_logic;
    DMA_16BIT_WIDTH         : out   std_logic;
    DMA_8BIT_WIDTH          : out   std_logic;
    DMA_READ_ENABLE         : out   std_logic;
    DMA_MEMORY_READY        : in    std_logic;
    DMA_MEMORY_DATA         : in    std_logic_vector(15 downto 0);

    -- POKEY/SIO interface
    SIO_POKEY_ENABLE        : in    std_logic;
    SIO_TXD             : out   std_logic;
    SIO_RXD             : in    std_logic;
    SIO_COMMAND         : in    std_logic;
    SIO_CLK             : in    std_logic;

    -- STM32 FSMC bus
    FSMC_A                  : in    std_logic_vector(22 downto 0);
    FSMC_D_IN               : in    std_logic_vector(15 downto 0);
    FSMC_D_OE               : out   std_logic;
    FSMC_D_OUT              : out   std_logic_vector(15 downto 0);
    FSMC_NBL                : in    std_logic_vector(1 downto 0);
    FSMC_NE                 : in    std_logic_vector(1 downto 1);
    FSMC_NOE                : in    std_logic;
    FSMC_NWE                : in    std_logic;
    FSMC_NWAIT              : out   std_logic;
    FSMC_IRQ                : out   std_logic;

    -- Configuration registers written by the STM32
    CONTROL                 : out   std_logic_vector(3 downto 0);
    RAMCONFIG               : out   std_logic_vector(2 downto 0);
    PERFORMANCE             : out   std_logic_vector(8 downto 0);
    CART                    : out   std_logic_vector(5 downto 0);
    VIDEO                   : out   std_logic_vector(6 downto 0);

    -- Keyboard and console input injection
    KEYBOARD_MATRIX         : out   std_logic_vector(63 downto 0);
    KEYBOARD_SHIFT          : out   std_logic;
    KEYBOARD_CONTROL        : out   std_logic;
    KEYBOARD_BREAK          : out   std_logic;
    CONSOLE_INJECT          : out   std_logic_vector(2 downto 0);
    CONSOLE_PHYS            : in    std_logic_vector(2 downto 0);

    -- Joystick input injection and physical state
    JOY0_INJECT             : out   std_logic_vector(4 downto 0);
    JOY1_INJECT             : out   std_logic_vector(4 downto 0);
    JOY2_INJECT             : out   std_logic_vector(4 downto 0);
    JOY3_INJECT             : out   std_logic_vector(4 downto 0);
    JOY0_PHYS               : in    std_logic_vector(4 downto 0);
    JOY1_PHYS               : in    std_logic_vector(4 downto 0);
    JOY2_PHYS               : in    std_logic_vector(4 downto 0);
    JOY3_PHYS               : in    std_logic_vector(4 downto 0);

    -- Paddle interface
    POT_TRIGGER             : out   std_logic_vector(7 downto 0);
    POT_RESET               : in    std_logic;

    -- Audio ADC
    AUDIO_ADC0              : out   signed(11 downto 0);
    AUDIO_ADC1              : out   signed(11 downto 0);
    AUDIO_ADC2              : out   signed(11 downto 0);
    AUDIO_ADC3              : out   signed(11 downto 0);

    -- Freezer/debug control
    FREEZE_ADDR             : out   std_logic_vector(15 downto 0);
    FREEZE_DATA_CTRL        : out   std_logic_vector(15 downto 0);
    DEBUG0                  : in    std_logic_vector(15 downto 0);
    DEBUG1                  : in    std_logic_vector(15 downto 0);
    DEBUG2                  : in    std_logic_vector(15 downto 0);
    DEBUG3                  : in    std_logic_vector(15 downto 0)
  );
end entity fsmc_adaptor;

architecture vhdl of fsmc_adaptor is

  -- CDC synchroniser outputs (FSMC strobes into CLK_FAST domain)
  signal FSMC_NE_REG        : std_logic;
  signal FSMC_NOE_REG       : std_logic;
  signal FSMC_NWE_REG       : std_logic;

  -- DELAYED FOR WRITES
  signal FSMC_D_REG, FSMC_D_NEXT : std_logic_vector(15 downto 0);
  signal FSMC_A_REG, FSMC_A_NEXT : std_logic_vector(22 downto 0);

  -- Capture state machine (CLK_FAST domain)
  type capture_state_t is (
    CAPTURE_STATE_DESELECTED,
    CAPTURE_STATE_SELECTED,
    CAPTURE_STATE_READ_CAPTURE,
    CAPTURE_STATE_READ_WAIT,
    CAPTURE_STATE_READ_COMPLETE,
    CAPTURE_STATE_WRITE,
    CAPTURE_STATE_WRITE_COMPLETE
  );
  signal CAPTURE_STATE_REG  : capture_state_t;
  signal CAPTURE_STATE_NEXT : capture_state_t;

  -- Capture FIFO write side (CLK_FAST)
  signal CAPTURE_FIFO_REQ   : std_logic;
  signal CAPTURE_FIFO_FULL  : std_logic;

  -- Read-ready handshake (CLK domain producer -> CLK_FAST consumer)
  signal READ_READY_SLOW_REG  : std_logic;   -- toggled in CLK domain when a read completes
  signal READ_READY_SLOW_NEXT : std_logic;
  signal READ_READY_REG       : std_logic;   -- synchronised into CLK_FAST
  signal READ_READY_LAST_REG  : std_logic;   -- previous value, for edge detect

  -- Read data returned to the FSMC bus
  signal READ_DATA_REG      : std_logic_vector(15 downto 0);
  signal READ_DATA_NEXT     : std_logic_vector(15 downto 0);

  -- Action FIFO read side (CLK domain)
  signal ACTION_FIFO_D      : std_logic_vector(15 downto 0);
  signal ACTION_FIFO_A      : std_logic_vector(22 downto 0);
  signal ACTION_FIFO_NWE    : std_logic;
  signal ACTION_FIFO_NBL    : std_logic_vector(1 downto 0);
  signal ACTION_FIFO_EMPTY  : std_logic;
  signal ACTION_FIFO_ACK    : std_logic;

  signal ACTION_READ_ENABLE  : std_logic;
  signal ACTION_WRITE_ENABLE : std_logic;
  signal ACTION_16BIT        : std_logic;
  signal ACTION_8BIT         : std_logic;
  signal ACTION_ADDR_L       : std_logic;

  -- Address conversion
  signal FPGA_ADDRESS         : std_logic_vector(29 downto 0);
  signal LEGACY_FPGA_ADDRESS  : std_logic_vector(29 downto 0);

  -- Device selects
  signal DMA_SELECT  : std_logic;
  signal REG_SELECT  : std_logic;
  signal SIO_SELECT  : std_logic;
  signal NO_SELECT   : std_logic;
  signal MEMORY_READY : std_logic;

  -- Register file (CLK domain), _REG/_NEXT pairs
  signal CONTROL_REG,          CONTROL_NEXT          : std_logic_vector(3 downto 0);
  signal RAMCONFIG_REG,        RAMCONFIG_NEXT        : std_logic_vector(2 downto 0);
  signal PERFORMANCE_REG,      PERFORMANCE_NEXT      : std_logic_vector(8 downto 0);
  signal CART_REG,             CART_NEXT             : std_logic_vector(5 downto 0);
  signal VIDEO_REG,            VIDEO_NEXT            : std_logic_vector(6 downto 0);

  signal KEYBOARD_MATRIX_REG,  KEYBOARD_MATRIX_NEXT  : std_logic_vector(63 downto 0);
  signal KEYBOARD_SHIFT_REG,   KEYBOARD_SHIFT_NEXT   : std_logic;
  signal KEYBOARD_CONTROL_REG, KEYBOARD_CONTROL_NEXT : std_logic;
  signal KEYBOARD_BREAK_REG,   KEYBOARD_BREAK_NEXT   : std_logic;
  signal CONSOLE_INJECT_REG,   CONSOLE_INJECT_NEXT   : std_logic_vector(2 downto 0);

  signal JOY0_INJECT_REG,      JOY0_INJECT_NEXT      : std_logic_vector(4 downto 0);
  signal JOY1_INJECT_REG,      JOY1_INJECT_NEXT      : std_logic_vector(4 downto 0);
  signal JOY2_INJECT_REG,      JOY2_INJECT_NEXT      : std_logic_vector(4 downto 0);
  signal JOY3_INJECT_REG,      JOY3_INJECT_NEXT      : std_logic_vector(4 downto 0);

  signal POT_TRIGGER_REG,      POT_TRIGGER_NEXT      : std_logic_vector(7 downto 0);
  signal POT_RESET_REG,        POT_RESET_NEXT        : std_logic;

  signal FREEZE_ADDR_REG,      FREEZE_ADDR_NEXT      : std_logic_vector(15 downto 0);
  signal FREEZE_DATA_CTRL_REG, FREEZE_DATA_CTRL_NEXT : std_logic_vector(15 downto 0);

  signal APERTURE1_EXT_REG,    APERTURE1_EXT_NEXT    : std_logic_vector(7 downto 0);
  signal APERTURE2_EXT_REG,    APERTURE2_EXT_NEXT    : std_logic_vector(7 downto 0);

  -- Register decode + read data
  signal REG_ADDR_DECODED    : std_logic_vector(63 downto 0);
  signal REG_DATA            : std_logic_vector(15 downto 0);

  -- SIO handler
  signal SIO_HANDLER_DATA    : std_logic_vector(15 downto 0);
  signal SIO_FIFO_RX_EMPTY   : std_logic;
  signal SIO_FIFO_TX_EMPTY   : std_logic;

  -- Interrupt controller (CLK domain)
  signal IRQ_ENABLE_REG,  IRQ_ENABLE_NEXT  : std_logic_vector(4 downto 0);
  signal IRQ_PENDING_REG, IRQ_PENDING_NEXT : std_logic_vector(4 downto 0);
  signal IRQ_EDGE_REG,    IRQ_EDGE_NEXT    : std_logic_vector(4 downto 0);
  signal IRQ_CLEAR        : std_logic_vector(4 downto 0);

  -- DMA (delayed for timing)
  signal DMA_ADDR_FETCH_REG         , DMA_ADDR_FETCH_NEXT         : std_logic_vector(23 downto 0);
  signal DMA_DATA_OUT_REG           , DMA_DATA_OUT_NEXT           : std_logic_vector(15 downto 0);
  signal DMA_FETCH_REG              , DMA_FETCH_NEXT              : std_logic;
  signal DMA_16BIT_WIDTH_REG        , DMA_16BIT_WIDTH_NEXT        : std_logic;
  signal DMA_8BIT_WIDTH_REG         , DMA_8BIT_WIDTH_NEXT         : std_logic;
  signal DMA_READ_ENABLE_REG        , DMA_READ_ENABLE_NEXT        : std_logic;
  
  -- AUDIO ADC
  signal AUDIO_ADC0_REG, AUDIO_ADC0_NEXT : signed(11 downto 0);
  signal AUDIO_ADC1_REG, AUDIO_ADC1_NEXT : signed(11 downto 0);
  signal AUDIO_ADC2_REG, AUDIO_ADC2_NEXT : signed(11 downto 0);
  signal AUDIO_ADC3_REG, AUDIO_ADC3_NEXT : signed(11 downto 0);  

begin
  -- Strategy
  -- All requests to a fifo -> write async, read flushes and gets response via another fifo

  process (CLK, RESET_N)
  begin
    if (RESET_N = '0') then
      KEYBOARD_MATRIX_REG  <= (others=>'0');
      KEYBOARD_SHIFT_REG   <= '0';
      KEYBOARD_CONTROL_REG <= '0';
      KEYBOARD_BREAK_REG   <= '0';
      CONSOLE_INJECT_REG   <= (others=>'1');
      JOY0_INJECT_REG      <= (others=>'1');
      JOY1_INJECT_REG      <= (others=>'1');
      JOY2_INJECT_REG      <= (others=>'1');
      JOY3_INJECT_REG      <= (others=>'1');
      POT_TRIGGER_REG      <= (others=>'0');
      FREEZE_ADDR_REG      <= (others=>'0');
      FREEZE_DATA_CTRL_REG <= (others=>'0');
      APERTURE1_EXT_REG    <= (others=>'0');
      APERTURE2_EXT_REG    <= (others=>'0');
      READ_READY_SLOW_REG  <= '0';
      IRQ_ENABLE_REG       <= (others=>'0');
      IRQ_PENDING_REG      <= (others=>'0');
      IRQ_EDGE_REG         <= (others=>'0');
      POT_RESET_REG        <= '0';
      CONTROL_REG          <= "0010"; -- 1 = pause Atari
      RAMCONFIG_REG        <= (others=>'0');
      PERFORMANCE_REG      <= (others=>'0');
      CART_REG             <= (others=>'0');
      VIDEO_REG            <= (others=>'0');
      READ_DATA_REG        <= (others=>'0');
      DMA_ADDR_FETCH_REG   <= (others=>'0');
      DMA_DATA_OUT_REG     <= (others=>'0');
      DMA_FETCH_REG        <= '0';
      DMA_16BIT_WIDTH_REG  <= '0';
      DMA_8BIT_WIDTH_REG   <= '0';
      DMA_READ_ENABLE_REG  <= '0';
      AUDIO_ADC0_REG       <= to_signed(0,12);
      AUDIO_ADC1_REG       <= to_signed(0,12);
      AUDIO_ADC2_REG       <= to_signed(0,12);
      AUDIO_ADC3_REG       <= to_signed(0,12);
    elsif (CLK'EVENT and CLK = '1') then
      KEYBOARD_MATRIX_REG  <= KEYBOARD_MATRIX_NEXT; 
      KEYBOARD_SHIFT_REG   <= KEYBOARD_SHIFT_NEXT;  
      KEYBOARD_CONTROL_REG <= KEYBOARD_CONTROL_NEXT;
      KEYBOARD_BREAK_REG   <= KEYBOARD_BREAK_NEXT;  
      CONSOLE_INJECT_REG   <= CONSOLE_INJECT_NEXT;  
      JOY0_INJECT_REG      <= JOY0_INJECT_NEXT;     
      JOY1_INJECT_REG      <= JOY1_INJECT_NEXT;     
      JOY2_INJECT_REG      <= JOY2_INJECT_NEXT;     
      JOY3_INJECT_REG      <= JOY3_INJECT_NEXT;     
      POT_TRIGGER_REG      <= POT_TRIGGER_NEXT;          
      FREEZE_ADDR_REG      <= FREEZE_ADDR_NEXT;     
      FREEZE_DATA_CTRL_REG <= FREEZE_DATA_CTRL_NEXT;
      APERTURE1_EXT_REG    <= APERTURE1_EXT_NEXT;
      APERTURE2_EXT_REG    <= APERTURE2_EXT_NEXT;
      READ_READY_SLOW_REG  <= READ_READY_SLOW_NEXT;
      IRQ_ENABLE_REG       <= IRQ_ENABLE_NEXT;
      IRQ_PENDING_REG      <= IRQ_PENDING_NEXT;
      IRQ_EDGE_REG         <= IRQ_EDGE_NEXT;
      POT_RESET_REG        <= POT_RESET_NEXT;
      CONTROL_REG          <= CONTROL_NEXT;
      RAMCONFIG_REG        <= RAMCONFIG_NEXT;
      PERFORMANCE_REG      <= PERFORMANCE_NEXT;
      CART_REG             <= CART_NEXT;
      VIDEO_REG            <= VIDEO_NEXT;
      READ_DATA_REG        <= READ_DATA_NEXT;
      DMA_ADDR_FETCH_REG   <= DMA_ADDR_FETCH_NEXT;
      DMA_DATA_OUT_REG     <= DMA_DATA_OUT_NEXT;
      DMA_FETCH_REG        <= DMA_FETCH_NEXT;
      DMA_16BIT_WIDTH_REG  <= DMA_16BIT_WIDTH_NEXT;
      DMA_8BIT_WIDTH_REG   <= DMA_8BIT_WIDTH_NEXT;
      DMA_READ_ENABLE_REG  <= DMA_READ_ENABLE_NEXT;
      AUDIO_ADC0_REG       <= AUDIO_ADC0_NEXT;
      AUDIO_ADC1_REG       <= AUDIO_ADC1_NEXT;
      AUDIO_ADC2_REG       <= AUDIO_ADC2_NEXT;
      AUDIO_ADC3_REG       <= AUDIO_ADC3_NEXT;
    end if;
  end process;

  synchronizer_ne : entity work.synchronizer
    port map (clk=>CLK_FAST, raw=>FSMC_NE(1), sync=>FSMC_NE_REG);
  synchronizer_noe : entity work.synchronizer
    port map (clk=>CLK_FAST, raw=>FSMC_NOE, sync=>FSMC_NOE_REG);
  synchronizer_nwe : entity work.synchronizer
    port map (clk=>CLK_FAST, raw=>FSMC_NWE, sync=>FSMC_NWE_REG);
  synchronizer_read_ready : entity work.synchronizer
    port map (clk=>CLK_FAST, raw=>READ_READY_SLOW_REG, sync=>READ_READY_REG);

  process (CLK_FAST, RESET_N)
  begin
    if (RESET_N = '0') then
      CAPTURE_STATE_REG <= CAPTURE_STATE_DESELECTED;
      READ_READY_LAST_REG <= '0';
    elsif (CLK_FAST'EVENT and CLK_FAST = '1') then
      CAPTURE_STATE_REG <= CAPTURE_STATE_NEXT;
      READ_READY_LAST_REG <= READ_READY_REG;
    end if;
  end process;

  -- NE low -> CS
  -- NOE drops -> read, please provide data
  -- NWE drops -> write, latch data on NWE low->high transition
  FSMC_D_OUT <= READ_DATA_REG;
  process (CAPTURE_STATE_REG,
    FSMC_NE_REG,
    FSMC_NOE_REG,
    FSMC_NWE_REG,
    CAPTURE_FIFO_FULL,
    READ_READY_REG, READ_READY_LAST_REG, READ_DATA_REG
  )
  begin
    CAPTURE_STATE_NEXT <= CAPTURE_STATE_REG;

    FSMC_NWAIT <= '0';
    CAPTURE_FIFO_REQ <= '0';

    FSMC_D_OE <= '0';

    case CAPTURE_STATE_REG is
      when CAPTURE_STATE_DESELECTED =>
        if FSMC_NE_REG = '0' then
          CAPTURE_STATE_NEXT <= CAPTURE_STATE_SELECTED;
        end if;
      when CAPTURE_STATE_SELECTED =>
        if FSMC_NOE_REG = '0' then
          CAPTURE_STATE_NEXT <= CAPTURE_STATE_READ_CAPTURE;
        elsif FSMC_NWE_REG = '0' then
          CAPTURE_STATE_NEXT <= CAPTURE_STATE_WRITE;
        elsif FSMC_NE_REG = '1' then
          CAPTURE_STATE_NEXT <= CAPTURE_STATE_DESELECTED;
        end if;
      when CAPTURE_STATE_READ_CAPTURE =>
          if CAPTURE_FIFO_FULL='0' then
              CAPTURE_FIFO_REQ <= '1';
              CAPTURE_STATE_NEXT <= CAPTURE_STATE_READ_WAIT;
          end if;
      when CAPTURE_STATE_READ_WAIT =>
          -- wait for the read-ready toggle to cross into this domain (edge)
          if READ_READY_REG /= READ_READY_LAST_REG then
              FSMC_D_OE <= '1';
              CAPTURE_STATE_NEXT <= CAPTURE_STATE_READ_COMPLETE;
          end if;
      when CAPTURE_STATE_READ_COMPLETE =>
          FSMC_NWAIT <= '1';
          FSMC_D_OE <= '1';
          if FSMC_NOE_REG = '1' then
              CAPTURE_STATE_NEXT <= CAPTURE_STATE_SELECTED;
          end if;
      when CAPTURE_STATE_WRITE =>
          FSMC_NWAIT <= not(CAPTURE_FIFO_FULL);
          if FSMC_NWE_REG='0' and CAPTURE_FIFO_FULL='0' then -- Cannot be raised when NWAIT is active. Address/data are valid on falling edge.
              CAPTURE_FIFO_REQ <= '1';
              CAPTURE_STATE_NEXT <= CAPTURE_STATE_WRITE_COMPLETE;
          end if;
      when CAPTURE_STATE_WRITE_COMPLETE =>
          FSMC_NWAIT <= '1';
          if FSMC_NWE_REG='1' then -- End of write
              CAPTURE_STATE_NEXT <= CAPTURE_STATE_SELECTED;
          end if;
      when others =>
          CAPTURE_STATE_NEXT <= CAPTURE_STATE_DESELECTED;
    end case;
  end process;

  fsmc_fifo1 : ENTITY work.fsmc_fifo
  PORT MAP
  (
    data(15 downto 0)  => FSMC_D_IN,
    data(38 downto 16) => FSMC_A,
    data(39)           => FSMC_NWE,
    data(41 downto 40) => FSMC_NBL,
    wrclk   => CLK_FAST, 
    wrreq   => CAPTURE_FIFO_REQ,
    wrfull  => CAPTURE_FIFO_FULL,

    rdclk   => CLK,
    rdreq   => ACTION_FIFO_ACK,
    q(15 downto 0)  => ACTION_FIFO_D,
    q(38 downto 16) => ACTION_FIFO_A,
    q(39)           => ACTION_FIFO_NWE,
    q(41 downto 40) => ACTION_FIFO_NBL,
    rdempty => ACTION_FIFO_EMPTY
  );

  process( 
    ACTION_FIFO_D, ACTION_FIFO_A, ACTION_FIFO_NWE, ACTION_FIFO_NBL, ACTION_FIFO_EMPTY,
    MEMORY_READY
  )
  begin
    ACTION_READ_ENABLE <= '0';
    ACTION_WRITE_ENABLE <= '0';
    ACTION_FIFO_ACK <= '0';
    ACTION_16BIT <= ACTION_FIFO_NBL(1) nor ACTION_FIFO_NBL(0);
    ACTION_8BIT <= ACTION_FIFO_NBL(1) xor ACTION_FIFO_NBL(0);
    ACTION_ADDR_L <= ACTION_FIFO_NBL(0) and not(ACTION_FIFO_NBL(1));

    if ACTION_FIFO_EMPTY='0' then
      if ACTION_FIFO_NWE = '0' then -- write
        ACTION_WRITE_ENABLE <= '1';
      else -- read
        ACTION_READ_ENABLE <= '1';
      end if;

      if MEMORY_READY = '1' then
        ACTION_FIFO_ACK <= '1';
      end if;
    end if;
  end process;

  -- Convert from the STM address to the FPGA address
  process(ACTION_FIFO_A,APERTURE1_EXT_REG, APERTURE2_EXT_REG)
  begin
    FPGA_ADDRESS(21 downto 0)  <= ACTION_FIFO_A(21 downto 0);

    if (ACTION_FIFO_A(22)='0') then
      FPGA_ADDRESS(29 downto 22) <= APERTURE1_EXT_REG;
    else
      FPGA_ADDRESS(29 downto 22) <= APERTURE2_EXT_REG;
      if (ACTION_FIFO_A(21 downto 19) = "111") then -- 1/8 of aperture 2 -> fixed block 64 in FPGA space
        FPGA_ADDRESS(29 downto 24) <= (others=>'1'); 
        FPGA_ADDRESS(23 downto 19) <= (others=>'0');
      end if;
    end if;
  end process;

  -- Convert from the FPGA adress to our legacy FPGA address and device selects
  process(FPGA_ADDRESS)
  begin
    DMA_SELECT <= '1';
    REG_SELECT <= '0';
    SIO_SELECT <= '0';
    NO_SELECT  <= '0';

    LEGACY_FPGA_ADDRESS <= FPGA_ADDRESS;

    -- Address maps in word space

    -- FPGA address map (new):
    -- Split into 64 BLOCKS (6 bits)
    -- Last block is then split into 8 blocks (7 bits)
    -- 0x00000000-0x07FFFFFF 256MB of SDRAM (8 blocks)
    -- 0x08000000-0x08FFFFFF 32MB SRAM1 (1 block)
    -- 0x09000000-0x09FFFFFF 32MB SRAM2 (1 block)
    -- 0x0A000000-0x0AFFFFFF 32MB "ROM" (1 block)
    -- Fixed window:
    --   0x3F000000-0x3F00FFFF Atari window (shared block for next 3, further split 8 ways)
    --   0x3F060000-0x3F06FFFF SIO handler
    --   0x3F070000-0x3F07FFFF Regs

    -- Legacy FPGA address map (old/temporary):
    -- 0x000000 Atari
    -- 0x100000 2MB of SRAM -> SRAM1
    -- 0x200000 2MB of ROM -> redirected to block ram
    -- 0x300000 2MB of ROM -> redirected to SRAM2? TODO
    -- 0x400000 8MB of SDRAM

    case FPGA_ADDRESS(29 downto 24) is
    when 
      "000000"|"000001"| "000010"|"000011" |
      "000100"|"000101"| "000110"|"000111"
      =>
      LEGACY_FPGA_ADDRESS(22) <= '1';
    when 
      "001000"
      =>
      LEGACY_FPGA_ADDRESS(22 downto 20) <= "001";
    when 
      "001001"
      =>
      LEGACY_FPGA_ADDRESS(22 downto 19) <= "0110";
    when 
      "001010"
      =>
      LEGACY_FPGA_ADDRESS(22 downto 19) <= "0111";
    when "111111" =>
      case FPGA_ADDRESS(18 downto 16) is
      when "000" =>
        -- Atari window
      when "110" =>
        DMA_SELECT <= '0';
        SIO_SELECT <= '1';
      when "111" =>
        DMA_SELECT <= '0';
        REG_SELECT <= '1';
      when others =>
        DMA_SELECT <= '0';
        NO_SELECT <= '1';
      end case;
    when others =>
      DMA_SELECT <= '0';
      NO_SELECT <= '1';
    end case;
  end process;

  DMA_ADDR_FETCH_NEXT(0) <= ACTION_ADDR_L;
  DMA_ADDR_FETCH_NEXT(23 downto 1) <= LEGACY_FPGA_ADDRESS(22 downto 0);
  DMA_DATA_OUT_NEXT <= ACTION_FIFO_D;
  DMA_FETCH_NEXT <= DMA_SELECT and (ACTION_READ_ENABLE or ACTION_WRITE_ENABLE) and not MEMORY_READY;
  DMA_16BIT_WIDTH_NEXT <= ACTION_16BIT;
  DMA_8BIT_WIDTH_NEXT <= ACTION_8BIT;
  DMA_READ_ENABLE_NEXT <= ACTION_READ_ENABLE;

  MEMORY_READY <= (DMA_MEMORY_READY and DMA_SELECT) or SIO_SELECT or REG_SELECT or NO_SELECT;

  process(MEMORY_READY, READ_READY_SLOW_REG, ACTION_READ_ENABLE)
  begin
    READ_READY_SLOW_NEXT <= READ_READY_SLOW_REG;
    if (ACTION_READ_ENABLE='1' and MEMORY_READY='1') then
      READ_READY_SLOW_NEXT <= not(READ_READY_SLOW_REG);
    end if;
  end process;

  process(
    DMA_SELECT, DMA_MEMORY_DATA, DMA_MEMORY_READY,
    SIO_SELECT, SIO_HANDLER_DATA,
    REG_SELECT, REG_DATA,
    READ_DATA_REG
  )
  begin
    READ_DATA_NEXT <= READ_DATA_REG;
    if (DMA_SELECT='1' and DMA_MEMORY_READY='1') then
      READ_DATA_NEXT <= DMA_MEMORY_DATA;
    elsif (SIO_SELECT='1') then
      READ_DATA_NEXT <= SIO_HANDLER_DATA;
    elsif (REG_SELECT='1') then
      READ_DATA_NEXT <= REG_DATA;
    end if;
  end process;

  sio_handler1 : ENTITY work.sio_handler
  PORT MAP
  ( 
    CLK => CLK,
    RESET_N => RESET_N,
    
    -- clock for pokey
    POKEY_ENABLE => SIO_POKEY_ENABLE,
    
    -- ATARI interface (in future we can also turbo load by directly hitting memory...)
    SIO_DATA_IN  => SIO_TXD, 
    SIO_COMMAND  => SIO_COMMAND, -- From Atari core, same clock
    SIO_DATA_OUT => SIO_RXD,     -- From Atari core, same clock
    SIO_CLK_OUT  => SIO_CLK,

    -- FIFO empty flags, for IRQ on tonnere
    FIFO_RX_EMPTY => SIO_FIFO_RX_EMPTY,
    FIFO_TX_EMPTY => SIO_FIFO_TX_EMPTY,
    
    -- CPU interface
    ADDR => FPGA_ADDRESS(4 downto 0),
    CPU_DATA_IN => ACTION_FIFO_D(7 downto 0),
    EN => ACTION_READ_ENABLE and SIO_SELECT,
    WR_EN => ACTION_WRITE_ENABLE and SIO_SELECT,
    DATA_OUT => SIO_HANDLER_DATA
  );

-- decode address
decode_addr : entity work.complete_address_decoder
	generic map(width=>6)
	port map (addr_in=>FPGA_ADDRESS(5 downto 0), addr_decoded=>REG_ADDR_DECODED);

  -- Register reads
  process(REG_ADDR_DECODED,
    CONSOLE_PHYS,
    JOY0_PHYS, JOY1_PHYS,
    JOY2_PHYS, JOY3_PHYS,
    IRQ_ENABLE_REG, IRQ_PENDING_REG,
	 DEBUG0, DEBUG1, DEBUG2, DEBUG3
  )
  begin
    REG_DATA <= (others=>'0');
    if (REG_ADDR_DECODED(0)='1') then  -- IFACE_MAGIC
      REG_DATA <= x"584c";
    end if;
    if (REG_ADDR_DECODED(1)='1') then  -- IFACE_VERSION
      REG_DATA <= std_logic_vector(to_unsigned(version,16));
    end if;
    if (REG_ADDR_DECODED(13)='1') then -- CONSOLE_PHYS
      REG_DATA(2 downto 0) <= CONSOLE_PHYS;
    end if;
    if (REG_ADDR_DECODED(16)='1') then -- JOY01_PHYS
      REG_DATA(4 downto 0)  <= JOY0_PHYS;
      REG_DATA(12 downto 8) <= JOY1_PHYS;
    end if;
    if (REG_ADDR_DECODED(17)='1') then -- JOY23_PHYS
      REG_DATA(4 downto 0)  <= JOY2_PHYS;
      REG_DATA(12 downto 8) <= JOY3_PHYS;
    end if;
    if (REG_ADDR_DECODED(22)='1') then -- IRQ_ENABLE
      REG_DATA(4 downto 0) <= IRQ_ENABLE_REG;
    end if;
    if (REG_ADDR_DECODED(23)='1') then -- IRQ_PENDING
      REG_DATA(4 downto 0) <= IRQ_PENDING_REG;
    end if;
    if (REG_ADDR_DECODED(25)='1') then -- DEBUG0
      REG_DATA <= DEBUG0;
    end if;
    if (REG_ADDR_DECODED(26)='1') then -- DEBUG1
      REG_DATA <= DEBUG1;
    end if;
    if (REG_ADDR_DECODED(27)='1') then -- DEBUG2
      REG_DATA <= DEBUG2;
    end if;
    if (REG_ADDR_DECODED(28)='1') then -- DEBUG3
      REG_DATA <= DEBUG3;
    end if;
  end process;

  -- Register writes
  process(REG_ADDR_DECODED,REG_SELECT,ACTION_FIFO_D,ACTION_WRITE_ENABLE,
    KEYBOARD_MATRIX_REG,
    KEYBOARD_SHIFT_REG,
    KEYBOARD_CONTROL_REG,
    KEYBOARD_BREAK_REG,
    CONSOLE_INJECT_REG,
    JOY0_INJECT_REG,
    JOY1_INJECT_REG,
    JOY2_INJECT_REG,
    JOY3_INJECT_REG,
    POT_TRIGGER_REG, POT_RESET,
    FREEZE_ADDR_REG,
    FREEZE_DATA_CTRL_REG,
    APERTURE1_EXT_REG,
    APERTURE2_EXT_REG,
    IRQ_ENABLE_REG,
    IRQ_PENDING_REG,
	 CONTROL_REG,
	 RAMCONFIG_REG,
	 PERFORMANCE_REG,
	 CART_REG,
	 VIDEO_REG,
	 POT_RESET_REG,
    AUDIO_ADC0_REG,
    AUDIO_ADC1_REG,
    AUDIO_ADC2_REG,
    AUDIO_ADC3_REG
  )
    variable PADDLE_OVER_THRESHOLD : std_logic;
  begin
    KEYBOARD_MATRIX_NEXT  <= KEYBOARD_MATRIX_REG;
    KEYBOARD_SHIFT_NEXT   <= KEYBOARD_SHIFT_REG;
    KEYBOARD_CONTROL_NEXT <= KEYBOARD_CONTROL_REG;
    KEYBOARD_BREAK_NEXT   <= KEYBOARD_BREAK_REG;
    CONSOLE_INJECT_NEXT   <= CONSOLE_INJECT_REG;
    JOY0_INJECT_NEXT      <= JOY0_INJECT_REG;
    JOY1_INJECT_NEXT      <= JOY1_INJECT_REG;
    JOY2_INJECT_NEXT      <= JOY2_INJECT_REG;
    JOY3_INJECT_NEXT      <= JOY3_INJECT_REG;
    POT_TRIGGER_NEXT      <= POT_TRIGGER_REG;
    FREEZE_ADDR_NEXT      <= FREEZE_ADDR_REG;
    FREEZE_DATA_CTRL_NEXT <= FREEZE_DATA_CTRL_REG;
    APERTURE1_EXT_NEXT    <= APERTURE1_EXT_REG;
    APERTURE2_EXT_NEXT    <= APERTURE2_EXT_REG;
    IRQ_ENABLE_NEXT       <= IRQ_ENABLE_REG;
    CONTROL_NEXT          <= CONTROL_REG;
    RAMCONFIG_NEXT        <= RAMCONFIG_REG;
    PERFORMANCE_NEXT      <= PERFORMANCE_REG;
    CART_NEXT             <= CART_REG;
    VIDEO_NEXT            <= VIDEO_REG;
    POT_RESET_NEXT        <= POT_RESET_REG;
    AUDIO_ADC0_NEXT       <= AUDIO_ADC0_REG;
    AUDIO_ADC1_NEXT       <= AUDIO_ADC1_REG;
    AUDIO_ADC2_NEXT       <= AUDIO_ADC2_REG;
    AUDIO_ADC3_NEXT       <= AUDIO_ADC3_REG;
    IRQ_CLEAR <= (others=>'1');

    if (POT_RESET='1') then
      POT_TRIGGER_NEXT <= (others=>'0'); -- Immediate, do not wait for STM
    end if;

    PADDLE_OVER_THRESHOLD := '0';
    if SIGNED(ACTION_FIFO_D(11 downto 0))>to_signed(4000,12) then -- TODO tune threshold!
        PADDLE_OVER_THRESHOLD := '1';
    end if;

    if (REG_SELECT = '1' and ACTION_WRITE_ENABLE = '1') then
      if (REG_ADDR_DECODED(2)='1') then  -- CONTROL
        CONTROL_NEXT <= ACTION_FIFO_D(3 downto 0);
      end if;
      if (REG_ADDR_DECODED(3)='1') then  -- RAMCONFIG
        RAMCONFIG_NEXT <= ACTION_FIFO_D(2 downto 0);
      end if;
      if (REG_ADDR_DECODED(4)='1') then  -- PERFORMANCE
        PERFORMANCE_NEXT <= ACTION_FIFO_D(8 downto 0);
      end if;
      if (REG_ADDR_DECODED(5)='1') then  -- CART
        CART_NEXT <= ACTION_FIFO_D(5 downto 0);
      end if;
      if (REG_ADDR_DECODED(6)='1') then  -- VIDEO
        VIDEO_NEXT <= ACTION_FIFO_D(6 downto 0);
      end if;
      if (REG_ADDR_DECODED(7)='1') then  -- KBD1
        KEYBOARD_MATRIX_NEXT(15 downto 0) <= ACTION_FIFO_D;
      end if;
      if (REG_ADDR_DECODED(8)='1') then  -- KBD2
        KEYBOARD_MATRIX_NEXT(31 downto 16) <= ACTION_FIFO_D;
      end if;
      if (REG_ADDR_DECODED(9)='1') then  -- KBD3
        KEYBOARD_MATRIX_NEXT(47 downto 32) <= ACTION_FIFO_D;
      end if;
      if (REG_ADDR_DECODED(10)='1') then  -- KBD4
        KEYBOARD_MATRIX_NEXT(63 downto 48) <= ACTION_FIFO_D;
      end if;
      if (REG_ADDR_DECODED(11)='1') then  -- KBD_SPECIAL
        KEYBOARD_SHIFT_NEXT   <= ACTION_FIFO_D(0);
        KEYBOARD_CONTROL_NEXT <= ACTION_FIFO_D(1);
        KEYBOARD_BREAK_NEXT   <= ACTION_FIFO_D(2);
      end if;
      if (REG_ADDR_DECODED(12)='1') then  -- CONSOLE_INJECT
        CONSOLE_INJECT_NEXT <= ACTION_FIFO_D(2 downto 0);
      end if;
      if (REG_ADDR_DECODED(14)='1') then  -- JOY01_INJECT
        JOY0_INJECT_NEXT <= ACTION_FIFO_D(4 downto 0);
        JOY1_INJECT_NEXT <= ACTION_FIFO_D(12 downto 8);
      end if;
      if (REG_ADDR_DECODED(15)='1') then  -- JOY23_INJECT
        JOY2_INJECT_NEXT <= ACTION_FIFO_D(4 downto 0);
        JOY3_INJECT_NEXT <= ACTION_FIFO_D(12 downto 8);
      end if;
      if (REG_ADDR_DECODED(18)='1') then  -- PADDLE_TRIGGER
        POT_TRIGGER_NEXT <= POT_TRIGGER_REG or ACTION_FIFO_D(7 downto 0);
      end if;
      if (REG_ADDR_DECODED(20)='1') then  -- FREEZE_ADDR
        FREEZE_ADDR_NEXT <= ACTION_FIFO_D;
      end if;
      if (REG_ADDR_DECODED(21)='1') then  -- FREEZE_DATA_CTRL
        FREEZE_DATA_CTRL_NEXT <= ACTION_FIFO_D;
      end if;
      if (REG_ADDR_DECODED(22)='1') then  -- IRQ_ENABLE
        IRQ_ENABLE_NEXT <= ACTION_FIFO_D(4 downto 0);
      end if;
      if (REG_ADDR_DECODED(24)='1') then  -- IRQ_CLEAR
        IRQ_CLEAR <= ACTION_FIFO_D(4 downto 0);
      end if;
      -- DEBUG writes are just for picking up on the logic analyzer, not stored
      if (REG_ADDR_DECODED(29)='1') then  -- APERTURE1_EXT
        APERTURE1_EXT_NEXT <= ACTION_FIFO_D(7 downto 0);
      end if;
      if (REG_ADDR_DECODED(30)='1') then  -- APERTURE1_EXT
        APERTURE2_EXT_NEXT <= ACTION_FIFO_D(7 downto 0);
      end if;
      if (REG_ADDR_DECODED(31)='1') then  -- PADDLE_ADC
        POT_TRIGGER_NEXT(0) <= POT_TRIGGER_REG(0) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(32)='1') then
        POT_TRIGGER_NEXT(1) <= POT_TRIGGER_REG(1) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(33)='1') then
        POT_TRIGGER_NEXT(2) <= POT_TRIGGER_REG(2) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(34)='1') then
        POT_TRIGGER_NEXT(3) <= POT_TRIGGER_REG(3) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(35)='1') then
        POT_TRIGGER_NEXT(4) <= POT_TRIGGER_REG(4) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(36)='1') then
        POT_TRIGGER_NEXT(5) <= POT_TRIGGER_REG(5) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(37)='1') then
        POT_TRIGGER_NEXT(6) <= POT_TRIGGER_REG(6) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(38)='1') then
        POT_TRIGGER_NEXT(7) <= POT_TRIGGER_REG(7) or PADDLE_OVER_THRESHOLD;
      end if;
      if (REG_ADDR_DECODED(39)='1') then  -- AUDIO_ADC
        AUDIO_ADC0_NEXT <= SIGNED(ACTION_FIFO_D(11 downto 0));
      end if;
      if (REG_ADDR_DECODED(40)='1') then  -- AUDIO_ADC
        AUDIO_ADC1_NEXT <= SIGNED(ACTION_FIFO_D(11 downto 0));
      end if;
      if (REG_ADDR_DECODED(41)='1') then  -- AUDIO_ADC
        AUDIO_ADC2_NEXT <= SIGNED(ACTION_FIFO_D(11 downto 0));
      end if;
      if (REG_ADDR_DECODED(42)='1') then  -- AUDIO_ADC
        AUDIO_ADC3_NEXT <= SIGNED(ACTION_FIFO_D(11 downto 0));
      end if;
    end if;
  end process;

  -- Interrupt controller
  IRQ_EDGE_NEXT(0) <= SIO_COMMAND;
  IRQ_EDGE_NEXT(1) <= not(SIO_FIFO_RX_EMPTY);
  IRQ_EDGE_NEXT(2) <= SIO_FIFO_TX_EMPTY;
  IRQ_EDGE_NEXT(3) <= POT_RESET;
  IRQ_EDGE_NEXT(4) <= NOT(POT_RESET);

  IRQ_PENDING_NEXT <= (IRQ_PENDING_REG or (IRQ_EDGE_NEXT and not(IRQ_EDGE_REG))) and IRQ_CLEAR;
  FSMC_IRQ <= or_reduce(IRQ_PENDING_REG and IRQ_ENABLE_REG);

  -- Outputs
  CONTROL          <= CONTROL_REG;
  RAMCONFIG        <= RAMCONFIG_REG;
  PERFORMANCE      <= PERFORMANCE_REG;
  CART             <= CART_REG;
  VIDEO            <= VIDEO_REG;
                                      
  KEYBOARD_MATRIX  <= KEYBOARD_MATRIX_REG;
  KEYBOARD_SHIFT   <= KEYBOARD_SHIFT_REG;
  KEYBOARD_CONTROL <= KEYBOARD_CONTROL_REG;
  KEYBOARD_BREAK   <= KEYBOARD_BREAK_REG;
  CONSOLE_INJECT   <= CONSOLE_INJECT_REG; 
                                      
  JOY0_INJECT      <= JOY0_INJECT_REG;
  JOY1_INJECT      <= JOY1_INJECT_REG;
  JOY2_INJECT      <= JOY2_INJECT_REG;
  JOY3_INJECT      <= JOY3_INJECT_REG;
                                      
  POT_TRIGGER      <= POT_TRIGGER_REG;
                                      
  FREEZE_ADDR      <= FREEZE_ADDR_REG;
  FREEZE_DATA_CTRL <= FREEZE_DATA_CTRL_REG;

  DMA_ADDR_FETCH   <= DMA_ADDR_FETCH_REG;
  DMA_DATA_OUT     <= DMA_DATA_OUT_REG;
  DMA_FETCH        <= DMA_FETCH_REG;
  DMA_16BIT_WIDTH  <= DMA_16BIT_WIDTH_REG;
  DMA_8BIT_WIDTH   <= DMA_8BIT_WIDTH_REG;
  DMA_READ_ENABLE  <= DMA_READ_ENABLE_REG;

  AUDIO_ADC0       <= AUDIO_ADC0_REG;
  AUDIO_ADC1       <= AUDIO_ADC1_REG;
  AUDIO_ADC2       <= AUDIO_ADC2_REG;
  AUDIO_ADC3       <= AUDIO_ADC3_REG;

end vhdl;
