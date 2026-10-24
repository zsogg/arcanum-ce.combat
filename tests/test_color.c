// Tests for `tig/color.c` and the inline colour maths in `tig/color.h`.
//
// Every pixel the game draws goes through these. They are also the easiest
// place in tig for a channel mix-up to hide, because a wrong shift still
// produces a plausible-looking colour.
//
// The module needs no video: `tig_color_set_rgba_settings` installs the masks
// and builds the multiply table on its own, so the tests pick the 32-bit
// ARGB8888 layout the game actually runs in (`main.c` asks for bpp 32) and work
// from there.
//
// The blend helpers are worth reading before editing these. `tig_color_add`,
// `tig_color_sub` and `tig_color_blend_alpha` all work on channel values left
// *in place* - masked but not shifted down - and rely on unsigned wraparound
// plus a final mask rather than on per-channel arithmetic. That is why the
// tests below check channel isolation so insistently: the failure mode is a
// carry leaking from one channel into its neighbour, not a wrong answer within
// a channel.

#include "tig/color.h"

#include "test_harness.h"

#define A_MASK 0xFF000000u
#define R_MASK 0x00FF0000u
#define G_MASK 0x0000FF00u
#define B_MASK 0x000000FFu

static void use_argb8888(void)
{
    tig_color_set_rgba_settings(R_MASK, G_MASK, B_MASK, A_MASK);
}

TEST(rgba_settings_derive_shifts_from_masks)
{
    unsigned int r;
    unsigned int g;
    unsigned int b;

    use_argb8888();
    tig_color_get_shifts(&r, &g, &b);

    CHECK_INT(r, 16);
    CHECK_INT(g, 8);
    CHECK_INT(b, 0);
}

TEST(masks_round_trip_through_the_getter)
{
    unsigned int r;
    unsigned int g;
    unsigned int b;

    use_argb8888();
    tig_color_get_masks(&r, &g, &b);

    CHECK_INT(r, R_MASK);
    CHECK_INT(g, G_MASK);
    CHECK_INT(b, B_MASK);
}

TEST(make_and_extract_round_trip)
{
    tig_color_t color;

    use_argb8888();
    color = tig_color_make(0x12, 0x34, 0x56);

    CHECK_INT(tig_color_get_red(color), 0x12);
    CHECK_INT(tig_color_get_green(color), 0x34);
    CHECK_INT(tig_color_get_blue(color), 0x56);
}

TEST(make_sets_alpha_opaque)
{
    // Every colour tig builds is fully opaque - nothing ever puts a real alpha
    // in here. This is what makes `tig_color_alpha` read the red channel
    // instead; see the note on it in color.h.
    tig_color_t color;

    use_argb8888();
    color = tig_color_make(0, 0, 0);

    CHECK_INT((color & A_MASK), A_MASK);
}

TEST(channels_do_not_bleed_into_each_other)
{
    // Pure red must leave green and blue at zero, and so on round. A wrong
    // shift shows up here and almost nowhere else.
    tig_color_t red;
    tig_color_t green;
    tig_color_t blue;

    use_argb8888();

    red = tig_color_make(255, 0, 0);
    green = tig_color_make(0, 255, 0);
    blue = tig_color_make(0, 0, 255);

    CHECK_INT(tig_color_get_red(red), 255);
    CHECK_INT(tig_color_get_green(red), 0);
    CHECK_INT(tig_color_get_blue(red), 0);

    CHECK_INT(tig_color_get_red(green), 0);
    CHECK_INT(tig_color_get_green(green), 255);
    CHECK_INT(tig_color_get_blue(green), 0);

    CHECK_INT(tig_color_get_red(blue), 0);
    CHECK_INT(tig_color_get_green(blue), 0);
    CHECK_INT(tig_color_get_blue(blue), 255);
}

TEST(mul_by_white_is_the_identity)
{
    tig_color_t color;
    tig_color_t white;
    tig_color_t result;

    use_argb8888();

    color = tig_color_make(10, 120, 250);
    white = tig_color_make(255, 255, 255);
    result = tig_color_mul(color, white);

    CHECK_INT(tig_color_get_red(result), 10);
    CHECK_INT(tig_color_get_green(result), 120);
    CHECK_INT(tig_color_get_blue(result), 250);
}

TEST(mul_by_black_is_black)
{
    tig_color_t result;

    use_argb8888();

    result = tig_color_mul(tig_color_make(200, 200, 200), tig_color_make(0, 0, 0));

    CHECK_INT(tig_color_get_red(result), 0);
    CHECK_INT(tig_color_get_green(result), 0);
    CHECK_INT(tig_color_get_blue(result), 0);
}

TEST(add_saturates_instead_of_wrapping)
{
    // The interesting property: a channel that overflows must clamp at 255 and
    // must not carry into the channel above it.
    tig_color_t result;

    use_argb8888();

    result = tig_color_add(tig_color_make(200, 0, 200), tig_color_make(100, 0, 100));

    CHECK_INT(tig_color_get_red(result), 255);
    CHECK_INT(tig_color_get_blue(result), 255);
    // The green channel sat between two saturating ones and must be untouched.
    CHECK_INT(tig_color_get_green(result), 0);
}

TEST(sub_clamps_at_zero_without_borrowing)
{
    // Mirror of the add case: a channel driven below zero must stop at zero and
    // must not borrow from its neighbour.
    tig_color_t result;

    use_argb8888();

    // sub(src, dst) subtracts src from dst.
    result = tig_color_sub(tig_color_make(100, 0, 100), tig_color_make(50, 200, 150));

    CHECK_INT(tig_color_get_red(result), 0);
    CHECK_INT(tig_color_get_blue(result), 50);
    CHECK_INT(tig_color_get_green(result), 200);
}

TEST(blend_alpha_at_the_extremes_picks_one_side)
{
    tig_color_t src;
    tig_color_t dst;
    tig_color_t result;

    use_argb8888();

    src = tig_color_make(255, 0, 0);
    dst = tig_color_make(0, 0, 255);

    // 0 means none of src.
    result = tig_color_blend_alpha(src, dst, 0);
    CHECK_INT(tig_color_get_red(result), 0);
    CHECK_INT(tig_color_get_blue(result), 255);

    // 255 is as close to all-src as an 8-bit shift gets: the helper divides by
    // 256, not 255, so src's own channel lands one step short at 254. The
    // asymmetry is worth pinning - dst's channel is removed *completely* (the
    // borrow carries out past the mask) while src's never quite arrives. Both
    // are the sort of off-by-one a "cleanup" would happily change.
    result = tig_color_blend_alpha(src, dst, 255);
    CHECK_INT(tig_color_get_red(result), 254);
    CHECK_INT(tig_color_get_blue(result), 0);
}

TEST(blend_alpha_keeps_channels_separate_when_interpolating_down)
{
    // The case that exercises the unsigned wraparound path: src darker than
    // dst, so `r1 - r2` underflows and the result is only correct because of
    // the final mask. A channel leaking here would be invisible in the average
    // frame and obvious in a gradient.
    tig_color_t result;

    use_argb8888();

    result = tig_color_blend_alpha(tig_color_make(0, 0, 0),
        tig_color_make(200, 100, 50),
        128);

    CHECK_INT(tig_color_get_red(result), 100);
    CHECK_INT(tig_color_get_green(result), 50);
    CHECK_INT(tig_color_get_blue(result), 25);
}

TEST(grayscale_brings_white_to_254_and_keeps_black)
{
    // Porting bug #347: the three weights are truncated separately (76.5,
    // 150.45 and 28.05 of the channel), so a fully white pixel comes back
    // 254, as the original's 0x52C370 answers under emulation: 0xFEFEFE.
    tig_color_t result;

    use_argb8888();

    result = tig_color_rgb_to_grayscale(tig_color_make(255, 255, 255));
    CHECK_INT(tig_color_get_red(result), 254);
    CHECK_INT(tig_color_get_green(result), 254);
    CHECK_INT(tig_color_get_blue(result), 254);

    result = tig_color_rgb_to_grayscale(tig_color_make(0, 0, 0));
    CHECK_INT(tig_color_get_red(result), 0);
}

TEST(grayscale_weights_green_most)
{
    // Luma weighting: equal-intensity green must come out brighter than red,
    // and red brighter than blue.
    tig_color_t r;
    tig_color_t g;
    tig_color_t b;

    use_argb8888();

    r = tig_color_rgb_to_grayscale(tig_color_make(255, 0, 0));
    g = tig_color_rgb_to_grayscale(tig_color_make(0, 255, 0));
    b = tig_color_rgb_to_grayscale(tig_color_make(0, 0, 255));

    CHECK(tig_color_get_red(g) > tig_color_get_red(r));
    CHECK(tig_color_get_red(r) > tig_color_get_red(b));

    // The original's own values for the three primaries (0x4C, 0x96, 0x1C).
    CHECK_INT(tig_color_get_red(r), 76);
    CHECK_INT(tig_color_get_red(g), 150);
    CHECK_INT(tig_color_get_red(b), 28);
}

TEST(color_alpha_reads_the_red_channel)
{
    // Pins the documented oddity in color.h: `tig_color_alpha` returns red. Its
    // callers in art.c pass greyscale alpha-mask palette entries, where that is
    // the value wanted. If someone "fixes" it to read the alpha channel this
    // fails, which is the point - every colour tig makes is opaque, so that
    // change would turn the art blend paths into no-ops.
    tig_color_t color;

    use_argb8888();
    color = tig_color_make(0x42, 0x00, 0x00);

    CHECK_INT(tig_color_alpha(color), 0x42);
}

void test_color_suite(void)
{
    RUN(rgba_settings_derive_shifts_from_masks);
    RUN(masks_round_trip_through_the_getter);
    RUN(make_and_extract_round_trip);
    RUN(make_sets_alpha_opaque);
    RUN(channels_do_not_bleed_into_each_other);

    RUN(mul_by_white_is_the_identity);
    RUN(mul_by_black_is_black);

    RUN(add_saturates_instead_of_wrapping);
    RUN(sub_clamps_at_zero_without_borrowing);

    RUN(blend_alpha_at_the_extremes_picks_one_side);
    RUN(blend_alpha_keeps_channels_separate_when_interpolating_down);

    RUN(grayscale_brings_white_to_254_and_keeps_black);
    RUN(grayscale_weights_green_most);

    RUN(color_alpha_reads_the_red_channel);
}
