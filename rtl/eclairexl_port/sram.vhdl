---------------------------------------------------------------------------
-- (c) 2013 mark watson
-- I am happy for anyone to use this for non-commercial use.
-- If my vhdl files are used commercially or otherwise sold,
-- please contact me for explicit permission at scrameta (gmail).
-- This applies for source and binary form and derived works.
---------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.all;

ENTITY sram IS
PORT 
( 
	ADDRESS : IN STD_LOGIC_VECTOR(18 DOWNTO 0);
	DIN : IN STD_LOGIC_vector(15 downto 0);
	WREN : IN STD_LOGIC;
	
	clk : in std_logic;
	reset_n : in std_logic;
	
	request : in std_logic;
	extra_cycle : in std_logic := '0';
	
	width_16bit : in std_logic := '0';
	
	-- SRAM interface
	SRAM_ADDR: OUT STD_LOGIC_VECTOR(17 downto 0);
	SRAM_CE_N: OUT STD_LOGIC;
	SRAM_OE_N: OUT STD_LOGIC;
	SRAM_WE_N: OUT STD_LOGIC;

	SRAM_LB_N: OUT STD_LOGIC;
	SRAM_UB_N: OUT STD_LOGIC;
	
	SRAM_DQ: INOUT STD_LOGIC_VECTOR(15 downto 0);
	
	-- Provide data to system
	DOUT : OUT STD_LOGIC_VECTOR(15 downto 0);
	complete : out std_logic
);

END sram;

-- TODO, implement 32-bit accesses in two cycles

-- first cycle, capture inputs
-- second cycle, sram access and result (if no extra cycle)
-- third cycle, result (if extra cycle)

ARCHITECTURE slow OF sram IS

	signal oe_n_next : std_logic;
	signal oe_n_reg : std_logic;

	signal we_n_next : std_logic;
	signal we_n_reg : std_logic;	

	signal data_next : std_logic_vector(15 downto 0);
	signal data_reg : std_logic_vector(15 downto 0);

	signal address_next : std_logic_vector(18 downto 0);
	signal address_reg : std_logic_vector(18 downto 0);

	signal complete_next : std_logic;
	signal complete_reg : std_logic;

	signal pending_next : std_logic;
	signal pending_reg : std_logic;

	signal low_byte : std_logic_vector(7 downto 0);
BEGIN
	-- registers
	process(clk,reset_n)
	begin
		if (reset_n = '0') then
			oe_n_reg <= '1';
			we_n_reg <= '1';
			data_reg <= (others=>'0');
			address_reg <= (others=>'0');
			complete_reg <= '0';
			pending_reg <= '0';
		elsif rising_edge(clk) then
			oe_n_reg <= oe_n_next;
			we_n_reg <= we_n_next;
			data_reg <= data_next;
			address_reg <= address_next;
			complete_reg <= complete_next;
			pending_reg <= pending_next;
		end if;
	end process;

	-- next state
	process(din,wren,address,request,extra_cycle,pending_reg,complete_reg,oe_n_reg,we_n_reg,address_reg,data_reg,width_16bit)
	begin
		data_next <= data_reg;
		address_next <= address_reg;
		oe_n_next <= oe_n_reg;
		we_n_next <= we_n_reg;
		complete_next <= '0';
		pending_next <= '0';
		
		if complete_reg = '1' then
			oe_n_next <= '0';
			we_n_next <= '1';
		end if;

		if (request = '1') then
			address_next <= address;
			data_next <= din;
			if (width_16bit = '0') then
				data_next(15 downto 8) <= din(7 downto 0);
			end if;
			oe_n_next <= wren;
			we_n_next <= not(wren);
			-- TODO To keep the VBXE DMA engine timing uniform we could
			-- consider not differentiating this on wren (ie. always add cycle)
			-- But if the main (Atari) SRAM wants to use it and be most efficient
			-- (for turbo) then "this is the way".
			complete_next <= wren or not(extra_cycle);
			pending_next <= not(wren) and extra_cycle;
		end if;

		if pending_reg = '1' then
			complete_next <= '1';
		end if;
	end process;
	
	LOW_BYTE <= SRAM_DQ(7 downto 0) when address_reg(0)='0' else SRAM_DQ(15 downto 8);

	-- output
	SRAM_ADDR <= address_reg(18 downto 1);
	SRAM_CE_N <= '0';
	SRAM_OE_N <= oe_n_reg;
	SRAM_WE_N <= we_n_reg;
	SRAM_LB_N <= not(width_16bit) and address_reg(0);
	SRAM_UB_N <= not(width_16bit) and NOT(address_reg(0));
	SRAM_DQ <= data_reg when we_n_reg = '0' else (others=>'Z');

	DOUT <= SRAM_DQ(15 downto 8)&LOW_BYTE;

	complete <= complete_reg;

	--GPIO <= (others=>'0');
END slow;

--ARCHITECTURE fast OF sram IS
--	signal we_n : std_logic;
--	signal dq : std_logic_vector(7 downto 0);
--	
--BEGIN
--	we_n <= not(clk) nand WREN;
--	dq <= SRAM_DQ(7 downto 0) when address(0)='0' else
--		SRAM_DQ(15 downto 8);
--		
--	-- output
--	SRAM_ADDR <= ADDRESS(18 downto 1);
--	--SRAM_ADDR <= ADDRESS(17 downto 0);
--	SRAM_CE_N <= '0';
--	SRAM_OE_N <= WREN;
--	SRAM_WE_N <= we_n;
--	
--	SRAM_LB_N <= ADDRESS(0);
--	SRAM_UB_N <= NOT(ADDRESS(0));
--	--SRAM_LB_N <= '0';
--	--SRAM_UB_N <= '0';
--		
--	DOUT <= dq;		
--	SRAM_DQ <= DIN&DIN when wren='1' else (others=>'Z');
--	
--	-- immediate completion
--	complete <= request;
--END fast;