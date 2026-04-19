library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity text_rom_3x40 is
    port (
        clk      : in  std_logic;
        line_sel : in  unsigned(1 downto 0);  -- 0..2
        col      : in  unsigned(5 downto 0);  -- 0..39
        q        : out unsigned(7 downto 0)   -- Atari character code
    );
end entity;

architecture rtl of text_rom_3x40 is

    type rom_t is array (0 to 119) of unsigned(7 downto 0);

    constant rom : rom_t := (
        -- " MULTI MODE TEST PATTERN                "
        0   => x"00", 1   => x"2D", 2   => x"35", 3   => x"2C", 4   => x"34",
        5   => x"29", 6   => x"00", 7   => x"2D", 8   => x"2F", 9   => x"24",
        10  => x"25", 11  => x"00", 12  => x"34", 13  => x"25", 14  => x"33",
        15  => x"34", 16  => x"00", 17  => x"30", 18  => x"21", 19  => x"34",
        20  => x"34", 21  => x"25", 22  => x"32", 23  => x"2E", 24  => x"00",
        25  => x"00", 26  => x"00", 27  => x"00", 28  => x"00", 29  => x"00",
        30  => x"00", 31  => x"00", 32  => x"00", 33  => x"00", 34  => x"00",
        35  => x"00", 36  => x"00", 37  => x"00", 38  => x"00", 39  => x"00",

        -- " 40 COL ATARI XL STYLE TEXT             "
        40  => x"00", 41  => x"14", 42  => x"10", 43  => x"00", 44  => x"23",
        45  => x"2F", 46  => x"2C", 47  => x"00", 48  => x"21", 49  => x"34",
        50  => x"21", 51  => x"32", 52  => x"29", 53  => x"00", 54  => x"38",
        55  => x"2C", 56  => x"00", 57  => x"33", 58  => x"34", 59  => x"39",
        60  => x"2C", 61  => x"25", 62  => x"00", 63  => x"34", 64  => x"25",
        65  => x"38", 66  => x"34", 67  => x"00", 68  => x"00", 69  => x"00",
        70  => x"00", 71  => x"00", 72  => x"00", 73  => x"00", 74  => x"00",
        75  => x"00", 76  => x"00", 77  => x"00", 78  => x"00", 79  => x"00",

        -- " GREY RGB RAINBOW GRID                  "
        80  => x"00", 81  => x"27", 82  => x"32", 83  => x"25", 84  => x"39",
        85  => x"00", 86  => x"32", 87  => x"27", 88  => x"22", 89  => x"00",
        90  => x"32", 91  => x"21", 92  => x"29", 93  => x"2E", 94  => x"22",
        95  => x"2F", 96  => x"37", 97  => x"00", 98  => x"27", 99  => x"32",
        100 => x"29", 101 => x"24", 102 => x"00", 103 => x"00", 104 => x"00",
        105 => x"00", 106 => x"00", 107 => x"00", 108 => x"00", 109 => x"00",
        110 => x"00", 111 => x"00", 112 => x"00", 113 => x"00", 114 => x"00",
        115 => x"00", 116 => x"00", 117 => x"00", 118 => x"00", 119 => x"00"
    );

    signal q_i : unsigned(7 downto 0) := (others => '0');

    attribute romstyle : string;
    attribute romstyle of rom : constant is "M9K";

begin
    process(clk)
        variable addr_v : integer range 0 to 119;
    begin
        if rising_edge(clk) then
            addr_v := to_integer(line_sel) * 40 + to_integer(col);
            q_i <= rom(addr_v);
        end if;
    end process;

    q <= q_i;

end architecture;