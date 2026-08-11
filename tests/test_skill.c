// Tests for `game/skill.c`.
//
// The other half of the character sheet, after `stat.c`: which stat governs
// each skill, how high a skill a given stat allows, what training costs, and
// which training tier a skill level qualifies for. All of it balance, none of
// it guarded by anything that would notice a wrong number.
//
// **Scope, as with stat.c.** `skill.c` is 2776 lines and most of it takes an
// `int64_t obj` - `skill_use`, `skill_invocation_run`, the steal and trap paths
// - which needs the object system, prototypes and a map. What is reachable
// without one is the arithmetic and the lookup tables:
//
//   - `basic_skill_money` / `tech_skill_money`, the training price;
//   - `basic_skill_stat` / `tech_skill_stat`, the governing-stat maps;
//   - `basic_skill_min_stat_level_required` / its tech twin, the stat needed to
//     reach a skill level;
//   - `training_min_skill_level_required`, the tier thresholds.
//
// Every expected value below was read out of Arcanum.exe rather than out of
// `skill.c`, so these pin Troika's balance and not CE's transcription of it:
// `basic_skill_stats` is 0x5B6F04, `tech_skill_stats` 0x5B6F34,
// `skill_level_training_tbl` 0x5B6F44 and `max_skill_level_tbl` 0x5B6FA4. All
// four were confirmed byte-identical to CE's tables when this was written.
// Same method as random.c and stat.c.

#include "game/skill.h"
#include "game/stat.h"

#include "test_harness.h"

// 0x5B6FA4, indexed by stat level - 1: the highest skill level each stat level
// permits. Read from the exe.
static const int EXE_MAX_SKILL_FOR_STAT[20] = {
    3, 3, 3, 3, 3,
    7, 7, 7,
    11, 11, 11,
    15, 15, 15,
    19, 19, 19,
    20, 20, 20
};

// 0x5B6F04, in BASIC_SKILL_* order.
static const int EXE_BASIC_SKILL_STATS[BASIC_SKILL_COUNT] = {
    STAT_DEXTERITY, STAT_DEXTERITY, STAT_DEXTERITY, STAT_DEXTERITY,
    STAT_DEXTERITY, STAT_DEXTERITY, STAT_PERCEPTION, STAT_PERCEPTION,
    STAT_INTELLIGENCE, STAT_WILLPOWER, STAT_INTELLIGENCE, STAT_CHARISMA
};

// 0x5B6F34, in TECH_SKILL_* order.
static const int EXE_TECH_SKILL_STATS[TECH_SKILL_COUNT] = {
    STAT_INTELLIGENCE, STAT_PERCEPTION, STAT_DEXTERITY, STAT_PERCEPTION
};

// 0x5B6F44, in TRAINING_* order.
static const int EXE_TRAINING_THRESHOLDS[TRAINING_COUNT] = { 0, 1, 9, 18 };

TEST(each_basic_skill_is_governed_by_the_stat_the_exe_says)
{
    int bs;
    int wrong = 0;

    for (bs = 0; bs < BASIC_SKILL_COUNT; bs++) {
        if (basic_skill_stat(bs) != EXE_BASIC_SKILL_STATS[bs]) {
            wrong++;
            CHECK_INT(basic_skill_stat(bs), EXE_BASIC_SKILL_STATS[bs]);
        }
    }

    CHECK_INT(wrong, 0);

    // Spelled out for the few a reader is likely to check by hand.
    CHECK_INT(basic_skill_stat(BASIC_SKILL_MELEE), STAT_DEXTERITY);
    CHECK_INT(basic_skill_stat(BASIC_SKILL_HAGGLE), STAT_WILLPOWER);
    CHECK_INT(basic_skill_stat(BASIC_SKILL_PERSUATION), STAT_CHARISMA);
    CHECK_INT(basic_skill_stat(BASIC_SKILL_HEAL), STAT_INTELLIGENCE);
}

TEST(each_tech_skill_is_governed_by_the_stat_the_exe_says)
{
    int ts;
    int wrong = 0;

    for (ts = 0; ts < TECH_SKILL_COUNT; ts++) {
        if (tech_skill_stat(ts) != EXE_TECH_SKILL_STATS[ts]) {
            wrong++;
            CHECK_INT(tech_skill_stat(ts), EXE_TECH_SKILL_STATS[ts]);
        }
    }

    CHECK_INT(wrong, 0);

    // Pick Locks is the one that surprises people - dexterity, not intelligence.
    CHECK_INT(tech_skill_stat(TECH_SKILL_PICK_LOCKS), STAT_DEXTERITY);
    CHECK_INT(tech_skill_stat(TECH_SKILL_REPAIR), STAT_INTELLIGENCE);
}

TEST(training_tiers_open_at_skill_levels_zero_one_nine_and_eighteen)
{
    int training;
    int wrong = 0;

    for (training = 0; training < TRAINING_COUNT; training++) {
        if (training_min_skill_level_required(training) != EXE_TRAINING_THRESHOLDS[training]) {
            wrong++;
            CHECK_INT(training_min_skill_level_required(training),
                EXE_TRAINING_THRESHOLDS[training]);
        }
    }

    CHECK_INT(wrong, 0);

    CHECK_INT(training_min_skill_level_required(TRAINING_NONE), 0);
    CHECK_INT(training_min_skill_level_required(TRAINING_APPRENTICE), 1);
    CHECK_INT(training_min_skill_level_required(TRAINING_EXPERT), 9);
    CHECK_INT(training_min_skill_level_required(TRAINING_MASTER), 18);
}

TEST(the_stat_required_for_a_skill_level_follows_the_exe_curve)
{
    // `min_stat_level_required` is the inverse of 0x5B6FA4: the lowest stat
    // level whose cap reaches the wanted skill level. Derived here from the
    // exe's table rather than restated, so the expectation cannot drift from
    // its source.
    int skill_level;
    int stat_level;
    int expected;
    int wrong = 0;

    for (skill_level = 1; skill_level <= 20; skill_level++) {
        expected = 0;
        for (stat_level = 1; stat_level <= 20; stat_level++) {
            if (EXE_MAX_SKILL_FOR_STAT[stat_level - 1] >= skill_level) {
                expected = stat_level;
                break;
            }
        }

        if (basic_skill_min_stat_level_required(skill_level) != expected) {
            wrong++;
            CHECK_INT(basic_skill_min_stat_level_required(skill_level), expected);
        }
    }

    CHECK_INT(wrong, 0);

    // The rungs of that curve, written out - this is the rule a player sees on
    // the character sheet.
    CHECK_INT(basic_skill_min_stat_level_required(1), 1);
    CHECK_INT(basic_skill_min_stat_level_required(3), 1);
    CHECK_INT(basic_skill_min_stat_level_required(4), 6);
    CHECK_INT(basic_skill_min_stat_level_required(7), 6);
    CHECK_INT(basic_skill_min_stat_level_required(8), 9);
    CHECK_INT(basic_skill_min_stat_level_required(11), 9);
    CHECK_INT(basic_skill_min_stat_level_required(12), 12);
    CHECK_INT(basic_skill_min_stat_level_required(15), 12);
    CHECK_INT(basic_skill_min_stat_level_required(16), 15);
    CHECK_INT(basic_skill_min_stat_level_required(19), 15);
    CHECK_INT(basic_skill_min_stat_level_required(20), 18);
}

TEST(an_unreachable_skill_level_requires_no_stat_at_all)
{
    // Past 20 no stat level qualifies, and the loop falls out returning 0 -
    // which reads as "no requirement" rather than "impossible". Faithful, and
    // worth pinning because a caller treating 0 as satisfiable would let a
    // skill past its cap.
    CHECK_INT(basic_skill_min_stat_level_required(21), 0);
    CHECK_INT(basic_skill_min_stat_level_required(1000), 0);
    CHECK_INT(tech_skill_min_stat_level_required(21), 0);
}

TEST(the_basic_and_tech_stat_requirement_functions_agree)
{
    // Two separate functions at 0x4C64E0 and 0x4C6B20, and both are the same
    // loop over the same table - neither takes a skill id. Troika duplicated
    // them; CE faithfully has two copies. Pinned so they cannot drift apart.
    int skill_level;
    int differed = 0;

    for (skill_level = 0; skill_level <= 25; skill_level++) {
        if (basic_skill_min_stat_level_required(skill_level)
            != tech_skill_min_stat_level_required(skill_level)) {
            differed++;
        }
    }

    CHECK_INT(differed, 0);
}

TEST(training_price_is_training_plus_one_times_skill_level_plus_two)
{
    // 0x4C62D0 is six instructions: `(skill_level + 2) * (training + 1)`.
    int skill_level;
    int training;
    int wrong = 0;

    for (training = 0; training < TRAINING_COUNT; training++) {
        for (skill_level = 0; skill_level <= 20; skill_level++) {
            if (basic_skill_money(BASIC_SKILL_MELEE, skill_level, training)
                != (training + 1) * (skill_level + 2)) {
                wrong++;
            }
        }
    }

    CHECK_INT(wrong, 0);

    // A few concrete prices, so the formula is legible without evaluating it.
    CHECK_INT(basic_skill_money(BASIC_SKILL_MELEE, 0, TRAINING_NONE), 2);
    CHECK_INT(basic_skill_money(BASIC_SKILL_MELEE, 1, TRAINING_APPRENTICE), 6);
    CHECK_INT(basic_skill_money(BASIC_SKILL_MELEE, 9, TRAINING_EXPERT), 33);
    CHECK_INT(basic_skill_money(BASIC_SKILL_MELEE, 18, TRAINING_MASTER), 80);
}

TEST(training_price_ignores_which_skill_is_being_trained)
{
    // The skill id is `(void)`-ed in both. Every skill costs the same to train
    // at a given level and tier - pinned because a price table per skill is the
    // obvious thing someone would later add, and that has to be a deliberate
    // change.
    int bs;
    int ts;
    int expected = basic_skill_money(0, 5, TRAINING_EXPERT);
    int wrong = 0;

    for (bs = 0; bs < BASIC_SKILL_COUNT; bs++) {
        if (basic_skill_money(bs, 5, TRAINING_EXPERT) != expected) {
            wrong++;
        }
    }

    for (ts = 0; ts < TECH_SKILL_COUNT; ts++) {
        if (tech_skill_money(ts, 5, TRAINING_EXPERT) != expected) {
            wrong++;
        }
    }

    CHECK_INT(wrong, 0);
}

TEST(basic_and_tech_training_prices_are_the_same_function)
{
    // 0x4C62D0 and 0x4C69E0 are byte-identical in the exe. Same story as the
    // stat-requirement pair above.
    int skill_level;
    int training;
    int differed = 0;

    for (training = 0; training < TRAINING_COUNT; training++) {
        for (skill_level = 0; skill_level <= 20; skill_level++) {
            if (basic_skill_money(BASIC_SKILL_MELEE, skill_level, training)
                != tech_skill_money(TECH_SKILL_REPAIR, skill_level, training)) {
                differed++;
            }
        }
    }

    CHECK_INT(differed, 0);
}

TEST(training_price_rises_with_both_level_and_tier)
{
    // Monotonic in each argument - the property the formula exists to have, and
    // the one a future price table would still need.
    int skill_level;
    int training;
    int not_rising = 0;

    for (training = 0; training < TRAINING_COUNT; training++) {
        for (skill_level = 0; skill_level < 20; skill_level++) {
            if (basic_skill_money(0, skill_level + 1, training)
                <= basic_skill_money(0, skill_level, training)) {
                not_rising++;
            }
        }
    }

    for (skill_level = 0; skill_level <= 20; skill_level++) {
        for (training = 0; training < TRAINING_COUNT - 1; training++) {
            if (basic_skill_money(0, skill_level, training + 1)
                <= basic_skill_money(0, skill_level, training)) {
                not_rising++;
            }
        }
    }

    CHECK_INT(not_rising, 0);
}

void test_skill_suite(void)
{
    RUN(each_basic_skill_is_governed_by_the_stat_the_exe_says);
    RUN(each_tech_skill_is_governed_by_the_stat_the_exe_says);
    RUN(training_tiers_open_at_skill_levels_zero_one_nine_and_eighteen);
    RUN(the_stat_required_for_a_skill_level_follows_the_exe_curve);
    RUN(an_unreachable_skill_level_requires_no_stat_at_all);
    RUN(the_basic_and_tech_stat_requirement_functions_agree);
    RUN(training_price_is_training_plus_one_times_skill_level_plus_two);
    RUN(training_price_ignores_which_skill_is_being_trained);
    RUN(basic_and_tech_training_prices_are_the_same_function);
    RUN(training_price_rises_with_both_level_and_tier);
}
