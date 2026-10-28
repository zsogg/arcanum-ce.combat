// Combat AI milestone AI-6 F2: the pure parts of the leash and of
// `follower conduct`. zsogg chose on 2026-10-28 that under `ai follower leash`
// an ordered target sticks and a follower on Wait Here guards its own post,
// and that a follower who will not defend the player says so once per
// attacker per fight.

#include "game/ai.h"

#include "test_harness.h"

TEST(stock_measures_twenty_from_the_leader_ordered_or_not)
{
    CHECK(!ai_follower_leash_refuses(false, false, false, 20, 30, 20));
    CHECK(ai_follower_leash_refuses(false, false, false, 21, 1, 20));
    // Waiting and ordered change nothing without the leash.
    CHECK(ai_follower_leash_refuses(false, true, true, 21, 1, 20));
}

TEST(the_leash_keeps_its_eight_tiles_from_the_leader)
{
    CHECK(!ai_follower_leash_refuses(true, false, false, 8, 12, AI_FOLLOWER_LEASH));
    CHECK(ai_follower_leash_refuses(true, false, false, 9, 2, AI_FOLLOWER_LEASH));
}

TEST(an_ordered_target_is_never_dropped_by_the_leash)
{
    CHECK(!ai_follower_leash_refuses(true, false, true, 15, 15, AI_FOLLOWER_LEASH));
    CHECK(!ai_follower_leash_refuses(true, true, true, 15, 15, AI_FOLLOWER_LEASH));
}

TEST(a_waiting_follower_counts_from_itself)
{
    // Ten tiles from the player, beside the follower: in reach.
    CHECK(!ai_follower_leash_refuses(true, true, false, 10, 1, AI_FOLLOWER_LEASH));
    // Beside the player, nine tiles from the follower's post: not.
    CHECK(ai_follower_leash_refuses(true, true, false, 1, 9, AI_FOLLOWER_LEASH));
}

TEST(a_refusal_is_said_once_per_attacker_per_fight)
{
    ai_conduct_refusals_forget();
    CHECK(ai_conduct_refusal_first(1, 100));
    CHECK(!ai_conduct_refusal_first(1, 100));
    // Another attacker, or another follower: said again.
    CHECK(ai_conduct_refusal_first(1, 101));
    CHECK(ai_conduct_refusal_first(2, 100));
    ai_conduct_refusals_forget();
    CHECK(ai_conduct_refusal_first(1, 100));
    ai_conduct_refusals_forget();
}

TEST(a_full_memory_forgets_its_oldest_refusal)
{
    int64_t attacker;

    ai_conduct_refusals_forget();
    for (attacker = 0; attacker < 32; attacker++) {
        CHECK(ai_conduct_refusal_first(1, 1000 + attacker));
    }
    CHECK(ai_conduct_refusal_first(1, 2000));
    CHECK(ai_conduct_refusal_first(1, 1000));
    CHECK(!ai_conduct_refusal_first(1, 1031));
    ai_conduct_refusals_forget();
}

void test_ai_follower_conduct_suite(void)
{
    RUN(stock_measures_twenty_from_the_leader_ordered_or_not);
    RUN(the_leash_keeps_its_eight_tiles_from_the_leader);
    RUN(an_ordered_target_is_never_dropped_by_the_leash);
    RUN(a_waiting_follower_counts_from_itself);
    RUN(a_refusal_is_said_once_per_attacker_per_fight);
    RUN(a_full_memory_forgets_its_oldest_refusal);
}
