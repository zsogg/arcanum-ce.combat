// Tests for `tig/palette.c`.
//
// Palettes are how 8-bit art reaches a 32-bit screen: every indexed blit looks
// its colours up in one of these, and `tig_palette_modify` is what greys out a
// disabled icon or tints a lit sprite.
//
// The allocator is the interesting part and the reason this suite exists. A
// palette is *not* the pointer that was malloc'd: `tig_palette_create` hands
// back an address one pointer *into* the allocation, and the bytes just before
// it hold a back-pointer to the free-list node.  `tig_palette_destroy` reads
// that back-pointer to find the node again. So the two functions agree on a
// piece of pointer arithmetic that nothing else checks, and if they ever
// disagree the failure is a corrupted free list rather than a wrong colour.
// These tests run under the ASan preset for that reason.
//
// `tig_palette_init` must be called first - it is what sets the palette size
// the allocator and `tig_palette_copy` both use - and `tig_color_set_rgba_settings`
// before that, because `tig_palette_modify` goes through the colour maths.
// Both are done once for the suite, mirroring `tig_init`.

#include <string.h>

#include "tig/color.h"
#include "tig/palette.h"

#include "test_harness.h"

#define A_MASK 0xFF000000u
#define R_MASK 0x00FF0000u
#define G_MASK 0x0000FF00u
#define B_MASK 0x000000FFu

TEST(fill_sets_every_entry)
{
    TigPalette* palette;
    unsigned int color;
    int index;
    int wrong = 0;

    palette = tig_palette_create();
    CHECK(palette != NULL);

    color = tig_color_make(1, 2, 3);
    tig_palette_fill(palette, color);

    for (index = 0; index < 256; index++) {
        if (palette->colors[index] != color) {
            wrong++;
        }
    }

    // All 256, not just the first - the loop bound is the thing being checked.
    CHECK_INT(wrong, 0);

    tig_palette_destroy(palette);
}

TEST(copy_duplicates_every_entry)
{
    TigPalette* src;
    TigPalette* dst;
    int index;
    int wrong = 0;

    src = tig_palette_create();
    dst = tig_palette_create();

    for (index = 0; index < 256; index++) {
        src->colors[index] = tig_color_make(index, 255 - index, index / 2);
    }

    tig_palette_copy(dst, src);

    for (index = 0; index < 256; index++) {
        if (dst->colors[index] != src->colors[index]) {
            wrong++;
        }
    }
    CHECK_INT(wrong, 0);

    // And the copy is independent - writing one must not move the other.
    dst->colors[0] = tig_color_make(9, 9, 9);
    CHECK(src->colors[0] != dst->colors[0]);

    tig_palette_destroy(dst);
    tig_palette_destroy(src);
}

TEST(create_and_destroy_recycle_without_corrupting_the_free_list)
{
    // The back-pointer contract: destroy has to recover the node from an
    // address that is not the one malloc returned. Churn enough palettes that a
    // mistake shows up as a bad pointer rather than by luck, and write to every
    // entry each round so ASan sees the full extent.
    TigPalette* palettes[40];
    int round;
    int i;

    for (round = 0; round < 4; round++) {
        for (i = 0; i < 40; i++) {
            palettes[i] = tig_palette_create();
            CHECK(palettes[i] != NULL);
            tig_palette_fill(palettes[i], tig_color_make(i, round, 0));
        }

        // Every palette handed out at once must be distinct.
        CHECK(palettes[0] != palettes[1]);
        CHECK_INT(tig_color_get_red(palettes[7]->colors[255]), 7);

        for (i = 0; i < 40; i++) {
            tig_palette_destroy(palettes[i]);
        }
    }
}

TEST(a_recycled_palette_is_still_usable)
{
    TigPalette* first;
    TigPalette* second;

    first = tig_palette_create();
    tig_palette_fill(first, tig_color_make(1, 1, 1));
    tig_palette_destroy(first);

    // Almost certainly the same block back off the free list; it must behave
    // like a fresh one.
    second = tig_palette_create();
    tig_palette_fill(second, tig_color_make(2, 2, 2));
    CHECK_INT(tig_color_get_red(second->colors[0]), 2);
    CHECK_INT(tig_color_get_red(second->colors[255]), 2);

    tig_palette_destroy(second);
}

TEST(modify_grayscale_converts_in_place)
{
    TigPalette* palette;
    TigPaletteModifyInfo info;

    palette = tig_palette_create();
    tig_palette_fill(palette, tig_color_make(255, 0, 0));

    info.flags = TIG_PALETTE_MODIFY_GRAYSCALE;
    info.tint_color = 0;
    info.src_palette = palette;
    info.dst_palette = palette;
    tig_palette_modify(&info);

    // Pure red greyscales to its luma weight, 76, on all three channels.
    CHECK_INT(tig_color_get_red(palette->colors[0]), 76);
    CHECK_INT(tig_color_get_green(palette->colors[0]), 76);
    CHECK_INT(tig_color_get_blue(palette->colors[0]), 76);
    CHECK_INT(tig_color_get_red(palette->colors[255]), 76);

    tig_palette_destroy(palette);
}

TEST(modify_writes_to_dst_and_leaves_src_alone)
{
    TigPalette* src;
    TigPalette* dst;
    TigPaletteModifyInfo info;

    src = tig_palette_create();
    dst = tig_palette_create();

    tig_palette_fill(src, tig_color_make(255, 0, 0));
    tig_palette_fill(dst, tig_color_make(0, 0, 0));

    info.flags = TIG_PALETTE_MODIFY_GRAYSCALE;
    info.tint_color = 0;
    info.src_palette = src;
    info.dst_palette = dst;
    tig_palette_modify(&info);

    CHECK_INT(tig_color_get_red(dst->colors[0]), 76);
    // The source must be untouched - `modify` copies before converting.
    CHECK_INT(tig_color_get_red(src->colors[0]), 255);
    CHECK_INT(tig_color_get_green(src->colors[0]), 0);

    tig_palette_destroy(dst);
    tig_palette_destroy(src);
}

TEST(modify_tint_multiplies_each_entry)
{
    TigPalette* palette;
    TigPaletteModifyInfo info;

    palette = tig_palette_create();
    tig_palette_fill(palette, tig_color_make(200, 100, 50));

    info.flags = TIG_PALETTE_MODIFY_TINT;
    info.tint_color = tig_color_make(255, 255, 0); // drop blue entirely
    info.src_palette = palette;
    info.dst_palette = palette;
    tig_palette_modify(&info);

    CHECK_INT(tig_color_get_red(palette->colors[0]), 200);
    CHECK_INT(tig_color_get_green(palette->colors[0]), 100);
    CHECK_INT(tig_color_get_blue(palette->colors[0]), 0);

    tig_palette_destroy(palette);
}

TEST(modify_applies_grayscale_before_tint)
{
    // Both flags at once is not commutative: greyscale runs first, then the
    // tint multiplies the grey. Tinting first and then greyscaling would give a
    // different, duller answer, so the order is a real contract.
    TigPalette* palette;
    TigPaletteModifyInfo info;

    palette = tig_palette_create();
    tig_palette_fill(palette, tig_color_make(255, 0, 0));

    info.flags = TIG_PALETTE_MODIFY_GRAYSCALE | TIG_PALETTE_MODIFY_TINT;
    info.tint_color = tig_color_make(255, 0, 0); // keep red only
    info.src_palette = palette;
    info.dst_palette = palette;
    tig_palette_modify(&info);

    // grey 76 first, then multiplied by pure red: red survives, others go.
    CHECK_INT(tig_color_get_red(palette->colors[0]), 76);
    CHECK_INT(tig_color_get_green(palette->colors[0]), 0);
    CHECK_INT(tig_color_get_blue(palette->colors[0]), 0);

    tig_palette_destroy(palette);
}

TEST(modify_with_no_flags_does_not_even_copy)
{
    // Worth pinning because the header reads the other way round: it says src is
    // copied into dst unless they are the same object, but the function returns
    // immediately when `flags == 0`, *before* the copy. A caller passing no
    // flags and expecting a plain copy gets an untouched destination.
    TigPalette* src;
    TigPalette* dst;
    TigPaletteModifyInfo info;

    src = tig_palette_create();
    dst = tig_palette_create();

    tig_palette_fill(src, tig_color_make(255, 0, 0));
    tig_palette_fill(dst, tig_color_make(0, 0, 255));

    info.flags = 0;
    info.tint_color = 0;
    info.src_palette = src;
    info.dst_palette = dst;
    tig_palette_modify(&info);

    // Still blue: no copy happened.
    CHECK_INT(tig_color_get_blue(dst->colors[0]), 255);
    CHECK_INT(tig_color_get_red(dst->colors[0]), 0);

    tig_palette_destroy(dst);
    tig_palette_destroy(src);
}

TEST(system_memory_size_accounts_for_the_back_pointer)
{
    // The art cache uses this to bill palette memory, so it has to include the
    // hidden node pointer in front of every palette, not just the colours.
    CHECK_INT(tig_palette_system_memory_size(),
        (long long)(sizeof(TigPalette) + sizeof(void*)));
}

void test_palette_suite(void)
{
    // Mirrors what `tig_init` does, in the same order: the colour masks feed
    // the palette maths, and `tig_palette_init` sets the size everything else
    // uses.
    tig_color_set_rgba_settings(R_MASK, G_MASK, B_MASK, A_MASK);
    tig_palette_init(NULL);

    RUN(fill_sets_every_entry);
    RUN(copy_duplicates_every_entry);
    RUN(create_and_destroy_recycle_without_corrupting_the_free_list);
    RUN(a_recycled_palette_is_still_usable);
    RUN(modify_grayscale_converts_in_place);
    RUN(modify_writes_to_dst_and_leaves_src_alone);
    RUN(modify_tint_multiplies_each_entry);
    RUN(modify_applies_grayscale_before_tint);
    RUN(modify_with_no_flags_does_not_even_copy);
    RUN(system_memory_size_accounts_for_the_back_pointer);

    // Returns every pooled block, so the suite is leak-clean under ASan.
    tig_palette_exit();
}
