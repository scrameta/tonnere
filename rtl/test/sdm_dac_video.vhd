library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sdm_dac_video is
    port (
        clk_135  : in  std_logic;
        clk_270  : in  std_logic;
		  clk_pixel: in std_logic;
        rst_n    : in  std_logic;
        in_r     : in  std_logic_vector(7 downto 0);
        in_g     : in  std_logic_vector(7 downto 0);
        in_b     : in  std_logic_vector(7 downto 0);
		  in_blank_n : in std_logic;
        dac_r    : out std_logic;
        dac_g    : out std_logic;
        dac_b    : out std_logic
    );
end entity sdm_dac_video;

architecture rtl of sdm_dac_video is
    signal adj_r  : std_logic_vector(7 downto 0);
    signal adj_g  : std_logic_vector(7 downto 0);
    signal adj_b  : std_logic_vector(7 downto 0);

    signal lvl_r : std_logic_vector(2 downto 0);
    signal lvl_g : std_logic_vector(2 downto 0);
    signal lvl_b : std_logic_vector(2 downto 0);
	 
	 signal pixel_next,pixel_reg : std_logic_vector(23 downto 0);
begin
	adj_r <= in_r when in_blank_n='1' else (others=>'0');
	adj_g <= in_g when in_blank_n='1' else (others=>'0');
	adj_b <= in_b when in_blank_n='1' else (others=>'0');

	pixel_sync : ENTITY work.pixel_fifo
		PORT MAP (
			data(23 downto 16) => adj_r,
			data(15 downto 8) => adj_g,
			data(7 downto 0) => adj_b,
			wrclk		=> clk_pixel,
			wrreq		=> '1',
			
			rdclk		=> clk_135,
			rdreq		=> '1',
			q		   => pixel_next
		);
	
	 process(clk_135,rst_n)
	 begin
		if (rst_n='0') then
			pixel_reg <= (others=>'0');
		elsif (clk_135'event and clk_135='1') then
			pixel_reg <= pixel_next;
		end if;
	 end process;


    u_sdm : entity work.sdm_level_gen_rgb
        port map (
            clk_135 => clk_135,
            rst_n   => rst_n,
            in_r    => pixel_reg(23 downto 16),
            in_g    => pixel_reg(15 downto 8),
            in_b    => pixel_reg(7 downto 0),
            lvl_r   => lvl_r,
            lvl_g   => lvl_g,
            lvl_b   => lvl_b
        );

    pattern_r : entity work.ddr_pattern_gen
        port map (
            clk_135 => clk_135,
            clk_270 => clk_270,
            rst_n   => rst_n,
            lvl     => lvl_r,
            ddr_out => dac_r
        );

    pattern_g : entity work.ddr_pattern_gen
        port map (
            clk_135 => clk_135,
            clk_270 => clk_270,
            rst_n   => rst_n,
            lvl     => lvl_g,
            ddr_out => dac_g
        );

    pattern_b : entity work.ddr_pattern_gen
        port map (
            clk_135 => clk_135,
            clk_270 => clk_270,
            rst_n   => rst_n,
            lvl     => lvl_b,
            ddr_out => dac_b
        );
end architecture rtl;
