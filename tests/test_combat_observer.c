// Tests for O1's bounded, default-off combat event writer.
#include <stdio.h>
#include <string.h>

#include "game/combat_observer.h"
#include "game/damage_type.h"
#include "game/random.h"

#include "test_harness.h"

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

void test_combat_observer_suite(void)
{
    RUN(absent_switch_does_no_output_work);
    RUN(a_complete_trace_has_ordered_control_and_semantic_events);
    RUN(hit_miss_critical_and_terminal_values_are_not_conflated);
    RUN(the_bound_marks_the_trace_incomplete_once);
    RUN(an_output_failure_disables_recording_and_leaves_no_false_terminal);
    RUN(observation_does_not_advance_the_combat_rng);
    clean();
}
