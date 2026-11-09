// A turn coming round in a turn-based fight ticks poison (`poison per turn`)
// and ends summoning sickness (MG-3), in `combat_turn_based_critter_skip`.
// Loading a turn-based save rebuilds the encounter through the same walk to
// the saved turn, which is not a turn coming round: before review 2026-10-31
// (finding 2) every load of one save ticked the first poisoned critter again.
//
// The two side effects and `gamelib_in_load` are wrapped at link time
// (`ld --wrap`), so the case counts calls rather than building a poisoned
// critter and a summon; the walk's function and its turn pointer are `static`
// in combat.c and globalized for the suite. The suite also holds the
// fumble get-up cost, which needs the same globalized combat.c.
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "game/combat.h"
#include "game/feature.h"
#include "game/fumble_rules.h"
#include "game/obj.h"
#include "game/object_node.h"
#include "game/stat.h"
#include "obj_harness.h"
#include "test_harness.h"

// Globalized out of combat.c by the build; in no header.
extern ObjectNode* combat_whos_turn_node;
extern int combat_turn_based_active;
extern int combat_action_points;
extern int64_t combat_fumble_falls[];
bool combat_turn_based_critter_skip(ObjectNode* object_node);

static bool forced_in_load;
static int poison_calls;
static int summons_calls;

bool __real_gamelib_in_load(void);
void __real_stat_poison_turn_reached(int64_t obj);
void __real_magictech_summons_turn_reached(int64_t caster_obj);

bool __wrap_gamelib_in_load(void);
void __wrap_stat_poison_turn_reached(int64_t obj);
void __wrap_magictech_summons_turn_reached(int64_t caster_obj);

bool __wrap_gamelib_in_load(void)
{
    return forced_in_load || __real_gamelib_in_load();
}

void __wrap_stat_poison_turn_reached(int64_t obj)
{
    poison_calls++;
    __real_stat_poison_turn_reached(obj);
}

void __wrap_magictech_summons_turn_reached(int64_t caster_obj)
{
    summons_calls++;
    __real_magictech_summons_turn_reached(caster_obj);
}

static void reach_turn(bool loading)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    ObjectNode node;

    memset(&node, 0, sizeof(node));
    node.obj = obj;
    combat_whos_turn_node = &node;
    // A tick that kills ends the encounter, and the walk stops there; this one
    // is in an encounter so the walk reaches the summons too.
    combat_turn_based_active = true;
    poison_calls = 0;
    summons_calls = 0;
    forced_in_load = loading;

    combat_turn_based_critter_skip(&node);

    forced_in_load = false;
    combat_turn_based_active = false;
    combat_whos_turn_node = NULL;
    obj_harness_destroy(obj);
}

TEST(a_turn_coming_round_reaches_poison_and_summons)
{
    reach_turn(false);
    CHECK_INT(poison_calls, 1);
    CHECK_INT(summons_calls, 1);
}

TEST(a_load_walking_to_the_saved_turn_reaches_neither)
{
    reach_turn(true);
    CHECK_INT(poison_calls, 0);
    CHECK_INT(summons_calls, 0);
}

// Review 2026-10-31, finding 6: a fumble's get-up cost 6 even for a critter
// whose whole turn is 5, which then lay down for the rest of the fight. The
// fall is planted in the table `combat_apply_fumble` fills.
static int get_up_cost_at_speed(int speed)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    int cost;

    stat_base_set(obj, STAT_DEXTERITY, speed);
    combat_fumble_falls[0] = obj;
    cost = combat_get_up_cost(obj);
    combat_fumble_fall_clear(obj);
    obj_harness_destroy(obj);
    return cost;
}

TEST(a_fumble_get_up_never_costs_more_than_a_whole_turn)
{
    CHECK_INT(get_up_cost_at_speed(5), 5);
    CHECK_INT(get_up_cost_at_speed(3), 5);
}

TEST(a_fumble_get_up_costs_six_to_a_critter_with_six_a_turn)
{
    CHECK_INT(get_up_cost_at_speed(6), FUMBLE_GET_UP_COST);
    CHECK_INT(get_up_cost_at_speed(12), FUMBLE_GET_UP_COST);
}

// Review 2026-11-08, healing-6: under `restorative cost` a drink cost 6
// against a turn of 5, so a critter with Speed 5 or less never drank in
// turn-based. Now it costs the whole turn.
static int restorative_cost_at_speed(int speed)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t food_obj = obj_harness_create_proto(OBJ_TYPE_FOOD);
    int cost;

    stat_base_set(obj, STAT_DEXTERITY, speed);
    cost = combat_use_item_cost(obj, food_obj);
    obj_harness_destroy(food_obj);
    obj_harness_destroy(obj);
    return cost;
}

TEST(a_restorative_never_costs_more_than_a_whole_turn)
{
    feature_override(FEATURE_RESTORATIVE_COST, true);
    CHECK_INT(restorative_cost_at_speed(5), 5);
    CHECK_INT(restorative_cost_at_speed(3), 5);
    CHECK_INT(restorative_cost_at_speed(6), COMBAT_RESTORATIVE_COST);
    CHECK_INT(restorative_cost_at_speed(12), COMBAT_RESTORATIVE_COST);
    feature_clear_overrides();
    CHECK_INT(restorative_cost_at_speed(5), 4); // stock
}

TEST(a_five_a_turn_npc_at_full_points_can_pay_for_a_drink)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t food_obj = obj_harness_create_proto(OBJ_TYPE_FOOD);
    ObjectNode node;

    feature_override(FEATURE_RESTORATIVE_COST, true);
    feature_override(FEATURE_EXERTION, false);
    stat_base_set(obj, STAT_DEXTERITY, 5);
    memset(&node, 0, sizeof(node));
    node.obj = obj;
    combat_whos_turn_node = &node;
    combat_turn_based_active = true;
    combat_action_points = 5;
    CHECK(combat_can_pay_action_points(obj, combat_use_item_cost(obj, food_obj)));
    combat_action_points = 4; // below full points it is still refused
    CHECK(!combat_can_pay_action_points(obj, combat_use_item_cost(obj, food_obj)));
    combat_action_points = 0;
    combat_turn_based_active = false;
    combat_whos_turn_node = NULL;
    feature_clear_overrides();
    obj_harness_destroy(food_obj);
    obj_harness_destroy(obj);
}

void test_turn_reached_suite(void)
{
    CHECK(obj_harness_init());
    RUN(a_turn_coming_round_reaches_poison_and_summons);
    RUN(a_load_walking_to_the_saved_turn_reaches_neither);
    RUN(a_fumble_get_up_never_costs_more_than_a_whole_turn);
    RUN(a_fumble_get_up_costs_six_to_a_critter_with_six_a_turn);
    RUN(a_restorative_never_costs_more_than_a_whole_turn);
    RUN(a_five_a_turn_npc_at_full_points_can_pay_for_a_drink);
}
