// Tests for `tig_art_rle_decode`, the RLE frame decoder `tig_art_file_read`
// runs over a frame's encoded bytes since card HW1 (it read each run header
// and fill colour from the stream, one byte at a time). A header byte's low 7
// bits are a length; with the top bit set that many literal pixels follow,
// otherwise one colour fills them.

#include <string.h>

#include "tig/art.h"

#include "test_harness.h"

TEST(literal_and_fill_runs_decode_in_order)
{
    // 3 literals, a fill of 4 x 0x55, 1 literal.
    static const uint8_t src[] = { 0x83, 1, 2, 3, 0x04, 0x55, 0x81, 9 };
    static const uint8_t want[] = { 1, 2, 3, 0x55, 0x55, 0x55, 0x55, 9 };
    uint8_t dst[8];

    memset(dst, 0xEE, sizeof(dst));
    CHECK_INT(tig_art_rle_decode(src, (int)sizeof(src), dst, (int)sizeof(dst)), 8);
    CHECK_INT(memcmp(dst, want, sizeof(want)), 0);
}

// Runs that fill less than the frame: the count says how far they reached,
// and the next frame starts there, as the original's pointer did.
TEST(a_frame_whose_runs_fall_short_answers_what_it_wrote)
{
    static const uint8_t src[] = { 0x02, 7 };
    uint8_t dst[10];

    memset(dst, 0xEE, sizeof(dst));
    CHECK_INT(tig_art_rle_decode(src, (int)sizeof(src), dst, (int)sizeof(dst)), 2);
    CHECK_INT(dst[1], 7);
    CHECK_INT(dst[2], 0xEE);
}

// The frame bounds every run (review 2026-10-22, finding 4), and the encoded
// bytes do too: a literal or a fill that needs bytes past them is malformed.
TEST(a_run_past_the_frame_or_the_encoded_bytes_is_refused)
{
    static const uint8_t past_frame[] = { 0x05, 1 };
    static const uint8_t short_literal[] = { 0x84, 1, 2 };
    static const uint8_t no_fill_colour[] = { 0x81, 4, 0x03 };
    uint8_t dst[4];

    CHECK_INT(tig_art_rle_decode(past_frame, (int)sizeof(past_frame), dst, 4), -1);
    CHECK_INT(tig_art_rle_decode(short_literal, (int)sizeof(short_literal), dst, 4), -1);
    CHECK_INT(tig_art_rle_decode(no_fill_colour, (int)sizeof(no_fill_colour), dst, 4), -1);
    CHECK_INT(tig_art_rle_decode(past_frame, 0, dst, 4), 0);
}

void test_art_rle_suite(void)
{
    RUN(literal_and_fill_runs_decode_in_order);
    RUN(a_frame_whose_runs_fall_short_answers_what_it_wrote);
    RUN(a_run_past_the_frame_or_the_encoded_bytes_is_refused);
}
