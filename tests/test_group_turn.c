// Simultaneous Turns (card TB1; zsogg, 2026-11-05): the group turn's rules -
// which critters a group pulls up behind its first, when a member is
// finished, the stall rule - and the pools: a member pays from its own
// action points, and everyone else's actions stay free, as anyone's whose
// turn it is not are in stock. The group over a live fight is measured on the
// observatory's bench (`mods/simultaneous-turns/bench`).
#include <string.h>

#include "game/combat.h"
#include "game/feature.h"
#include "game/object_node.h"
#include "test_harness.h"

// Globalized out of combat.c by the build; in no header.
extern int combat_turn_based_active;
extern ObjectNode* combat_whos_turn_node;
extern int combat_action_points;
extern int combat_required_action_points;
extern int combat_group_count;
extern int combat_round_acted_count;
bool combat_round_has_acted(int64_t obj);
void combat_round_mark_acted(int64_t obj);

// Handles for the list surgery: plain numbers, never looked up.
#define LEADER 100
#define NPC(n) (200 + (n))
#define PARTY(n) (300 + (n))

static bool joins_npc(int64_t obj)
{
    return obj >= 200 && obj < 300;
}

static void chain(ObjectNode* nodes, const int64_t* objs, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        nodes[i].obj = objs[i];
        nodes[i].next = i + 1 < count ? &nodes[i + 1] : NULL;
    }
}

static int walk(ObjectNode* head, int64_t* out, int cap)
{
    int count = 0;

    for (; head != NULL && count < cap; head = head->next) {
        out[count++] = head->obj;
    }
    return head == NULL ? count : -1; // -1: longer than cap, or a cycle
}

TEST(later_npcs_are_pulled_up_behind_the_first_in_order)
{
    // zsogg's mixed example: a wolf first, Virgil, a wolf, the player, wolves.
    const int64_t before[7] = { LEADER, PARTY(1), NPC(1), PARTY(2), NPC(2), NPC(3), PARTY(3) };
    const int64_t expected[7] = { LEADER, NPC(1), NPC(2), NPC(3), PARTY(1), PARTY(2), PARTY(3) };
    int64_t after[16];
    ObjectNode nodes[7];
    ObjectNode* tail;
    int i;

    chain(nodes, before, 7);
    tail = combat_group_pull_forward(&nodes[0], joins_npc);

    CHECK_INT(walk(&nodes[0], after, 16), 7);
    for (i = 0; i < 7; i++) {
        CHECK(after[i] == expected[i]);
    }
    CHECK(tail->obj == NPC(3));
}

TEST(a_run_already_together_is_left_as_it_is)
{
    const int64_t before[5] = { LEADER, NPC(1), NPC(2), PARTY(1), PARTY(2) };
    int64_t after[16];
    ObjectNode nodes[5];
    ObjectNode* tail;
    int i;

    chain(nodes, before, 5);
    tail = combat_group_pull_forward(&nodes[0], joins_npc);

    CHECK_INT(walk(&nodes[0], after, 16), 5);
    for (i = 0; i < 5; i++) {
        CHECK(after[i] == before[i]);
    }
    CHECK(tail->obj == NPC(2));
}

TEST(a_lone_npc_is_a_group_of_one)
{
    const int64_t before[3] = { LEADER, PARTY(1), PARTY(2) };
    int64_t after[16];
    ObjectNode nodes[3];

    chain(nodes, before, 3);
    CHECK(combat_group_pull_forward(&nodes[0], joins_npc) == &nodes[0]);
    CHECK_INT(walk(&nodes[0], after, 16), 3);
    CHECK(after[1] == PARTY(1));
    CHECK(after[2] == PARTY(2));
}

TEST(a_member_is_finished_when_spent_and_idle_or_gone)
{
    CHECK(combat_group_member_finished(false, 0, true));
    CHECK(!combat_group_member_finished(false, 0, false)); // its last swing still falls
    CHECK(!combat_group_member_finished(false, 3, true)); // points left
    CHECK(combat_group_member_finished(true, 5, false)); // dead or gone
}

TEST(standing_idle_twice_on_the_same_points_is_a_stall)
{
    bool seen = false;
    int last = 0;

    CHECK(!combat_group_stall_step(6, true, &seen, &last)); // first look
    CHECK(combat_group_stall_step(6, true, &seen, &last)); // nothing spent since
    CHECK(!combat_group_stall_step(4, true, &seen, &last)); // spent two
    CHECK(!combat_group_stall_step(4, false, &seen, &last)); // busy: never a stall
    CHECK(combat_group_stall_step(4, true, &seen, &last));
    CHECK(!combat_group_stall_step(0, true, &seen, &last)); // spent out
}

TEST(each_member_pays_from_its_own_pool_and_others_go_free)
{
    ObjectNode leader_node;

    leader_node.obj = LEADER;
    leader_node.next = NULL;
    combat_turn_based_active = 1;
    combat_whos_turn_node = &leader_node;
    combat_action_points = 0;
    combat_required_action_points = 0;

    CHECK(combat_group_add(LEADER, 8));
    CHECK(combat_group_add(NPC(1), 6));

    CHECK(combat_turn_based_is_acting(LEADER));
    CHECK(combat_turn_based_is_acting(NPC(1)));
    CHECK(!combat_turn_based_is_acting(PARTY(1)));
    CHECK_INT(combat_action_points_of(NPC(1)), 6);

    CHECK(combat_consume_action_points(NPC(1), 4));
    CHECK_INT(combat_action_points_of(NPC(1)), 2);
    CHECK_INT(combat_action_points_of(LEADER), 8);
    CHECK(!combat_can_pay_action_points(NPC(1), 3));
    CHECK(combat_can_pay_action_points(LEADER, 3));

    // Not acting: free, and nobody's pool moves.
    CHECK(combat_consume_action_points(PARTY(1), 100));
    CHECK(combat_can_pay_action_points(PARTY(1), 100));
    CHECK_INT(combat_action_points_of(NPC(1)), 2);
    CHECK_INT(combat_action_points_of(LEADER), 8);

    // A member's requirement is its own; the turn's, which the player's AP
    // bar previews, is untouched.
    CHECK(combat_check_action_points(LEADER, 5));
    CHECK_INT(combat_required_action_points_of(LEADER), 5);
    CHECK_INT(combat_required_action_points, 0);

    combat_group_count = 0;
    combat_whos_turn_node = NULL;
    combat_turn_based_active = 0;
}

TEST(with_no_group_only_whose_turn_it_is_acts)
{
    ObjectNode leader_node;

    leader_node.obj = LEADER;
    leader_node.next = NULL;
    combat_turn_based_active = 1;
    combat_whos_turn_node = &leader_node;
    combat_action_points = 7;

    CHECK(combat_turn_based_is_acting(LEADER));
    CHECK(!combat_turn_based_is_acting(NPC(1)));
    CHECK_INT(combat_action_points_of(LEADER), 7);
    // Stock: anyone's read of the pool is the turn's.
    CHECK_INT(combat_action_points_of(NPC(1)), 7);
    CHECK(combat_consume_action_points(LEADER, 3));
    CHECK_INT(combat_action_points, 4);

    combat_whos_turn_node = NULL;
    combat_turn_based_active = 0;
    CHECK(!combat_turn_based_is_acting(LEADER));
}

TEST(a_turn_comes_round_once_a_round_only_under_the_switch)
{
    combat_round_acted_count = 0;
    combat_round_mark_acted(NPC(1));
    CHECK(!combat_round_has_acted(NPC(1)));

    feature_override(FEATURE_SIMULTANEOUS_NPC_TURNS, true);
    combat_round_mark_acted(NPC(1));
    combat_round_mark_acted(NPC(1));
    CHECK(combat_round_has_acted(NPC(1)));
    CHECK_INT(combat_round_acted_count, 1);
    CHECK(!combat_round_has_acted(NPC(2)));

    feature_clear_overrides();
    combat_round_acted_count = 0;
}

void test_group_turn_suite(void)
{
    RUN(later_npcs_are_pulled_up_behind_the_first_in_order);
    RUN(a_run_already_together_is_left_as_it_is);
    RUN(a_lone_npc_is_a_group_of_one);
    RUN(a_member_is_finished_when_spent_and_idle_or_gone);
    RUN(standing_idle_twice_on_the_same_points_is_a_stall);
    RUN(each_member_pays_from_its_own_pool_and_others_go_free);
    RUN(with_no_group_only_whose_turn_it_is_acts);
    RUN(a_turn_comes_round_once_a_round_only_under_the_switch);
}
