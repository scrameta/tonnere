-------------------------------------------------------------------[09.05.2016]
-- DVI
-------------------------------------------------------------------------------
-- Engineer: MVV

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.std_logic_unsigned.all;

entity dvi is
port (
	I_CLK_PIXEL	: in std_logic;		-- pixelclock
	I_HSYNC		: in std_logic;
	I_VSYNC		: in std_logic;
	I_BLANK		: in std_logic;
	I_RED		: in std_logic_vector(7 downto 0);
	I_GREEN		: in std_logic_vector(7 downto 0);
	I_BLUE		: in std_logic_vector(7 downto 0);

 	O_R	: out std_logic_vector(9 downto 0);
 	O_G	: out std_logic_vector(9 downto 0);
 	O_B	: out std_logic_vector(9 downto 0));
end entity dvi;

architecture rtl of dvi is

	signal r	: std_logic_vector(9 downto 0);
	signal g	: std_logic_vector(9 downto 0);
	signal b	: std_logic_vector(9 downto 0);
   
begin
	encode_r : entity work.encoder
	port map (
		CLK	=> I_CLK_PIXEL,
		DATA	=> I_RED,
		C	=> "00",
		VDE	=> not(I_BLANK),
		ENCODED	=> r);

	encode_g : entity work.encoder
	port map (
		CLK   => I_CLK_PIXEL,
		DATA  => I_GREEN,
		C     => "00",
		VDE   => not(I_BLANK),
		ENCODED  => g);

	encode_b : entity work.encoder
	port map (
		CLK   => I_CLK_PIXEL,
		DATA  => I_BLUE,
		C     => (I_VSYNC & I_HSYNC),
		VDE   => not(I_BLANK),
		ENCODED  => b);

	o_r <= r;
	o_g <= g;
	o_b <= b;

end architecture rtl;


