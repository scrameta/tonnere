library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sdm_dac_video is
    port (
	clk_pixel: in std_logic;
        clk_pattern  : in  std_logic;
        rst_n    : in  std_logic;
        in_r     : in  std_logic_vector(7 downto 0);
        in_g     : in  std_logic_vector(7 downto 0);
        in_b     : in  std_logic_vector(7 downto 0);
        in_blank_n : in std_logic;
        dac_r    : out std_logic_vector(1 downto 0);
        dac_g    : out std_logic_vector(1 downto 0);
        dac_b    : out std_logic_vector(1 downto 0)
    );
end entity sdm_dac_video;

architecture rtl of sdm_dac_video is
    signal adj_r  : std_logic_vector(7 downto 0);
    signal adj_g  : std_logic_vector(7 downto 0);
    signal adj_b  : std_logic_vector(7 downto 0);

    signal lvl_r : std_logic_vector(2 downto 0);
    signal lvl_g : std_logic_vector(2 downto 0);
    signal lvl_b : std_logic_vector(2 downto 0);
	 
    signal pixel_slow_next,pixel_slow_reg : std_logic_vector(23 downto 0);
    signal pixel_fast_next,pixel_fast_reg : std_logic_vector(23 downto 0);
	 
	 signal clk_pattern2 : std_logic;
begin
	adj_r <= in_r when in_blank_n='1' else (others=>'0');
	adj_g <= in_g when in_blank_n='1' else (others=>'0');
	adj_b <= in_b when in_blank_n='1' else (others=>'0');

	 process(clk_pixel,rst_n)
	 begin
		if (rst_n='0') then
			pixel_slow_reg <= (others=>'0');
		elsif (clk_pixel'event and clk_pixel='1') then
			pixel_slow_reg <= adj_r&adj_g&adj_b;
		end if;
	 end process;
	
	clk_pattern2 <= clk_pattern;
	
	 process(clk_pattern2,rst_n)
	 begin
		if (rst_n='0') then
			pixel_fast_reg <= (others=>'0');
		elsif (clk_pattern2'event and clk_pattern2='1') then
			pixel_fast_reg <= pixel_fast_next;
		end if;
	 end process;
	 pixel_fast_next <= pixel_slow_reg;

    pattern_r : entity work.ddr_pattern_gen
        port map (
            clk_pattern => clk_pattern2,
            rst_n   => rst_n,
            lvl     => pixel_fast_reg(23 downto 21),
            ddr_out => dac_r(1)
        );

    pattern_g : entity work.ddr_pattern_gen
        port map (
            clk_pattern => clk_pattern2,
            rst_n   => rst_n,
            lvl     => pixel_fast_reg(15 downto 13),
            ddr_out => dac_g(1)
        );

    pattern_b : entity work.ddr_pattern_gen
        port map (
            clk_pattern => clk_pattern2,
            rst_n   => rst_n,
            lvl     => pixel_fast_reg(7 downto 5),
            ddr_out => dac_b(1)
        );


    pattern_rl : entity work.ddr_pattern_gen
        port map (
            clk_pattern => clk_pattern2,
            rst_n   => rst_n,
            lvl     => pixel_fast_reg(20 downto 18),
            ddr_out => dac_r(0)
        );

    pattern_gl : entity work.ddr_pattern_gen
        port map (
            clk_pattern => clk_pattern2,
            rst_n   => rst_n,
            lvl     => pixel_fast_reg(12 downto 10),
            ddr_out => dac_g(0)
        );

    pattern_bl : entity work.ddr_pattern_gen
        port map (
            clk_pattern => clk_pattern2,
            rst_n   => rst_n,
            lvl     => pixel_fast_reg(4 downto 2),
            ddr_out => dac_b(0)
        );
end architecture rtl;
