library ieee;
use ieee.std_logic_1164.all;

entity fsmc_adaptor is
  generic (
    platform : integer := 1
  );
  port (
    CLK                     : in    std_logic;
    RESET_N                 : in    std_logic;

    -- Atari core DMA/memory interface
    STM_ADDR_FETCH          : out   std_logic_vector(23 downto 0);
    STM_DATA_OUT            : out   std_logic_vector(31 downto 0);
    STM_FETCH               : out   std_logic;
    STM_16BIT_WRITE_ENABLE  : out   std_logic;
    STM_8BIT_WRITE_ENABLE   : out   std_logic;
    STM_READ_ENABLE         : out   std_logic;
    STM_MEMORY_READY        : in    std_logic;
    STM_MEMORY_DATA         : in    std_logic_vector(31 downto 0);
    STM_ADDR_ROM            : out   std_logic_vector(15 downto 0);
    STM_ROM_DATA            : in    std_logic_vector(31 downto 0);

    -- POKEY/SIO interface
    STM_POKEY_ENABLE        : in    std_logic;
    STM_SIO_TXD             : out   std_logic;
    STM_SIO_RXD             : in    std_logic;
    STM_SIO_COMMAND         : in    std_logic;
    STM_SIO_CLK             : in    std_logic;

    -- STM32 FSMC bus
    FSMC_A                  : in    std_logic_vector(22 downto 0);
    FSMC_D                  : inout std_logic_vector(15 downto 0);
    FSMC_NBL                : in    std_logic_vector(1 downto 0);
    FSMC_NE                 : in    std_logic_vector(1 downto 1);
    FSMC_NOE                : in    std_logic;
    FSMC_NWE                : in    std_logic;
    FSMC_NWAIT              : out   std_logic;
    FSMC_IRQ                : out   std_logic;

    -- Configuration registers written by the STM32
    CONTROL                 : out   std_logic_vector(15 downto 0);
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
    CONSOLE_PHYS            : in    std_logic_vector(3 downto 0);

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
    POT_IN                  : out   std_logic_vector(7 downto 0);
    POT_RESET               : in    std_logic;

    -- Freezer/debug control
    FREEZE_ADDR             : out   std_logic_vector(15 downto 0);
    FREEZE_DATA_CTRL        : out   std_logic_vector(15 downto 0);
    DEBUG0                  : in    std_logic_vector(31 downto 0);
    DEBUG1                  : in    std_logic_vector(31 downto 0);
    DEBUG2                  : in    std_logic_vector(31 downto 0);
    DEBUG3                  : in    std_logic_vector(31 downto 0)
  );
end entity fsmc_adaptor;
