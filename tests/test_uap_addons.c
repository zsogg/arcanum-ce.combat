// UAP's four optional engine add-ons (release 1, R1, 2026-10-13): the one
// with a rule worth pinning, Master Educator's follower training, pure. The
// cases are a step-by-step trace of the original's and the patched exe's
// basic-skill loop in `background_educate_followers` (0x4C29AE-0x4C29E6).
// The other three - the level cap, no XP per hit, no combat music - are each
// a switch at one or two sites, exercised in scripted runs.

#include "game/background.h"
#include "game/skill.h"

#include "test_harness.h"

static int levels_of(int educator, int follower, bool uap, int* levels)
{
    return background_educate_levels(educator, follower, uap, levels, TRAINING_COUNT + 1);
}

TEST(the_original_trains_followers_to_one_below_the_educator)
{
    int levels[TRAINING_COUNT + 1];

    // A Master (3) trains an untrained follower (0) through 1 and 2.
    CHECK_INT(levels_of(TRAINING_MASTER, TRAINING_NONE, false, levels), 2);
    CHECK_INT(levels[0], TRAINING_APPRENTICE);
    CHECK_INT(levels[1], TRAINING_EXPERT);
    // An Expert raises an untrained follower to Apprentice only.
    CHECK_INT(levels_of(TRAINING_EXPERT, TRAINING_NONE, false, levels), 1);
    CHECK_INT(levels[0], TRAINING_APPRENTICE);
    // Nothing at Apprentice or below, nor for a follower already one below.
    CHECK_INT(levels_of(TRAINING_APPRENTICE, TRAINING_NONE, false, levels), 0);
    CHECK_INT(levels_of(TRAINING_MASTER, TRAINING_EXPERT, false, levels), 0);
    CHECK_INT(levels_of(TRAINING_MASTER, TRAINING_MASTER, false, levels), 0);
}

TEST(uap_master_educator_raises_only_a_follower_one_below_to_equal)
{
    int levels[TRAINING_COUNT + 1];

    // One below: its own level is set again, then the educator's.
    CHECK_INT(levels_of(TRAINING_MASTER, TRAINING_EXPERT, true, levels), 2);
    CHECK_INT(levels[0], TRAINING_EXPERT);
    CHECK_INT(levels[1], TRAINING_MASTER);
    // An Apprentice educator now teaches an untrained follower.
    CHECK_INT(levels_of(TRAINING_APPRENTICE, TRAINING_NONE, true, levels), 2);
    CHECK_INT(levels[1], TRAINING_APPRENTICE);
    // Two or more below: the follower's own level, set again, and no more.
    CHECK_INT(levels_of(TRAINING_MASTER, TRAINING_NONE, true, levels), 1);
    CHECK_INT(levels[0], TRAINING_NONE);
    CHECK_INT(levels_of(TRAINING_MASTER, TRAINING_APPRENTICE, true, levels), 1);
    CHECK_INT(levels[0], TRAINING_APPRENTICE);
    // Equal: nothing. Untrained educator: nothing.
    CHECK_INT(levels_of(TRAINING_EXPERT, TRAINING_EXPERT, true, levels), 0);
    CHECK_INT(levels_of(TRAINING_NONE, TRAINING_NONE, true, levels), 0);
    // A follower above the educator: its own level, unchanged.
    CHECK_INT(levels_of(TRAINING_APPRENTICE, TRAINING_MASTER, true, levels), 1);
    CHECK_INT(levels[0], TRAINING_MASTER);
}

void test_uap_addons_suite(void)
{
    RUN(the_original_trains_followers_to_one_below_the_educator);
    RUN(uap_master_educator_raises_only_a_follower_one_below_to_equal);
}
