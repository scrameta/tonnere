library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library altera_mf;
use altera_mf.altera_mf_components.all;

entity ddr_pattern_gen is
    port (
        clk_pattern  : in  std_logic;  
        rst_n    : in  std_logic;   -- async reset, active low

        lvl : in  std_logic_vector(2 downto 0); -- registered with this clock already

        ddr_out    : out std_logic
    );
end entity ddr_pattern_gen;

architecture rtl of ddr_pattern_gen is

    subtype t_lvl is std_logic_vector(2 downto 0);

    function lvl_to_sym9(l : t_lvl) return std_logic_vector is
    begin
        case l is
            when "000" =>
                return "000000010";
            when "001" =>
                return "000000110";
            when "010" =>
                return "000001110";
            when "011" =>
                return "000011110";
            when "100" =>
                return "000111110";
            when "101" =>
                return "001111110";
            when "110" =>
                return "011111110";
            when others =>
                return "111111110";
        end case;
    end function;

    -- at 364.5MHz
    signal ddr_h_reg, ddr_h_next         : std_logic_vector(0 downto 0);
    signal ddr_l_reg, ddr_l_next         : std_logic_vector(0 downto 0);

    signal ddr_pattern_reg, ddr_pattern_next         : std_logic_vector(9 downto 0);

    signal state_reg, state_next : std_logic_vector(8 downto 0);

    signal aclr : std_logic;

begin
    process(clk_pattern,rst_n)
    begin
        if (rst_n='0') then
            state_reg <= "000000001";
            ddr_pattern_reg <= (others=>'0');		
            ddr_l_reg <= (others=>'0');		
            ddr_h_reg <= (others=>'0');		
	elsif (clk_pattern'event and clk_pattern='1') then
            state_reg <= state_next;
            ddr_pattern_reg <= ddr_pattern_next;		
            ddr_l_reg <= ddr_l_next;		
            ddr_h_reg <= ddr_h_next;		
	end if;
    end process;	    
    state_next(0) <= state_reg(8);
	 state_next(8 downto 1) <= state_reg(7 downto 0);

    process(state_reg,lvl,ddr_pattern_reg) is
	 begin
        ddr_pattern_next(9 downto 8) <= "00";
        ddr_pattern_next(7 downto 0) <= ddr_pattern_reg(9 downto 2);

			if (state_reg(0)='1') then
				ddr_pattern_next(8 downto 0) <= lvl_to_sym9(lvl);
			elsif (state_reg(4)='1') then
				ddr_pattern_next(9 downto 1) <= lvl_to_sym9(lvl);
			end if;
	end process;
	
	--     8 7  6  5  4L 3  2  1 0L
	-- 87 65 43 21 08 76 54 32 10

    ddr_h_next(0) <= ddr_pattern_reg(0);
    ddr_l_next(0) <= ddr_pattern_reg(1);

    aclr  <= not rst_n;

    u_ddio : altddio_out
        generic map (
            extend_oe_disable      => "OFF",
            intended_device_family => "Cyclone 10 LP",
            invert_output          => "OFF",
            lpm_hint               => "UNUSED",
            lpm_type               => "altddio_out",
            oe_reg                 => "UNREGISTERED",
            power_up_high          => "OFF",
            width                  => 1
        )
        port map (
            datain_h   => ddr_h_reg,
            datain_l   => ddr_l_reg,
            outclock   => clk_pattern,
            outclocken => '1',
            oe         => '1',
            aclr       => aclr,
            aset       => '0',
            sclr       => '0',
            sset       => '0',
            dataout(0)    => ddr_out
        );

end architecture rtl;
