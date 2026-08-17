// Tests for the pure threshold at the bottom of `game/reaction.c`.
//
// `reaction_translate` maps a 0..100+ reaction value onto the seven named
// reaction bands that dialogue, barter and AI all key off. The rest of
// reaction.c is object/effect/AI state and belongs to the object-harness
// layer; this function has no dependencies and is load-bearing enough to pin
// on its own.

#include "game/reaction.h"

#include "test_harness.h"

TEST(bands_follow_the_documented_thresholds)
{
    CHECK_INT(reaction_translate(0), REACTION_HATRED);
    CHECK_INT(reaction_translate(1), REACTION_DISLIKE);
    CHECK_INT(reaction_translate(20), REACTION_DISLIKE);
    CHECK_INT(reaction_translate(21), REACTION_SUSPICIOUS);
    CHECK_INT(reaction_translate(40), REACTION_SUSPICIOUS);
    CHECK_INT(reaction_translate(41), REACTION_NEUTRAL);
    CHECK_INT(reaction_translate(60), REACTION_NEUTRAL);
    CHECK_INT(reaction_translate(61), REACTION_COURTEOUS);
    CHECK_INT(reaction_translate(80), REACTION_COURTEOUS);
    CHECK_INT(reaction_translate(81), REACTION_AMIABLE);
    CHECK_INT(reaction_translate(100), REACTION_AMIABLE);
    CHECK_INT(reaction_translate(101), REACTION_LOVE);
}

TEST(negative_values_are_hatred)
{
    CHECK_INT(reaction_translate(-1), REACTION_HATRED);
    CHECK_INT(reaction_translate(-1000), REACTION_HATRED);
}

void test_reaction_suite(void)
{
    RUN(bands_follow_the_documented_thresholds);
    RUN(negative_values_are_hatred);
}
