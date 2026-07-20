library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Master reset generator for Tonnere
-- --------------------------------------------------------------------------
-- Holds a single system reset asserted until EVERY clock source is stable:
--   * the Si5351 references have settled (implicitly: the FPGA PLLs that are
--     fed from them cannot report 'locked' until their input clock is clean)
--   * all downstream FPGA PLLs (atari, aud, vdac, hdmi) report 'locked'
--   * that combined-locked condition has held continuously for
--     STABLE_CYCLES clocks of clk (so brief lock bounces during Si5351
--     start-up do not release reset early)
--
-- Reset is asserted asynchronously (so a lost clock forces reset even with no
-- running edge) and deasserted synchronously to clk.
--
-- clk MUST be a clock that only runs once its PLL is locked (e.g. CLK56 from
-- pll_atari). That way the stability counter can never advance on a dead clock.
--
-- Output is active-low: reset_n = '1' means "released / run".

entity reset_gen is
  generic (
    STABLE_CYCLES : natural := 65536   -- clk cycles all-locked must persist
  );
  port (
    clk            : in  std_logic;    -- e.g. CLK56 (present once pll_atari locked)

    -- Raw PLL lock inputs (active-high, 1 = locked)
    pll_atari_lock : in  std_logic;
    pll_aud_lock   : in  std_logic;
    pll_vdac_lock  : in  std_logic;
    pll_hdmi_lock  : in  std_logic;

    -- Master reset, active-low: '1' = released
    reset_n        : out std_logic
  );
end entity;

architecture rtl of reset_gen is

  function clog2(n : natural) return natural is
    variable i : natural := 0;
    variable v : natural := 1;
  begin
    while v < n loop
      v := v * 2;
      i := i + 1;
    end loop;
    return i;
  end function;

  constant CNT_W : natural := clog2(STABLE_CYCLES);

  -- Synchronise the (asynchronous, other-domain) lock flags into clk with 2 FFs.
  signal lock_meta   : std_logic := '0';
  signal lock_sync   : std_logic := '0';

  signal all_locked  : std_logic;

  signal stable_cnt  : unsigned(CNT_W-1 downto 0) := (others => '0');
  signal stable_ok   : std_logic := '0';

  -- Async-assert / sync-deassert release shift register
  signal rel_sync    : std_logic_vector(1 downto 0) := (others => '0');

begin

  -- All PLLs locked (raw combinational). Each lock is in a foreign domain, so
  -- we treat the AND as asynchronous and synchronise it below.
  all_locked <= pll_atari_lock and pll_aud_lock and pll_vdac_lock and pll_hdmi_lock;

  -- 2-FF synchroniser for the combined lock into clk
  process(clk)
  begin
    if rising_edge(clk) then
      lock_meta <= all_locked;
      lock_sync <= lock_meta;
    end if;
  end process;

  -- Stability filter: require lock_sync high continuously for STABLE_CYCLES.
  -- Any drop resets the counter and clears stable_ok.
  process(clk)
  begin
    if rising_edge(clk) then
      if lock_sync = '0' then
        stable_cnt <= (others => '0');
        stable_ok  <= '0';
      elsif stable_ok = '0' then
        if stable_cnt = to_unsigned(STABLE_CYCLES-1, CNT_W) then
          stable_ok <= '1';
        else
          stable_cnt <= stable_cnt + 1;
        end if;
      end if;
    end if;
  end process;

  -- Async assert, sync deassert. When stable_ok drops, reset asserts
  -- immediately (async path); when it is high, ones shift in over 2 clocks.
  process(clk, stable_ok)
  begin
    if stable_ok = '0' then
      rel_sync <= "00";                 -- async assert reset
    elsif rising_edge(clk) then
      rel_sync <= rel_sync(0) & '1';    -- sync deassert over 2 cycles
    end if;
  end process;

  reset_n <= rel_sync(1);

end architecture;
