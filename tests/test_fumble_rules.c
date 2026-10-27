// Melee, bow and thrown fumbles (card FU1, 2026-10-27): the outcome table and
// how training changes it, pure. The engine side - the fall, the drop, the
// self-hit, the grenade's flight - is exercised on the fumble bench.

#include "game/fumble_rules.h"
#include "game/skill.h"

#include "test_harness.h"

TEST(a_fumble_is_half_stumbles_and_rarely_a_critical_self_hit)
{
    int roll;
    int counts[5] = { 0 };

    // zsogg's split: 50 stumble, 30 self-hit, 15 drop, 5 critical self-hit.
    for (roll = 1; roll <= 100; roll++) {
        counts[fumble_rules_outcome(roll, TRAINING_NONE, true)]++;
    }
    CHECK_INT(counts[FUMBLE_STUMBLE], 50);
    CHECK_INT(counts[FUMBLE_SELF_HIT], 30);
    CHECK_INT(counts[FUMBLE_DROP], 15);
    CHECK_INT(counts[FUMBLE_CRITICAL_SELF_HIT], 5);
    CHECK_INT(counts[FUMBLE_NONE], 0);

    // The boundaries.
    CHECK_INT(fumble_rules_outcome(50, TRAINING_NONE, true), FUMBLE_STUMBLE);
    CHECK_INT(fumble_rules_outcome(51, TRAINING_NONE, true), FUMBLE_SELF_HIT);
    CHECK_INT(fumble_rules_outcome(80, TRAINING_APPRENTICE, true), FUMBLE_SELF_HIT);
    CHECK_INT(fumble_rules_outcome(81, TRAINING_APPRENTICE, true), FUMBLE_DROP);
    CHECK_INT(fumble_rules_outcome(95, TRAINING_NONE, true), FUMBLE_DROP);
    CHECK_INT(fumble_rules_outcome(96, TRAINING_NONE, true), FUMBLE_CRITICAL_SELF_HIT);
}

TEST(bare_hands_have_nothing_to_drop_and_stumble_instead)
{
    CHECK_INT(fumble_rules_outcome(90, TRAINING_NONE, false), FUMBLE_STUMBLE);
    CHECK_INT(fumble_rules_outcome(60, TRAINING_NONE, false), FUMBLE_SELF_HIT);
    CHECK_INT(fumble_rules_outcome(99, TRAINING_NONE, false), FUMBLE_CRITICAL_SELF_HIT);
}

TEST(an_expert_never_hits_themselves_and_a_master_never_fumbles)
{
    int roll;

    for (roll = 1; roll <= 100; roll++) {
        FumbleOutcome expert = fumble_rules_outcome(roll, TRAINING_EXPERT, true);
        CHECK(expert != FUMBLE_SELF_HIT && expert != FUMBLE_CRITICAL_SELF_HIT);
        CHECK_INT(fumble_rules_outcome(roll, TRAINING_MASTER, true), FUMBLE_NONE);
    }
    // An Expert still drops.
    CHECK_INT(fumble_rules_outcome(90, TRAINING_EXPERT, true), FUMBLE_DROP);
    CHECK_INT(fumble_rules_outcome(60, TRAINING_EXPERT, true), FUMBLE_STUMBLE);
    CHECK_INT(fumble_rules_outcome(99, TRAINING_EXPERT, true), FUMBLE_STUMBLE);
}

TEST(each_fumble_outcome_has_its_name)
{
    CHECK_STR(fumble_rules_outcome_name(FUMBLE_STUMBLE), "stumble");
    CHECK_STR(fumble_rules_outcome_name(FUMBLE_SELF_HIT), "self_hit");
    CHECK_STR(fumble_rules_outcome_name(FUMBLE_DROP), "drop");
    CHECK_STR(fumble_rules_outcome_name(FUMBLE_CRITICAL_SELF_HIT), "critical_self_hit");
    CHECK_STR(fumble_rules_outcome_name(FUMBLE_NONE), "none");
}

void test_fumble_rules_suite(void)
{
    RUN(a_fumble_is_half_stumbles_and_rarely_a_critical_self_hit);
    RUN(bare_hands_have_nothing_to_drop_and_stumble_instead);
    RUN(an_expert_never_hits_themselves_and_a_master_never_fumbles);
    RUN(each_fumble_outcome_has_its_name);
}
