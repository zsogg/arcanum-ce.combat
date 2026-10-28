// `combat music shuffle`: the pure pick behind it. zsogg chose on 2026-10-28
// every area track, never the one the area was playing, never the last pick.

#include "game/gsound.h"

#include "test_harness.h"

TEST(every_roll_skips_the_area_and_the_last_track)
{
    unsigned int roll;
    int track;
    int seen[5] = { 0 };

    for (roll = 0; roll < 30; roll++) {
        track = gsound_combat_shuffle_pick(5, 1, 3, roll);
        CHECK(track != 1);
        CHECK(track != 3);
        CHECK(track >= 0 && track < 5);
        seen[track]++;
    }

    // The three left share the rolls evenly.
    CHECK_INT(seen[0], 10);
    CHECK_INT(seen[2], 10);
    CHECK_INT(seen[4], 10);
}

TEST(no_area_track_skips_only_the_last)
{
    CHECK_INT(gsound_combat_shuffle_pick(3, -1, 0, 0), 1);
    CHECK_INT(gsound_combat_shuffle_pick(3, -1, 0, 1), 2);
    CHECK_INT(gsound_combat_shuffle_pick(3, -1, 0, 2), 1);
}

TEST(the_first_pick_of_a_session_can_be_any_track)
{
    CHECK_INT(gsound_combat_shuffle_pick(3, -1, -1, 0), 0);
    CHECK_INT(gsound_combat_shuffle_pick(3, -1, -1, 2), 2);
}

TEST(the_area_track_as_the_last_is_skipped_once)
{
    CHECK_INT(gsound_combat_shuffle_pick(3, 1, 1, 0), 0);
    CHECK_INT(gsound_combat_shuffle_pick(3, 1, 1, 1), 2);
}

TEST(with_every_track_excluded_the_last_repeats)
{
    CHECK_INT(gsound_combat_shuffle_pick(2, 0, 1, 7), 1);
    CHECK_INT(gsound_combat_shuffle_pick(1, 0, -1, 7), 0);
}

void test_gsound_shuffle_suite(void)
{
    RUN(every_roll_skips_the_area_and_the_last_track);
    RUN(no_area_track_skips_only_the_last);
    RUN(the_first_pick_of_a_session_can_be_any_track);
    RUN(the_area_track_as_the_last_is_skipped_once);
    RUN(with_every_track_excluded_the_last_repeats);
}
