library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_pattern_generator is
    generic (
        ACTIVE_W : integer := 720;
        ACTIVE_H : integer := 576
    );
    port (
        clk       : in  std_logic;
        reset     : in  std_logic;

        active    : in  std_logic;
        active_x  : in  integer;
        active_y  : in  integer;
        field_odd : in  std_logic;

        r         : out std_logic_vector(7 downto 0);
        g         : out std_logic_vector(7 downto 0);
        b         : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of test_pattern_generator is

    function min_i(a : integer; b : integer) return integer is
    begin
        if a < b then
            return a;
        else
            return b;
        end if;
    end function;

    function sat_add_16(v : unsigned(7 downto 0)) return unsigned is
        variable t : unsigned(8 downto 0);
    begin
        t := ('0' & v) + to_unsigned(16, 9);
        if t > to_unsigned(255, 9) then
            return to_unsigned(255, 8);
        else
            return t(7 downto 0);
        end if;
    end function;

    function mul3_sat(v : unsigned(7 downto 0)) return unsigned is
        variable t : unsigned(9 downto 0);
    begin
        t := resize(v, 10) + shift_left(resize(v, 10), 1);
        if t > to_unsigned(255, 10) then
            return to_unsigned(255, 8);
        else
            return t(7 downto 0);
        end if;
    end function;

    constant HALF_H   : integer := ACTIVE_H / 2;

    constant Q0_START : integer := 0;
    constant Q1_START : integer := ACTIVE_W / 4;
    constant Q2_START : integer := ACTIVE_W / 2;
    constant Q3_START : integer := (3 * ACTIVE_W) / 4;

    constant Q0_W     : integer := Q1_START - Q0_START;
    constant Q1_W     : integer := Q2_START - Q1_START;
    constant Q2_W     : integer := Q3_START - Q2_START;
    constant Q3_W     : integer := ACTIVE_W - Q3_START;

    constant BAR1_END : integer := (1 * ACTIVE_W) / 8;
    constant BAR2_END : integer := (2 * ACTIVE_W) / 8;
    constant BAR3_END : integer := (3 * ACTIVE_W) / 8;
    constant BAR4_END : integer := (4 * ACTIVE_W) / 8;
    constant BAR5_END : integer := (5 * ACTIVE_W) / 8;
    constant BAR6_END : integer := (6 * ACTIVE_W) / 8;
    constant BAR7_END : integer := (7 * ACTIVE_W) / 8;

    constant CELL_W : integer := ACTIVE_W / 40;
    constant CELL_H : integer := ACTIVE_H / 24;

    function glyph_scale_fn return integer is
        variable s : integer;
    begin
        if (CELL_W / 8) < (CELL_H / 8) then
            s := CELL_W / 8;
        else
            s := CELL_H / 8;
        end if;

        if s < 1 then
            s := 1;
        end if;
        return s;
    end function;

    constant SCALE_USE : integer := glyph_scale_fn;
    constant GLYPH_W   : integer := 8 * SCALE_USE;
    constant GLYPH_H   : integer := 8 * SCALE_USE;
    constant GX_OFF    : integer := (CELL_W - GLYPH_W) / 2;
    constant GY_OFF    : integer := (CELL_H - GLYPH_H) / 2;

    -- 24-bit accumulators: upper byte used as 0..255 ramp
    constant ACC_W : integer := 24;
    constant ACC_ONE : integer := 65536;       -- 2^16
    constant ACC_255 : integer := 255 * ACC_ONE;

    function step_const(span : integer) return unsigned is
        variable v : integer;
    begin
        if span <= 1 then
            v := 0;
        else
            v := ACC_255 / (span - 1);
        end if;
        return to_unsigned(v, ACC_W);
    end function;

    constant STEP_Q0     : unsigned(ACC_W-1 downto 0) := step_const(Q0_W);
    constant STEP_Q1     : unsigned(ACC_W-1 downto 0) := step_const(Q1_W);
    constant STEP_Q2     : unsigned(ACC_W-1 downto 0) := step_const(Q2_W);
    constant STEP_Q3     : unsigned(ACC_W-1 downto 0) := step_const(Q3_W);

    constant BOT_H       : integer := ACTIVE_H - HALF_H;
    constant STEP_V_BOT  : unsigned(ACC_W-1 downto 0) := step_const(BOT_H);
    constant STEP_V_FULL : unsigned(ACC_W-1 downto 0) := step_const(ACTIVE_H);

    --------------------------------------------------------------------
    -- S0: sampled input stream and accumulators
    --------------------------------------------------------------------
    signal s0_active      : std_logic := '0';
    signal s0_x           : integer := 0;
    signal s0_y           : integer := 0;

    signal prev_active    : std_logic := '0';
    signal prev_y         : integer := 0;

    signal ramp_acc_h     : unsigned(ACC_W-1 downto 0) := (others => '0');
    signal bright_bot_acc : unsigned(ACC_W-1 downto 0) := (others => '0');
    signal bright_full_acc: unsigned(ACC_W-1 downto 0) := (others => '0');

    signal ramp_q_s0      : unsigned(7 downto 0) := (others => '0');
    signal bright_bot_s0  : unsigned(7 downto 0) := (others => '0');
    signal bright_full_s0 : unsigned(7 downto 0) := (others => '0');

    --------------------------------------------------------------------
    -- S1: base pattern + text decode, aligned with text ROM output next cycle
    --------------------------------------------------------------------
    signal s1_active      : std_logic := '0';
    signal s1_border      : std_logic := '0';
    signal s1_line        : std_logic := '0';
    signal s1_r           : unsigned(7 downto 0) := (others => '0');
    signal s1_g           : unsigned(7 downto 0) := (others => '0');
    signal s1_b           : unsigned(7 downto 0) := (others => '0');

    signal s1_text_valid  : std_logic := '0';
    signal s1_gx          : integer range 0 to 7 := 0;
    signal s1_gy          : integer range 0 to 7 := 0;
    signal s1_text_line   : integer range 0 to 2 := 0;
    signal s1_text_col    : integer range 0 to 39 := 0;

    --------------------------------------------------------------------
    -- text ROM interface
    --------------------------------------------------------------------
    signal text_line_sel  : unsigned(1 downto 0) := (others => '0');
    signal text_col       : unsigned(5 downto 0) := (others => '0');
    signal text_char_q    : unsigned(7 downto 0) := (others => '0');

    --------------------------------------------------------------------
    -- S2: align base RGB with char, drive font ROM
    --------------------------------------------------------------------
    signal s2_active      : std_logic := '0';
    signal s2_border      : std_logic := '0';
    signal s2_line        : std_logic := '0';
    signal s2_r           : unsigned(7 downto 0) := (others => '0');
    signal s2_g           : unsigned(7 downto 0) := (others => '0');
    signal s2_b           : unsigned(7 downto 0) := (others => '0');

    signal s2_text_valid  : std_logic := '0';
    signal s2_gx          : integer range 0 to 7 := 0;
    signal s2_gy          : integer range 0 to 7 := 0;
    signal s2_char        : unsigned(7 downto 0) := (others => '0');

    --------------------------------------------------------------------
    -- font ROM interface
    --------------------------------------------------------------------
    signal font_addr      : unsigned(9 downto 0) := (others => '0');
    signal font_row_q     : std_logic_vector(7 downto 0) := (others => '0');

    --------------------------------------------------------------------
    -- S3: final compose
    --------------------------------------------------------------------
    signal r_i            : std_logic_vector(7 downto 0) := (others => '0');
    signal g_i            : std_logic_vector(7 downto 0) := (others => '0');
    signal b_i            : std_logic_vector(7 downto 0) := (others => '0');

begin

    --------------------------------------------------------------------
    -- External ROMs
    --------------------------------------------------------------------
    text_line_sel <= to_unsigned(s1_text_line, 2);
    text_col      <= to_unsigned(s1_text_col, 6);

    text_rom_i : entity work.text_rom_3x40
        port map (
            clk      => clk,
            line_sel => text_line_sel,
            col      => text_col,
            q        => text_char_q
        );

    font_addr <= shift_left(resize(s2_char, 10), 3) + to_unsigned(s2_gy, 10);

    font_rom_i : entity work.font_rom_8x8
        port map (
            clk  => clk,
            addr => font_addr,
            q    => font_row_q
        );

    --------------------------------------------------------------------
    -- S0: sample inputs, update accumulators
    --------------------------------------------------------------------
    process(clk)
        variable new_line_v : boolean;
        variable enter_active_v : boolean;
    begin
        if rising_edge(clk) then
            if reset = '1' then
                s0_active       <= '0';
                s0_x            <= 0;
                s0_y            <= 0;
                prev_active     <= '0';
                prev_y          <= 0;
                ramp_acc_h      <= (others => '0');
                bright_bot_acc  <= (others => '0');
                bright_full_acc <= (others => '0');
                ramp_q_s0       <= (others => '0');
                bright_bot_s0   <= (others => '0');
                bright_full_s0  <= (others => '0');
            else
                s0_active <= active;
                s0_x      <= active_x;
                s0_y      <= active_y;

                new_line_v := false;
                enter_active_v := false;

                if (prev_active = '0') and (active = '1') then
                    enter_active_v := true;
                end if;

                if (active = '1') and ((prev_active = '0') or (active_y /= prev_y)) then
                    new_line_v := true;
                end if;

                -- Vertical accumulators: updated once per active line
                if enter_active_v then
                    bright_full_acc <= (others => '0');

                    if active_y >= HALF_H then
                        if active_y = HALF_H then
                            bright_bot_acc <= (others => '0');
                        else
                            bright_bot_acc <= bright_bot_acc + STEP_V_BOT;
                        end if;
                    else
                        bright_bot_acc <= (others => '0');
                    end if;

                elsif new_line_v then
                    bright_full_acc <= bright_full_acc + STEP_V_FULL;

                    if active_y >= HALF_H then
                        if active_y = HALF_H then
                            bright_bot_acc <= (others => '0');
                        else
                            bright_bot_acc <= bright_bot_acc + STEP_V_BOT;
                        end if;
                    else
                        bright_bot_acc <= (others => '0');
                    end if;
                end if;

                -- Horizontal accumulator: updated per pixel in bottom half only
                if active = '1' then
                    if new_line_v then
                        ramp_acc_h <= (others => '0');
                    else
                        if active_y < HALF_H then
                            ramp_acc_h <= (others => '0');
                        else
                            if active_x = Q0_START then
                                ramp_acc_h <= (others => '0');
                            elsif active_x = Q1_START then
                                ramp_acc_h <= (others => '0');
                            elsif active_x = Q2_START then
                                ramp_acc_h <= (others => '0');
                            elsif active_x = Q3_START then
                                ramp_acc_h <= (others => '0');
                            elsif active_x < Q1_START then
                                ramp_acc_h <= ramp_acc_h + STEP_Q0;
                            elsif active_x < Q2_START then
                                ramp_acc_h <= ramp_acc_h + STEP_Q1;
                            elsif active_x < Q3_START then
                                ramp_acc_h <= ramp_acc_h + STEP_Q2;
                            else
                                ramp_acc_h <= ramp_acc_h + STEP_Q3;
                            end if;
                        end if;
                    end if;
                else
                    ramp_acc_h <= (others => '0');
                end if;

                ramp_q_s0      <= ramp_acc_h(23 downto 16);
                bright_bot_s0  <= bright_bot_acc(23 downto 16);
                bright_full_s0 <= bright_full_acc(23 downto 16);

                prev_active <= active;
                prev_y      <= active_y;
            end if;
        end if;
    end process;

    --------------------------------------------------------------------
    -- S1: base pattern + text decode
    --------------------------------------------------------------------
    process(clk)
        variable x, y        : integer;
        variable border_v    : std_logic;
        variable line_v      : std_logic;
        variable rv, gv, bv  : unsigned(7 downto 0);
        variable col_v       : integer;
        variable row_v       : integer;
        variable px_v        : integer;
        variable py_v        : integer;
        variable gx_v        : integer;
        variable gy_v        : integer;
        variable text_ok_v   : std_logic;
        variable text_line_v : integer range 0 to 2;
        variable text_col_v  : integer range 0 to 39;
        variable t           : unsigned(7 downto 0);
        variable m           : unsigned(15 downto 0);
    begin
        if rising_edge(clk) then
            if reset = '1' then
                s1_active     <= '0';
                s1_border     <= '0';
                s1_line       <= '0';
                s1_r          <= (others => '0');
                s1_g          <= (others => '0');
                s1_b          <= (others => '0');
                s1_text_valid <= '0';
                s1_gx         <= 0;
                s1_gy         <= 0;
                s1_text_line  <= 0;
                s1_text_col   <= 0;
            else
                x := s0_x;
                y := s0_y;

                border_v := '0';
                line_v   := '0';
                rv := (others => '0');
                gv := (others => '0');
                bv := (others => '0');

                text_ok_v := '0';
                text_line_v := 0;
                text_col_v := 0;
                gx_v := 0;
                gy_v := 0;

                if s0_active = '1' then
                    if (x = 0) or (x = ACTIVE_W - 1) or
                       (y = 0) or (y = ACTIVE_H - 1) then
                        border_v := '1';
                    end if;

                    -- Sparse guide lines only
                    if (x = ACTIVE_W / 4) or
                       (x = ACTIVE_W / 2) or
                       (x = (3 * ACTIVE_W) / 4) or
                       (y = ACTIVE_H / 2) then
                        line_v := '1';
                    end if;

                    if y < HALF_H then
                        -- Top half: colour bars
                        if x < BAR1_END then
                            rv := x"FF"; gv := x"FF"; bv := x"FF";
                        elsif x < BAR2_END then
                            rv := x"FF"; gv := x"FF"; bv := x"00";
                        elsif x < BAR3_END then
                            rv := x"00"; gv := x"FF"; bv := x"FF";
                        elsif x < BAR4_END then
                            rv := x"00"; gv := x"FF"; bv := x"00";
                        elsif x < BAR5_END then
                            rv := x"FF"; gv := x"00"; bv := x"FF";
                        elsif x < BAR6_END then
                            rv := x"FF"; gv := x"00"; bv := x"00";
                        elsif x < BAR7_END then
                            rv := x"00"; gv := x"00"; bv := x"FF";
                        else
                            rv := x"00"; gv := x"00"; bv := x"00";
                        end if;
                    else
                        -- Bottom half from accumulators
                        if x < Q1_START then
                            rv := ramp_q_s0;
                            gv := ramp_q_s0;
                            bv := ramp_q_s0;

                        elsif x < Q2_START then
                            if y < (HALF_H + ACTIVE_H / 6) then
                                rv := ramp_q_s0;
                                gv := (others => '0');
                                bv := (others => '0');
                            elsif y < (HALF_H + 2 * ACTIVE_H / 6) then
                                rv := (others => '0');
                                gv := ramp_q_s0;
                                bv := (others => '0');
                            else
                                rv := (others => '0');
                                gv := (others => '0');
                                bv := ramp_q_s0;
                            end if;

                        elsif x < Q3_START then
                            if ramp_q_s0 < to_unsigned(85, 8) then
                                t := mul3_sat(ramp_q_s0);
                                rv := to_unsigned(255, 8) - t;
                                gv := t;
                                bv := (others => '0');
                            elsif ramp_q_s0 < to_unsigned(170, 8) then
                                t := mul3_sat(ramp_q_s0 - to_unsigned(85, 8));
                                rv := (others => '0');
                                gv := to_unsigned(255, 8) - t;
                                bv := t;
                            else
                                t := mul3_sat(ramp_q_s0 - to_unsigned(170, 8));
                                rv := t;
                                gv := (others => '0');
                                bv := to_unsigned(255, 8) - t;
                            end if;

                            m := rv * bright_bot_s0;
                            rv := m(15 downto 8);
                            m := gv * bright_bot_s0;
                            gv := m(15 downto 8);
                            m := bv * bright_bot_s0;
                            bv := m(15 downto 8);

                        else
                            if x = Q3_START then
                                rv := x"00";
                                gv := bright_full_s0;
                                bv := x"FF";
                            else
                                rv := ramp_q_s0;
                                gv := bright_full_s0;
                                bv := to_unsigned(255, 8) - ramp_q_s0;
                            end if;
                        end if;			    
                    end if;

                    -- Text decode
                    if (CELL_W > 0) and (CELL_H > 0) then
                        col_v := x / CELL_W;
                        row_v := y / CELL_H;

                        if (col_v >= 0) and (col_v < 40) and
                           ((row_v = 16) or (row_v = 17) or (row_v = 18)) then

                            px_v := x - (col_v * CELL_W);
                            py_v := y - (row_v * CELL_H);

                            if (px_v >= GX_OFF) and (px_v < GX_OFF + GLYPH_W) and
                               (py_v >= GY_OFF) and (py_v < GY_OFF + GLYPH_H) then

                                gx_v := (px_v - GX_OFF) / SCALE_USE;
                                gy_v := (py_v - GY_OFF) / SCALE_USE;

                                if (gx_v >= 0) and (gx_v < 8) and
                                   (gy_v >= 0) and (gy_v < 8) then
                                    text_ok_v := '1';
                                    text_line_v := row_v - 16;
                                    text_col_v := col_v;
                                end if;
                            end if;
                        end if;
                    end if;
                end if;

                s1_active     <= s0_active;
                s1_border     <= border_v;
                s1_line       <= line_v;
                s1_r          <= rv;
                s1_g          <= gv;
                s1_b          <= bv;

                s1_text_valid <= text_ok_v;
                s1_gx         <= gx_v;
                s1_gy         <= gy_v;
                s1_text_line  <= text_line_v;
                s1_text_col   <= text_col_v;
            end if;
        end if;
    end process;

    --------------------------------------------------------------------
    -- S2: align base RGB with text char and font address
    --------------------------------------------------------------------
    process(clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                s2_active     <= '0';
                s2_border     <= '0';
                s2_line       <= '0';
                s2_r          <= (others => '0');
                s2_g          <= (others => '0');
                s2_b          <= (others => '0');
                s2_text_valid <= '0';
                s2_gx         <= 0;
                s2_gy         <= 0;
                s2_char       <= (others => '0');
            else
                s2_active     <= s1_active;
                s2_border     <= s1_border;
                s2_line       <= s1_line;
                s2_r          <= s1_r;
                s2_g          <= s1_g;
                s2_b          <= s1_b;

                s2_text_valid <= s1_text_valid;
                s2_gx         <= s1_gx;
                s2_gy         <= s1_gy;
                s2_char       <= text_char_q;
            end if;
        end if;
    end process;

    --------------------------------------------------------------------
    -- S3: final compose with font row
    --------------------------------------------------------------------
    process(clk)
        variable rv       : unsigned(7 downto 0);
        variable gv       : unsigned(7 downto 0);
        variable bv       : unsigned(7 downto 0);
        variable text_pix : std_logic;
    begin
        if rising_edge(clk) then
            if reset = '1' then
                r_i <= (others => '0');
                g_i <= (others => '0');
                b_i <= (others => '0');
            else
                rv := s2_r;
                gv := s2_g;
                bv := s2_b;

                if s2_line = '1' then
                    rv := sat_add_16(rv);
                    gv := sat_add_16(gv);
                    bv := sat_add_16(bv);
                end if;

                text_pix := '0';
                if s2_text_valid = '1' then
                    text_pix := font_row_q(7 - s2_gx);
                end if;

                if text_pix = '1' then
                    rv := x"FF";
                    gv := x"FF";
                    bv := x"80";
                end if;

                if s2_border = '1' then
                    rv := x"FF";
                    gv := x"FF";
                    bv := x"FF";
                end if;

                if s2_active = '0' then
                    rv := x"00";
                    gv := x"00";
                    bv := x"00";
                end if;

                r_i <= std_logic_vector(rv);
                g_i <= std_logic_vector(gv);
                b_i <= std_logic_vector(bv);
            end if;
        end if;
    end process;

    r <= r_i;
    g <= g_i;
    b <= b_i;

end architecture;
