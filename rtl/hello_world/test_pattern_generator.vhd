library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

----------------------------------------------------------------------
-- test_pattern_generator
--
-- Multi-band test pattern with text overlay, written as a 4-stage
-- pipeline.  Designed for Cyclone 10 LP at 74.25 MHz (1080i pixel
-- clock).  The previous version had four integer divides and a
-- triple multiply chain in a single combinational stage; both are
-- gone.
--
-- Coding style: every register is `<name>_reg`, driven by the
-- corresponding `<name>_next` combinational signal of the same shape.
-- Each pipeline stage has exactly one clocked process whose only job
-- is `_reg <= _next` (with synchronous reset).  All real logic lives
-- in the unclocked assignments to `_next`.  This makes the pipeline
-- depth obvious by reading top to bottom.
--
-- Pipeline (latency = 4 clocks from active_x/active_y inputs):
--
--   S1 : grid-coord counters       (cell_col, cell_row, char_x, char_y)
--   S2 : pattern decode + text-ROM address
--   S3 : text-ROM output captured -> font-ROM address
--   S4 : font-ROM output captured -> final RGB compose
--
-- The text and font ROMs are themselves clocked (1-cycle read
-- latency) so the text character emerges aligned with stage S3's
-- registers, and the font row emerges aligned with stage S4's
-- registers.
--
-- The font is rendered 1:1 at 8x8 - we DO NOT scale.  At higher
-- resolutions the text appears smaller, which is intentional: it
-- doubles as a sharpness / DAC-settling check.
----------------------------------------------------------------------

entity test_pattern_generator is
    generic (
        ACTIVE_W : integer := 720;
        ACTIVE_H : integer := 576
    );
    port (
        clk       : in  std_logic;
        reset     : in  std_logic;

        active    : in  std_logic;
        active_x  : in  integer;
        active_y  : in  integer;
        field_odd : in  std_logic;

        r         : out std_logic_vector(7 downto 0);
        g         : out std_logic_vector(7 downto 0);
        b         : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of test_pattern_generator is

    ----------------------------------------------------------------
    -- Geometry constants (all elaboration-time)
    ----------------------------------------------------------------

    -- Native 8x8 cell grid.  Power-of-two so all cell <-> pixel
    -- arithmetic is just bit-slicing or shifting.
    constant CELL_BITS  : integer := 3;                -- 2^3 = 8
    constant CELL_SIZE  : integer := 2 ** CELL_BITS;   -- = 8
    constant N_COLS     : integer := ACTIVE_W / CELL_SIZE;
    constant N_ROWS     : integer := ACTIVE_H / CELL_SIZE;

    -- Pattern-band boundaries in cell-rows.  Each band is roughly a
    -- quarter of the screen.  Bottom band hosts the text overlay.
    constant BAND0_END_ROW : integer := (1 * N_ROWS) / 4;
    constant BAND1_END_ROW : integer := (2 * N_ROWS) / 4;
    constant BAND2_END_ROW : integer := (3 * N_ROWS) / 4;

    -- Text overlay region: 3 cell rows centred vertically inside the
    -- bottom band, 40 cells centred horizontally.
    constant TEXT_NROWS    : integer := 3;
    constant TEXT_NCOLS    : integer := 40;
    constant BAND3_HEIGHT  : integer := N_ROWS - BAND2_END_ROW;
    constant TEXT_ROW_TOP  : integer := BAND2_END_ROW
                                        + (BAND3_HEIGHT - TEXT_NROWS) / 2;
    constant TEXT_COL_LEFT : integer := (N_COLS - TEXT_NCOLS) / 2;

    -- Eighths and thirds and halves of active width (constant).
    constant BAR_W   : integer := ACTIVE_W / 8;
    constant THIRD_W : integer := ACTIVE_W / 3;
    constant HALF_W  : integer := ACTIVE_W / 2;

    -- 75% intensity for the colour bars.
    constant LVL75   : unsigned(7 downto 0) := x"C0";

    -- Pick a shift amount so that x_within_half >> RAMP_SHIFT lands
    -- in roughly 0..255.  No multiplier, no divider - just a wire
    -- slice once we cast to unsigned.
    function ceil_log2(n : integer) return integer is
        variable acc_v : integer := 1;
        variable bits_v : integer := 0;
    begin
        while acc_v < n loop
            acc_v  := acc_v * 2;
            bits_v := bits_v + 1;
        end loop;
        return bits_v;
    end function;

    -- For HALF_W = 960 -> ceil_log2 = 10 -> RAMP_SHIFT = 2 -> 0..239
    -- For HALF_W = 640 -> ceil_log2 = 10 -> RAMP_SHIFT = 2 -> 0..159
    -- For HALF_W = 360 -> ceil_log2 =  9 -> RAMP_SHIFT = 1 -> 0..179
    constant RAMP_SHIFT : integer := ceil_log2(HALF_W) - 8;

    ----------------------------------------------------------------
    -- Stage 1: grid coordinates by bit-slicing the inputs.
    --
    -- Because CELL_SIZE is a power of two (8), the cell-coordinate
    -- breakdown of the input pixel position is just:
    --
    --   cell_col = active_x >> 3      char_x = active_x(2:0)
    --   cell_row = active_y >> 3      char_y = active_y(2:0)
    --
    -- No counters, no edge detection, no prev_* state.  We just
    -- register the inputs and present the slices.  In interlace the
    -- LSB of active_y is 0 in the even field and 1 in the odd field,
    -- which is exactly the right phase offset for char_y.
    ----------------------------------------------------------------
    signal active_s1_reg     : std_logic := '0';
    signal x_s1_reg          : integer   := 0;
    signal y_s1_reg          : integer   := 0;
    signal cell_col_s1_reg   : integer range 0 to 511 := 0;
    signal cell_row_s1_reg   : integer range 0 to 511 := 0;
    signal char_x_s1_reg     : integer range 0 to 7   := 0;
    signal char_y_s1_reg     : integer range 0 to 7   := 0;

    -- combinational _next signals
    signal active_s1_next    : std_logic;
    signal x_s1_next         : integer;
    signal y_s1_next         : integer;
    signal cell_col_s1_next  : integer range 0 to 511;
    signal cell_row_s1_next  : integer range 0 to 511;
    signal char_x_s1_next    : integer range 0 to 7;
    signal char_y_s1_next    : integer range 0 to 7;

    ----------------------------------------------------------------
    -- Stage 2: pattern decode + text-ROM address
    ----------------------------------------------------------------
    signal active_s2_reg      : std_logic := '0';
    signal border_s2_reg      : std_logic := '0';
    signal r_pat_s2_reg       : unsigned(7 downto 0) := (others => '0');
    signal g_pat_s2_reg       : unsigned(7 downto 0) := (others => '0');
    signal b_pat_s2_reg       : unsigned(7 downto 0) := (others => '0');
    signal text_active_s2_reg : std_logic := '0';
    signal char_x_s2_reg      : integer range 0 to 7 := 0;
    signal char_y_s2_reg      : integer range 0 to 7 := 0;

    signal active_s2_next      : std_logic;
    signal border_s2_next      : std_logic;
    signal r_pat_s2_next       : unsigned(7 downto 0);
    signal g_pat_s2_next       : unsigned(7 downto 0);
    signal b_pat_s2_next       : unsigned(7 downto 0);
    signal text_active_s2_next : std_logic;

    -- text ROM address inputs come straight from S1 registers.
    signal text_line_sel : unsigned(1 downto 0);
    signal text_col      : unsigned(5 downto 0);
    signal text_char_q   : unsigned(7 downto 0);

    ----------------------------------------------------------------
    -- Stage 3: text-ROM output captured + font-ROM address
    ----------------------------------------------------------------
    signal active_s3_reg      : std_logic := '0';
    signal border_s3_reg      : std_logic := '0';
    signal r_pat_s3_reg       : unsigned(7 downto 0) := (others => '0');
    signal g_pat_s3_reg       : unsigned(7 downto 0) := (others => '0');
    signal b_pat_s3_reg       : unsigned(7 downto 0) := (others => '0');
    signal text_active_s3_reg : std_logic := '0';
    signal char_x_s3_reg      : integer range 0 to 7 := 0;

    -- font ROM addr = { char_code(6:0) , char_y(2:0) }  (10 bits)
    signal font_addr  : unsigned(9 downto 0);
    signal font_row_q : std_logic_vector(7 downto 0);

    ----------------------------------------------------------------
    -- Stage 4: final RGB
    ----------------------------------------------------------------
    signal r_reg : std_logic_vector(7 downto 0) := (others => '0');
    signal g_reg : std_logic_vector(7 downto 0) := (others => '0');
    signal b_reg : std_logic_vector(7 downto 0) := (others => '0');

    signal r_next : std_logic_vector(7 downto 0);
    signal g_next : std_logic_vector(7 downto 0);
    signal b_next : std_logic_vector(7 downto 0);

begin

    --================================================================
    -- External ROMs
    --================================================================

    -- The text ROM address comes straight from S1 registers, so the
    -- ROM lookup runs during stage 2 and `text_char_q` becomes valid
    -- at the start of stage 3.

    -- text_line_sel: 3-way constant compare on cell_row_s1_reg.
    text_line_sel <=
        to_unsigned(0, 2) when (cell_row_s1_reg = TEXT_ROW_TOP    ) else
        to_unsigned(1, 2) when (cell_row_s1_reg = TEXT_ROW_TOP + 1) else
        to_unsigned(2, 2) when (cell_row_s1_reg = TEXT_ROW_TOP + 2) else
        (others => '0');

    -- text_col: cell_col relative to TEXT_COL_LEFT, clipped into
    -- 0..63.  Outside the text window we drive 0 (a blank cell in
    -- the ROM), so the ROM never sees a negative or out-of-range
    -- address.  This avoids a separate "valid" gating path inside
    -- the ROM.
    process(cell_col_s1_reg)
        variable rel_v : integer;
    begin
        rel_v := cell_col_s1_reg - TEXT_COL_LEFT;
        if rel_v < 0 or rel_v > 63 then
            text_col <= (others => '0');
        else
            text_col <= to_unsigned(rel_v, 6);
        end if;
    end process;

    text_rom_i : entity work.text_rom_3x40
        port map (
            clk      => clk,
            line_sel => text_line_sel,
            col      => text_col,
            q        => text_char_q
        );

    -- Font ROM address: top-bit of the character code is masked off
    -- to keep us inside the 1024-entry ROM.  The 8x glyph row index
    -- is char_y from S2.
    font_addr <= text_char_q(6 downto 0)
                 & to_unsigned(char_y_s2_reg, 3);

    font_rom_i : entity work.font_rom_8x8
        port map (
            clk  => clk,
            addr => font_addr,
            q    => font_row_q
        );

    --================================================================
    -- Stage 1: register the inputs and slice them into cell coords.
    --
    -- All four counters are produced by bit-slicing active_x and
    -- active_y - no real arithmetic, no edge detection, no carries.
    -- The integer-to-unsigned conversion is wide enough to cover the
    -- biggest mode we care about (1920x1080 fits in 11 bits; we use
    -- 12 to leave headroom).
    --================================================================

    active_s1_next <= active;
    x_s1_next      <= active_x;
    y_s1_next      <= active_y;

    -- char_x = active_x mod 8, cell_col = active_x / 8
    -- char_y = active_y mod 8, cell_row = active_y / 8
    -- Implemented as bit-slicing of an unsigned view of the input.
    process(active, active_x, active_y)
        variable xu_v : unsigned(11 downto 0);
        variable yu_v : unsigned(11 downto 0);
    begin
        if active = '1' then
            xu_v := to_unsigned(active_x, 12);
            yu_v := to_unsigned(active_y, 12);
            char_x_s1_next   <= to_integer(xu_v(2 downto 0));
            char_y_s1_next   <= to_integer(yu_v(2 downto 0));
            cell_col_s1_next <= to_integer(xu_v(11 downto 3));
            cell_row_s1_next <= to_integer(yu_v(11 downto 3));
        else
            -- During blanking the input integers can be undefined or
            -- out-of-range; force to zero so to_unsigned never raises.
            char_x_s1_next   <= 0;
            char_y_s1_next   <= 0;
            cell_col_s1_next <= 0;
            cell_row_s1_next <= 0;
        end if;
    end process;

    -- Stage 1 register
    process(clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                active_s1_reg   <= '0';
                x_s1_reg        <= 0;
                y_s1_reg        <= 0;
                cell_col_s1_reg <= 0;
                cell_row_s1_reg <= 0;
                char_x_s1_reg   <= 0;
                char_y_s1_reg   <= 0;
            else
                active_s1_reg   <= active_s1_next;
                x_s1_reg        <= x_s1_next;
                y_s1_reg        <= y_s1_next;
                cell_col_s1_reg <= cell_col_s1_next;
                cell_row_s1_reg <= cell_row_s1_next;
                char_x_s1_reg   <= char_x_s1_next;
                char_y_s1_reg   <= char_y_s1_next;
            end if;
        end if;
    end process;

    --================================================================
    -- Stage 2: pattern decode (combinational) + register
    --
    -- Inputs are S1 registers only.  The longest combinational path
    -- here is a band-select compare followed by a 7-way colour-bar
    -- compare and a 3:1 mux to pattern RGB.  No multiplies.  Divides
    -- are all by elaboration-time constants and either resolve to
    -- bit-slicing (CELL_SIZE = 8, RAMP_SHIFT) or to constant
    -- multipliers Quartus folds into shifts / additions.
    --================================================================

    active_s2_next <= active_s1_reg;

    -- 1-pixel border on the outer edge of the active area.
    border_s2_next <= '1' when (active_s1_reg = '1' and
                                (x_s1_reg = 0 or x_s1_reg = ACTIVE_W - 1 or
                                 y_s1_reg = 0 or y_s1_reg = ACTIVE_H - 1))
                      else '0';

    -- Text is "active" only inside the 3 designated text cell-rows
    -- AND the 40-cell horizontal text window.
    text_active_s2_next <= '1' when (active_s1_reg = '1' and
        (cell_row_s1_reg = TEXT_ROW_TOP    or
         cell_row_s1_reg = TEXT_ROW_TOP + 1 or
         cell_row_s1_reg = TEXT_ROW_TOP + 2) and
        cell_col_s1_reg >= TEXT_COL_LEFT and
        cell_col_s1_reg <  TEXT_COL_LEFT + TEXT_NCOLS)
        else '0';

    -- Pattern RGB selector.  Plain comparator + mux logic; no
    -- arithmetic deeper than a single constant compare or a constant
    -- shift.
    process(active_s1_reg, x_s1_reg, y_s1_reg,
            cell_row_s1_reg, cell_col_s1_reg)
        variable rv     : unsigned(7 downto 0);
        variable gv     : unsigned(7 downto 0);
        variable bv     : unsigned(7 downto 0);
        variable ramp_v : unsigned(7 downto 0);
        variable xu_v   : unsigned(15 downto 0);
        variable xrelu_v: unsigned(15 downto 0);
    begin
        rv := (others => '0');
        gv := (others => '0');
        bv := (others => '0');

        if active_s1_reg = '1' then

            ----------------------------------------------------
            -- Band 0: SMPTE-style 75% colour bars (8 vertical bars)
            ----------------------------------------------------
            if cell_row_s1_reg < BAND0_END_ROW then
                if    x_s1_reg <     BAR_W then  -- 75% white
                    rv := LVL75; gv := LVL75; bv := LVL75;
                elsif x_s1_reg < 2 * BAR_W then  -- yellow
                    rv := LVL75; gv := LVL75; bv := x"00";
                elsif x_s1_reg < 3 * BAR_W then  -- cyan
                    rv := x"00"; gv := LVL75; bv := LVL75;
                elsif x_s1_reg < 4 * BAR_W then  -- green
                    rv := x"00"; gv := LVL75; bv := x"00";
                elsif x_s1_reg < 5 * BAR_W then  -- magenta
                    rv := LVL75; gv := x"00"; bv := LVL75;
                elsif x_s1_reg < 6 * BAR_W then  -- red
                    rv := LVL75; gv := x"00"; bv := x"00";
                elsif x_s1_reg < 7 * BAR_W then  -- blue
                    rv := x"00"; gv := x"00"; bv := LVL75;
                else                              -- black
                    rv := x"00"; gv := x"00"; bv := x"00";
                end if;

            ----------------------------------------------------
            -- Band 1: high-frequency stripe tests
            --   left third  : 1-pixel vertical stripes  (Nyquist)
            --   middle third: 2-pixel vertical stripes  (Nyquist/2)
            --   right third : 1-line  horizontal stripes
            -- These are the most useful patterns for evaluating
            -- output filter rolloff and DAC settling.
            ----------------------------------------------------
            elsif cell_row_s1_reg < BAND1_END_ROW then
                if x_s1_reg < THIRD_W then
                    -- bit 0 of x toggles every pixel
                    if (x_s1_reg mod 2) = 0 then
                        rv := x"FF"; gv := x"FF"; bv := x"FF";
                    else
                        rv := x"00"; gv := x"00"; bv := x"00";
                    end if;
                elsif x_s1_reg < 2 * THIRD_W then
                    -- bit 1 of x toggles every two pixels
                    if ((x_s1_reg / 2) mod 2) = 0 then
                        rv := x"FF"; gv := x"FF"; bv := x"FF";
                    else
                        rv := x"00"; gv := x"00"; bv := x"00";
                    end if;
                else
                    -- 1-line horizontal stripes (per displayed line)
                    if (y_s1_reg mod 2) = 0 then
                        rv := x"FF"; gv := x"FF"; bv := x"FF";
                    else
                        rv := x"00"; gv := x"00"; bv := x"00";
                    end if;
                end if;

            ----------------------------------------------------
            -- Band 2: ramps
            --   left half : grey ramp    (R=G=B = (x       ) >> RAMP_SHIFT)
            --   right half: split into 3 sub-bands by cell_row -
            --               R-only, G-only, B-only ramp using
            --               (x - HALF_W) >> RAMP_SHIFT.
            -- Both ramps are pure shift-and-truncate; no multipliers.
            ----------------------------------------------------
            elsif cell_row_s1_reg < BAND2_END_ROW then
                if x_s1_reg < HALF_W then
                    xu_v   := to_unsigned(x_s1_reg, 16);
                    ramp_v := xu_v(RAMP_SHIFT + 7 downto RAMP_SHIFT);
                    rv := ramp_v; gv := ramp_v; bv := ramp_v;
                else
                    xrelu_v := to_unsigned(x_s1_reg - HALF_W, 16);
                    ramp_v  := xrelu_v(RAMP_SHIFT + 7 downto RAMP_SHIFT);
                    if cell_row_s1_reg <
                       BAND1_END_ROW
                       + (BAND2_END_ROW - BAND1_END_ROW) / 3 then
                        rv := ramp_v; gv := x"00";   bv := x"00";
                    elsif cell_row_s1_reg <
                          BAND1_END_ROW
                          + 2 * (BAND2_END_ROW - BAND1_END_ROW) / 3 then
                        rv := x"00";  gv := ramp_v; bv := x"00";
                    else
                        rv := x"00";  gv := x"00";  bv := ramp_v;
                    end if;
                end if;

            ----------------------------------------------------
            -- Band 3: 8x8 checkerboard background for the text.
            -- We use cell_col + cell_row directly (already / 8) so
            -- there's no divide.
            ----------------------------------------------------
            else
                if ((cell_row_s1_reg + cell_col_s1_reg) mod 2) = 0 then
                    rv := x"40"; gv := x"40"; bv := x"40";
                else
                    rv := x"20"; gv := x"20"; bv := x"20";
                end if;
            end if;
        end if;

        r_pat_s2_next <= rv;
        g_pat_s2_next <= gv;
        b_pat_s2_next <= bv;
    end process;

    -- Stage 2 register
    process(clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                active_s2_reg      <= '0';
                border_s2_reg      <= '0';
                r_pat_s2_reg       <= (others => '0');
                g_pat_s2_reg       <= (others => '0');
                b_pat_s2_reg       <= (others => '0');
                text_active_s2_reg <= '0';
                char_x_s2_reg      <= 0;
                char_y_s2_reg      <= 0;
            else
                active_s2_reg      <= active_s2_next;
                border_s2_reg      <= border_s2_next;
                r_pat_s2_reg       <= r_pat_s2_next;
                g_pat_s2_reg       <= g_pat_s2_next;
                b_pat_s2_reg       <= b_pat_s2_next;
                text_active_s2_reg <= text_active_s2_next;
                char_x_s2_reg      <= char_x_s1_reg;
                char_y_s2_reg      <= char_y_s1_reg;
            end if;
        end if;
    end process;

    --================================================================
    -- Stage 3: text-ROM captured -> font-ROM addressed.
    -- Pure shift register: everything just propagates one cycle so it
    -- aligns with font_row_q at stage 4.
    --================================================================
    process(clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                active_s3_reg      <= '0';
                border_s3_reg      <= '0';
                r_pat_s3_reg       <= (others => '0');
                g_pat_s3_reg       <= (others => '0');
                b_pat_s3_reg       <= (others => '0');
                text_active_s3_reg <= '0';
                char_x_s3_reg      <= 0;
            else
                active_s3_reg      <= active_s2_reg;
                border_s3_reg      <= border_s2_reg;
                r_pat_s3_reg       <= r_pat_s2_reg;
                g_pat_s3_reg       <= g_pat_s2_reg;
                b_pat_s3_reg       <= b_pat_s2_reg;
                text_active_s3_reg <= text_active_s2_reg;
                char_x_s3_reg      <= char_x_s2_reg;
            end if;
        end if;
    end process;

    --================================================================
    -- Stage 4: final compose with font_row_q.
    --
    -- text_pix is a single 8:1 mux selecting one bit of font_row_q.
    -- Final priority: inactive -> black; border -> white;
    -- text pixel -> bright yellow; otherwise pattern colour.
    --================================================================
    process(active_s3_reg, border_s3_reg, text_active_s3_reg,
            char_x_s3_reg, font_row_q,
            r_pat_s3_reg, g_pat_s3_reg, b_pat_s3_reg)
        variable text_pix_v : std_logic;
        variable rv, gv, bv : unsigned(7 downto 0);
    begin
        rv := r_pat_s3_reg;
        gv := g_pat_s3_reg;
        bv := b_pat_s3_reg;

        text_pix_v := text_active_s3_reg
                      and font_row_q(7 - char_x_s3_reg);

        if text_pix_v = '1' then
            rv := x"FF"; gv := x"FF"; bv := x"80";
        end if;

        if border_s3_reg = '1' then
            rv := x"FF"; gv := x"FF"; bv := x"FF";
        end if;

        if active_s3_reg = '0' then
            rv := x"00"; gv := x"00"; bv := x"00";
        end if;

        r_next <= std_logic_vector(rv);
        g_next <= std_logic_vector(gv);
        b_next <= std_logic_vector(bv);
    end process;

    -- Stage 4 (output) register
    process(clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                r_reg <= (others => '0');
                g_reg <= (others => '0');
                b_reg <= (others => '0');
            else
                r_reg <= r_next;
                g_reg <= g_next;
                b_reg <= b_next;
            end if;
        end if;
    end process;

    r <= r_reg;
    g <= g_reg;
    b <= b_reg;

    -- field_odd is unused now (we deliberately don't try to undo
    -- interlace - the test pattern is meant to be displayed as-is
    -- so any interlace artifacts are part of the test).  Reading
    -- the port avoids "unused signal" warnings in some flows.
    -- (Strictly, leaving an input unused is fine in VHDL; this is
    -- just documentation.)

end architecture;
