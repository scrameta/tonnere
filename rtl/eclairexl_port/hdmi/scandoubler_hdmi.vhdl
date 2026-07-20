---------------------------------------------------------------------------
-- (c) 2016 mark watson
-- I am happy for anyone to use this for non-commercial use.
-- If my vhdl files are used commercially or otherwise sold,
-- please contact me for explicit permission at scrameta (gmail).
-- This applies for source and binary form and derived works.
---------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.all;
use ieee.numeric_std.all;
USE IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.STD_LOGIC_MISC.all;

ENTITY scandoubler_hdmi IS
PORT 
( 
	-- Atari clock_domain
	CLK_ATARI_IN : IN STD_LOGIC;           -- 56MHz approx (PAL 50Hz exactly/NTSC 59.94Hz exactly)

	RESET_N : IN STD_LOGIC;

 	-- Audio
 	audio_left : in std_logic_vector(15 downto 0);
 	audio_right: in std_logic_vector(15 downto 0);
	
	-- GTIA interface
	pal : in std_logic;
	scanlines_on : in std_logic;
	csync_on : in std_logic;
	colour_enable : in std_logic;  -- 720 pixels/line (at least...)
	colour_in : in std_logic_vector(7 downto 0);
	vsync_in : in std_logic;       -- high for new 
	hsync_in : in std_logic;       -- high for new line
	format : in std_logic_vector(1 downto 0);  -- "00"=VGA,"01"=DVI,"10"=HDMI
	
	--HDMI clock domain
	CLK_HDMI_IN : IN STD_LOGIC;   -- 135MHz exactly
	CLK_PIXEL_IN : IN STD_LOGIC;  -- 27 MHz exactly (aligned with HDMI)

	-- VGA output - in clk pixel domain
	O_hsync : out std_logic;
	O_vsync : out std_logic;
	O_blank : out std_logic;
	O_red : out std_logic_vector(7 downto 0);
	O_green : out std_logic_vector(7 downto 0);
	O_blue : out std_logic_vector(7 downto 0);

	-- TO TV...
	O_TMDS_H : OUT STD_LOGIC_VECTOR(7 downto 0);
	O_TMDS_L : OUT STD_LOGIC_VECTOR(7 downto 0)
);
END scandoubler_hdmi;

ARCHITECTURE vhdl OF scandoubler_hdmi IS

component hdmi_line_buffer IS
	PORT
	(
		data		: IN STD_LOGIC_VECTOR (7 DOWNTO 0);
		rdaddress		: IN STD_LOGIC_VECTOR (12 DOWNTO 0);
		rdclock		: IN STD_LOGIC ;
		wraddress		: IN STD_LOGIC_VECTOR (12 DOWNTO 0);
		wrclock		: IN STD_LOGIC  := '1';
		wren		: IN STD_LOGIC  := '0';
		q		: OUT STD_LOGIC_VECTOR (7 DOWNTO 0)
	);
END component;

signal colour_mask : std_logic_vector(7 downto 0);

-- TODO: NTSC!
-- ModeLine " 720x 480@59.94Hz"  27.00  720  736  798  858  480  489  495  525 -HSync -VSync

signal hcnt		: std_logic_vector(11 downto 0) := "000000000000"; 	-- horizontal pixel counter
signal vcnt		: std_logic_vector(11 downto 0) := "000000000000"; 	-- vertical line counter

signal hsync_next		: std_logic;
signal vsync_next		: std_logic;
signal blank_next		: std_logic;
signal red_next		: std_logic_vector(7 downto 0);
signal green_next		: std_logic_vector(7 downto 0);
signal blue_next		: std_logic_vector(7 downto 0);

signal hsync_reg		: std_logic;
signal vsync_reg		: std_logic;
signal blank_reg		: std_logic;
signal red_reg		: std_logic_vector(7 downto 0);
signal green_reg		: std_logic_vector(7 downto 0);
signal blue_reg		: std_logic_vector(7 downto 0);
signal hs_hsync_next		: std_logic;
signal hs_vsync_next		: std_logic;
signal hs_hsync_reg		: std_logic;
signal hs_vsync_reg		: std_logic;
signal hs_blank_reg		: std_logic;
signal hs_red_reg		: std_logic_vector(7 downto 0);
signal hs_green_reg		: std_logic_vector(7 downto 0);
signal hs_blue_reg		: std_logic_vector(7 downto 0);
signal hs_red_reg2		: std_logic_vector(7 downto 4);
signal hs_green_reg2		: std_logic_vector(7 downto 4);
signal hs_blue_reg2		: std_logic_vector(7 downto 4);

--
signal cap_vsync_shift_next : std_logic_vector(3 downto 0);
signal cap_vsync_shift_reg : std_logic_vector(3 downto 0);
signal cap_vcount_next : std_logic_vector(1 downto 0);
signal cap_vcount_reg : std_logic_vector(1 downto 0);
signal cap_hcount_next : std_logic_vector(10 downto 0);
signal cap_hcount_reg : std_logic_vector(10 downto 0);
signal cap_vsync_reg : std_logic;
signal cap_hsync_reg : std_logic;
signal cap_frame_start : std_logic;
signal cap_line_start_raw : std_logic;
signal cap_line_start : std_logic;
signal cap_format : std_logic_vector(1 downto 0);

-- 
signal out_colour_raw : std_logic_vector(7 downto 0);
signal out_colour : std_logic_vector(7 downto 0);
signal out_pal : std_logic;
signal out_scanlines : std_logic;
signal out_csync : std_logic;
signal out_frame_start : std_logic;
signal out_format : std_logic_vector(1 downto 0);

-- Horizontal Timing   
signal h_pixels_across	: integer;
signal h_sync_on		: integer;
signal h_sync_off		: integer;
signal h_end_count		: integer;
-- Vertical Timing 
signal v_pixels_down		: integer;
signal v_sync_on		: integer;
signal v_sync_off		: integer;
signal v_end_count		: integer;

-- signed audio
signal audio_left_signed : std_logic_vector(15 downto 0);
signal audio_right_signed: std_logic_vector(15 downto 0);

-- audio synchronizer
signal audio_left_reg1  : std_logic_vector(15 downto 0);
signal audio_right_reg1 : std_logic_vector(15 downto 0);
signal audio_left_reg2  : std_logic_vector(15 downto 0);
signal audio_right_reg2 : std_logic_vector(15 downto 0);

-- hdmi/dvi->tmds
signal hdmi_encoded_red   : std_logic_vector(9 downto 0);
signal hdmi_encoded_green : std_logic_vector(9 downto 0);
signal hdmi_encoded_blue  : std_logic_vector(9 downto 0);
signal dvi_encoded_red   : std_logic_vector(9 downto 0);
signal dvi_encoded_green : std_logic_vector(9 downto 0);
signal dvi_encoded_blue  : std_logic_vector(9 downto 0);

signal encoded_red_next   : std_logic_vector(9 downto 0);
signal encoded_green_next : std_logic_vector(9 downto 0);
signal encoded_blue_next  : std_logic_vector(9 downto 0);
signal encoded_red_reg   : std_logic_vector(9 downto 0);
signal encoded_green_reg : std_logic_vector(9 downto 0);
signal encoded_blue_reg  : std_logic_vector(9 downto 0);

signal mod5	: std_logic_vector(2 downto 0) := "000";	-- modulus 5 counter
signal shift_r	: std_logic_vector(9 downto 0) := "0000000000";
signal shift_g	: std_logic_vector(9 downto 0) := "0000000000";
signal shift_b	: std_logic_vector(9 downto 0) := "0000000000";
signal shift_clk : std_logic_vector(9 downto 0) := "0000000000";

	
BEGIN

--------------------------------------------------------
-- Capture side
-- Store 4 lines of data in dual port ram

-- register
process(CLK_ATARI_IN,reset_n)
begin
	if (reset_n = '0') then
		cap_hcount_reg <= (others=>'0');
		cap_vcount_reg <= (others=>'0');
		cap_vsync_reg <= '0';
		cap_hsync_reg <= '0';
		cap_vsync_shift_reg <= (others=>'0');
	elsif (CLK_ATARI_IN'event and CLK_ATARI_IN='1') then
		cap_hcount_reg <= cap_hcount_next;
		cap_vcount_reg <= cap_vcount_next;
		cap_vsync_reg <= vsync_in;
		cap_hsync_reg <= hsync_in;
		cap_vsync_shift_reg <= cap_vsync_shift_next;
	end if;
end process;

cap_line_start_raw <= not(hsync_in) and cap_hsync_reg;
hsync_delay : entity work.delay_line
	generic map (COUNT=>184)
	port map(clk=>clk_atari_in,sync_reset=>'0',data_in=>cap_line_start_raw,enable=>colour_enable,reset_n=>'1',data_out=>cap_line_start);			

-- our location
process(cap_hcount_reg, colour_enable, cap_line_start)
begin
	colour_mask <= x"ff";

	cap_hcount_next <= cap_hcount_reg;
	if (colour_enable = '1') then
		cap_hcount_next <= std_logic_vector(unsigned(cap_hcount_reg)+1);
	end if;
	if (cap_line_start = '1') then
		cap_hcount_next <= (others=>'0');
	end if;
	if (or_reduce(cap_hcount_reg(10 downto 6))='0') then
		colour_mask <= x"00";
	end if;
	if (cap_hcount_reg(10 downto 7) >= "1011") then --1408
		colour_mask <= x"00";
	end if;
end process;

process(cap_vcount_reg, cap_line_start, cap_vsync_reg, vsync_in, cap_vsync_shift_reg)
begin
	cap_vcount_next <= cap_vcount_reg;
	cap_vsync_shift_next <= cap_vsync_shift_reg;
	if (cap_line_start = '1') then
		cap_vcount_next <= std_logic_vector(unsigned(cap_vcount_reg)+1);
		cap_vsync_shift_next <= cap_vsync_shift_reg(2 downto 0)&'0';
	end if;
	if (cap_vsync_reg = '1' and vsync_in = '0') then
		cap_vcount_next <= (others=>'0');
		cap_vsync_shift_next <= "0001";
	end if;
end process;

cap_frame_start <= cap_vsync_shift_reg(2);

hdmi_line_buffer_inst : hdmi_line_buffer
port map 
	(
		data		=> colour_in and colour_mask,
		wraddress	=> cap_vcount_reg&cap_hcount_reg,
		wrclock		=> CLK_ATARI_IN,
		wren		=> colour_enable, -- 1824 times/line

		rdaddress	=> vcnt(2 downto 1)&hcnt(9 downto 0)&'0',
		rdclock		=> CLK_PIXEL_IN,
		q		=> out_colour_raw
	);

-- Audio should be signed

audio_left_signed  <= std_logic_vector(to_signed(to_integer(unsigned(audio_left))-32768,16));
audio_right_signed <= std_logic_vector(to_signed(to_integer(unsigned(audio_right))-32768,16));

--------------------------------------------------------
-- Output side
-- Retrieve data from dual port ram, do palette lookup and send to hdmi encoder

pal_synchronizer : entity work.synchronizer
	port map (clk=>clk_pixel_in, raw=>pal, sync=>out_pal);						

scanline_synchronizer : entity work.synchronizer
	port map (clk=>clk_pixel_in, raw=>scanlines_on, sync=>out_scanlines);						

csync_synchronizer : entity work.synchronizer
	port map (clk=>clk_pixel_in, raw=>csync_on, sync=>out_csync);						

cap_vsync_synchronizer : entity work.synchronizer
	port map (clk=>clk_pixel_in, raw=>cap_frame_start, sync=>out_frame_start);						
cap_format0_synchronizer : entity work.synchronizer
	port map (clk=>clk_pixel_in, raw=>format(0), sync=>out_format(0));						
cap_format1_synchronizer : entity work.synchronizer
	port map (clk=>clk_pixel_in, raw=>format(1), sync=>out_format(1));						

out_colour(7 downto 4) <= out_colour_raw(7 downto 4);
out_colour(3 downto 0) <= out_colour_raw(3 downto 0) when (not(out_scanlines) or vcnt(0))='1' else '0'&out_colour_raw(3 downto 1);

-- colour palette
-- TODO- share!!
palette4 : entity work.gtia_palette
	port map (PAL=>out_pal,ATARI_COLOUR=>out_colour, R_next=>red_next, G_next=>green_next, B_next=>blue_next);		
	
-- extract from fifo to line buffer (720 pixels)
-- sync vsync between sides...

process (clk_pixel_in, hcnt, out_frame_start)
begin
	if clk_pixel_in'event and clk_pixel_in = '1' then
		if hcnt = h_end_count then
			hcnt <= (others => '0');
		else
			hcnt <= hcnt + 1;
		end if;
		if hcnt = h_sync_on then
			if (vcnt = v_end_count) then
				vcnt <= (others => '0');
			else
				vcnt <= vcnt + 1;
			end if;
		end if;

		if (out_frame_start='1' and (vcnt <= (v_sync_off-4) or vcnt >= (v_sync_off+4))) then
			vcnt <= std_logic_vector(to_unsigned(v_sync_off,12));
		end if;
	end if;
end process;

hsync_next	<= '0' when (hcnt <= h_sync_on) or (hcnt > h_sync_off) else '1';
vsync_next	<= '0' when (vcnt <= v_sync_on) or (vcnt > v_sync_off) else '1';
blank_next	<= '1' when (hcnt > h_pixels_across) or (vcnt > v_pixels_down) else '0';

-- TODO -- allow ZPU to write these - simple CRTC!
process(out_pal)
begin
	if (out_pal = '1') then
		h_pixels_across	 <= 720 - 1;
		h_sync_on <= 732 - 1;
		h_sync_off <= 795 - 1;
		h_end_count <= 864 - 1;
		v_pixels_down <= 576 - 1;
		v_sync_on <=  581 - 1;
		v_sync_off <= 586 - 1;
		v_end_count <=  625 - 1;
	else
		h_pixels_across	 <= 720 - 1;
		h_sync_on <= 736 -1;
		h_sync_off <= 798 -1;
		h_end_count <= 858 -1;
		v_pixels_down <= 480 -1;
		v_sync_on <=  489 -1;
		v_sync_off <= 495 -1;
		v_end_count <=  525 -1;
	end if;
end process;

hdmiav_inst : entity work.hdmi
port map (
	I_CLK_PIXEL => clk_pixel_in,
	I_RESET => not(reset_n),
	I_AUDIO_PCM_L   => audio_left_reg2,
	I_AUDIO_PCM_R   => audio_right_reg2,
	I_HSYNC		=> hsync_reg,
	I_VSYNC		=> vsync_reg,
	I_BLANK		=> blank_reg,
	I_R		=> red_reg,
	I_G		=> green_reg,
	I_B		=> blue_reg,
	O_R	=> hdmi_encoded_red,
	O_G	=> hdmi_encoded_green,
	O_B	=> hdmi_encoded_blue);
	
dvi_inst: entity work.dvi
port map (
	I_CLK_PIXEL	=> clk_pixel_in,

	I_HSYNC		=> hsync_reg,
	I_VSYNC		=> vsync_reg,
	I_BLANK		=> blank_reg,
	I_RED		=> red_reg,
	I_GREEN		=> green_reg,
	I_BLUE		=> blue_reg,
	O_R	=> dvi_encoded_red,
	O_G	=> dvi_encoded_green,
	O_B	=> dvi_encoded_blue);

process (out_format,dvi_encoded_red,dvi_encoded_green,dvi_encoded_blue,hdmi_encoded_red,hdmi_encoded_green,hdmi_encoded_blue)
begin
	encoded_red_next <= (others=>'0');
	encoded_green_next <= (others=>'0');
	encoded_blue_next <= (others=>'0');

	case out_format is
		when "01" =>
			encoded_red_next   <= dvi_encoded_red;
			encoded_green_next <= dvi_encoded_green;
			encoded_blue_next  <= dvi_encoded_blue;
		when "10" =>
			encoded_red_next   <= hdmi_encoded_red;
			encoded_green_next <= hdmi_encoded_green;
			encoded_blue_next  <= hdmi_encoded_blue;
		when others =>
			encoded_red_next <= (others=>'0');
			encoded_green_next <= (others=>'0');
			encoded_blue_next <= (others=>'0');
	end case;
end process;

process (CLK_HDMI_IN)
begin
	if (CLK_HDMI_IN'event and CLK_HDMI_IN = '1') then
		if mod5(2) = '1' then
			mod5 <= "000";
			shift_r <= encoded_red_reg;
			shift_g <= encoded_green_reg;
			shift_b <= encoded_blue_reg;
			shift_clk <= "0000011111";
		else
			mod5 <= mod5 + "001";
			shift_r <= "00" & shift_r(9 downto 2);
			shift_g <= "00" & shift_g(9 downto 2);
			shift_b <= "00" & shift_b(9 downto 2);
			shift_clk <= "00" & shift_clk(9 downto 2);
		end if;
	end if;
end process;


process(clk_pixel_in)
begin
	if (clk_pixel_in'event and clk_pixel_in='1') then
		audio_left_reg1 <= audio_left_signed;
		audio_left_reg2 <= audio_left_reg1;
		audio_right_reg1 <= audio_right_signed;
		audio_right_reg2 <= audio_right_reg1;

		hsync_reg <= hsync_next;
		vsync_reg <= vsync_next;
		blank_reg <= blank_next;
		red_reg <= red_next;
		green_reg <= green_next;
		blue_reg <= blue_next;

		encoded_blue_reg <= encoded_blue_next;
		encoded_green_reg <= encoded_green_next;
		encoded_red_reg <= encoded_red_next;
	end if;
end process;

process(clk_hdmi_in)
begin
	if (clk_hdmi_in'event and clk_hdmi_in='1') then
		hs_hsync_reg <= hs_hsync_next;
		hs_vsync_reg <= hs_vsync_next;
		hs_blank_reg <= blank_reg;
		hs_red_reg <= red_reg;
		hs_green_reg <= green_reg;
		hs_blue_reg <= blue_reg;
		hs_red_reg2(7 downto 4) <= hs_red_reg(7 downto 4);
		hs_green_reg2(7 downto 4) <= hs_green_reg(7 downto 4);
		hs_blue_reg2(7 downto 4) <= hs_blue_reg(7 downto 4);
	end if;
end process;

process(out_csync,hsync_reg,vsync_reg)
begin
	if (out_csync = '1') then
		hs_hsync_next <= not(hsync_reg xor vsync_reg);
		hs_vsync_next <='1';
	else
		hs_hsync_next <= not(hsync_reg);			
		hs_vsync_next <= not(vsync_reg);
	end if;
end process;

-- VGA outputs - with specified timings
O_hsync <= hs_hsync_reg;
O_vsync <= hs_vsync_reg;
O_blank <= hs_blank_reg;
O_red <= hs_red_reg2(7 downto 4)&hs_red_reg(3 downto 0); -- on different ddr edges
O_green <= hs_green_reg2(7 downto 4)&hs_green_reg(3 downto 0);
O_blue <= hs_blue_reg2(7 downto 4)&hs_blue_reg(3 downto 0);

-- TMDS outputs - with specified timings
O_TMDS_H <= shift_r(0) & not(shift_r(0)) & shift_g(0) & not(shift_g(0)) & shift_b(0) & not(shift_b(0)) & shift_clk(0) & not(shift_clk(0));
O_TMDS_L <= shift_r(1) & not(shift_r(1)) & shift_g(1) & not(shift_g(1)) & shift_b(1) & not(shift_b(1)) & shift_clk(1) & not(shift_clk(1));


end vhdl;


