// Tests for O1's bounded, default-off combat event writer.
#include <stdio.h>
#include <string.h>

#include "game/combat_observer.h"
#include "game/damage_type.h"
#include "game/random.h"

#include "test_harness.h"
#include "tig/file.h"

#define TRACE_FILE "./combat-observer-test.jsonl"
#define TRACE_SIZE 16384

static bool read_trace(char* buffer, size_t size)
{
    FILE* stream = fopen(TRACE_FILE, "rb");
    size_t got;

    if (stream == NULL) return false;
    got = fread(buffer, 1, size - 1, stream);
    fclose(stream);
    buffer[got] = '\0';
    return true;
}

static int count_lines(const char* text)
{
    int count = 0;
    while (*text != '\0') {
        if (*text++ == '\n') count++;
    }
    return count;
}

static void clean(void)
{
    combat_observer_test_reset();
    remove(TRACE_FILE);
}

TEST(absent_switch_does_no_output_work)
{
    clean();
    CHECK(!combat_observer_is_enabled());
    combat_observer_turn_started("fighter", 8);
    combat_observer_actor_snapshot("fighter", 1, 8, 8, 8, 0, 0,
        22, 22, 20, 20, 6056, 8042);
    CHECK(!read_trace((char[2]) { 0 }, 2));
}

TEST(a_complete_trace_has_ordered_control_and_semantic_events)
{
    char trace[TRACE_SIZE];
    int damage[DAMAGE_TYPE_COUNT] = {
        [DAMAGE_TYPE_NORMAL] = 4,
        [DAMAGE_TYPE_FATIGUE] = 2,
    };
    unsigned long long action;

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_turn_started("fighter", 8);
    combat_observer_actor_snapshot("fighter", 1, 8, 8, 8, 0, 0,
        22, 22, 20, 20, 6056, 8042);
    combat_observer_ap_checked("fighter", 4, 8, true);
    combat_observer_ap_committed("fighter", 4, 8, 4, true, 0, 0);
    action = combat_observer_attack_started("fighter", "opponent", 2, 0);
    combat_observer_skill_roll("fighter", "opponent", 2,
        55, 0, 40, 99, true, false);
    combat_observer_damage_committed("fighter", "opponent", damage,
        0, 4, 0, 4, 0, 2, false, false);
    combat_observer_attack_resolved(action, "fighter", "opponent",
        2, 0, 0x2, 1);
    combat_observer_turn_ended("fighter", 4);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"run.started\"") != NULL);
    CHECK(strstr(trace, "\"sequence\":1,\"event\":\"turn.started\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"skill.roll\"") != NULL);
    CHECK(strstr(trace, "\"weapon_description\":6056") != NULL);
    CHECK(strstr(trace, "\"success\":true,\"critical\":false") != NULL);
    CHECK(strstr(trace, "\"normal\":4,\"fatigue\":2") != NULL);
    CHECK(strstr(trace, "\"hp_before\":0,\"hp_after\":4") != NULL);
    CHECK(strstr(trace, "\"event\":\"run.ended\"") != NULL);
    CHECK(strstr(trace, "\"complete\":true") != NULL);
}

TEST(hit_miss_critical_and_terminal_values_are_not_conflated)
{
    char trace[TRACE_SIZE];
    int damage[DAMAGE_TYPE_COUNT] = { [DAMAGE_TYPE_NORMAL] = 12 };
    unsigned long long hit;
    unsigned long long miss;

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    hit = combat_observer_attack_started("fighter", "opponent", 2, 0);
    combat_observer_attack_resolved(hit, "fighter", "opponent", 2, 0,
        0x2 | 0x4, 1);
    miss = combat_observer_attack_started("opponent", "fighter", 2, 0);
    combat_observer_attack_resolved(miss, "opponent", "fighter", 2, 0,
        0, 0);
    combat_observer_damage_committed("fighter", "opponent", damage,
        4, 12, 3, 15, 1, 1, true, false);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"hit\":true,\"critical\":true") != NULL);
    CHECK(strstr(trace, "\"hit\":false,\"critical\":false") != NULL);
    CHECK(strstr(trace, "\"dead\":true,\"unconscious\":false") != NULL);
}

TEST(the_bound_marks_the_trace_incomplete_once)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_test_set_event_limit(2);
    combat_observer_turn_started("fighter", 8);
    combat_observer_ap_checked("fighter", 2, 8, true);
    combat_observer_ap_committed("fighter", 2, 8, 6, true, 0, 0);
    combat_observer_ap_committed("fighter", 2, 6, 4, true, 0, 0);
    CHECK(!combat_observer_is_complete());
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK_INT(count_lines(trace), 5);
    CHECK(strstr(trace, "\"event\":\"observer.truncated\"") != NULL);
    CHECK(strstr(trace, "\"complete\":false") != NULL);
}

TEST(an_output_failure_disables_recording_and_leaves_no_false_terminal)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_test_fail_after(1);
    combat_observer_turn_started("fighter", 8);
    CHECK(!combat_observer_is_enabled());
    CHECK(!combat_observer_is_complete());
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK_INT(count_lines(trace), 1);
    CHECK(strstr(trace, "run.started") != NULL);
    CHECK(strstr(trace, "run.ended") == NULL);
}

TEST(observation_does_not_advance_the_combat_rng)
{
    int first_without;
    int second_without;
    int first_with;
    int second_with;

    clean();
    random_seed(99117);
    first_without = random_rand();
    second_without = random_rand();

    random_seed(99117);
    first_with = random_rand();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_turn_started("fighter", 8);
    combat_observer_skill_roll("fighter", "opponent", 2,
        50, 0, 10, 10, true, true);
    combat_observer_close();
    second_with = random_rand();

    CHECK_INT(first_with, first_without);
    CHECK_INT(second_with, second_without);
}


#include "game/combat.h"
#include "game/obj.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/mes.h"
#include "game/tf.h"
#include "obj_harness.h"

// --- card O2: the player's command, and whether the engine took it -------

TEST(a_command_records_its_request_before_its_resolution)
{
    char trace[TRACE_SIZE];
    const char* requested;
    const char* resolved;

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_command_requested("fighter", "object", "opponent", 4242);
    combat_observer_command_resolved("fighter", true);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    requested = strstr(trace, "\"event\":\"command.requested\"");
    resolved = strstr(trace, "\"event\":\"command.resolved\"");
    CHECK(requested != NULL);
    CHECK(resolved != NULL);
    // Ordering is the point: anything the command causes must follow the
    // request, not precede the thing that caused it.
    CHECK(requested < resolved);
}

TEST(a_refused_command_is_not_recorded_as_accepted)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_command_requested("fighter", "location", "NULL", 99);
    combat_observer_command_resolved("fighter", false);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"accepted\":false") != NULL);
    CHECK(strstr(trace, "\"accepted\":true") == NULL);
}

TEST(a_location_command_carries_its_target_tile_and_no_object)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_command_requested("fighter", "location", "NULL", 361700864);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"target_kind\":\"location\"") != NULL);
    CHECK(strstr(trace, "\"target\":\"NULL\"") != NULL);
    // A packed location names its tile reproducibly across runs, which is why
    // it is recorded as the target rather than a screen position.
    CHECK(strstr(trace, "\"target_location\":361700864") != NULL);
}

// --- card W4: the same real attack, observed and not ---------------------
//
// The RNG check above compares draws around the *writer* calls. It never drives
// an attack, so it cannot see an observer that perturbs combat through any other
// route, and it cannot show that the trace describes a real one. This drives the
// engine's own `combat_attack` (0x4B3170) twice from identical state and seed.
//
// Three functions are wrapped, and all three are environment the harness cannot
// stand up rather than combat logic: a lighting sample that needs a sector grid,
// a message lookup that needs `.mes` files, and the floating damage text that
// needs a font and a video buffer. Each behaves identically in both runs, so
// none of them can bias the comparison - which is the only property the
// neutrality claim needs from them.
static bool stub_environment;
static bool inject_observer_draw;

extern void __real_mes_get_msg(mes_file_handle_t handle, MesFileEntry* entry);
extern void __real_tf_add(int64_t obj, int type, const char* str);
extern unsigned long long __real_combat_observer_attack_started(const char* attacker,
    const char* target, int skill, int hit_location);

void __wrap_mes_get_msg(mes_file_handle_t handle, MesFileEntry* entry)
{
    static char text[] = "combat";
    if (stub_environment) {
        entry->str = text;
        return;
    }
    __real_mes_get_msg(handle, entry);
}

void __wrap_tf_add(int64_t obj, int type, const char* str)
{
    if (stub_environment) return;
    __real_tf_add(obj, type, str);
}

int __wrap_light_obj_grayscale_get(int64_t obj)
{
    extern int __real_light_obj_grayscale_get(int64_t obj);
    if (stub_environment) return 0;
    return __real_light_obj_grayscale_get(obj);
}

// The negative control. Armed only by its own case, this draws once at the
// exact point the engine hands control to the observer - which is what an
// observer that consumed randomness would do.
unsigned long long __wrap_combat_observer_attack_started(const char* attacker,
    const char* target, int skill, int hit_location)
{
    if (inject_observer_draw) {
        (void)random_rand();
    }
    return __real_combat_observer_attack_started(attacker, target, skill, hit_location);
}

// Seed 13 lands a hit that computes damage, so the comparison covers the damage
// path and not only a miss. Seeds that miss agree just as exactly.
#define ATTACK_SEED 13

typedef struct ObservedAttack {
    int rc;
    unsigned int flags;
    int total_dam;
    int dam[DAMAGE_TYPE_COUNT];
    int target_hp_damage;
    int rng_after[4];
} ObservedAttack;

static void drive_one_attack(bool observe, ObservedAttack* out)
{
    int64_t attacker;
    int64_t target;
    CombatContext combat;
    int index;

    clean();
    CHECK(obj_harness_init());
    attacker = obj_harness_create_proto(OBJ_TYPE_NPC);
    target = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(attacker != OBJ_HANDLE_NULL);
    CHECK(target != OBJ_HANDLE_NULL);

    stat_base_set(attacker, STAT_STRENGTH, 20);
    stat_base_set(attacker, STAT_DEXTERITY, 20);
    basic_skill_points_set(attacker, BASIC_SKILL_MELEE, 15);
    stat_base_set(target, STAT_DEXTERITY, 1);
    obj_field_int32_set(target, OBJ_F_HP_ADJ, 40);
    obj_field_int32_set(target, OBJ_F_HP_DAMAGE, 0);

    memset(&combat, 0, sizeof(combat));
    combat.attacker_obj = attacker;
    combat.target_obj = target;
    combat.field_28 = target;
    combat.weapon_obj = OBJ_HANDLE_NULL;
    combat.skill = SKILL_MELEE;
    combat.hit_loc = HIT_LOC_TORSO;

    stub_environment = true;
    random_seed(ATTACK_SEED);
    if (observe) {
        CHECK(combat_observer_open(TRACE_FILE));
    }
    out->rc = combat_attack(&combat);
    if (observe) {
        combat_observer_close();
    }

    out->flags = combat.flags;
    out->total_dam = combat.total_dam;
    for (index = 0; index < DAMAGE_TYPE_COUNT; index++) {
        out->dam[index] = combat.dam[index];
    }
    out->target_hp_damage = obj_field_int32_get(target, OBJ_F_HP_DAMAGE);

    // Subsequent RNG state, which is what a hidden draw would move.
    for (index = 0; index < 4; index++) {
        out->rng_after[index] = random_rand();
    }
    stub_environment = false;

    obj_harness_destroy(attacker);
    obj_harness_destroy(target);
}

static void check_attacks_agree(const ObservedAttack* a, const ObservedAttack* b)
{
    int index;

    CHECK_INT(a->rc, b->rc);
    CHECK_INT((int)a->flags, (int)b->flags);
    CHECK_INT(a->total_dam, b->total_dam);
    CHECK_INT(a->target_hp_damage, b->target_hp_damage);
    for (index = 0; index < DAMAGE_TYPE_COUNT; index++) {
        CHECK_INT(a->dam[index], b->dam[index]);
    }
    for (index = 0; index < 4; index++) {
        CHECK_INT(a->rng_after[index], b->rng_after[index]);
    }
}

TEST(observing_a_real_attack_changes_neither_its_outcome_nor_the_rng)
{
    ObservedAttack without;
    ObservedAttack with;

    drive_one_attack(false, &without);
    drive_one_attack(true, &with);

    // The case is only worth anything if the attack actually did something.
    CHECK(without.flags != 0);
    CHECK(without.total_dam > 0);

    check_attacks_agree(&without, &with);
}

TEST(the_observed_run_records_that_same_attack)
{
    ObservedAttack with;
    char trace[TRACE_SIZE];

    drive_one_attack(true, &with);

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"attack.started\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"skill.roll\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"attack.resolved\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"damage.committed\"") != NULL);
    CHECK(strstr(trace, "\"complete\":true") != NULL);
    // Not a trace of nothing: the engine reached the writer repeatedly.
    CHECK(count_lines(trace) >= 6);
}

TEST(one_extra_observer_rng_draw_is_detected)
{
    ObservedAttack without;
    ObservedAttack with;
    int index;
    bool differs = false;

    drive_one_attack(false, &without);

    inject_observer_draw = true;
    drive_one_attack(true, &with);
    inject_observer_draw = false;

    // A single draw inside the observer must move the comparison. Without this
    // the agreement above would be worth nothing, because a check that cannot
    // fail has not measured anything.
    for (index = 0; index < 4; index++) {
        if (with.rng_after[index] != without.rng_after[index]) {
            differs = true;
        }
    }
    CHECK(differs);
}

void test_combat_observer_suite(void)
{
    RUN(absent_switch_does_no_output_work);
    RUN(a_complete_trace_has_ordered_control_and_semantic_events);
    RUN(hit_miss_critical_and_terminal_values_are_not_conflated);
    RUN(the_bound_marks_the_trace_incomplete_once);
    RUN(an_output_failure_disables_recording_and_leaves_no_false_terminal);
    RUN(observation_does_not_advance_the_combat_rng);
    RUN(a_command_records_its_request_before_its_resolution);
    RUN(a_refused_command_is_not_recorded_as_accepted);
    RUN(a_location_command_carries_its_target_tile_and_no_object);
    RUN(observing_a_real_attack_changes_neither_its_outcome_nor_the_rng);
    RUN(the_observed_run_records_that_same_attack);
    RUN(one_extra_observer_rng_draw_is_detected);
    // The harness registers "." as a repository; later file-listing suites
    // enumerate repositories and would see this one. Hand it back.
    obj_harness_exit();
    tig_file_repository_remove(".");
    clean();
}
