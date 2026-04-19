library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library altera_mf;
use altera_mf.altera_mf_components.all;

entity ddr_pattern_gen is
    port (
        clk_135  : in  std_logic;   -- sd_in
        clk_270  : in  std_logic;   -- pattern_transfer (synced)
        rst_n    : in  std_logic;   -- async reset, active low

        lvl : in  std_logic_vector(2 downto 0);

        ddr_out    : out std_logic
    );
end entity ddr_pattern_gen;

architecture rtl of ddr_pattern_gen is

    subtype t_lvl is std_logic_vector(2 downto 0);
    constant LVL_M2 : t_lvl := "000";
    constant LVL_M1 : t_lvl := "001";
    constant LVL_0  : t_lvl := "010";
    constant LVL_P1 : t_lvl := "011";
    constant LVL_P2 : t_lvl := "100";

    function lvl_to_sym4(l : t_lvl; ph : unsigned(1 downto 0)) return std_logic_vector is
    begin
        case l is
            when LVL_M2 =>
                return "0000";
            when LVL_M1 =>
                return "1000";
            when LVL_0 =>
                return "1010";
            when LVL_P1 =>
                return "1110";
            when others =>
                return "1111";
        end case;
    end function;

    -- 135MHz
    signal lvl_slow_reg, lvl_slow_next         : std_logic_vector(2 downto 0);

    -- 270MHz
    signal lvl_fast_reg, lvl_fast_next         : std_logic_vector(2 downto 0);
    signal ddr_h_reg, ddr_h_next         : std_logic_vector(0 downto 0);
    signal ddr_l_reg, ddr_l_next         : std_logic_vector(0 downto 0);

    signal ddr_pattern_reg, ddr_pattern_next         : std_logic_vector(3 downto 0);

    signal state_reg, state_next : std_logic;

    signal rot_reg, rot_next : unsigned(1 downto 0);

    signal aclr : std_logic;

begin
    process(clk_135,rst_n)
    begin
        if (rst_n='0') then
            lvl_slow_reg <= (others=>'0');		
	elsif (clk_135'event and clk_135='1') then
            lvl_slow_reg <= lvl_slow_next;		
	end if;
    end process;	    
    lvl_slow_next <= lvl;

    process(clk_270,rst_n)
    begin
        if (rst_n='0') then
            lvl_fast_reg <= (others=>'0');		
            state_reg <= '0';
            ddr_pattern_reg <= (others=>'0');		
            ddr_l_reg <= (others=>'0');		
            ddr_h_reg <= (others=>'0');		
            rot_reg <= (others=>'0');		
	elsif (clk_270'event and clk_270='1') then
            lvl_fast_reg <= lvl_fast_next;
            state_reg <= state_next;
            ddr_pattern_reg <= ddr_pattern_next;		
            ddr_l_reg <= ddr_l_next;		
            ddr_h_reg <= ddr_h_next;		
            rot_reg <= rot_next;
	end if;
    end process;	    
    lvl_fast_next <= lvl_slow_reg;
    state_next <= not(state_reg);

    process(state_reg,lvl_fast_reg,ddr_pattern_reg,rot_reg) is
	 begin
        ddr_pattern_next(3 downto 2) <= ddr_pattern_reg(1 downto 0);
        ddr_pattern_next(1 downto 0) <= ddr_pattern_reg(3 downto 2);

	rot_next <= rot_reg;
	--rot_next <= rot_reg+1; --Switch over when working

	if (state_reg='1') then
		ddr_pattern_next <= lvl_to_sym4(lvl_fast_reg, rot_reg);
	end if;
	end process;

    ddr_h_next(0) <= ddr_pattern_reg(1);
    ddr_l_next(0) <= ddr_pattern_reg(0);

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
            outclock   => clk_270,
            outclocken => '1',
            oe         => '1',
            aclr       => aclr,
            aset       => '0',
            sclr       => '0',
            sset       => '0',
            dataout(0)    => ddr_out
        );

end architecture rtl;
