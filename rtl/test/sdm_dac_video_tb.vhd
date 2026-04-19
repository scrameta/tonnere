-- =============================================================================
-- sdm_dac_video_tb.vhd
-- Testbench for sdm_dac_video
-- VHDL-93, suitable for ModelSim / Questa / GHDL
--
-- Tests:
--   1. Reset behaviour: outputs stay low during reset
--   2. Mid-scale DC (0x80): ~50% duty cycle on all channels
--   3. Full-scale DC (0xFF): >93% duty cycle
--   4. Zero-scale DC (0x00): <7%  duty cycle
--   5. Ramp: R rises 0x00->0xFF, B falls 0xFF->0x00, G held at 0x80
--   6. CDC: pixels driven from separate 27 MHz pixel clock (SD rate),
--      verifying the synchroniser passes data correctly to the SDM
-- =============================================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sdm_dac_video_tb is
end entity sdm_dac_video_tb;

architecture sim of sdm_dac_video_tb is

    -- 270 MHz SDM clock period
    constant CLK_SDM_PERIOD : time := 3703 ps;   -- 1/270e6 ≈ 3.703 ns
    -- 27 MHz pixel clock period (SD resolution)
    constant CLK_PIX_PERIOD : time := 37037 ps;  -- 1/27e6  ≈ 37.037 ns

    -- Measurement window: 5400 SDM cycles = ~20 µs
    constant WINDOW     : integer := 5400;

    -- DUT signals
    signal clk_sdm : std_logic := '0';
    signal clk_pix : std_logic := '0';
    signal rst_n   : std_logic := '0';
    signal in_r    : std_logic_vector(7 downto 0) := x"80";
    signal in_g    : std_logic_vector(7 downto 0) := x"80";
    signal in_b    : std_logic_vector(7 downto 0) := x"80";
    signal dac_r   : std_logic;
    signal dac_g   : std_logic;
    signal dac_b   : std_logic;

    -- Measurement
    signal ones_r  : integer := 0;
    signal ones_g  : integer := 0;
    signal ones_b  : integer := 0;
    signal cyc_cnt : integer := 0;
    signal measure : boolean := false;

    function pct (ones, total : integer) return integer is
    begin
        return (ones * 100) / total;
    end function;

begin

    -- -------------------------------------------------------------------------
    -- Clocks: independent frequencies, no phase relationship
    -- -------------------------------------------------------------------------
    clk_sdm <= not clk_sdm after CLK_SDM_PERIOD / 2;
    clk_pix <= not clk_pix after CLK_PIX_PERIOD / 2;

    -- -------------------------------------------------------------------------
    -- DUT
    -- -------------------------------------------------------------------------
    u_dut : entity work.sdm_dac_video
        port map (
            clk_sdm => clk_sdm,
            clk_pix => clk_pix,
            rst_n   => rst_n,
            in_r    => in_r,
            in_g    => in_g,
            in_b    => in_b,
            dac_r   => dac_r,
            dac_g   => dac_g,
            dac_b   => dac_b
        );

    -- -------------------------------------------------------------------------
    -- Duty cycle counter: runs on clk_sdm, counts DAC output ones
    -- -------------------------------------------------------------------------
    process (clk_sdm)
    begin
        if rising_edge(clk_sdm) then
            if measure then
                if dac_r = '1' then ones_r <= ones_r + 1; end if;
                if dac_g = '1' then ones_g <= ones_g + 1; end if;
                if dac_b = '1' then ones_b <= ones_b + 1; end if;
                cyc_cnt <= cyc_cnt + 1;
            end if;
        end if;
    end process;

    -- -------------------------------------------------------------------------
    -- Stimulus: pixel values driven synchronously to clk_pix (SD pixel clock)
    -- This exercises the CDC path — the SDM sees asynchronous input changes.
    -- -------------------------------------------------------------------------
    process
        -- Apply new pixel values on a clk_pix rising edge
        procedure set_pixels (
            r, g, b : std_logic_vector(7 downto 0)
        ) is
        begin
            wait until rising_edge(clk_pix);
            in_r <= r;
            in_g <= g;
            in_b <= b;
        end procedure;

        procedure reset_counters is
        begin
            ones_r  <= 0;
            ones_g  <= 0;
            ones_b  <= 0;
            cyc_cnt <= 0;
        end procedure;

        procedure run_window is
        begin
            measure <= true;
            wait for CLK_SDM_PERIOD * WINDOW;
            measure <= false;
            wait for CLK_SDM_PERIOD * 4;
        end procedure;

        procedure check_duty (
            label    : string;
            ch       : string;
            ones     : integer;
            lo_pct   : integer;
            hi_pct   : integer
        ) is
            variable actual : integer;
        begin
            actual := pct(ones, WINDOW);
            report label & " ch=" & ch &
                   "  ones=" & integer'image(ones) &
                   "  duty=" & integer'image(actual) & "%" &
                   "  expected " & integer'image(lo_pct) &
                   "-" & integer'image(hi_pct) & "%";
            assert actual >= lo_pct and actual <= hi_pct
                report "FAIL: " & label & " ch=" & ch &
                       " duty " & integer'image(actual) &
                       "% out of range [" & integer'image(lo_pct) &
                       "%, " & integer'image(hi_pct) & "%]"
                severity error;
        end procedure;

    begin
        -- ---------------------------------------------------------------------
        -- 1. Reset — both clocks running, outputs should stay low
        -- ---------------------------------------------------------------------
        rst_n <= '0';
        wait for CLK_SDM_PERIOD * 8;

        assert dac_r = '0' and dac_g = '0' and dac_b = '0'
            report "FAIL: outputs not low during reset" severity error;

        rst_n <= '1';
        -- Allow synchroniser pipeline and integrators to settle (2 sync
        -- stages + 1 pipeline cycle = 3 clk_sdm cycles minimum)
        wait for CLK_SDM_PERIOD * 16;

        -- ---------------------------------------------------------------------
        -- 2. Mid-scale: all channels 0x80 -> expect ~50% duty cycle
        --    Pixels applied on clk_pix edges to exercise CDC
        -- ---------------------------------------------------------------------
        set_pixels(x"80", x"80", x"80");
        wait for CLK_SDM_PERIOD * 200;   -- settle through sync + integrators
        reset_counters;
        run_window;

        check_duty("Mid-scale", "R", ones_r, 45, 55);
        check_duty("Mid-scale", "G", ones_g, 45, 55);
        check_duty("Mid-scale", "B", ones_b, 45, 55);

        -- ---------------------------------------------------------------------
        -- 3. Full-scale: 0xFF -> expect >93% duty cycle
        -- ---------------------------------------------------------------------
        set_pixels(x"FF", x"FF", x"FF");
        wait for CLK_SDM_PERIOD * 200;
        reset_counters;
        run_window;

        check_duty("Full-scale", "R", ones_r, 93, 100);
        check_duty("Full-scale", "G", ones_g, 93, 100);
        check_duty("Full-scale", "B", ones_b, 93, 100);

        -- ---------------------------------------------------------------------
        -- 4. Zero-scale: 0x00 -> expect <7% duty cycle
        -- ---------------------------------------------------------------------
        set_pixels(x"00", x"00", x"00");
        wait for CLK_SDM_PERIOD * 200;
        reset_counters;
        run_window;

        check_duty("Zero-scale", "R", ones_r, 0, 7);
        check_duty("Zero-scale", "G", ones_g, 0, 7);
        check_duty("Zero-scale", "B", ones_b, 0, 7);

        -- ---------------------------------------------------------------------
        -- 5. Ramp: R 0x00->0xFF, B 0xFF->0x00, G steady 0x80
        --    Pixels change every clk_pix cycle (maximum rate).
        --    G duty cycle checked at end as stability proxy.
        -- ---------------------------------------------------------------------
        for i in 0 to 255 loop
            set_pixels(
                std_logic_vector(to_unsigned(i,       8)),
                x"80",
                std_logic_vector(to_unsigned(255 - i, 8))
            );
        end loop;

        reset_counters;
        run_window;
        check_duty("Post-ramp G steady", "G", ones_g, 45, 55);

        -- ---------------------------------------------------------------------
        report "*** Simulation complete ***" severity note;
        wait;
    end process;

end architecture sim;

