library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.video_modes_pkg.all;

entity video_test_top is
    generic (
        MODE : integer := 1
    );
    port (
        clk27  : in  std_logic;
        clk74  : in  std_logic;
        reset  : in  std_logic;

		  clk_used : out std_logic;
        active_x    : out unsigned(10 downto 0);
        active_y    : out unsigned(10 downto 0);
		  
        hsync  : out std_logic;
        vsync  : out std_logic;
		  blank_n: out std_logic;
        r      : out std_logic_vector(7 downto 0);
        g      : out std_logic_vector(7 downto 0);
        b      : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of video_test_top is
    constant ACTIVE_W   : integer := get_active_width(MODE);
    constant ACTIVE_H   : integer := get_active_height_full(MODE);
    constant USE_CLK74  : integer := get_use_clk74(MODE);
    constant LATENCY    : integer := 5;

    signal hsync_raw    : std_logic := '0';
    signal vsync_raw    : std_logic := '0';
    signal active_s     : std_logic := '0';
    signal active_x_s   : integer := 0;
    signal active_y_s   : integer := 0;
    signal field_odd_s  : std_logic := '0';

    signal r_s          : std_logic_vector(7 downto 0);
    signal g_s          : std_logic_vector(7 downto 0);
    signal b_s          : std_logic_vector(7 downto 0);
begin

    gen_sd : if USE_CLK74 = 0 generate
        signal hs_d : std_logic_vector(LATENCY downto 0) := (others => '0');
        signal vs_d : std_logic_vector(LATENCY downto 0) := (others => '0');
    begin
		  clk_used <= clk27;
        timing_i : entity work.video_timing_core
            generic map ( MODE => MODE )
            port map (
                clk       => clk27,
                reset     => reset,
                hsync     => hsync_raw,
                vsync     => vsync_raw,
                active    => active_s,
                active_x  => active_x_s,
                active_y  => active_y_s,
                field_odd => field_odd_s
            );

        pattern_i : entity work.test_pattern_generator
            generic map (
                ACTIVE_W => ACTIVE_W,
                ACTIVE_H => ACTIVE_H
            )
            port map (
                clk       => clk27,
                reset     => reset,
                active    => active_s,
                active_x  => active_x_s,
                active_y  => active_y_s,
                field_odd => field_odd_s,
                r         => r_s,
                g         => g_s,
                b         => b_s
            );

        process(clk27)
        begin
            if rising_edge(clk27) then
                if reset = '1' then
                    hs_d <= (others => '0');
                    vs_d <= (others => '0');
                else
                    hs_d(0) <= hsync_raw;
                    vs_d(0) <= vsync_raw;
                    for i in 1 to LATENCY loop
                        hs_d(i) <= hs_d(i - 1);
                        vs_d(i) <= vs_d(i - 1);
                    end loop;
                end if;
            end if;
        end process;

        hsync <= hs_d(LATENCY);
        vsync <= vs_d(LATENCY);
        r <= r_s;
        g <= g_s;
        b <= b_s;
    end generate;

    gen_hd : if USE_CLK74 = 1 generate
        signal hs_d : std_logic_vector(LATENCY downto 0) := (others => '0');
        signal vs_d : std_logic_vector(LATENCY downto 0) := (others => '0');
    begin
		  clk_used <= clk74;
        timing_i : entity work.video_timing_core
            generic map ( MODE => MODE )
            port map (
                clk       => clk74,
                reset     => reset,
                hsync     => hsync_raw,
                vsync     => vsync_raw,
                active    => active_s,
                active_x  => active_x_s,
                active_y  => active_y_s,
                field_odd => field_odd_s
            );

        pattern_i : entity work.test_pattern_generator
            generic map (
                ACTIVE_W => ACTIVE_W,
                ACTIVE_H => ACTIVE_H
            )
            port map (
                clk       => clk74,
                reset     => reset,
                active    => active_s,
                active_x  => active_x_s,
                active_y  => active_y_s,
                field_odd => field_odd_s,
                r         => r_s,
                g         => g_s,
                b         => b_s
            );

        process(clk74)
        begin
            if rising_edge(clk74) then
                if reset = '1' then
                    hs_d <= (others => '0');
                    vs_d <= (others => '0');
                else
                    hs_d(0) <= hsync_raw;
                    vs_d(0) <= vsync_raw;
                    for i in 1 to LATENCY loop
                        hs_d(i) <= hs_d(i - 1);
                        vs_d(i) <= vs_d(i - 1);
                    end loop;
                end if;
            end if;
        end process;

        hsync <= hs_d(LATENCY);
        vsync <= vs_d(LATENCY);
        r <= r_s;
        g <= g_s;
        b <= b_s;
    end generate;

	 blank_n <= active_s;	 
	 active_x <= to_unsigned(active_x_s,11);
	 active_y <= to_unsigned(active_y_s,11);
end architecture;