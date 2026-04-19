library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sdm_level_gen is
    port (
        clk_135 : in  std_logic;
        rst_n   : in  std_logic;
        pix_in  : in  std_logic_vector(7 downto 0);
        lvl_out : out std_logic_vector(2 downto 0);
        dbg_x   : out signed(9 downto 0);
        dbg_i1  : out signed(9 downto 0);
        dbg_i2  : out signed(9 downto 0);
        dbg_fb  : out signed(9 downto 0)
    );
end entity sdm_level_gen;

architecture rtl of sdm_level_gen is
    constant W  : integer := 10;
    constant WC : integer := 12;

    subtype t_acc is signed(W-1 downto 0);
    subtype t_wacc is signed(WC-1 downto 0);
    subtype t_lvl is std_logic_vector(2 downto 0);

    constant LVL_M2 : t_lvl := "000";
    constant LVL_M1 : t_lvl := "001";
    constant LVL_0  : t_lvl := "010";
    constant LVL_P1 : t_lvl := "011";
    constant LVL_P2 : t_lvl := "100";

    constant FB_M2 : t_acc := to_signed(-144, W);
    constant FB_M1 : t_acc := to_signed( -72, W);
    constant FB_0  : t_acc := to_signed(   0, W);
    constant FB_P1 : t_acc := to_signed(  72, W);
    constant FB_P2 : t_acc := to_signed( 144, W);

    constant SAT_MAX : t_wacc := to_signed( 511, WC);
    constant SAT_MIN : t_wacc := to_signed(-511, WC);

    function sat(x : t_wacc) return t_acc is
    begin
        if x > SAT_MAX then
            return SAT_MAX(W-1 downto 0);
        elsif x < SAT_MIN then
            return SAT_MIN(W-1 downto 0);
        else
            return x(W-1 downto 0);
        end if;
    end function;

    function lvl_to_fb(l : t_lvl) return t_acc is
    begin
        case l is
            when LVL_M2 => return FB_M2;
            when LVL_M1 => return FB_M1;
            when LVL_0  => return FB_0;
            when LVL_P1 => return FB_P1;
            when others => return FB_P2;
        end case;
    end function;

    signal x_reg,  x_next  : t_acc;
    signal i1_reg, i1_next : t_acc;
    signal i2_reg, i2_next : t_acc;
    signal fb_reg, fb_next : t_acc;
    signal lvl_reg, lvl_next : t_lvl;

    signal lfsr_reg, lfsr_next : std_logic_vector(15 downto 0);
    signal lfsr2_reg, lfsr2_next : std_logic_vector(15 downto 0);
    signal dither_a  : signed(5 downto 0);
    signal dither_b  : signed(5 downto 0);
    signal dither_sum : t_wacc;
    signal th_n2_d, th_n1_d, th_p1_d, th_p2_d : t_wacc;
begin
    dither_a   <= resize(signed(lfsr_reg(4 downto 0)), 6);
    dither_b   <= resize(signed(lfsr2_reg(4 downto 0)), 6);
    dither_sum <= resize(dither_a, WC) + resize(dither_b, WC);

    comb : process(i1_reg, i2_reg, lvl_reg, pix_in, lfsr_reg, lfsr2_reg,
                   th_n2_d, th_n1_d, th_p1_d, th_p2_d, x_reg, fb_reg)
        variable lfsr_fb  : std_logic;
        variable lfsr2_fb : std_logic;
        variable x_v      : t_acc;
        variable xv       : t_wacc;
        variable y1       : t_wacc;
        variable y2       : t_wacc;
        variable v1       : t_wacc;
        variable sum3     : t_wacc;
        variable v2       : t_wacc;
        variable lvl_v    : t_lvl;
    begin
        x_next   <= x_reg;
        i1_next  <= i1_reg;
        i2_next  <= i2_reg;
        fb_next  <= fb_reg;
        lvl_next <= lvl_reg;
        lfsr_next  <= lfsr_reg;
        lfsr2_next <= lfsr2_reg;

        lfsr_fb := lfsr_reg(0);
        lfsr_next(15)         <= lfsr_fb;
        lfsr_next(14)         <= lfsr_reg(15);
        lfsr_next(13)         <= lfsr_reg(14) xor lfsr_fb;
        lfsr_next(12)         <= lfsr_reg(13) xor lfsr_fb;
        lfsr_next(11)         <= lfsr_reg(12);
        lfsr_next(10)         <= lfsr_reg(11) xor lfsr_fb;
        lfsr_next(9 downto 0) <= lfsr_reg(10 downto 1);

        lfsr2_fb := lfsr2_reg(0);
        lfsr2_next(15)         <= lfsr2_fb xor lfsr2_reg(15);
        lfsr2_next(14)         <= lfsr2_reg(15);
        lfsr2_next(13)         <= lfsr2_reg(14);
        lfsr2_next(12)         <= lfsr2_reg(13) xor lfsr2_fb;
        lfsr2_next(11)         <= lfsr2_reg(12);
        lfsr2_next(10)         <= lfsr2_reg(11);
        lfsr2_next(9)          <= lfsr2_reg(10);
        lfsr2_next(8)          <= lfsr2_reg(9);
        lfsr2_next(7)          <= lfsr2_reg(8);
        lfsr2_next(6)          <= lfsr2_reg(7);
        lfsr2_next(5)          <= lfsr2_reg(6);
        lfsr2_next(4)          <= lfsr2_reg(5);
        lfsr2_next(3)          <= lfsr2_reg(4) xor lfsr2_fb;
        lfsr2_next(2)          <= lfsr2_reg(3);
        lfsr2_next(1)          <= lfsr2_reg(2);
        lfsr2_next(0)          <= lfsr2_reg(1);

        x_v := signed(resize(unsigned(pix_in), W)) - to_signed(128, W);
        xv  := resize(x_v, WC);

        case lvl_reg is
            when LVL_M2 => y1 := to_signed(-144, WC); y2 := to_signed(-288, WC);
            when LVL_M1 => y1 := to_signed( -72, WC); y2 := to_signed(-144, WC);
            when LVL_0  => y1 := to_signed(   0, WC); y2 := to_signed(   0, WC);
            when LVL_P1 => y1 := to_signed(  72, WC); y2 := to_signed( 144, WC);
            when others => y1 := to_signed( 144, WC); y2 := to_signed( 288, WC);
        end case;

        v1   := resize(i1_reg, WC) + xv - y1;
        sum3 := resize(i1_reg, WC) + resize(i2_reg, WC) + xv;
        v2   := sum3 - y2;

        if    v2 < th_n2_d then lvl_v := LVL_M2;
        elsif v2 < th_n1_d then lvl_v := LVL_M1;
        elsif v2 < th_p1_d then lvl_v := LVL_0;
        elsif v2 < th_p2_d then lvl_v := LVL_P1;
        else                    lvl_v := LVL_P2;
        end if;

        x_next   <= x_v;
        i1_next  <= sat(v1);
        i2_next  <= sat(v2);
        lvl_next <= lvl_v;
        fb_next  <= lvl_to_fb(lvl_v);
    end process;

    regs : process(clk_135, rst_n)
    begin
        if rst_n = '0' then
            x_reg    <= (others => '0');
            i1_reg   <= (others => '0');
            i2_reg   <= (others => '0');
            fb_reg   <= (others => '0');
            lvl_reg  <= LVL_0;
            lfsr_reg  <= x"ACE1";
            lfsr2_reg <= x"7F53";
            th_n2_d   <= to_signed( -96, WC);
            th_n1_d   <= to_signed( -32, WC);
            th_p1_d   <= to_signed(  32, WC);
            th_p2_d   <= to_signed(  96, WC);
        elsif rising_edge(clk_135) then
            x_reg    <= x_next;
            i1_reg   <= i1_next;
            i2_reg   <= i2_next;
            fb_reg   <= fb_next;
            lvl_reg  <= lvl_next;
            lfsr_reg  <= lfsr_next;
            lfsr2_reg <= lfsr2_next;
            th_n2_d <= to_signed( -96, WC) - dither_sum;
            th_n1_d <= to_signed( -32, WC) - dither_sum;
            th_p1_d <= to_signed(  32, WC) - dither_sum;
            th_p2_d <= to_signed(  96, WC) - dither_sum;
        end if;
    end process;

    lvl_out <= lvl_reg;
    dbg_x   <= x_reg;
    dbg_i1  <= i1_reg;
    dbg_i2  <= i2_reg;
    dbg_fb  <= fb_reg;
end architecture rtl;
