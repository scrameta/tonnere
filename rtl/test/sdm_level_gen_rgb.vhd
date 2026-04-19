library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sdm_level_gen_rgb is
    port (
        clk_135  : in  std_logic;
        rst_n    : in  std_logic;
        in_r     : in  std_logic_vector(7 downto 0);
        in_g     : in  std_logic_vector(7 downto 0);
        in_b     : in  std_logic_vector(7 downto 0);
        lvl_r    : out std_logic_vector(2 downto 0);
        lvl_g    : out std_logic_vector(2 downto 0);
        lvl_b    : out std_logic_vector(2 downto 0)
    );
end entity sdm_level_gen_rgb;

architecture rtl of sdm_level_gen_rgb is
begin
    u_r : entity work.sdm_level_gen
        port map (
            clk_135 => clk_135,
            rst_n   => rst_n,
            pix_in  => in_r,
            lvl_out => lvl_r
        );

    u_g : entity work.sdm_level_gen
        port map (
            clk_135 => clk_135,
            rst_n   => rst_n,
            pix_in  => in_g,
            lvl_out => lvl_g
        );

    u_b : entity work.sdm_level_gen
        port map (
            clk_135 => clk_135,
            rst_n   => rst_n,
            pix_in  => in_b,
            lvl_out => lvl_b
        );
end architecture rtl;
