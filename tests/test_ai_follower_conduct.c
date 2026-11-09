// Combat AI milestone AI-6 F2: the pure parts of the leash and of
// `follower conduct`. zsogg chose on 2026-10-28 that under `ai follower leash`
// an ordered target sticks and a follower on Wait Here guards its own post,
// and that a follower who will not defend the player says so once per
// attacker per fight.

#include "game/ai.h"
#include "game/critter.h"
#include "game/item.h"
#include "game/obj.h"
#include "game/reload.h"

#include "obj_harness.h"
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

// F3 (8a): a reaching weapon must be one its wielder is trained with.
TEST(stock_takes_any_weapon_that_reaches)
{
    CHECK(item_reaching_weapon_wins(false, -40));
    CHECK(item_reaching_weapon_wins(false, 900));
}

TEST(armor_aware_takes_a_reaching_weapon_only_with_a_positive_score)
{
    CHECK(item_reaching_weapon_wins(true, 1));
    CHECK(!item_reaching_weapon_wins(true, 0));
    CHECK(!item_reaching_weapon_wins(true, -40));
}

// Quirk 57, under `ai armor aware`: in turn-based an NPC's weapon must fit in
// a turn. A follower keeping what it holds is held to it too (review
// 2026-11-08, weapons-1: one handed a rifle it could not afford never fired).
TEST(a_weapon_must_fit_in_a_turn_based_turn_under_armor_aware)
{
    CHECK(item_weapon_affordable(true, true, 6, 6));
    CHECK(!item_weapon_affordable(true, true, 7, 6));
    CHECK(item_weapon_affordable(true, false, 7, 6)); // real time: no turn
    CHECK(item_weapon_affordable(false, true, 7, 6)); // stock
}

// The guns are only remembered, never read, so plain numbers stand in for them.
static int64_t follower_of(int64_t pc)
{
    int64_t npc = obj_harness_create_proto(OBJ_TYPE_NPC);

    critter_leader_set(npc, pc);
    return npc;
}

// F3 (8b-i): the gun a melee backup replaced, remembered per follower.
TEST(a_backup_remembers_the_gun_it_replaced)
{
    int64_t pc = obj_harness_create_proto(OBJ_TYPE_PC);
    int64_t a = follower_of(pc);
    int64_t b = follower_of(pc);

    CHECK(reload_backup_gun(a) == OBJ_HANDLE_NULL);
    reload_backup_note(a, 70);
    reload_backup_note(b, 80);
    CHECK(reload_backup_gun(a) == 70);
    CHECK(reload_backup_gun(b) == 80);
    // A second backup for the same critter replaces the first.
    reload_backup_note(a, 71);
    CHECK(reload_backup_gun(a) == 71);
    reload_backup_forget(a);
    CHECK(reload_backup_gun(a) == OBJ_HANDLE_NULL);
    CHECK(reload_backup_gun(b) == 80);
    reload_backup_forget(b);
    obj_harness_destroy(a);
    obj_harness_destroy(b);
    obj_harness_destroy(pc);
}

// Review 2026-10-31, finding 3: every NPC that swapped to a backup was noted
// and only followers were forgotten, so 16 enemy gunmen filled the memory
// for good and a follower's gun was no longer given back.
TEST(enemies_never_fill_the_backup_memory)
{
    int64_t pc = obj_harness_create_proto(OBJ_TYPE_PC);
    int64_t follower = follower_of(pc);
    int64_t enemy;
    int index;

    for (index = 0; index < 20; index++) {
        enemy = obj_harness_create_proto(OBJ_TYPE_NPC);
        reload_backup_note(enemy, 90);
        CHECK(reload_backup_gun(enemy) == OBJ_HANDLE_NULL);
        obj_harness_destroy(enemy);
    }
    reload_backup_note(follower, 70);
    CHECK(reload_backup_gun(follower) == 70);
    reload_backup_forget(follower);
    obj_harness_destroy(follower);
    obj_harness_destroy(pc);
}

TEST(a_follower_that_left_frees_its_slot)
{
    int64_t pc = obj_harness_create_proto(OBJ_TYPE_PC);
    int64_t gone[16];
    int64_t newcomer;
    int index;

    for (index = 0; index < 16; index++) {
        gone[index] = follower_of(pc);
        reload_backup_note(gone[index], 90 + index);
        critter_leader_set(gone[index], OBJ_HANDLE_NULL);
    }
    newcomer = follower_of(pc);
    reload_backup_note(newcomer, 70);
    CHECK(reload_backup_gun(newcomer) == 70);

    reload_backup_forget(newcomer);
    obj_harness_destroy(newcomer);
    for (index = 0; index < 16; index++) {
        reload_backup_forget(gone[index]);
        obj_harness_destroy(gone[index]);
    }
    obj_harness_destroy(pc);
}

void test_ai_follower_conduct_suite(void)
{
    RUN(stock_measures_twenty_from_the_leader_ordered_or_not);
    RUN(the_leash_keeps_its_eight_tiles_from_the_leader);
    RUN(an_ordered_target_is_never_dropped_by_the_leash);
    RUN(a_waiting_follower_counts_from_itself);
    RUN(a_refusal_is_said_once_per_attacker_per_fight);
    RUN(a_full_memory_forgets_its_oldest_refusal);
    RUN(stock_takes_any_weapon_that_reaches);
    RUN(armor_aware_takes_a_reaching_weapon_only_with_a_positive_score);
    CHECK(obj_harness_init());
    RUN(a_weapon_must_fit_in_a_turn_based_turn_under_armor_aware);
    RUN(a_backup_remembers_the_gun_it_replaced);
    RUN(enemies_never_fill_the_backup_memory);
    RUN(a_follower_that_left_frees_its_slot);
}
