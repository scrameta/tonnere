library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

entity audio_sine_sweep is
    generic (
        G_CLK_HZ        : positive := 100_000_000;
        G_SAMPLE_RATE_HZ: positive := 48_000;
        G_SWEEP_SECONDS : positive := 10
    );
    port (
        clk           : in  std_logic;
        rst           : in  std_logic;

        sample_out    : out signed(15 downto 0);
        sample_strobe : out std_logic
    );
end entity;

architecture rtl of audio_sine_sweep is

    --------------------------------------------------------------------------
    -- DDS parameters
    --------------------------------------------------------------------------

    constant C_PHASE_BITS : natural := 32;
    constant C_LUT_BITS   : natural := 8;
    constant C_LUT_SIZE   : natural := 2 ** C_LUT_BITS;

    constant C_FREQ_MIN_HZ : real := 10.0;
    constant C_FREQ_MAX_HZ : real := 16_000.0;

    -- One full up/down sweep takes G_SWEEP_SECONDS.
    constant C_TOTAL_SAMPLES : natural := G_SAMPLE_RATE_HZ * G_SWEEP_SECONDS;
    constant C_HALF_SAMPLES  : natural := C_TOTAL_SAMPLES / 2;

    --------------------------------------------------------------------------
    -- Helpers
    --------------------------------------------------------------------------

    function freq_to_phase_inc(freq_hz : real) return natural is
        variable inc : real;
    begin
        inc := freq_hz * (2.0 ** C_PHASE_BITS) / real(G_SAMPLE_RATE_HZ);
        return natural(inc + 0.5);
    end function;

    constant C_INC_MIN : natural := freq_to_phase_inc(C_FREQ_MIN_HZ);
    constant C_INC_MAX : natural := freq_to_phase_inc(C_FREQ_MAX_HZ);

    -- Phase increment is swept with 16 fractional bits to allow smooth changes.
    constant C_SWEEP_FRAC_BITS : natural := 16;

    constant C_INC_MIN_EXT : unsigned(47 downto 0) :=
        shift_left(resize(to_unsigned(C_INC_MIN, 32), 48), C_SWEEP_FRAC_BITS);

    constant C_INC_MAX_EXT : unsigned(47 downto 0) :=
        shift_left(resize(to_unsigned(C_INC_MAX, 32), 48), C_SWEEP_FRAC_BITS);

    function sweep_step return natural is
        variable diff : real;
        variable step : real;
    begin
        diff := real(C_INC_MAX - C_INC_MIN);
        step := diff * real(2 ** C_SWEEP_FRAC_BITS) / real(C_HALF_SAMPLES);
        return natural(step + 0.5);
    end function;

    constant C_SWEEP_STEP : unsigned(47 downto 0) :=
        resize(to_unsigned(sweep_step, 48), 48);

    --------------------------------------------------------------------------
    -- Sine lookup table
    --------------------------------------------------------------------------

    type t_sine_lut is array (0 to C_LUT_SIZE - 1) of signed(15 downto 0);

    function make_sine_lut return t_sine_lut is
        variable lut : t_sine_lut;
        variable x   : real;
        variable s   : real;
        variable v   : integer;
    begin
        for i in 0 to C_LUT_SIZE - 1 loop
            x := 2.0 * MATH_PI * real(i) / real(C_LUT_SIZE);
            s := sin(x);

            -- 90% full scale to avoid clipping if later filtered/processed.
            v := integer(s * 29490.0);

            lut(i) := to_signed(v, 16);
        end loop;

        return lut;
    end function;

    constant C_SINE_LUT : t_sine_lut := make_sine_lut;

    --------------------------------------------------------------------------
    -- Sample-rate divider
    --------------------------------------------------------------------------

    constant C_SAMPLE_DIV : natural := G_CLK_HZ / G_SAMPLE_RATE_HZ;

    signal sample_div_cnt : natural range 0 to C_SAMPLE_DIV - 1 := 0;

    --------------------------------------------------------------------------
    -- DDS state
    --------------------------------------------------------------------------

    signal phase_acc  : unsigned(31 downto 0) := (others => '0');
    signal inc_acc    : unsigned(47 downto 0) := C_INC_MIN_EXT;

    signal sweep_cnt  : natural range 0 to C_HALF_SAMPLES - 1 := 0;
    signal sweep_up   : std_logic := '1';

begin

    process(clk)
        variable lut_index : natural range 0 to C_LUT_SIZE - 1;
        variable phase_inc : unsigned(31 downto 0);
    begin
        if rising_edge(clk) then
            sample_strobe <= '0';

            if rst = '1' then
                sample_div_cnt <= 0;
                phase_acc      <= (others => '0');
                inc_acc        <= C_INC_MIN_EXT;
                sweep_cnt      <= 0;
                sweep_up       <= '1';
                sample_out     <= (others => '0');

            else
                if sample_div_cnt = C_SAMPLE_DIV - 1 then
                    sample_div_cnt <= 0;
                    sample_strobe  <= '1';

                    ------------------------------------------------------------------
                    -- Generate sine sample
                    ------------------------------------------------------------------

                    phase_inc := inc_acc(47 downto 16);
                    phase_acc <= phase_acc + phase_inc;

                    lut_index := to_integer(phase_acc(31 downto 24));
                    sample_out <= C_SINE_LUT(lut_index);

                    ------------------------------------------------------------------
                    -- Sweep phase increment
                    ------------------------------------------------------------------

                    if sweep_cnt = C_HALF_SAMPLES - 1 then
                        sweep_cnt <= 0;
                        sweep_up  <= not sweep_up;
                    else
                        sweep_cnt <= sweep_cnt + 1;
                    end if;

                    if sweep_up = '1' then
                        if inc_acc < C_INC_MAX_EXT - C_SWEEP_STEP then
                            inc_acc <= inc_acc + C_SWEEP_STEP;
                        else
                            inc_acc <= C_INC_MAX_EXT;
                        end if;
                    else
                        if inc_acc > C_INC_MIN_EXT + C_SWEEP_STEP then
                            inc_acc <= inc_acc - C_SWEEP_STEP;
                        else
                            inc_acc <= C_INC_MIN_EXT;
                        end if;
                    end if;

                else
                    sample_div_cnt <= sample_div_cnt + 1;
                end if;
            end if;
        end if;
    end process;

end architecture;
