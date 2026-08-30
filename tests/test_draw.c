// Tests for `tig/draw.c` - four accessors and two statics.
//
// 56 lines, and the module's own header comment is candid about it: "The
// functions of this module does not draw anything. It only provide accessors to
// line properties which are used by `tig_video_buffer_line` exclusively." The
// name is a guess from its position in the binary, between file cache and
// drop-down. Written 2026-08-29.
//
// **Small does not mean pointless here.** These two structs are global drawing
// state with no owner: anything that sets a line mode changes it for every later
// caller of `tig_video_buffer_line` until something sets it back. The defaults
// are therefore load-bearing, and they are asymmetric - the mode has an
// initialiser and the style does not - which is exactly the kind of thing a
// suite should pin before someone "tidies" it.

#include <string.h>

#include "tig/draw.h"

#include "test_harness.h"

// The module has no reset, so every case restores what it found. Without this a
// case that set a thick dotted line would hand it to `tig_video_buffer_line`
// for the rest of the run.
static TigDrawLineModeInfo saved_mode;
static TigDrawLineStyleInfo saved_style;

static void draw_begin(void)
{
    tig_draw_get_line_mode(&saved_mode);
    tig_draw_get_line_style(&saved_style);
}

static void draw_end(void)
{
    tig_draw_set_line_mode(&saved_mode);
    tig_draw_set_line_style(&saved_style);
}

TEST(init_and_exit_are_no_ops_that_report_success)
{
    CHECK_INT(tig_draw_init(NULL), TIG_OK);
    tig_draw_exit();
    CHECK_INT(tig_draw_init(NULL), TIG_OK);
}

// The defaults are asymmetric and both are deliberate: the mode static carries
// `{ TIG_DRAW_LINE_MODE_NORMAL, 1 }` while the style static is left zeroed,
// which happens to be `TIG_LINE_STYLE_SOLID`. Neither `init` sets them - they
// are file-scope initialisers - so these are the values in force before anyone
// calls a setter.
TEST(the_defaults_are_normal_mode_thickness_one_and_a_solid_style)
{
    TigDrawLineModeInfo mode;
    TigDrawLineStyleInfo style;

    tig_draw_get_line_mode(&mode);
    tig_draw_get_line_style(&style);

    CHECK_INT(mode.mode, TIG_DRAW_LINE_MODE_NORMAL);
    CHECK_INT(mode.thickness, 1);
    CHECK_INT(style.style, TIG_LINE_STYLE_SOLID);
}

TEST(a_line_mode_round_trips)
{
    TigDrawLineModeInfo mode;

    draw_begin();

    mode.mode = TIG_DRAW_LINE_MODE_THICK;
    mode.thickness = 5;
    tig_draw_set_line_mode(&mode);

    memset(&mode, 0, sizeof(mode));
    tig_draw_get_line_mode(&mode);
    CHECK_INT(mode.mode, TIG_DRAW_LINE_MODE_THICK);
    CHECK_INT(mode.thickness, 5);

    draw_end();
}

TEST(a_line_style_round_trips)
{
    TigDrawLineStyleInfo style;

    draw_begin();

    style.style = TIG_LINE_STYLE_DOTTED;
    tig_draw_set_line_style(&style);

    memset(&style, 0, sizeof(style));
    tig_draw_get_line_style(&style);
    CHECK_INT(style.style, TIG_LINE_STYLE_DOTTED);

    draw_end();
}

// The setters copy, they do not alias. A caller that reuses its local after
// setting must not be able to change the module's state behind its back.
TEST(the_setters_copy_rather_than_keep_the_callers_struct)
{
    TigDrawLineModeInfo mode;
    TigDrawLineModeInfo read_back;

    draw_begin();

    mode.mode = TIG_DRAW_LINE_MODE_NORMAL;
    mode.thickness = 3;
    tig_draw_set_line_mode(&mode);

    // Scribble on the caller's copy; the module must be unaffected.
    mode.mode = TIG_DRAW_LINE_MODE_THICK;
    mode.thickness = 99;

    tig_draw_get_line_mode(&read_back);
    CHECK_INT(read_back.mode, TIG_DRAW_LINE_MODE_NORMAL);
    CHECK_INT(read_back.thickness, 3);

    draw_end();
}

// Two independent statics. A single shared struct would make setting a style
// clobber the mode, which is the shape of mistake this module invites.
TEST(mode_and_style_are_independent)
{
    TigDrawLineModeInfo mode;
    TigDrawLineStyleInfo style;

    draw_begin();

    mode.mode = TIG_DRAW_LINE_MODE_THICK;
    mode.thickness = 7;
    tig_draw_set_line_mode(&mode);

    style.style = TIG_LINE_STYLE_DASHED;
    tig_draw_set_line_style(&style);

    tig_draw_get_line_mode(&mode);
    CHECK_INT(mode.mode, TIG_DRAW_LINE_MODE_THICK);
    CHECK_INT(mode.thickness, 7);

    tig_draw_get_line_style(&style);
    CHECK_INT(style.style, TIG_LINE_STYLE_DASHED);

    draw_end();
}

// State survives init/exit, because neither touches the statics. Worth pinning:
// a reader would reasonably expect `tig_draw_init` to reset them, and code that
// assumed it does would be wrong.
TEST(init_does_not_reset_the_line_state)
{
    TigDrawLineModeInfo mode;

    draw_begin();

    mode.mode = TIG_DRAW_LINE_MODE_THICK;
    mode.thickness = 4;
    tig_draw_set_line_mode(&mode);

    tig_draw_exit();
    CHECK_INT(tig_draw_init(NULL), TIG_OK);

    memset(&mode, 0, sizeof(mode));
    tig_draw_get_line_mode(&mode);
    CHECK_INT(mode.mode, TIG_DRAW_LINE_MODE_THICK);
    CHECK_INT(mode.thickness, 4);

    draw_end();
}

void test_draw_suite(void)
{
    RUN(init_and_exit_are_no_ops_that_report_success);
    RUN(the_defaults_are_normal_mode_thickness_one_and_a_solid_style);
    RUN(a_line_mode_round_trips);
    RUN(a_line_style_round_trips);
    RUN(the_setters_copy_rather_than_keep_the_callers_struct);
    RUN(mode_and_style_are_independent);
    RUN(init_does_not_reset_the_line_state);
}
