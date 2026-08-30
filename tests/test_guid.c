// Tests for `tig/guid.c` - UUID v4 generation.
//
// 50 lines, and **CE's own code rather than a transcription**: the original
// calls `CoCreateGuid`, a Win32 API, and CE replaces it with `rand()`. So there
// is no address to be faithful to and the question is whether it honours RFC
// 4122 and whether it can be made reproducible. Written 2026-08-29.
//
// **Why it matters more than its size suggests.** A GUID reaches disk -
// `tig_database_create` writes one into every `.dat` it makes - so an engine
// that could not reproduce them could not produce a byte-identical archive, and
// the save round-trip and determinism gates would have a permanent unexplained
// difference. `tig_guid_seed_set` exists for exactly that, and
// `determinism.c:70` calls it.
//
// **One cross-module fact these cases establish, which is easy to miss:**
// `tig_guid_init` calls `srand(time(NULL))`, and `srand` is process-global. It
// is the LAST entry in `core.c`'s module table, so it reseeds the CRT generator
// after every other subsystem has started - and `tig_file_cache`'s eviction
// draws from the same `rand()`. Determinism therefore depends on
// `tig_guid_seed_set` being called *after* `tig_init`, which is what
// `determinism.c` does.

#include <stdlib.h>
#include <string.h>

#include "tig/guid.h"

#include "test_harness.h"

// Restore an arbitrary seed so a later suite that draws from `rand()` is not
// handed this suite's sequence. `test_file_cache.c` seeds immediately before it
// draws, so it is safe either way - this is belt and braces.
static void guid_end(void)
{
    srand(1);
}

TEST(init_and_exit_report_success_and_keep_no_state)
{
    CHECK_INT(tig_guid_init(NULL), TIG_OK);
    tig_guid_exit();
    CHECK_INT(tig_guid_init(NULL), TIG_OK);
    tig_guid_exit();

    guid_end();
}

// RFC 4122 section 4.4: the version nibble is 4 and the variant is 1. Both are
// forced after the random fill, so they hold whatever `rand()` produced.
TEST(every_guid_carries_the_version_four_and_variant_one_bits)
{
    TigGuid guid;
    int index;
    int bad_version = 0;
    int bad_variant = 0;

    tig_guid_seed_set(12345);

    for (index = 0; index < 64; index++) {
        tig_guid_create(&guid);
        if ((guid.data[6] & 0xF0) != 0x40) {
            bad_version++;
        }
        if ((guid.data[8] & 0xC0) != 0x80) {
            bad_variant++;
        }
    }

    CHECK_INT(bad_version, 0);
    CHECK_INT(bad_variant, 0);

    guid_end();
}

// The forced bits must not destroy the rest of those two bytes: the low nibble
// of byte 6 and the low six bits of byte 8 stay random. A mask that cleared
// them would halve the entropy quietly.
TEST(the_forced_bits_leave_the_rest_of_their_bytes_alone)
{
    TigGuid guid;
    int index;
    unsigned int seen_low6 = 0;
    unsigned int seen_low8 = 0;

    tig_guid_seed_set(999);

    for (index = 0; index < 64; index++) {
        tig_guid_create(&guid);
        seen_low6 |= (unsigned int)(guid.data[6] & 0x0F);
        seen_low8 |= (unsigned int)(guid.data[8] & 0x3F);
    }

    // Over 64 draws every bit of both fields should have been set at least once.
    CHECK_INT(seen_low6, 0x0F);
    CHECK_INT(seen_low8, 0x3F);

    guid_end();
}

// The property the determinism gate rests on.
TEST(the_same_seed_produces_the_same_guids)
{
    TigGuid first[4];
    TigGuid second[4];
    int index;
    int differing = 0;

    tig_guid_seed_set(4242);
    for (index = 0; index < 4; index++) {
        tig_guid_create(&first[index]);
    }

    tig_guid_seed_set(4242);
    for (index = 0; index < 4; index++) {
        tig_guid_create(&second[index]);
    }

    for (index = 0; index < 4; index++) {
        if (!tig_guid_is_equal(&first[index], &second[index])) {
            differing++;
        }
    }
    CHECK_INT(differing, 0);

    guid_end();
}

TEST(different_seeds_produce_different_guids)
{
    TigGuid a;
    TigGuid b;

    tig_guid_seed_set(1);
    tig_guid_create(&a);

    tig_guid_seed_set(2);
    tig_guid_create(&b);

    CHECK(!tig_guid_is_equal(&a, &b));

    guid_end();
}

// Successive draws from one seed differ - a generator that returned the same
// value twice would satisfy the reproducibility case above and still be useless.
TEST(successive_guids_from_one_seed_differ)
{
    TigGuid guid[8];
    int index;
    int other;
    int collisions = 0;

    tig_guid_seed_set(77);
    for (index = 0; index < 8; index++) {
        tig_guid_create(&guid[index]);
    }

    for (index = 0; index < 8; index++) {
        for (other = index + 1; other < 8; other++) {
            if (tig_guid_is_equal(&guid[index], &guid[other])) {
                collisions++;
            }
        }
    }
    CHECK_INT(collisions, 0);

    guid_end();
}

// `tig_guid_create` fills all sixteen bytes. A short fill would leave the tail
// at whatever the caller's stack held, which reproduces under a seed and still
// leaks - so this checks the whole struct moved.
TEST(all_sixteen_bytes_are_written)
{
    TigGuid guid;
    TigGuid again;

    memset(&guid, 0xCD, sizeof(guid));
    memset(&again, 0xAB, sizeof(again));

    tig_guid_seed_set(31337);
    tig_guid_create(&guid);
    tig_guid_seed_set(31337);
    tig_guid_create(&again);

    // Both were pre-filled with different junk; if any byte were left untouched
    // the two would differ there.
    CHECK(tig_guid_is_equal(&guid, &again));

    guid_end();
}

TEST(is_equal_compares_every_byte)
{
    TigGuid a;
    TigGuid b;
    int index;
    int missed = 0;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    CHECK(tig_guid_is_equal(&a, &b));
    CHECK(tig_guid_is_equal(&a, &a));

    // Flip one byte at a time; every position must be noticed.
    for (index = 0; index < 16; index++) {
        b.data[index] = 1;
        if (tig_guid_is_equal(&a, &b)) {
            missed++;
        }
        b.data[index] = 0;
    }
    CHECK_INT(missed, 0);

    guid_end();
}

void test_guid_suite(void)
{
    RUN(init_and_exit_report_success_and_keep_no_state);
    RUN(every_guid_carries_the_version_four_and_variant_one_bits);
    RUN(the_forced_bits_leave_the_rest_of_their_bytes_alone);
    RUN(the_same_seed_produces_the_same_guids);
    RUN(different_seeds_produce_different_guids);
    RUN(successive_guids_from_one_seed_differ);
    RUN(all_sixteen_bytes_are_written);
    RUN(is_equal_compares_every_byte);
}
