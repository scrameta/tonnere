library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.video_modes_pkg.all;

entity video_timing_core is
    generic (
        MODE : integer := 1
    );
    port (
        clk         : in  std_logic;
        reset       : in  std_logic;

        hsync       : out std_logic;
        vsync       : out std_logic;

        active      : out std_logic;
        active_x    : out integer;
        active_y    : out integer;
        field_odd   : out std_logic
    );
end entity;

architecture rtl of video_timing_core is
    constant C : video_mode_t := get_video_mode(MODE);

    constant H_TOTAL        : integer := C.h_syncLen + C.h_preActiveLen + C.h_activeLen + C.h_postActiveLen;
    constant V_TOTAL        : integer := C.v_syncLen + C.v_preActiveLen + C.v_activeLen + C.v_postActiveLen;

    constant H_ACTIVE_START : integer := C.h_syncLen + C.h_preActiveLen;
    constant H_ACTIVE_END   : integer := C.h_syncLen + C.h_preActiveLen + C.h_activeLen;

    constant V_ACTIVE_START : integer := C.v_syncLen + C.v_preActiveLen;
    constant V_ACTIVE_END   : integer := C.v_syncLen + C.v_preActiveLen + C.v_activeLen;

    signal h_count    : integer range 0 to H_TOTAL - 1 := 0;
    signal v_count    : integer range 0 to V_TOTAL - 1 := 0;
    signal field_i    : std_logic := '0';

    signal active_i   : std_logic := '0';
    signal hsync_i    : std_logic := '0';
    signal vsync_i    : std_logic := '0';
    signal active_x_i : integer := 0;
    signal active_y_i : integer := 0;
begin
    process(clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                h_count <= 0;
                v_count <= 0;
                field_i <= '0';
            else
                if h_count = H_TOTAL - 1 then
                    h_count <= 0;
                    if v_count = V_TOTAL - 1 then
                        v_count <= 0;
                        if C.v_interlace = 1 then
                            field_i <= not field_i;
                        else
                            field_i <= '0';
                        end if;
                    else
                        v_count <= v_count + 1;
                    end if;
                else
                    h_count <= h_count + 1;
                end if;
            end if;
        end if;
    end process;

    hsync_i <= '1' when (h_count < C.h_syncLen) else '0';
    vsync_i <= '1' when (v_count < C.v_syncLen) else '0';

    active_i <= '1' when
        (h_count >= H_ACTIVE_START and h_count < H_ACTIVE_END and
         v_count >= V_ACTIVE_START and v_count < V_ACTIVE_END)
        else '0';

    process(h_count, v_count, field_i)
        variable ax : integer;
        variable ay : integer;
    begin
        ax := 0;
        ay := 0;

        if (h_count >= H_ACTIVE_START) and (h_count < H_ACTIVE_END) then
            ax := h_count - H_ACTIVE_START;
        end if;

        if (v_count >= V_ACTIVE_START) and (v_count < V_ACTIVE_END) then
            if C.v_interlace = 1 then
                if field_i = '0' then
                    ay := (v_count - V_ACTIVE_START) * 2;
                else
                    ay := (v_count - V_ACTIVE_START) * 2 + 1;
                end if;
            else
                ay := v_count - V_ACTIVE_START;
            end if;
        end if;

        active_x_i <= ax;
        active_y_i <= ay;
    end process;

    hsync     <= hsync_i;
    vsync     <= vsync_i;
    active    <= active_i;
    active_x  <= active_x_i;
    active_y  <= active_y_i;
    field_odd <= field_i;
end architecture;