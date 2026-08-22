// Tests for the parts of `tig/art.c` that pack and unpack art ids.
//
// These exist because of porting bug #32. The emulation differential
// (`arcanum-work/harness/emudiff`) is what found it and is the stronger
// check - it compares against the original machine code over tens of
// thousands of inputs - but it is *not* part of `gate.sh`, because it needs
// Unicorn and a copy of `Arcanum.exe`. So nothing that runs on every commit
// protected the fix.
//
// The behaviour pinned here is deliberately the surprising half: what
// `tig_art_portal_id_create` does with the `palette` argument it is given.
// The obvious "fix" for a future reader is to delete the rotation_set call
// as redundant, or to stop it clobbering a caller's palette. Both would
// re-break the port. These tests fail if either happens.
//
// Everything here is pure: no art database, no video, no init. The one
// function in the chain that could have needed state, `sub_504790`, only
// looks at the type field.

#include "tig/art.h"

#include "test_harness.h"

// The portal constructor's parameter order, kept here so the tests read as
// the calls do: (num, type, a3, frame, rotation, palette, out).
static tig_art_id_t portal(unsigned int num, int type, int a3,
    unsigned int frame, int rotation, unsigned int palette)
{
    tig_art_id_t art_id = 0;

    CHECK(tig_art_portal_id_create(num, type, a3, frame, rotation, palette,
              &art_id)
        == TIG_OK);
    return art_id;
}

TEST(portal_discards_the_palette_it_is_given)
{
    // The original ends with tig_art_id_rotation_set, whose portal branch
    // overwrites flags and palette from the rotation's cardinal direction.
    // A rotation of 0 means cardinal direction 0, so both are cleared - the
    // palette 3 asked for here does not survive.
    //
    // Measured against the original at 0x5041D0: CE returned 0x30000030
    // before the fix where the original returns 0x30000000.
    tig_art_id_t art_id = portal(0, 0, 0, 0, 0, 3);

    CHECK_INT(tig_art_id_palette_get(art_id), 0);
    CHECK_INT(tig_art_id_flags_get(art_id), 0);
    CHECK_INT(art_id, 0x30000000);
}

TEST(portal_sets_flags_and_palette_for_the_odd_cardinal_directions)
{
    // rotation / 2 of 1 or 3 - that is rotations 2, 3, 6 and 7 - sets both
    // to 1 instead. Rotation 7 measured against the original: 0x30003811.
    int rotation;

    for (rotation = 0; rotation < 8; rotation++) {
        int expected = (rotation / 2 == 1 || rotation / 2 == 3) ? 1 : 0;
        tig_art_id_t art_id = portal(0, 0, 0, 0, rotation, 0);

        CHECK_INT(tig_art_id_palette_get(art_id), expected);
        CHECK_INT(tig_art_id_flags_get(art_id), expected);
    }

    CHECK_INT(portal(0, 0, 0, 0, 7, 0), 0x30003811);
}

TEST(portal_keeps_the_rotation_it_was_given)
{
    // The clobbering is confined to flags and palette: the rotation field
    // itself still round-trips, which is what stops an over-eager reading of
    // the two tests above from concluding the call is destructive.
    int rotation;

    for (rotation = 0; rotation < 8; rotation++) {
        CHECK_INT(tig_art_id_rotation_get(portal(3, 1, 0, 5, rotation, 0)),
            rotation);
    }
}

TEST(portal_round_trips_its_other_fields)
{
    tig_art_id_t art_id = portal(511, 1, 0x200, 31, 0, 0);

    CHECK_INT(tig_art_type(art_id), TIG_ART_TYPE_PORTAL);
    CHECK_INT(tig_art_num_get(art_id), 511);
    CHECK_INT(tig_art_portal_id_type_get(art_id), 1);
    CHECK_INT(tig_art_id_frame_get(art_id), 31);
}

TEST(portal_rejects_out_of_range_fields_without_writing)
{
    // A rejected call must leave the out-param alone. The emudiff cases lean
    // on this: they seed the pointer and compare it afterwards, so a side
    // that wrongly writes is caught.
    tig_art_id_t art_id = 0xDEADBEEF;

    CHECK(tig_art_portal_id_create(512, 0, 0, 0, 0, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 2, 0, 0, 0, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 1, 0, 0, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 0, 32, 0, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 0, 0, 8, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 0, 0, 0, 4, &art_id) != TIG_OK);
    CHECK_INT(art_id, 0xDEADBEEF);
}

TEST(portal_a3_is_a_mask_not_a_range)
{
    // `(a3 & ~0x200u) != 0` rejects, so the only valid values are 0 and
    // 0x200. This is what the emudiff case's MaskFieldArg encodes; the old
    // domain drew 0..3 and could only ever produce one of them.
    tig_art_id_t art_id = 0;

    CHECK(tig_art_portal_id_create(0, 0, 0x200, 0, 0, 0, &art_id) == TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 0x201, 0, 0, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 1, 0, 0, 0, &art_id) != TIG_OK);
    CHECK(tig_art_portal_id_create(0, 0, 0x400, 0, 0, 0, &art_id) != TIG_OK);
}

TEST(eye_candy_accepts_type_three_and_aliases_negatives_onto_it)
{
    // Settled by emudiff 2026-08-21: the original bounds at 4, not at
    // TIG_ART_EYE_CANDY_TYPE_COUNT, and the compare is signed so a negative
    // type is accepted and the 2-bit mask folds it onto 3. Both faithful.
    // Pinned here so the old FIXME's suggested "fix" cannot be applied by
    // someone who does not have Unicorn to hand.
    tig_art_id_t three = 0;
    tig_art_id_t negative = 0;
    tig_art_id_t rejected = 0xDEADBEEF;

    CHECK(tig_art_eye_candy_id_create(7, 3, 2, 1, 3, 2, 4, &three) == TIG_OK);
    CHECK(tig_art_eye_candy_id_create(7, 3, 2, 1, -1, 2, 4, &negative)
        == TIG_OK);
    CHECK_INT(negative, three);

    CHECK(tig_art_eye_candy_id_create(7, 3, 2, 1, 4, 2, 4, &rejected)
        != TIG_OK);
    CHECK_INT(rejected, 0xDEADBEEF);
}

void test_art_id_suite(void)
{
    RUN(portal_discards_the_palette_it_is_given);
    RUN(portal_sets_flags_and_palette_for_the_odd_cardinal_directions);
    RUN(portal_keeps_the_rotation_it_was_given);
    RUN(portal_round_trips_its_other_fields);
    RUN(portal_rejects_out_of_range_fields_without_writing);
    RUN(portal_a3_is_a_mask_not_a_range);

    RUN(eye_candy_accepts_type_three_and_aliases_negatives_onto_it);
}
