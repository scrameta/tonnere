library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Re-packed text ROM.
--
-- 3 rows of 40 visible characters, stored at a power-of-two stride of
-- 64 entries per row.  This means the read address is just the
-- concatenation
--     addr = line_sel(1:0) & col(5:0)
-- with no multiply or adder anywhere on the address path.  Columns
-- 40..63 of every row hold blanks (0x00).
--
-- One-cycle read latency (q is registered), same as before.

entity text_rom_3x40 is
    port (
        clk      : in  std_logic;
        line_sel : in  unsigned(1 downto 0);  -- 0..2
        col      : in  unsigned(5 downto 0);  -- 0..63 (only 0..39 carry text)
        q        : out unsigned(7 downto 0)   -- Atari character code
    );
end entity;

architecture rtl of text_rom_3x40 is

    constant ROM_DEPTH : integer := 192;
    type rom_t is array (0 to ROM_DEPTH - 1) of unsigned(7 downto 0);

    function build_rom return rom_t is
        type row_t is array (0 to 39) of unsigned(7 downto 0);

        -- " MULTI MODE TEST PATTERN                "
        constant ROW0 : row_t := (
            x"00", x"2D", x"35", x"2C", x"34", x"29", x"00", x"2D",
            x"2F", x"24", x"25", x"00", x"34", x"25", x"33", x"34",
            x"00", x"30", x"21", x"34", x"34", x"25", x"32", x"2E",
            x"00", x"00", x"00", x"00", x"00", x"00", x"00", x"00",
            x"00", x"00", x"00", x"00", x"00", x"00", x"00", x"00"
        );

        -- " 40 COL ATARI XL STYLE TEXT             "
        constant ROW1 : row_t := (
            x"00", x"14", x"10", x"00", x"23", x"2F", x"2C", x"00",
            x"21", x"34", x"21", x"32", x"29", x"00", x"38", x"2C",
            x"00", x"33", x"34", x"39", x"2C", x"25", x"00", x"34",
            x"25", x"38", x"34", x"00", x"00", x"00", x"00", x"00",
            x"00", x"00", x"00", x"00", x"00", x"00", x"00", x"00"
        );

        -- " STRIPES RAMP CHECKER GRID              "
        constant ROW2 : row_t := (
            x"00", x"33", x"34", x"32", x"29", x"30", x"25", x"33",
            x"00", x"32", x"21", x"2D", x"30", x"00", x"23", x"28",
            x"25", x"23", x"2B", x"25", x"32", x"00", x"27", x"32",
            x"29", x"24", x"00", x"00", x"00", x"00", x"00", x"00",
            x"00", x"00", x"00", x"00", x"00", x"00", x"00", x"00"
        );

        variable r : rom_t := (others => x"00");
    begin
        for i in 0 to 39 loop
            r(0 * 64 + i) := ROW0(i);
            r(1 * 64 + i) := ROW1(i);
            r(2 * 64 + i) := ROW2(i);
        end loop;
        return r;
    end function;

    constant rom : rom_t := build_rom;

    signal q_reg : unsigned(7 downto 0) := (others => '0');

    attribute romstyle : string;
    attribute romstyle of rom : constant is "M9K";

begin

    process(clk)
        variable addr_v : unsigned(7 downto 0);
    begin
        if rising_edge(clk) then
            addr_v := line_sel & col;
            q_reg  <= rom(to_integer(addr_v));
        end if;
    end process;

    q <= q_reg;

end architecture;
