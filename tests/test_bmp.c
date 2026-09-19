// Tests for `tig/bmp.c` - the .bmp loader.
//
// 85 lines, two functions, no suite. Written 2026-08-29.
//
// **One real caller, and it is picky.** `light.c:2263` loads
// `art\light\shadowmap.bmp` and then *requires* `bpp == 8`, returning failure
// otherwise. So the 8-bit paletted path is the production one and the palette
// conversion is what the shadow rendering reads.
//
// **The fixtures are hand-written, on purpose.** `SDL_SaveBMP` cannot emit
// 4-bit paletted files at all, and using SDL to write what SDL then reads would
// check the two halves of one library against each other rather than checking
// `tig_bmp_create`'s contract. These build the byte layout directly - file
// header, `BITMAPINFOHEADER`, palette, bottom-up padded rows - which is also
// what lets a case ask for a bit depth the loader is supposed to REFUSE.
//
// `tig_bmp_create` accepts 4, 8 and 24 bits per pixel and rejects everything
// else, so 16 and 32 are the refusal cases.

#include <stdio.h>
#include <string.h>

#include "tig/bmp.h"
#include "tig/color.h"
#include "tig/file.h"

#include "test_harness.h"

#define BMP_DIR "./arcanum_test_bmp"
#define BMP_PATH BMP_DIR "/fixture.bmp"

static void bmp_remove(void)
{
    remove(BMP_PATH);
    remove(BMP_DIR);
}

static void put_u16(unsigned char* out, unsigned int value)
{
    out[0] = (unsigned char)(value & 0xFF);
    out[1] = (unsigned char)((value >> 8) & 0xFF);
}

static void put_u32(unsigned char* out, unsigned int value)
{
    out[0] = (unsigned char)(value & 0xFF);
    out[1] = (unsigned char)((value >> 8) & 0xFF);
    out[2] = (unsigned char)((value >> 16) & 0xFF);
    out[3] = (unsigned char)((value >> 24) & 0xFF);
}

// Write a BMP with the given depth. `palette` is `ncolors` BGRA quads, or NULL
// for a depth that carries no table. `rows` is `height` rows of already-packed
// pixel bytes, each `row_bytes` long before padding; they are written
// bottom-up, which is what a positive height means in this format.
static void bmp_write(int bpp, int width, int height, int ncolors,
    const unsigned char* palette, const unsigned char* rows, int row_bytes)
{
    unsigned char file_header[14];
    unsigned char info_header[40];
    unsigned char pad[4] = { 0, 0, 0, 0 };
    int padding = (4 - (row_bytes % 4)) % 4;
    int palette_bytes = ncolors * 4;
    int pixels_offset = 14 + 40 + palette_bytes;
    int image_bytes = (row_bytes + padding) * height;
    FILE* stream;
    int row;

    memset(file_header, 0, sizeof(file_header));
    file_header[0] = 'B';
    file_header[1] = 'M';
    put_u32(file_header + 2, (unsigned int)(pixels_offset + image_bytes));
    put_u32(file_header + 10, (unsigned int)pixels_offset);

    memset(info_header, 0, sizeof(info_header));
    put_u32(info_header + 0, 40);
    put_u32(info_header + 4, (unsigned int)width);
    put_u32(info_header + 8, (unsigned int)height);
    put_u16(info_header + 12, 1);
    put_u16(info_header + 14, (unsigned int)bpp);
    put_u32(info_header + 16, 0); // BI_RGB
    put_u32(info_header + 20, (unsigned int)image_bytes);
    put_u32(info_header + 32, (unsigned int)ncolors);
    put_u32(info_header + 36, (unsigned int)ncolors);

    stream = fopen(BMP_PATH, "wb");
    if (stream == NULL) {
        return;
    }

    fwrite(file_header, 1, sizeof(file_header), stream);
    fwrite(info_header, 1, sizeof(info_header), stream);
    if (palette != NULL && palette_bytes > 0) {
        fwrite(palette, 1, (size_t)palette_bytes, stream);
    }
    for (row = height - 1; row >= 0; row--) {
        fwrite(rows + (size_t)row * (size_t)row_bytes, 1, (size_t)row_bytes, stream);
        if (padding > 0) {
            fwrite(pad, 1, (size_t)padding, stream);
        }
    }
    fclose(stream);
}

static void bmp_fixture_dir(void)
{
    bmp_remove();
    tig_file_mkdir(BMP_DIR);
}

// The palette conversion goes through `tig_color_make`, which reads the masks
// installed by `tig_color_set_rgba_settings`. Set them here rather than relying
// on whichever suite ran last - the game runs 32-bit ARGB8888 (`main.c` asks
// for bpp 32).
static void bmp_use_argb8888(void)
{
    tig_color_set_rgba_settings(0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
}

// A 4x2 paletted image using colours 0..3, plus a 4-entry BGRA table.
static const unsigned char PAL4[16] = {
    0x00,
    0x00,
    0x00,
    0x00, // 0: black
    0x00,
    0x00,
    0xFF,
    0x00, // 1: red    (B,G,R,reserved)
    0x00,
    0xFF,
    0x00,
    0x00, // 2: green
    0xFF,
    0x00,
    0x00,
    0x00, // 3: blue
};

// ---------------------------------------------------------------------------

TEST(a_null_bmp_is_refused)
{
    CHECK_INT(tig_bmp_create(NULL), TIG_ERR_INVALID_PARAM);
}

TEST(a_missing_file_reports_an_io_error)
{
    TigBmp bmp;

    bmp_fixture_dir();
    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_DIR "/not-here.bmp");

    // A sentinel, because a zeroed struct cannot show that `create` clears the
    // field - it was already NULL, and the assertion below passed whether the
    // clear happened or not. Measured 2026-08-29 as a MISSED mutation.
    bmp.pixels = (void*)&bmp;

    CHECK_INT(tig_bmp_create(&bmp), TIG_ERR_IO);
    // The pixel pointer is cleared before the open is attempted, so a caller
    // that ignores the return value at least does not free a stale pointer.
    CHECK(bmp.pixels == NULL);

    bmp_remove();
}

TEST(a_file_that_is_not_a_bmp_is_refused)
{
    TigBmp bmp;
    FILE* stream;

    bmp_fixture_dir();
    stream = fopen(BMP_PATH, "wb");
    if (stream != NULL) {
        fwrite("not a bitmap at all", 1, 19, stream);
        fclose(stream);
    }

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);
    bmp.pixels = (void*)&bmp;
    CHECK_INT(tig_bmp_create(&bmp), TIG_ERR_GENERIC);
    CHECK(bmp.pixels == NULL);

    bmp_remove();
}

// The production path: 8-bit paletted, which is what `light.c` demands.
TEST(an_eight_bit_paletted_bmp_loads_with_its_palette)
{
    TigBmp bmp;
    unsigned char rows[8] = {
        0,
        1,
        2,
        3,
        3,
        2,
        1,
        0,
    };

    bmp_use_argb8888();
    bmp_fixture_dir();
    bmp_write(8, 4, 2, 4, PAL4, rows, 4);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);

    CHECK_INT(tig_bmp_create(&bmp), TIG_OK);
    CHECK_INT(bmp.bpp, 8);
    CHECK_INT(bmp.width, 4);
    CHECK_INT(bmp.height, 2);
    CHECK(bmp.pixels != NULL);
    CHECK(bmp.pitch >= 4);

    // The palette arrives converted through `tig_color_make`, not as raw BGRA.
    CHECK_INT(bmp.palette[0], 0xFF000000u);
    CHECK_INT(bmp.palette[1], 0xFFFF0000u);
    CHECK_INT(bmp.palette[2], 0xFF00FF00u);
    CHECK_INT(bmp.palette[3], 0xFF0000FFu);

    // Top row first: the loader hands back top-down rows from a bottom-up file.
    CHECK_INT(((unsigned char*)bmp.pixels)[0], 0);
    CHECK_INT(((unsigned char*)bmp.pixels)[1], 1);
    CHECK_INT(((unsigned char*)bmp.pixels)[2], 2);
    CHECK_INT(((unsigned char*)bmp.pixels)[3], 3);
    CHECK_INT(((unsigned char*)bmp.pixels)[bmp.pitch + 0], 3);
    CHECK_INT(((unsigned char*)bmp.pixels)[bmp.pitch + 3], 0);

    tig_bmp_destroy(&bmp);
    bmp_remove();
}

// 4-bit paletted: the third depth the loader accepts, and the one SDL cannot
// write, which is why these fixtures are hand-built. Two pixels per byte,
// high nibble first.
TEST(a_four_bit_paletted_bmp_loads)
{
    TigBmp bmp;
    // 4 pixels wide: indices 0,1,2,3 -> bytes 0x01, 0x23. Row is 2 bytes.
    unsigned char rows[2] = { 0x01, 0x23 };

    bmp_use_argb8888();
    bmp_fixture_dir();
    bmp_write(4, 4, 1, 4, PAL4, rows, 2);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);

    CHECK_INT(tig_bmp_create(&bmp), TIG_OK);
    CHECK_INT(bmp.bpp, 4);
    CHECK_INT(bmp.width, 4);
    CHECK_INT(bmp.height, 1);
    CHECK(bmp.pixels != NULL);
    CHECK_INT(bmp.palette[1], 0xFFFF0000u);

    tig_bmp_destroy(&bmp);
    bmp_remove();
}

TEST(a_twenty_four_bit_bmp_loads_without_a_palette)
{
    TigBmp bmp;
    // Two pixels per row, BGR order: red then green; blue then black.
    unsigned char rows[12] = {
        0x00,
        0x00,
        0xFF,
        0x00,
        0xFF,
        0x00,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };

    bmp_use_argb8888();
    bmp_fixture_dir();
    bmp_write(24, 2, 2, 0, NULL, rows, 6);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);

    CHECK_INT(tig_bmp_create(&bmp), TIG_OK);
    CHECK_INT(bmp.bpp, 24);
    CHECK_INT(bmp.width, 2);
    CHECK_INT(bmp.height, 2);
    CHECK(bmp.pixels != NULL);
    CHECK(bmp.pitch >= 6);

    // Pixel bytes stay in the surface's own order - the loader copies, it does
    // not convert. First pixel of the top row is the red one.
    CHECK_INT(((unsigned char*)bmp.pixels)[0], 0x00);
    CHECK_INT(((unsigned char*)bmp.pixels)[1], 0x00);
    CHECK_INT(((unsigned char*)bmp.pixels)[2], 0xFF);

    tig_bmp_destroy(&bmp);
    bmp_remove();
}

// 32-bit is one of the depths the loader is written to refuse.
TEST(a_thirty_two_bit_bmp_is_refused)
{
    TigBmp bmp;
    unsigned char rows[16] = {
        0,
        0,
        0xFF,
        0xFF,
        0,
        0xFF,
        0,
        0xFF,
        0xFF,
        0,
        0,
        0xFF,
        0,
        0,
        0,
        0xFF,
    };

    bmp_fixture_dir();
    bmp_write(32, 2, 2, 0, NULL, rows, 8);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);
    bmp.pixels = (void*)&bmp;

    CHECK_INT(tig_bmp_create(&bmp), TIG_ERR_GENERIC);
    // Refused before any allocation, so there is nothing to free.
    CHECK(bmp.pixels == NULL);

    bmp_remove();
}

TEST(a_sixteen_bit_bmp_is_refused)
{
    TigBmp bmp;
    unsigned char rows[8] = { 0, 0, 0xFF, 0xFF, 0xE0, 0x07, 0x1F, 0x00 };

    bmp_fixture_dir();
    bmp_write(16, 2, 2, 0, NULL, rows, 4);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);
    bmp.pixels = (void*)&bmp;

    CHECK_INT(tig_bmp_create(&bmp), TIG_ERR_GENERIC);
    CHECK(bmp.pixels == NULL);

    bmp_remove();
}

// `pitch * height` is exactly what the loader copies, so the buffer it hands
// back is that size and no larger. Reading the last byte of the last row must
// be in bounds - under the ASan preset this case is what proves it.
TEST(the_buffer_is_pitch_times_height)
{
    TigBmp bmp;
    unsigned char rows[15];
    int index;
    volatile unsigned char last;

    for (index = 0; index < 15; index++) {
        rows[index] = (unsigned char)(index % 4);
    }

    bmp_use_argb8888();
    bmp_fixture_dir();
    // 5 wide so the row needs padding to a 4-byte boundary.
    bmp_write(8, 5, 3, 4, PAL4, rows, 5);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);

    CHECK_INT(tig_bmp_create(&bmp), TIG_OK);
    CHECK_INT(bmp.width, 5);
    CHECK_INT(bmp.height, 3);
    CHECK(bmp.pitch >= bmp.width);

    last = ((unsigned char*)bmp.pixels)[bmp.pitch * bmp.height - 1];
    CHECK(last == last);

    tig_bmp_destroy(&bmp);
    bmp_remove();
}

// `tig_bmp_destroy` frees the pixels and reports success. It does **not** clear
// the pointer, which is faithful - the original at 0x5360D0 is a null test, a
// `free`, and `xor eax,eax`, with no store back. So destroying the same struct
// twice is a double free in both engines. Checked by reading the field, never
// by calling destroy again.
TEST(destroy_frees_the_pixels_but_leaves_the_pointer_set)
{
    TigBmp bmp;
    unsigned char rows[4] = { 0, 1, 2, 3 };

    bmp_use_argb8888();
    bmp_fixture_dir();
    bmp_write(8, 4, 1, 4, PAL4, rows, 4);

    memset(&bmp, 0, sizeof(bmp));
    strcpy(bmp.name, BMP_PATH);
    CHECK_INT(tig_bmp_create(&bmp), TIG_OK);
    CHECK(bmp.pixels != NULL);

    CHECK_INT(tig_bmp_destroy(&bmp), TIG_OK);
    // The field still holds the freed address - reading the pointer value, not
    // the memory it named.
    CHECK(bmp.pixels != NULL);

    bmp_remove();
}

// Destroy guards on NULL, so a caller that cleared the field itself - or that
// is unwinding a create which never allocated - is safe.
TEST(destroy_is_safe_when_there_are_no_pixels)
{
    TigBmp bmp;

    memset(&bmp, 0, sizeof(bmp));
    CHECK_INT(tig_bmp_destroy(&bmp), TIG_OK);
    CHECK(bmp.pixels == NULL);
}

void test_bmp_suite(void)
{
    RUN(a_null_bmp_is_refused);
    RUN(a_missing_file_reports_an_io_error);
    RUN(a_file_that_is_not_a_bmp_is_refused);
    RUN(an_eight_bit_paletted_bmp_loads_with_its_palette);
    RUN(a_four_bit_paletted_bmp_loads);
    RUN(a_twenty_four_bit_bmp_loads_without_a_palette);
    RUN(a_thirty_two_bit_bmp_is_refused);
    RUN(a_sixteen_bit_bmp_is_refused);
    RUN(the_buffer_is_pitch_times_height);
    RUN(destroy_frees_the_pixels_but_leaves_the_pointer_set);
    RUN(destroy_is_safe_when_there_are_no_pixels);
}
