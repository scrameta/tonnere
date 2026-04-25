library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

entity io_square_test is
    generic (
        G_CLK_HZ        : positive := 100_000_000;
        G_NUM_PINS      : positive := 8;
        G_BASE_FREQ_HZ  : positive := 10_000;
        G_STEP_FREQ_HZ  : natural  := 100
    );
    port (
        clk     : in  std_logic;
        rst     : in  std_logic;

        io_out  : out std_logic_vector(G_NUM_PINS - 1 downto 0)
    );
end entity;

architecture rtl of io_square_test is

    constant C_PHASE_BITS : natural := 32;

    subtype t_phase is unsigned(C_PHASE_BITS - 1 downto 0);

    type t_phase_array is array (natural range <>) of t_phase;

    signal phase_acc : t_phase_array(0 to G_NUM_PINS - 1) :=
        (others => (others => '0'));

    signal out_reg : std_logic_vector(G_NUM_PINS - 1 downto 0) :=
        (others => '0');

    function freq_to_phase_inc(freq_hz : real) return natural is
        variable inc : real;
    begin
        inc := freq_hz * (2.0 ** C_PHASE_BITS) / real(G_CLK_HZ);
        return natural(inc + 0.5);
    end function;

    function make_phase_inc_table return t_phase_array is
        variable table : t_phase_array(0 to G_NUM_PINS - 1);
        variable f_hz  : real;
        variable inc   : natural;
    begin
        for i in 0 to G_NUM_PINS - 1 loop
            f_hz := real(G_BASE_FREQ_HZ) + real(i * G_STEP_FREQ_HZ);
            inc  := freq_to_phase_inc(f_hz);
            table(i) := to_unsigned(inc, C_PHASE_BITS);
        end loop;

        return table;
    end function;

    constant C_PHASE_INC : t_phase_array(0 to G_NUM_PINS - 1) :=
        make_phase_inc_table;

begin

    io_out <= out_reg;

    process(clk)
        variable next_phase : t_phase;
    begin
        if rising_edge(clk) then
            if rst = '1' then
                phase_acc <= (others => (others => '0'));
                out_reg   <= (others => '0');
            else
                for i in 0 to G_NUM_PINS - 1 loop
                    next_phase := phase_acc(i) + C_PHASE_INC(i);

                    phase_acc(i) <= next_phase;

                    -- MSB of the phase accumulator gives a 50% duty square wave.
                    out_reg(i) <= next_phase(C_PHASE_BITS - 1);
                end loop;
            end if;
        end if;
    end process;

end architecture;
