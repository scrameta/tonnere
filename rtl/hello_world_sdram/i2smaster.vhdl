---------------------------------------------------------------------------
-- (c) 2013 mark watson
-- I am happy for anyone to use this for non-commercial use.
-- If my vhdl files are used commercially or otherwise sold,
-- please contact me for explicit permission at scrameta (gmail).
-- This applies for source and binary form and derived works.
---------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.all;

ENTITY i2smaster IS
PORT 
( 
	CLK : IN STD_LOGIC; --1.536Mhz
	RESET_N : IN STD_LOGIC;
	
	BCLK : OUT STD_LOGIC;
	DACLRC : OUT STD_LOGIC;
	
	LEFT_IN : in std_logic_vector(15 downto 0);
	RIGHT_IN : in std_logic_vector(15 downto 0);
	
	DACDAT : OUT STD_LOGIC
);
END i2smaster;

ARCHITECTURE vhdl OF i2smaster IS
	signal daclrc_reg : std_logic;
	signal daclrc_next : std_logic;

	signal dacdat_reg : std_logic;
	signal dacdat_next : std_logic;
	
	signal shiftreg_reg : std_logic_vector(15 downto 0);
	signal shiftreg_next : std_logic_vector(15 downto 0);
	
	signal reload_reg : std_logic_vector(15 downto 0);
	signal reload_next : std_logic_vector(15 downto 0);	
BEGIN
	-- Data read on bclk low->high transition
	-- daclrc is set on bclk high->low transition
	
	process(CLK,RESET_N)
	begin	
		if (RESET_N='0') then
			daclrc_reg <= '0';
			dacdat_reg <= '0';
			shiftreg_reg <= (others=>'0');
			reload_reg(0) <= '1';
			reload_reg(15 downto 1) <= (others=>'0');
		elsif (CLK'event and CLK='0') then
			daclrc_reg <= daclrc_next;
			dacdat_reg <= dacdat_next;
			shiftreg_reg <= shiftreg_next;
			reload_reg <= reload_next;
		end if;
	end process;
	
	-- sample on change to daclrc, shift out bit if required
	process(daclrc_reg,shiftreg_reg,reload_reg,left_in,right_in)
		variable reload : std_logic;
	begin
		reload := reload_reg(0);
		shiftreg_next <= shiftreg_reg(14 downto 0)&'0';
		reload_next <= reload_reg(14 downto 0)&reload_reg(15);
		daclrc_next <= daclrc_reg;		
		dacdat_next <= shiftreg_reg(15);
		
		if (reload='1') then
			daclrc_next <= not(daclrc_reg);			
			
			if (daclrc_reg = '0') then
				shiftreg_next <= right_in;
			else				
				shiftreg_next <= left_in;
			end if;			
		end if;		
			
	end process;

	BCLK <= CLK;	
	DACDAT <= dacdat_reg;
	DACLRC <= daclrc_reg;
END vhdl;
