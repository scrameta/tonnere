library ieee;
use ieee.std_logic_1164.all;

package video_modes_pkg is

    type video_mode_t is record
        h_syncLen           : integer;
        h_preActiveLen      : integer;
        h_activeLen         : integer;
        h_postActiveLen     : integer;
        v_syncLen           : integer;
        v_preActiveLen      : integer;
        v_activeLen         : integer;
        v_postActiveLen     : integer;
        v_interlace         : integer;
        v_interlaceDelayLen : integer;
        use_clk74           : integer;
    end record;

    function get_video_mode(mode : integer) return video_mode_t;
    function get_active_width(mode : integer) return integer;
    function get_active_height_full(mode : integer) return integer;
    function get_use_clk74(mode : integer) return integer;

end package;

package body video_modes_pkg is

    function get_video_mode(mode : integer) return video_mode_t is
        variable t : video_mode_t;
    begin
        case mode is
            when 0 =>   -- 1440x576i50
                t := (
                    h_syncLen => 126,
                    h_preActiveLen => 138,
                    h_activeLen => 1440,
                    h_postActiveLen => 24,
                    v_syncLen => 3,
                    v_preActiveLen => 19,
                    v_activeLen => 288,
                    v_postActiveLen => 2,
                    v_interlace => 1,
                    v_interlaceDelayLen => 864,
                    use_clk74 => 0
                );
            when 1 =>   -- 720x576p50
                t := (
                    h_syncLen => 64,
                    h_preActiveLen => 68,
                    h_activeLen => 720,
                    h_postActiveLen => 12,
                    v_syncLen => 5,
                    v_preActiveLen => 39,
                    v_activeLen => 576,
                    v_postActiveLen => 5,
                    v_interlace => 0,
                    v_interlaceDelayLen => 0,
                    use_clk74 => 0
                );
            when 2 =>   -- 1280x720p50
                t := (
                    h_syncLen => 40,
                    h_preActiveLen => 220,
                    h_activeLen => 1280,
                    h_postActiveLen => 440,
                    v_syncLen => 5,
                    v_preActiveLen => 20,
                    v_activeLen => 720,
                    v_postActiveLen => 5,
                    v_interlace => 0,
                    v_interlaceDelayLen => 0,
                    use_clk74 => 1
                );
            when 3 =>   -- 1920x1080i50
                t := (
                    h_syncLen => 44,
                    h_preActiveLen => 148,
                    h_activeLen => 1920,
                    h_postActiveLen => 528,
                    v_syncLen => 5,
                    v_preActiveLen => 15,
                    v_activeLen => 540,
                    v_postActiveLen => 2,
                    v_interlace => 1,
                    v_interlaceDelayLen => 1320,
                    use_clk74 => 1
                );
            when 4 =>   -- 1440x480i59.94-ish
                t := (
                    h_syncLen => 124,
                    h_preActiveLen => 114,
                    h_activeLen => 1440,
                    h_postActiveLen => 38,
                    v_syncLen => 3,
                    v_preActiveLen => 15,
                    v_activeLen => 240,
                    v_postActiveLen => 4,
                    v_interlace => 1,
                    v_interlaceDelayLen => 858,
                    use_clk74 => 0
                );
            when 5 =>   -- 720x480p59.94-ish
                t := (
                    h_syncLen => 62,
                    h_preActiveLen => 60,
                    h_activeLen => 720,
                    h_postActiveLen => 16,
                    v_syncLen => 6,
                    v_preActiveLen => 30,
                    v_activeLen => 480,
                    v_postActiveLen => 9,
                    v_interlace => 0,
                    v_interlaceDelayLen => 0,
                    use_clk74 => 0
                );
            when 6 =>   -- 1280x720p60
                t := (
                    h_syncLen => 40,
                    h_preActiveLen => 220,
                    h_activeLen => 1280,
                    h_postActiveLen => 110,
                    v_syncLen => 5,
                    v_preActiveLen => 20,
                    v_activeLen => 720,
                    v_postActiveLen => 5,
                    v_interlace => 0,
                    v_interlaceDelayLen => 0,
                    use_clk74 => 1
                );
            when others =>  -- 1920x1080i60
                t := (
                    h_syncLen => 44,
                    h_preActiveLen => 148,
                    h_activeLen => 1920,
                    h_postActiveLen => 88,
                    v_syncLen => 5,
                    v_preActiveLen => 15,
                    v_activeLen => 540,
                    v_postActiveLen => 2,
                    v_interlace => 1,
                    v_interlaceDelayLen => 1100,
                    use_clk74 => 1
                );
        end case;
        return t;
    end function;

    function get_active_width(mode : integer) return integer is
        variable t : video_mode_t;
    begin
        t := get_video_mode(mode);
        return t.h_activeLen;
    end function;

    function get_active_height_full(mode : integer) return integer is
        variable t : video_mode_t;
    begin
        t := get_video_mode(mode);
        if t.v_interlace = 1 then
            return t.v_activeLen * 2;
        else
            return t.v_activeLen;
        end if;
    end function;

    function get_use_clk74(mode : integer) return integer is
        variable t : video_mode_t;
    begin
        t := get_video_mode(mode);
        return t.use_clk74;
    end function;

end package body;