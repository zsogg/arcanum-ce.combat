// Tests for `game/stat.c`.
//
// These are the numbers a character is made of, and they are balance: the
// racial caps, the floors every stat is clamped to, and the character-point
// cost of raising one. A wrong entry here does not crash - it makes an ogre
// cheaper or a gnome weaker than Troika built them, and nothing else in the
// tree would notice.
//
// **Scope, stated plainly.** Most of `stat.c` takes an `int64_t obj` and reads
// object fields, which needs the object system, the prototype tables and a
// loaded map - far past what a unit test should stand up. What *is* reachable
// is the part that does not touch an object at all:
//
//   - `stat_cost`, pure arithmetic over a table;
//   - `stat_level_min`, which takes an `obj` and ignores it entirely;
//   - `stat_level_max`, which accepts `OBJ_HANDLE_NULL` and treats it as human.
//
// That is the whole pure surface, and it happens to be the balance-critical
// part. `stat_level_get`, `stat_base_set` and `stat_level_set` are left for
// whenever the object system becomes testable.
//
// The expected vectors below are **read out of the exe**, not out of `stat.c`,
// so they pin Troika's numbers rather than CE's transcription of them:
// `stat_min_values` is 0x5B5194, `stat_max_values` is 0x5B5204 and
// `stat_cost_tbl` is 0x5B52E4. All three were confirmed byte-identical to CE's
// tables when this suite was written; if a transcription ever drifts, these
// fail. Same method as the `random.c` suite - compute or extract the truth
// outside the codebase first, then compare.

#include "game/obj.h"
#include "game/stat.h"
#include "game/uap.h"

#include "test_harness.h"

// Read from 0x5B5194 in Arcanum.exe, in STAT_* order.
static const int EXE_STAT_MIN[STAT_COUNT] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    300, -50, -9, 1, 0, 1, -65, 1,
    -100, 0, 0, -1000, 0, 0, 0, 0,
    0, 20, 0, 0
};

// Read from 0x5B5204 in Arcanum.exe, in STAT_* order.
static const int EXE_STAT_MAX[STAT_COUNT] = {
    20, 20, 20, 20, 20, 20, 20, 20,
    10000, 50, 95, 100, 6, 20, 200, 7,
    100, 51, 2000000000, 1000, 100, 56, 210, 210,
    1000, 1000, 1, 11
};

TEST(the_uap_switch_is_off_so_these_test_the_faithful_path)
{
    // `uap_fixes` is a plain static that nothing in a test initialises, so it
    // reads false and `stat_level_min`/`stat_level_max` take their original
    // branches. Asserted rather than assumed, because three of the values below
    // are exactly the ones UAP changes - the STAT_AGE floor of 20, the
    // STAT_HEAL_RATE floor of 0, and the STAT_UNSPENT_POINTS cap of 56 - so if
    // the switch were ever on by default in a test build, this suite would be
    // silently checking the wrong numbers.
    CHECK(!uap_fixes_enabled());
}

TEST(stat_min_matches_the_exe_for_every_stat)
{
    int stat;
    int wrong = 0;

    for (stat = 0; stat < STAT_COUNT; stat++) {
        if (stat_level_min(OBJ_HANDLE_NULL, stat) != EXE_STAT_MIN[stat]) {
            wrong++;
            CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, stat), EXE_STAT_MIN[stat]);
        }
    }

    CHECK_INT(wrong, 0);
}

TEST(stat_max_matches_the_exe_for_every_stat_for_a_human)
{
    // `OBJ_HANDLE_NULL` means "no critter", which the function treats as human,
    // and human is the race with no entry in the cap switch - so this is the
    // base table, unmodified.
    int stat;
    int wrong = 0;

    for (stat = 0; stat < STAT_COUNT; stat++) {
        if (stat_level_max(OBJ_HANDLE_NULL, stat) != EXE_STAT_MAX[stat]) {
            wrong++;
            CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, stat), EXE_STAT_MAX[stat]);
        }
    }

    CHECK_INT(wrong, 0);
}

TEST(the_eight_primary_stats_run_one_to_twenty)
{
    // Spelled out separately from the table sweep because it is the rule a
    // reader actually needs, and because a table that was wholesale wrong in
    // both CE and this file would still pass the sweep.
    int stat;

    for (stat = STAT_STRENGTH; stat <= STAT_CHARISMA; stat++) {
        CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, stat), 1);
        CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, stat), 20);
    }
}

TEST(the_stats_uap_would_change_hold_their_original_values)
{
    // With the switch off these are Troika's, and each is a documented UAP
    // site. The heal-rate floor is **0**, which is the whole point of UAP's
    // change - "CN1 doesn't result in heal rate of 0 anymore" - so a floor of 1
    // here would mean the switch was on, or that the port had quietly adopted
    // the fix. Age floors at adulthood; unspent points cap at 56.
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_HEAL_RATE), 0);
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_AGE), 20);
    CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, STAT_UNSPENT_POINTS), 56);
}

TEST(derived_stat_bounds_allow_negatives)
{
    // Several derived stats are signed, and a clamp that assumed unsigned would
    // pin them at zero: damage bonus, AC adjustment, reaction modifier,
    // aptitude and alignment all go below zero.
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_DAMAGE_BONUS), -50);
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_AC_ADJUSTMENT), -9);
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_REACTION_MODIFIER), -65);
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_MAGICK_TECH_APTITUDE), -100);
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_ALIGNMENT), -1000);

    CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, STAT_MAGICK_TECH_APTITUDE), 100);
    CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, STAT_ALIGNMENT), 1000);
}

TEST(the_race_cap_is_one_past_the_last_race_and_the_gender_cap_is_not)
{
    // Gender's cap is exactly its last valid value. Race's is not: there are
    // eleven races (RACE_HUMAN..RACE_ORC, RACE_COUNT == 11) and the cap is 11,
    // one past RACE_ORC.
    //
    // Faithful - 0x5B5204 holds 11 - so it is pinned rather than fixed, but it
    // is worth knowing, because `race_names` is a `RACE_COUNT`-entry array and
    // `race_name` indexes it without a bounds check. A critter whose STAT_RACE
    // reached the cap would read one past that array. Nothing does: character
    // creation picks from the valid races, so this is latent, not live.
    //
    // Written the other way round first, asserting symmetry with the enum, and
    // the test failed - which is the asymmetry announcing itself.
    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_GENDER), 0);
    CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, STAT_GENDER), GENDER_COUNT - 1);

    CHECK_INT(stat_level_min(OBJ_HANDLE_NULL, STAT_RACE), 0);
    CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, STAT_RACE), RACE_COUNT);
    CHECK_INT(stat_level_max(OBJ_HANDLE_NULL, STAT_RACE), 11);
}

TEST(stat_level_min_ignores_the_object_it_is_given)
{
    // The parameter exists for symmetry with `stat_level_max` and is
    // `(void)`-ed in the body. Pinned so nobody assumes a critter can carry a
    // custom floor - if that ever becomes true, this fails and says so.
    int stat;
    int differed = 0;

    for (stat = 0; stat < STAT_COUNT; stat++) {
        if (stat_level_min(OBJ_HANDLE_NULL, stat) != stat_level_min(0x1234567890LL, stat)) {
            differed++;
        }
    }

    CHECK_INT(differed, 0);
}

TEST(stat_cost_is_zero_at_one_and_a_point_thereafter)
{
    // 0x5B52E4 is `{0, 1, 1, ... 1}` - the first level is free and every level
    // after costs exactly one character point. Checked against the exe, because
    // an all-ones table reads like a transcription that lost a curve, and it
    // is not: the original is identical.
    int value;

    CHECK_INT(stat_cost(1), 0);

    for (value = 2; value <= 20; value++) {
        CHECK_INT(stat_cost(value), 1);
    }
}

TEST(stat_cost_clamps_out_of_range_values)
{
    // Below 1 clamps to 1, above 20 clamps to 20, so the table is never indexed
    // out of bounds. This is the only guard between a caller's arithmetic and a
    // 20-entry array.
    CHECK_INT(stat_cost(0), stat_cost(1));
    CHECK_INT(stat_cost(-1), stat_cost(1));
    CHECK_INT(stat_cost(-100000), stat_cost(1));

    CHECK_INT(stat_cost(21), stat_cost(20));
    CHECK_INT(stat_cost(1000), stat_cost(20));
    CHECK_INT(stat_cost(100000), stat_cost(20));
}

void test_stat_suite(void)
{
    RUN(the_uap_switch_is_off_so_these_test_the_faithful_path);
    RUN(stat_min_matches_the_exe_for_every_stat);
    RUN(stat_max_matches_the_exe_for_every_stat_for_a_human);
    RUN(the_eight_primary_stats_run_one_to_twenty);
    RUN(the_stats_uap_would_change_hold_their_original_values);
    RUN(derived_stat_bounds_allow_negatives);
    RUN(the_race_cap_is_one_past_the_last_race_and_the_gender_cap_is_not);
    RUN(stat_level_min_ignores_the_object_it_is_given);
    RUN(stat_cost_is_zero_at_one_and_a_point_thereafter);
    RUN(stat_cost_clamps_out_of_range_values);
}
