library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity vga_576p50_test is
    port (
        clk27  : in  std_logic;
        reset  : in  std_logic;

        hsync  : out std_logic;
        vsync  : out std_logic;
        r      : out std_logic_vector(7 downto 0);
        g      : out std_logic_vector(7 downto 0);
        b      : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of vga_576p50_test is

    --------------------------------------------------------------------
    -- 576p50 timing @ 27 MHz
    --------------------------------------------------------------------
    constant H_ACTIVE : integer := 720;
    constant H_FP     : integer := 12;
    constant H_SYNC   : integer := 64;
    constant H_BP     : integer := 68;
    constant H_TOTAL  : integer := H_ACTIVE + H_FP + H_SYNC + H_BP;  -- 864

    constant V_ACTIVE : integer := 576;
    constant V_FP     : integer := 5;
    constant V_SYNC   : integer := 5;
    constant V_BP     : integer := 39;
    constant V_TOTAL  : integer := V_ACTIVE + V_FP + V_SYNC + V_BP;  -- 625

    --------------------------------------------------------------------
    -- 40 column text layout
    --------------------------------------------------------------------
    constant TXT_COLS      : integer := 40;
    constant TXT_ROWS      : integer := 24;
    constant CELL_W        : integer := 18;  -- 720 / 40
    constant CELL_H        : integer := 24;  -- 576 / 24

    constant GLYPH_W       : integer := 16;  -- 8 pixels * 2
    constant GLYPH_H       : integer := 16;  -- 8 pixels * 2
    constant GLYPH_X_OFF   : integer := 1;   -- centre in 18-wide cell
    constant GLYPH_Y_OFF   : integer := 4;   -- centre in 24-high cell

    signal h_count : integer range 0 to H_TOTAL - 1 := 0;
    signal v_count : integer range 0 to V_TOTAL - 1 := 0;

    signal active_video : std_logic;
    signal r_i, g_i, b_i : std_logic_vector(7 downto 0) := (others => '0');

    subtype text_line_t is string(1 to 40);

    constant TEXT_LINE_0 : text_line_t := "  ATARI XL STYLE 40 COLUMN TEXT DEMO    ";
    constant TEXT_LINE_1 : text_line_t := "   576P50   27MHZ   RGB GREY HSV TEST   ";
    constant TEXT_LINE_2 : text_line_t := "    CHATGPT GENERATED VGA PATTERN       ";

    --------------------------------------------------------------------
    -- Return one 8-bit row of an 8x8 font for a limited character set
    -- bit 7 = leftmost pixel, bit 0 = rightmost pixel
    --------------------------------------------------------------------
    function glyph_row(ch : character; row : integer) return std_logic_vector is
        variable bits : std_logic_vector(7 downto 0) := (others => '0');
    begin
        case ch is
            when 'A' =>
                case row is
                    when 0 => bits := x"18";
                    when 1 => bits := x"24";
                    when 2 => bits := x"42";
                    when 3 => bits := x"7E";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"42";
                    when others => bits := x"00";
                end case;
            when 'B' =>
                case row is
                    when 0 => bits := x"7C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"7C";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"7C";
                    when others => bits := x"00";
                end case;
            when 'C' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"40";
                    when 3 => bits := x"40";
                    when 4 => bits := x"40";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when 'D' =>
                case row is
                    when 0 => bits := x"78";
                    when 1 => bits := x"44";
                    when 2 => bits := x"42";
                    when 3 => bits := x"42";
                    when 4 => bits := x"42";
                    when 5 => bits := x"44";
                    when 6 => bits := x"78";
                    when others => bits := x"00";
                end case;
            when 'E' =>
                case row is
                    when 0 => bits := x"7E";
                    when 1 => bits := x"40";
                    when 2 => bits := x"40";
                    when 3 => bits := x"7C";
                    when 4 => bits := x"40";
                    when 5 => bits := x"40";
                    when 6 => bits := x"7E";
                    when others => bits := x"00";
                end case;
            when 'G' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"40";
                    when 3 => bits := x"4E";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when 'H' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"7E";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"42";
                    when others => bits := x"00";
                end case;
            when 'I' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"18";
                    when 2 => bits := x"18";
                    when 3 => bits := x"18";
                    when 4 => bits := x"18";
                    when 5 => bits := x"18";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when 'L' =>
                case row is
                    when 0 => bits := x"40";
                    when 1 => bits := x"40";
                    when 2 => bits := x"40";
                    when 3 => bits := x"40";
                    when 4 => bits := x"40";
                    when 5 => bits := x"40";
                    when 6 => bits := x"7E";
                    when others => bits := x"00";
                end case;
            when 'M' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"66";
                    when 2 => bits := x"5A";
                    when 3 => bits := x"42";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"42";
                    when others => bits := x"00";
                end case;
            when 'N' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"62";
                    when 2 => bits := x"52";
                    when 3 => bits := x"4A";
                    when 4 => bits := x"46";
                    when 5 => bits := x"42";
                    when 6 => bits := x"42";
                    when others => bits := x"00";
                end case;
            when 'O' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"42";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when 'P' =>
                case row is
                    when 0 => bits := x"7C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"7C";
                    when 4 => bits := x"40";
                    when 5 => bits := x"40";
                    when 6 => bits := x"40";
                    when others => bits := x"00";
                end case;
            when 'R' =>
                case row is
                    when 0 => bits := x"7C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"7C";
                    when 4 => bits := x"48";
                    when 5 => bits := x"44";
                    when 6 => bits := x"42";
                    when others => bits := x"00";
                end case;
            when 'S' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"40";
                    when 3 => bits := x"3C";
                    when 4 => bits := x"02";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when 'T' =>
                case row is
                    when 0 => bits := x"7E";
                    when 1 => bits := x"18";
                    when 2 => bits := x"18";
                    when 3 => bits := x"18";
                    when 4 => bits := x"18";
                    when 5 => bits := x"18";
                    when 6 => bits := x"18";
                    when others => bits := x"00";
                end case;
            when 'U' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"42";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when 'V' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"42";
                    when 2 => bits := x"42";
                    when 3 => bits := x"42";
                    when 4 => bits := x"24";
                    when 5 => bits := x"24";
                    when 6 => bits := x"18";
                    when others => bits := x"00";
                end case;
            when 'X' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"24";
                    when 2 => bits := x"18";
                    when 3 => bits := x"18";
                    when 4 => bits := x"18";
                    when 5 => bits := x"24";
                    when 6 => bits := x"42";
                    when others => bits := x"00";
                end case;
            when 'Y' =>
                case row is
                    when 0 => bits := x"42";
                    when 1 => bits := x"24";
                    when 2 => bits := x"18";
                    when 3 => bits := x"18";
                    when 4 => bits := x"18";
                    when 5 => bits := x"18";
                    when 6 => bits := x"18";
                    when others => bits := x"00";
                end case;
            when '0' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"46";
                    when 3 => bits := x"4A";
                    when 4 => bits := x"52";
                    when 5 => bits := x"62";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when '2' =>
                case row is
                    when 0 => bits := x"3C";
                    when 1 => bits := x"42";
                    when 2 => bits := x"02";
                    when 3 => bits := x"0C";
                    when 4 => bits := x"30";
                    when 5 => bits := x"40";
                    when 6 => bits := x"7E";
                    when others => bits := x"00";
                end case;
            when '4' =>
                case row is
                    when 0 => bits := x"08";
                    when 1 => bits := x"18";
                    when 2 => bits := x"28";
                    when 3 => bits := x"48";
                    when 4 => bits := x"7E";
                    when 5 => bits := x"08";
                    when 6 => bits := x"08";
                    when others => bits := x"00";
                end case;
            when '5' =>
                case row is
                    when 0 => bits := x"7E";
                    when 1 => bits := x"40";
                    when 2 => bits := x"7C";
                    when 3 => bits := x"02";
                    when 4 => bits := x"02";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when '6' =>
                case row is
                    when 0 => bits := x"1C";
                    when 1 => bits := x"20";
                    when 2 => bits := x"40";
                    when 3 => bits := x"7C";
                    when 4 => bits := x"42";
                    when 5 => bits := x"42";
                    when 6 => bits := x"3C";
                    when others => bits := x"00";
                end case;
            when '7' =>
                case row is
                    when 0 => bits := x"7E";
                    when 1 => bits := x"02";
                    when 2 => bits := x"04";
                    when 3 => bits := x"08";
                    when 4 => bits := x"10";
                    when 5 => bits := x"10";
                    when 6 => bits := x"10";
                    when others => bits := x"00";
                end case;
            when others =>
                bits := x"00";
        end case;

        return bits;
    end function;

begin

    --------------------------------------------------------------------
    -- Timing generator
    --------------------------------------------------------------------
    process(clk27)
    begin
        if rising_edge(clk27) then
            if reset = '1' then
                h_count <= 0;
                v_count <= 0;
            else
                if h_count = H_TOTAL - 1 then
                    h_count <= 0;
                    if v_count = V_TOTAL - 1 then
                        v_count <= 0;
                    else
                        v_count <= v_count + 1;
                    end if;
                else
                    h_count <= h_count + 1;
                end if;
            end if;
        end if;
    end process;

    active_video <= '1' when (h_count < H_ACTIVE and v_count < V_ACTIVE) else '0';

    --------------------------------------------------------------------
    -- Active-high syncs
    --------------------------------------------------------------------
    hsync <= '1' when (h_count >= H_ACTIVE + H_FP and
                       h_count <  H_ACTIVE + H_FP + H_SYNC) else '0';

    vsync <= '1' when (v_count >= V_ACTIVE + V_FP and
                       v_count <  V_ACTIVE + V_FP + V_SYNC) else '0';

    --------------------------------------------------------------------
    -- Video generator + text overlay
    --------------------------------------------------------------------
    process(h_count, v_count, active_video)
        variable x, y         : integer;
        variable section      : integer;
        variable bar          : integer;

        variable rv, gv, bv   : unsigned(7 downto 0);
        variable ramp_y       : unsigned(7 downto 0);
        variable local_x_8    : unsigned(7 downto 0);
        variable gray_step    : integer;
        variable gray_val     : unsigned(7 downto 0);

        variable border       : boolean;
        variable grid         : boolean;

        -- HSV helpers
        variable hue          : integer;
        variable seg          : integer;
        variable frac         : integer;
        variable p            : integer;
        variable q            : integer;
        variable t            : integer;
        variable r16, g16, b16 : unsigned(15 downto 0);

        -- text helpers
        variable col          : integer;
        variable row          : integer;
        variable px           : integer;
        variable py           : integer;
        variable gx           : integer;
        variable gy           : integer;
        variable line_ch      : character;
        variable glyph_bits   : std_logic_vector(7 downto 0);
        variable text_on      : boolean;
    begin
        rv := (others => '0');
        gv := (others => '0');
        bv := (others => '0');
        text_on := false;

        if active_video = '1' then
            x := h_count;
            y := v_count;

            border := (x = 0) or (x = H_ACTIVE - 1) or
                      (y = 0) or (y = V_ACTIVE - 1);

            grid := ((x mod 32) = 0) or ((y mod 32) = 0);
            ramp_y := to_unsigned((y * 255) / (V_ACTIVE - 1), 8);

            if border then
                rv := x"FF";
                gv := x"FF";
                bv := x"FF";

            elsif y < (V_ACTIVE / 2) then
                ----------------------------------------------------------------
                -- TOP HALF: colour bars
                ----------------------------------------------------------------
                bar := x / (H_ACTIVE / 8);

                case bar is
                    when 0 => rv := x"FF"; gv := x"FF"; bv := x"FF";
                    when 1 => rv := x"FF"; gv := x"FF"; bv := x"00";
                    when 2 => rv := x"00"; gv := x"FF"; bv := x"FF";
                    when 3 => rv := x"00"; gv := x"FF"; bv := x"00";
                    when 4 => rv := x"FF"; gv := x"00"; bv := x"FF";
                    when 5 => rv := x"FF"; gv := x"00"; bv := x"00";
                    when 6 => rv := x"00"; gv := x"00"; bv := x"FF";
                    when others => rv := x"00"; gv := x"00"; bv := x"00";
                end case;

            else
                ----------------------------------------------------------------
                -- BOTTOM HALF split into 4 sections
                ----------------------------------------------------------------
                section := x / 180;

                case section is
                    when 0 =>
                        -- 256-step greyscale
                        gray_step := (x * 256) / 180;
                        if gray_step > 255 then
                            gray_step := 255;
                        end if;
                        gray_val := to_unsigned(gray_step, 8);
                        rv := gray_val;
                        gv := gray_val;
                        bv := gray_val;

                    when 1 =>
                        -- RGB ramps
                        local_x_8 := to_unsigned(((x - 180) * 255) / 179, 8);

                        if y < (V_ACTIVE/2 + V_ACTIVE/6) then
                            rv := local_x_8;
                            gv := (others => '0');
                            bv := (others => '0');
                        elsif y < (V_ACTIVE/2 + 2*V_ACTIVE/6) then
                            rv := (others => '0');
                            gv := local_x_8;
                            bv := (others => '0');
                        else
                            rv := (others => '0');
                            gv := (others => '0');
                            bv := local_x_8;
                        end if;

                    when 2 =>
                        -- HSV-style rainbow
                        hue  := ((x - 360) * 255) / 179;
                        seg  := (hue * 6) / 256;
                        frac := (hue * 6) mod 256;

                        p := 0;
                        q := 255 - frac;
                        t := frac;

                        case seg is
                            when 0 =>
                                rv := to_unsigned(255, 8);
                                gv := to_unsigned(t,   8);
                                bv := to_unsigned(p,   8);
                            when 1 =>
                                rv := to_unsigned(q,   8);
                                gv := to_unsigned(255, 8);
                                bv := to_unsigned(p,   8);
                            when 2 =>
                                rv := to_unsigned(p,   8);
                                gv := to_unsigned(255, 8);
                                bv := to_unsigned(t,   8);
                            when 3 =>
                                rv := to_unsigned(p,   8);
                                gv := to_unsigned(q,   8);
                                bv := to_unsigned(255, 8);
                            when 4 =>
                                rv := to_unsigned(t,   8);
                                gv := to_unsigned(p,   8);
                                bv := to_unsigned(255, 8);
                            when others =>
                                rv := to_unsigned(255, 8);
                                gv := to_unsigned(p,   8);
                                bv := to_unsigned(q,   8);
                        end case;

                        r16 := rv * ramp_y;
                        g16 := gv * ramp_y;
                        b16 := bv * ramp_y;
                        rv := r16(15 downto 8);
                        gv := g16(15 downto 8);
                        bv := b16(15 downto 8);

                    when others =>
                        -- combined gradient
                        rv := to_unsigned(((x - 540) * 255) / 179, 8);
                        gv := ramp_y;
                        bv := not rv;
                end case;
            end if;

            if grid then
                rv := rv or x"10";
                gv := gv or x"10";
                bv := bv or x"10";
            end if;

            ----------------------------------------------------------------
            -- Atari XL style chunky text overlay
            -- Put three lines near the lower third
            ----------------------------------------------------------------
            col := x / CELL_W;
            row := y / CELL_H;

            if (col >= 0 and col < TXT_COLS and row >= 0 and row < TXT_ROWS) then
                if row = 16 or row = 17 or row = 18 then
                    px := x mod CELL_W;
                    py := y mod CELL_H;

                    if (px >= GLYPH_X_OFF and px < GLYPH_X_OFF + GLYPH_W and
                        py >= GLYPH_Y_OFF and py < GLYPH_Y_OFF + GLYPH_H) then

                        gx := (px - GLYPH_X_OFF) / 2;  -- 0..7
                        gy := (py - GLYPH_Y_OFF) / 2;  -- 0..7

                        case row is
                            when 16 => line_ch := TEXT_LINE_0(col + 1);
                            when 17 => line_ch := TEXT_LINE_1(col + 1);
                            when others => line_ch := TEXT_LINE_2(col + 1);
                        end case;

                        glyph_bits := glyph_row(line_ch, gy);

                        if glyph_bits(7 - gx) = '1' then
                            text_on := true;
                        end if;
                    end if;
                end if;
            end if;

            if text_on then
                -- bright Atari-ish text with a little cyan tint
                rv := x"FF";
                gv := x"FF";
                bv := x"80";
            end if;

        else
            rv := (others => '0');
            gv := (others => '0');
            bv := (others => '0');
        end if;

        r_i <= std_logic_vector(rv);
        g_i <= std_logic_vector(gv);
        b_i <= std_logic_vector(bv);
    end process;

    r <= r_i;
    g <= g_i;
    b <= b_i;

end architecture;
