// Tests for O1's bounded, default-off combat event writer.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/timer.h>

#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/crit_effects.h"
#include "game/damage_type.h"
#include "game/item.h"
#include "game/random.h"
#include "game/skill.h"
#include "ui/called_shot.h"
#include "ui/replay_ui.h"

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

static int count_events(const char* text, const char* needle)
{
    int count = 0;
    size_t width = strlen(needle);

    while ((text = strstr(text, needle)) != NULL) {
        count++;
        text += width;
    }
    return count;
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
        22, 22, 20, 20, 6056, 8042, SKILL_MELEE, 3, TRAINING_NONE, 10);
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
        22, 22, 20, 20, 6056, 8042, SKILL_MELEE, 3, TRAINING_NONE, 10);
    combat_observer_ap_checked("fighter", 4, 8, true);
    combat_observer_ap_committed("fighter", 4, 8, 4, true, 0, 0);
    action = combat_observer_attack_started("fighter", "opponent", 2, 0, 0, false);
    combat_observer_skill_roll("fighter", "opponent", 2,
        55, 0, 40, 99, 2, true, false);
    combat_observer_critical_rolled("fighter", "opponent", true, 1, 10,
        true, false, 0, 0, CDF_BONUS_DAM_100 | CDF_STUN, CDF_KNOCKDOWN, true,
        CRIT_EFFECT_STAGGERED, NULL, 7, 14);
    // Milestone CR-4.0: a fumble says what it did.
    combat_observer_critical_rolled("fighter", "fighter", false, 0, 0,
        false, false, 2, -1, 0, 0, false, 0, "self_hit", 0, 0);
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
    CHECK(strstr(trace, "\"weapon_skill_level\":3,\"weapon_training\":0,\"weapon_speed\":10") != NULL);
    CHECK(strstr(trace, "\"critical_roll\":99,\"critical_chance\":2,\"success\":true,\"critical\":false") != NULL);
    // Criticals, milestone CR-0: the effects by name, in their flags' order.
    CHECK(strstr(trace, "\"event\":\"critical.rolled\"") != NULL);
    CHECK(strstr(trace, "\"kind\":\"hit\",\"hit_location\":1,\"effect_bonus\":10,\"aimed\":true,\"player_spared\":false") != NULL);
    CHECK(strstr(trace, "\"effects\":[\"bonus_100\",\"stun\"],\"resisted\":[\"knockdown\"],\"target_down\":true,\"temporary\":[\"staggered\"],\"outcome\":null,\"winded_fatigue\":7,\"winded_from\":14}") != NULL);
    CHECK(strstr(trace, "\"kind\":\"miss\"") != NULL);
    CHECK(strstr(trace, "\"outcome\":\"self_hit\",\"winded_fatigue\":0,\"winded_from\":0}") != NULL);
    CHECK(strstr(trace, "\"normal\":4,\"fatigue\":2") != NULL);
    CHECK(strstr(trace, "\"hp_before\":0,\"hp_after\":4") != NULL);
    CHECK(strstr(trace, "\"event\":\"run.ended\"") != NULL);
    CHECK(strstr(trace, "\"complete\":true") != NULL);
}

TEST(an_ai_decision_lists_what_it_weighed)
{
    char trace[TRACE_SIZE];
    const int considered[][2] = { { 11, 2 }, { 25, 9 }, { 40, 0 } };
    int many[COMBAT_OBSERVER_AI_CONSIDERED_MAX + 3][2] = { { 0 } };

    // Combat AI milestone AI-0: what the NPC did, and each spell it weighed
    // with the code `ai_check_use_spell` gave it.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_ai_decided("mage", "fighter", "combat", "decide", "cast",
        40, "NULL", "NULL", 50, 17, considered, 3, true);
    combat_observer_ai_decided("guard", "fighter", "combat", "decide", "attack",
        -1, "NULL", "G_SWORD", 25, -1, many, COMBAT_OBSERVER_AI_CONSIDERED_MAX + 3, false);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"ai.decided\",") != NULL);
    CHECK(strstr(trace, "\"actor\":\"mage\",\"target\":\"fighter\",\"danger\":\"combat\","
                        "\"origin\":\"decide\",\"action\":\"cast\",\"spell\":40,")
        != NULL);
    CHECK(strstr(trace, "\"cast_chance\":50,\"cast_roll\":17,\"considered\":[[11,2],[25,9],[40,0]],"
                        "\"desperate\":true}")
        != NULL);
    CHECK(strstr(trace, "\"weapon\":\"G_SWORD\",\"cast_chance\":25,\"cast_roll\":-1,") != NULL);
    // No more than the cap is written, however many are passed.
    CHECK(strstr(trace, "[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0]],\"desperate\":false}") != NULL);
    CHECK(strstr(trace, "[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0]") == NULL);
}

TEST(an_armor_aware_weapon_choice_lists_its_candidates)
{
    char trace[TRACE_SIZE];
    const int candidates[][3] = { { 6074, 168, 746 }, { 6057, 136, 355 } };
    const int unaffordable[][2] = { { 6067, 6 } };

    // Combat AI milestone AI-1: each weapon weighed, with its score and the
    // share expected through the target's armor.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_ai_weapon_chosen("guard", "fighter", "G_SWORD", "G_HAMMER", false,
        candidates, 2, unaffordable, 1);
    combat_observer_ai_weapon_chosen("guard", "fighter", "G_HAMMER", "G_HAMMER", true,
        candidates, 2, unaffordable, 0);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"ai.weapon_chosen\",") != NULL);
    CHECK(strstr(trace, "\"previous\":\"G_SWORD\",\"chosen\":\"G_HAMMER\",\"kept\":false,"
                        "\"candidates\":[[6074,168,746],[6057,136,355]],"
                        "\"unaffordable\":[[6067,6]]}")
        != NULL);
    CHECK(strstr(trace, "\"kept\":true,") != NULL);
    // 2026-10-05: a weapon the NPC cannot swing in one turn, with its cost.
    CHECK(strstr(trace, "\"unaffordable\":[]}") != NULL);
}

TEST(every_event_carries_the_clock_the_baseline_needs)
{
    char trace[TRACE_SIZE];
    const char* line;
    int lines = 0;
    int stamped = 0;

    // Schema 4. The observer could say what happened and in what order and
    // never how often, so a firing rate could not be measured from it at all.
    // The field is injected in one place precisely so a new event cannot
    // forget it -- which is what this walks the whole file to check.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_turn_started("fighter", 8);
    combat_observer_actor_snapshot("fighter", 1, 8, 8, 8, 0, 0,
        22, 22, 20, 20, 6056, 8042, SKILL_MELEE, 3, TRAINING_NONE, 10);
    combat_observer_ap_checked("fighter", 4, 8, true);
    combat_observer_turn_ended("fighter", 4);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    for (line = trace; line != NULL && *line != '\0';) {
        if (*line == '{') {
            lines++;
            // Not merely present: FIRST, so a line is self-describing from its
            // opening brace whatever the event turns out to be.
            if (strncmp(line, "{\"ms\":", 6) == 0) {
                stamped++;
            }
        }
        line = strchr(line, '\n');
        if (line != NULL) {
            line++;
        }
    }
    CHECK(lines > 4);
    CHECK(stamped == lines);
}

TEST(the_trace_says_which_clock_produced_its_numbers)
{
    char trace[TRACE_SIZE];

    // `ms` counts real milliseconds normally and fixed steps per frame under
    // the deterministic virtual clock. Reading one as the other turns a frame
    // count into a firing rate, which is the specific mistake the session's
    // acceptance names. So the file states which clock it ran on rather than
    // leaving a reader to assume, and it states it on its first line.
    clean();
    CHECK(!tig_timer_virtual_enabled());
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_close();
    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"virtual_clock\":false") != NULL);

    tig_timer_virtual_set(true, 16);
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_close();
    tig_timer_virtual_set(false, 0);
    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"virtual_clock\":true") != NULL);
}

TEST(the_clock_starts_at_the_stream_and_never_goes_backwards)
{
    char trace[TRACE_SIZE];
    const char* cursor;
    long long previous = -1;
    long long value;
    int seen = 0;

    // Run-relative, so the first line is 0 rather than however long the
    // process had been up. Elapsing against an unset baseline is not zero, it
    // is whatever was on the stack, which is why `run.started` takes the
    // timestamp before it writes itself.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_turn_started("fighter", 8);
    combat_observer_ap_checked("fighter", 4, 8, true);
    combat_observer_turn_ended("fighter", 4);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strncmp(trace, "{\"ms\":0,", 8) == 0);
    for (cursor = trace; (cursor = strstr(cursor, "{\"ms\":")) != NULL;) {
        value = atoll(cursor + 6);
        CHECK(value >= previous);
        previous = value;
        seen++;
        cursor++;
    }
    CHECK(seen > 3);
}

TEST(the_release_impact_and_charge_boundaries_are_each_their_own_event)
{
    char trace[TRACE_SIZE];

    // Real-time combat session 1 needs these four apart, because in real time
    // they stop coinciding: a bow rewinds its animation and releases again
    // without a new attack, an NPC with no PC leader releases without paying
    // ammunition, and an arrow that reaches nothing commits no damage. A trace
    // that could only count damage would report all three as the same number.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_action_released("fighter", 5, 6, 6, 3, 10, 11, 10, 90);
    combat_observer_resource_charged("fighter", "ammo", -1, 1, 20, 19);
    combat_observer_projectile_ended("fighter", "opponent", "opponent", "struck");
    combat_observer_resource_charged("mage", "fatigue", 91, 5, 0, 5);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"action.released\"") != NULL);
    CHECK(strstr(trace, "\"goal\":5,\"frame\":6,\"action_frame\":6,\"weapon_type\":3") != NULL);
    CHECK(strstr(trace, "\"event\":\"projectile.ended\"") != NULL);
    CHECK(strstr(trace, "\"resource\":\"ammo\",\"spell\":null,\"cost\":1,\"before\":20,\"after\":19") != NULL);
    CHECK(strstr(trace, "\"resource\":\"fatigue\",\"spell\":91,\"cost\":5,\"before\":0,\"after\":5") != NULL);
}

TEST(a_release_carries_the_inputs_that_made_its_step_delay)
{
    char trace[TRACE_SIZE];

    // Card RT1. 467 ms over four steps is ~117 ms a step and the plan had
    // calculated 70; without the frame rate and the weapon speed on the record
    // the difference could only be restated. Each input has its own field, in
    // the order the attack path applies them, and the result beside them.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_action_released("archer", 5, 6, 6, 3, 10, 11, 7, 120);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"art_fps\":10,\"speed_fps\":11,\"weapon_speed\":7,\"step_ms\":120") != NULL);
}

TEST(every_way_a_flight_ends_is_its_own_outcome)
{
    char trace[TRACE_SIZE];

    // Card RT1. The event this replaces fired on the cleanup path alone - 3 of
    // 34 releases, always with a null target. A reader counting arrivals has to
    // be able to tell a hit from an interception from an arrow that flew out
    // of range, and each of those from an arrow that simply stopped existing.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_projectile_ended("archer", "opponent", "opponent", "struck");
    combat_observer_projectile_ended("archer", "opponent", "bystander", "struck");
    combat_observer_projectile_ended("archer", "NULL", "NULL", "tile");
    combat_observer_projectile_ended("archer", "archer", "NULL", "returned");
    combat_observer_projectile_ended("archer", "opponent", "NULL", "out_of_range");
    combat_observer_projectile_ended("archer", "opponent", "NULL", "abandoned");
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK_INT(count_events(trace, "\"event\":\"projectile.ended\""), 6);
    CHECK(strstr(trace, "\"target\":\"opponent\",\"struck\":\"opponent\",\"outcome\":\"struck\"") != NULL);
    CHECK(strstr(trace, "\"target\":\"opponent\",\"struck\":\"bystander\",\"outcome\":\"struck\"") != NULL);
    CHECK(strstr(trace, "\"outcome\":\"tile\"") != NULL);
    CHECK(strstr(trace, "\"outcome\":\"returned\"") != NULL);
    CHECK(strstr(trace, "\"outcome\":\"out_of_range\"") != NULL);
    CHECK(strstr(trace, "\"outcome\":\"abandoned\"") != NULL);
    CHECK(strstr(trace, "projectile.impact") == NULL);
}

TEST(a_cast_is_identified_by_its_spell_not_by_its_cost)
{
    char trace[TRACE_SIZE];

    // Card RT1. Harm and Minor Healing both cost 5 fatigue, and session 1
    // reported one as the other. Harm is spell 55 and Minor Healing 60; two
    // casts at the same price must name two
    // different spells, and a free cast must say it was free.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_resource_charged("mage", "fatigue", 55, 5, 0, 5);
    combat_observer_spell_cast("mage", "mage", "opponent", 55, false, 4);
    combat_observer_resource_charged("mage", "fatigue", 60, 5, 5, 10);
    combat_observer_spell_cast("mage", "mage", "mage", 60, false, 3);
    combat_observer_spell_cast("fighter", "grenade", "NULL", 175, true, 0);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK_INT(count_events(trace, "\"event\":\"spell.cast\""), 3);
    CHECK(strstr(trace, "\"target\":\"opponent\",\"spell\":55,\"free\":false") != NULL);
    CHECK(strstr(trace, "\"target\":\"mage\",\"spell\":60,\"free\":false") != NULL);
    CHECK(strstr(trace, "\"caster\":\"fighter\",\"source\":\"grenade\"") != NULL);
    CHECK(strstr(trace, "\"spell\":175,\"free\":true,\"ap_cost\":0}") != NULL);
    // Magic milestone MG-1: the AP each cast was priced at.
    CHECK(strstr(trace, "\"spell\":55,\"free\":false,\"ap_cost\":4}") != NULL);
    CHECK(strstr(trace, "\"spell\":60,\"free\":false,\"ap_cost\":3}") != NULL);
    // Magic milestone MG-2: a spell's critical on one target.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_spell_critical("mage", "opponent", 33, "long_stun", 25, 13, 7);
    combat_observer_spell_critical("mage", "mage", 10, "refund", 0, 7, 0);
    combat_observer_close();
    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"spell.critical\"") != NULL);
    CHECK(strstr(trace, "\"caster\":\"mage\",\"target\":\"opponent\",\"spell\":33,\"critical\":\"long_stun\",\"bonus\":25,\"chance\":13,\"roll\":7}") != NULL);
    CHECK(strstr(trace, "\"critical\":\"refund\",\"bonus\":0,\"chance\":7,\"roll\":0}") != NULL);
    // Magic milestone MG-3: summoning sickness, and its end.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_summon_sick("mage", "skeleton", true);
    combat_observer_summon_sick("mage", "ogre", false);
    combat_observer_summon_ready("skeleton", "caster_turn");
    combat_observer_close();
    CHECK(read_trace(trace, sizeof(trace)));
    CHECK_INT(count_events(trace, "\"event\":\"summon.sick\""), 2);
    CHECK(strstr(trace, "\"summoner\":\"mage\",\"summon\":\"skeleton\",\"until\":\"caster_turn\"}") != NULL);
    CHECK(strstr(trace, "\"summon\":\"ogre\",\"until\":\"2000ms\"}") != NULL);
    CHECK(strstr(trace, "\"event\":\"summon.ready\"") != NULL);
    CHECK(strstr(trace, "\"summon\":\"skeleton\",\"reason\":\"caster_turn\"}") != NULL);
    // 2026-10-12: prowling starts and stops.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_prowl_changed("rogue", true, false, 40);
    combat_observer_prowl_changed("rogue", false, true, 253);
    combat_observer_close();
    CHECK(read_trace(trace, sizeof(trace)));
    CHECK_INT(count_events(trace, "\"event\":\"prowl.changed\""), 2);
    CHECK(strstr(trace, "\"actor\":\"rogue\",\"prowling\":true,\"combat_mode\":false,\"light\":40}") != NULL);
    CHECK(strstr(trace, "\"prowling\":false,\"combat_mode\":true,\"light\":253}") != NULL);
}

TEST(an_unreadable_frame_is_reported_as_unknown_not_as_a_number)
{
    char trace[TRACE_SIZE];

    // The release hook asks the art system for the frame and can be refused.
    // -1 says so. Emitting whatever was on the stack would put a plausible
    // number into a timing baseline as though it had been measured, which is
    // the one thing this session must not do.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_action_released("fighter", 5, -1, -1, 0, -1, -1, 10, 100);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"frame\":-1,\"action_frame\":-1") != NULL);
    CHECK(strstr(trace, "\"art_fps\":-1,\"speed_fps\":-1") != NULL);
}

TEST(repeated_releases_are_countable_and_carry_the_clock)
{
    char trace[TRACE_SIZE];
    const char* cursor;
    int releases = 0;

    // The bow rewind clears the latch and releases again without restarting
    // the attack, so the spacing between these events is the firing rate. They
    // have to be individually countable and individually timed, or the rate
    // cannot be read off the trace at all.
    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_action_released("archer", 5, 6, 6, 3, 10, 11, 10, 90);
    combat_observer_action_released("archer", 5, 6, 6, 3, 10, 11, 10, 90);
    combat_observer_action_released("archer", 5, 6, 6, 3, 10, 11, 10, 90);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    for (cursor = trace;
        (cursor = strstr(cursor, "\"event\":\"action.released\"")) != NULL;
        cursor++) {
        releases++;
    }
    CHECK(releases == 3);
    // Identical events, so `sequence` and `ms` are the only things telling
    // them apart. Both must be there.
    CHECK(strstr(trace, "{\"ms\":") != NULL);
}

TEST(a_selected_limb_is_distinguishable_from_one_that_was_rolled)
{
    char trace[TRACE_SIZE];
    const char* aimed_line;
    const char* rolled_line;

    clean();
    CHECK(combat_observer_open(TRACE_FILE));

    // A selected head shot: the request survived to resolution.
    combat_observer_attack_started("fighter", "opponent", 2,
        HIT_LOC_HEAD, HIT_LOC_HEAD, true);

    // An ordinary attack that *rolled* a head. `combat_attack` has already
    // replaced the zero with a random limb by the time it reports, so the
    // resolved location is identical to the line above. Before schema 2 these
    // two were the same event.
    combat_observer_attack_started("fighter", "opponent", 2,
        HIT_LOC_HEAD, HIT_LOC_TORSO, false);

    combat_observer_close();
    CHECK(read_trace(trace, sizeof(trace)));

    aimed_line = strstr(trace,
        "\"hit_location\":1,\"requested_hit_location\":1,\"aimed\":true");
    rolled_line = strstr(trace,
        "\"hit_location\":1,\"requested_hit_location\":0,\"aimed\":false");

    CHECK(aimed_line != NULL);
    CHECK(rolled_line != NULL);

    // The whole point: same resolved limb, different events.
    CHECK(aimed_line != rolled_line);

    // And the version says the contract changed.
    // The current contract version, not a literal that goes stale: this
    // assertion said 2 while the stream said 3, and nothing noticed because
    // the test was never registered.
    {
        char expected[32];
        snprintf(expected, sizeof(expected), "\"schema\":%d",
            COMBAT_OBSERVER_SCHEMA);
        CHECK(strstr(trace, expected) != NULL);
    }
}

TEST(hit_miss_critical_and_terminal_values_are_not_conflated)
{
    char trace[TRACE_SIZE];
    int damage[DAMAGE_TYPE_COUNT] = { [DAMAGE_TYPE_NORMAL] = 12 };
    unsigned long long hit;
    unsigned long long miss;

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    hit = combat_observer_attack_started("fighter", "opponent", 2, 0, 0, false);
    combat_observer_attack_resolved(hit, "fighter", "opponent", 2, 0,
        0x2 | 0x4, 1);
    miss = combat_observer_attack_started("opponent", "fighter", 2, 0, 0, false);
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
        50, 0, 10, 10, 2, true, true);
    combat_observer_close();
    second_with = random_rand();

    CHECK_INT(first_with, first_without);
    CHECK_INT(second_with, second_without);
}

#include "game/combat.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/skill.h"
#include "game/stat.h"
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
    combat_observer_command_requested("fighter", "object", "opponent", 4242, HIT_LOC_TORSO);
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
    combat_observer_command_requested("fighter", "location", "NULL", 99, HIT_LOC_TORSO);
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
    combat_observer_command_requested("fighter", "location", "NULL", 361700864, HIT_LOC_TORSO);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"target_kind\":\"location\"") != NULL);
    CHECK(strstr(trace, "\"target\":\"NULL\"") != NULL);
    // A packed location names its tile reproducibly across runs, which is why
    // it is recorded as the target rather than a screen position.
    CHECK(strstr(trace, "\"target_location\":361700864") != NULL);
}

TEST(every_goal_state_decides_acceptance_the_same_way)
{
    // All eight combinations, because this is the judgement a replay trusts:
    // a refusal recorded as an acceptance is re-submitted and diverges.
    //
    //                                     busy_before  busy_after  same_goal
    // nothing queued afterwards -> the dispatch took nothing, whatever it did.
    CHECK(!combat_observer_command_accepted(false, false, false));
    CHECK(!combat_observer_command_accepted(false, false, true));
    CHECK(!combat_observer_command_accepted(true, false, false));
    CHECK(!combat_observer_command_accepted(true, false, true));

    // Idle before, busy after: a goal appeared. Taken.
    CHECK(combat_observer_command_accepted(false, true, false));
    CHECK(combat_observer_command_accepted(false, true, true));

    // Busy before and after with a *different* goal: the old one was replaced.
    CHECK(combat_observer_command_accepted(true, true, false));

    // Busy before and after with the *same* goal: the dispatch refused, or
    // re-issued what was already running. This is the case that matters -- an
    // actor mid-attack whose next click is rejected still looks busy, and
    // reading "busy" alone as acceptance would record a refusal as taken.
    CHECK(!combat_observer_command_accepted(true, true, true));
}

TEST(a_refused_command_leaves_the_goal_state_untouched)
{
    // The shape of a real rejection, stated as the predicate sees it. Every
    // early return in the dispatch exits before a goal is queued, so the
    // actor's state is identical either side -- which is precisely the
    // `same_goal` case above when it was already busy, and the `!busy_after`
    // case when it was idle. Both must read false.
    CHECK(!combat_observer_command_accepted(false, false, false));
    CHECK(!combat_observer_command_accepted(true, true, true));
}

TEST(a_command_waits_for_the_mode_it_was_recorded_in)
{
    // Measured 2026-09-14: the same click is an attack inside combat mode and
    // something else outside it. A replay that ignores the mode does not fail --
    // it performs a different action and reports it accepted, which is worse
    // than stalling. All four states, so the gate cannot quietly invert.
    CHECK(replay_ui_mode_matches(0, false)); // recorded before combat, no combat now
    CHECK(!replay_ui_mode_matches(0, true)); // recorded before combat, combat now: wait
    CHECK(replay_ui_mode_matches(2, true)); // recorded in combat, combat now
    CHECK(!replay_ui_mode_matches(2, false)); // recorded in combat, none now: wait

    // Turn 1 is the first combat turn, so it must behave like turn 2 and not
    // like turn 0 -- an off-by-one here would submit every combat command
    // before combat started.
    CHECK(replay_ui_mode_matches(1, true));
    CHECK(!replay_ui_mode_matches(1, false));
}

// --- real-time session A: the rows a real-time script needs --------------

#define REPLAY_SCRIPT "./combat-observer-replay.txt"

static bool load_script(const char* text)
{
    FILE* stream = fopen(REPLAY_SCRIPT, "wb");
    bool loaded;

    if (stream == NULL) return false;
    fputs(text, stream);
    fclose(stream);
    loaded = replay_ui_load(REPLAY_SCRIPT);
    remove(REPLAY_SCRIPT);
    return loaded;
}

static bool row_is(int index, const char* expected)
{
    char described[256];

    if (!replay_ui_describe(index, described, sizeof(described))) return false;
    if (strcmp(described, expected) != 0) {
        fprintf(stderr, "  row %d: %s\n  wanted: %s\n", index, described, expected);
        return false;
    }
    return true;
}

TEST(every_existing_row_form_still_loads_as_it_did)
{
    // The three kinds every recorded script uses, with and without an aim. A
    // parser rewrite that changed any of these would break every replay made
    // since card O2 without a single one failing to load.
    CHECK(load_script(
        "# a comment\n"
        "0 key combat-mode\n"
        "2 object G_00000084_0104_4040_8000_000000000401\n"
        "2 object G_00000084_0104_4040_8000_000000000401 1\n"
        "3 location 361533167198256 3\n"));
    CHECK_INT(replay_ui_count(), 4);
    CHECK(row_is(0, "turn=0 now=0 kind=key target=combat-mode loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(1, "turn=2 now=0 kind=object target=G_00000084_0104_4040_8000_000000000401 loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(2, "turn=2 now=0 kind=object target=G_00000084_0104_4040_8000_000000000401 loc=0 aim=1 spell=0 cast=- wait=0"));
    CHECK(row_is(3, "turn=3 now=0 kind=location target= loc=361533167198256 aim=3 spell=0 cast=- wait=0"));
}

TEST(the_real_time_rows_load_into_what_they_say)
{
    CHECK(load_script(
        "0 wield G_00000084_0502_4040_8000_000000000205\n"
        "0 cast 55 object G_A09DCD63_7A15_D411_8F1D_00E02920220C\n"
        "0 cast 60 self\n"
        "0 cast 55 location 361533167198256\n"
        "0 wait 250\n"
        "0 now object G_00000084_0104_4040_8000_000000000401 2\n"
        "0 key character-points\n"
        "0 learn 55\n"));
    CHECK_INT(replay_ui_count(), 8);
    CHECK(row_is(0, "turn=0 now=0 kind=wield target=G_00000084_0502_4040_8000_000000000205 loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(1, "turn=0 now=0 kind=cast target=G_A09DCD63_7A15_D411_8F1D_00E02920220C loc=0 aim=0 spell=55 cast=object wait=0"));
    CHECK(row_is(2, "turn=0 now=0 kind=cast target= loc=0 aim=0 spell=60 cast=self wait=0"));
    CHECK(row_is(3, "turn=0 now=0 kind=cast target= loc=361533167198256 aim=0 spell=55 cast=location wait=0"));
    CHECK(row_is(4, "turn=0 now=0 kind=wait target= loc=0 aim=0 spell=0 cast=- wait=250"));
    CHECK(row_is(5, "turn=0 now=1 kind=object target=G_00000084_0104_4040_8000_000000000401 loc=0 aim=2 spell=0 cast=- wait=0"));
    CHECK(row_is(6, "turn=0 now=0 kind=key target=character-points loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(7, "turn=0 now=0 kind=learn target= loc=0 aim=0 spell=55 cast=- wait=0"));
}

TEST(the_throw_rows_load_into_what_they_say)
{
    // Real-time session C: stock the character with grenades, then throw one
    // at a tile - on the queue, and during a recovery.
    CHECK(load_script(
        "0 give 15171 6\n"
        "0 throw 15171 361533167198256\n"
        "0 now throw 15171 361533167198256\n"
        "0 wield 6053\n"
        "0 now skill 10 G_A09DCD63_7A15_D411_8F1D_00E02920220C\n"
        "0 raise 4\n"
        "0 skill-level 13 1\n"
        "0 stat-level 5 8\n"));
    CHECK_INT(replay_ui_count(), 8);
    CHECK(row_is(0, "turn=0 now=0 kind=give target= loc=0 aim=0 spell=0 cast=- wait=0 proto=15171 count=6"));
    CHECK(row_is(1, "turn=0 now=0 kind=throw target= loc=361533167198256 aim=0 spell=0 cast=- wait=0 proto=15171 count=0"));
    CHECK(row_is(2, "turn=0 now=1 kind=throw target= loc=361533167198256 aim=0 spell=0 cast=- wait=0 proto=15171 count=0"));
    // A wield by prototype, for a carried item: the Revolver.
    CHECK(row_is(3, "turn=0 now=0 kind=wield target=6053 loc=0 aim=0 spell=0 cast=- wait=0"));
    // Heal (10) on Virgil, during a recovery.
    CHECK(row_is(4, "turn=0 now=1 kind=skill target=G_A09DCD63_7A15_D411_8F1D_00E02920220C loc=0 aim=0 spell=0 cast=- wait=0 skill=10"));
    // One point into Intelligence.
    CHECK(row_is(5, "turn=0 now=0 kind=raise target= loc=0 aim=0 spell=0 cast=- wait=0 stat=4"));
    // Criticals CR-2: Firearms (13) at one point, a starting character's.
    CHECK(row_is(6, "turn=0 now=0 kind=skill-level target= loc=0 aim=0 spell=0 cast=- wait=0 skill=13 points=1"));
    // And Perception (5) at 8.
    CHECK(row_is(7, "turn=0 now=0 kind=stat-level target= loc=0 aim=0 spell=0 cast=- wait=0 stat=5 value=8"));

    CHECK(!load_script("0 give 15171\n")); // how many
    CHECK(!load_script("0 give 15171 0\n")); // nothing is not a gift
    CHECK(!load_script("0 give grenade 2\n"));
    CHECK(!load_script("0 throw 15171\n")); // where
    CHECK(!load_script("0 throw 15171 0\n")); // not a tile
    CHECK(!load_script("0 throw 15171 361533167198256 extra\n"));
    CHECK(!load_script("0 skill 10\n")); // on what
    CHECK(!load_script("0 skill 99 G_X\n")); // no such skill
    CHECK(!load_script("0 skill heal G_X\n"));
    CHECK(!load_script("0 raise 8\n")); // not a primary stat
    CHECK(!load_script("0 raise\n"));
    CHECK(!load_script("0 skill-level 13\n")); // how many points
    CHECK(!load_script("0 skill-level 13 6\n")); // at most 5
    CHECK(!load_script("0 skill-level 16 1\n")); // no such skill
    CHECK(!load_script("0 stat-level 8 8\n")); // not a primary stat
    CHECK(!load_script("0 stat-level 5 0\n")); // 1-20
}

TEST(the_called_shot_rows_load_into_what_they_say)
{
    // 2026-09-25: prowling, a click on the panel, and an attack that takes
    // the panel's aim rather than forcing one (aim -1).
    CHECK(load_script(
        "0 prowl on\n"
        "0 prowl off\n"
        "0 key skills\n"
        "0 panel 1\n"
        "0 object G_00000084_0104_4040_8000_000000000401 panel\n"));
    CHECK_INT(replay_ui_count(), 5);
    CHECK(row_is(0, "turn=0 now=0 kind=prowl target= loc=0 aim=0 spell=0 cast=- wait=0 prowl=1"));
    CHECK(row_is(1, "turn=0 now=0 kind=prowl target= loc=0 aim=0 spell=0 cast=- wait=0 prowl=0"));
    CHECK(row_is(2, "turn=0 now=0 kind=key target=skills loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(3, "turn=0 now=0 kind=panel target= loc=0 aim=1 spell=0 cast=- wait=0"));
    CHECK(row_is(4, "turn=0 now=0 kind=object target=G_00000084_0104_4040_8000_000000000401 loc=0 aim=-1 spell=0 cast=- wait=0"));

    CHECK(!load_script("0 prowl\n")); // on or off
    CHECK(!load_script("0 prowl maybe\n"));
    CHECK(!load_script("0 panel 4\n")); // not a hit location
    CHECK(!load_script("0 panel head\n"));
    CHECK(!load_script("0 object G_X panel extra\n"));
}

TEST(the_persistence_rows_load_into_what_they_say)
{
    // Reload prototype session 3 (2026-09-26): saving, looking, and moving a
    // gun between owners.
    CHECK(load_script(
        "0 save Slot0009\n"
        "0 inspect guns\n"
        "0 transfer 6065 G_A09DCD63_7A15_D411_8F1D_00E02920220C\n"
        "0 take 6065 G_A09DCD63_7A15_D411_8F1D_00E02920220C\n"
        "0 drop 6101\n"
        "0 pickup 6101\n"));
    CHECK_INT(replay_ui_count(), 6);
    CHECK(row_is(0, "turn=0 now=0 kind=save target=Slot0009 loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(1, "turn=0 now=0 kind=inspect target= loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(2, "turn=0 now=0 kind=transfer target=G_A09DCD63_7A15_D411_8F1D_00E02920220C loc=0 aim=0 spell=0 cast=- wait=0 proto=6065 count=0"));
    CHECK(row_is(3, "turn=0 now=0 kind=take target=G_A09DCD63_7A15_D411_8F1D_00E02920220C loc=0 aim=0 spell=0 cast=- wait=0 proto=6065 count=0"));
    CHECK(row_is(4, "turn=0 now=0 kind=drop target= loc=0 aim=0 spell=0 cast=- wait=0 proto=6101 count=0"));
    CHECK(row_is(5, "turn=0 now=0 kind=pickup target= loc=0 aim=0 spell=0 cast=- wait=0 proto=6101 count=0"));

    CHECK(!load_script("0 save Slot00099\n")); // more than eight characters
    CHECK(!load_script("0 inspect\n"));
    CHECK(!load_script("0 inspect everything\n"));
    // 2026-10-12: `inspect awareness`, for prowling.
    CHECK(load_script("0 inspect awareness\n"));
    CHECK(row_is(0, "turn=0 now=0 kind=inspect target= loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(!load_script("0 transfer 6065\n")); // to whom
    CHECK(!load_script("0 drop flintlock\n"));
}

TEST(the_jam_row_loads_alone)
{
    // Criticals CR-4 (2026-10-10): a test row that jams the wielded gun,
    // `jam gun` as `inspect guns`.
    CHECK(load_script("0 jam gun\n1 jam gun\n"));
    CHECK_INT(replay_ui_count(), 2);
    CHECK(row_is(0, "turn=0 now=0 kind=jam target= loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(1, "turn=1 now=0 kind=jam target= loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(!load_script("0 jam\n"));
    CHECK(!load_script("0 jam 6100\n"));
    CHECK(!load_script("0 jam gun now\n"));
}

TEST(the_display_and_creature_rows_load_into_what_they_say)
{
    // The Combat Rebalance displays (2026-10-21): hover and screenshot; and
    // creature profiles (2026-10-21): a critter from its prototype, named for
    // later rows, and an invulnerable player.
    CHECK(load_script(
        "1 hover G_00000084_1104_4040_8000_000000000411\n"
        "1 screenshot hover-full-turn\n"
        "0 spawn 28341 3 -2 skeleton\n"
        "1 object @skeleton 2\n"
        "0 invulnerable on\n"
        "1 throw 15152 @skeleton\n"));
    CHECK_INT(replay_ui_count(), 6);
    CHECK(row_is(0, "turn=1 now=0 kind=hover target=G_00000084_1104_4040_8000_000000000411 loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(1, "turn=1 now=0 kind=screenshot target=hover-full-turn loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(2, "turn=0 now=0 kind=spawn target=skeleton loc=0 aim=0 spell=0 cast=- wait=0 proto=28341 dx=3 dy=-2"));
    CHECK(row_is(3, "turn=1 now=0 kind=object target=@skeleton loc=0 aim=2 spell=0 cast=- wait=0"));
    CHECK(row_is(4, "turn=0 now=0 kind=invulnerable target= loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(row_is(5, "turn=1 now=0 kind=throw target=@skeleton loc=0 aim=0 spell=0 cast=- wait=0 proto=15152 count=0"));
    CHECK(!load_script("0 hover\n"));
    CHECK(!load_script("0 spawn 28341 3 skeleton\n"));
    CHECK(!load_script("0 spawn 11 0 0 nothing\n"));
    CHECK(!load_script("0 spawn 28341 3 -2 skeleton extra\n"));
    CHECK(!load_script("0 invulnerable off\n"));
    CHECK(!load_script("0 invulnerable on now\n"));
}

TEST(the_await_death_row_names_the_critter_it_waits_for)
{
    // Creature profiles (2026-10-22): a hold until a critter is dead, so a
    // playtest's next creature comes when the last has died.
    CHECK(load_script("0 spawn 28341 3 -2 skeleton\n1 await-death @skeleton\n"));
    CHECK_INT(replay_ui_count(), 2);
    CHECK(row_is(1, "turn=1 now=0 kind=await-death target=@skeleton loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(!load_script("0 await-death\n"));
    CHECK(!load_script("0 await-death @skeleton now\n"));
}

TEST(the_remove_row_names_the_object_it_takes_off_the_map)
{
    // Creature profiles (2026-10-22): a playtest clears the range's
    // bystanders, so each creature is fought alone.
    CHECK(load_script("0 remove G_00000084_0006_4040_8000_000000000600\n"));
    CHECK_INT(replay_ui_count(), 1);
    CHECK(row_is(0, "turn=0 now=0 kind=remove target=G_00000084_0006_4040_8000_000000000600 loc=0 aim=0 spell=0 cast=- wait=0"));
    CHECK(!load_script("0 remove\n"));
    CHECK(!load_script("0 remove G_00000084_0006_4040_8000_000000000600 now\n"));
}

TEST(the_afflict_row_names_a_debuff_critical)
{
    // Magic MG-2 (2026-10-11): a test row that gives the player a debuff
    // critical's effect - the effect's bit is carried as the proto.
    CHECK(load_script("0 afflict unfocused\n1 afflict exposed\n"));
    CHECK_INT(replay_ui_count(), 2);
    CHECK(row_is(0, "turn=0 now=0 kind=afflict target= loc=0 aim=0 spell=0 cast=- wait=0 proto=64 count=0"));
    CHECK(row_is(1, "turn=1 now=0 kind=afflict target= loc=0 aim=0 spell=0 cast=- wait=0 proto=32 count=0"));
    CHECK(!load_script("0 afflict\n"));
    CHECK(!load_script("0 afflict winded\n"));
    CHECK(!load_script("0 afflict unfocused now\n"));
}

TEST(the_poison_row_sets_a_level_on_the_player_or_a_critter)
{
    // 2026-10-12: a test row for poison's pace; the level is carried as the
    // count.
    CHECK(load_script("0 poison 100\n1 poison 550 G_00000084_1104_4040_8000_000000000411\n"));
    CHECK_INT(replay_ui_count(), 2);
    CHECK(row_is(0, "turn=0 now=0 kind=poison target= loc=0 aim=0 spell=0 cast=- wait=0 proto=0 count=100"));
    CHECK(row_is(1, "turn=1 now=0 kind=poison target=G_00000084_1104_4040_8000_000000000411 loc=0 aim=0 spell=0 cast=- wait=0 proto=0 count=550"));
    CHECK(!load_script("0 poison\n"));
    CHECK(!load_script("0 poison 0\n"));
    CHECK(!load_script("0 poison 1001\n"));
}

TEST(the_use_row_names_a_prototype_and_perhaps_a_target)
{
    // 2026-10-13: a restorative used by the player, on themselves or another.
    CHECK(load_script("0 use 10094\n1 use 10079 G_00000084_1104_4040_8000_000000000411\n"));
    CHECK_INT(replay_ui_count(), 2);
    CHECK(row_is(0, "turn=0 now=0 kind=use target= loc=0 aim=0 spell=0 cast=- wait=0 proto=10094 count=0"));
    CHECK(row_is(1, "turn=1 now=0 kind=use target=G_00000084_1104_4040_8000_000000000411 loc=0 aim=0 spell=0 cast=- wait=0 proto=10079 count=0"));
    CHECK(!load_script("0 use\n"));
    CHECK(!load_script("0 use potion\n"));
    CHECK(!load_script("0 use 10094 G_X extra\n"));
}

TEST(the_xp_train_and_inspect_xp_rows_load)
{
    // Release 1, R1 (2026-10-13): test rows for UAP's level-cap and Educator
    // add-ons. The amount is carried as the count; the training too.
    CHECK(load_script("0 xp 2500000\n0 train 3 2\n0 inspect xp\n"));
    CHECK_INT(replay_ui_count(), 3);
    CHECK(row_is(0, "turn=0 now=0 kind=xp target= loc=0 aim=0 spell=0 cast=- wait=0 proto=0 count=2500000"));
    CHECK(!load_script("0 xp\n"));
    CHECK(!load_script("0 xp 0\n"));
    CHECK(!load_script("0 train 3\n"));
    CHECK(!load_script("0 train 12 1\n"));
    CHECK(!load_script("0 train 3 4\n"));
    CHECK(!load_script("0 inspect level\n"));
}

TEST(the_wear_row_loads_with_or_without_a_wearer)
{
    // Armor milestone 3 (2026-10-03): armor on the player, or on a critter.
    CHECK(load_script(
        "0 wear 8095\n"
        "0 wear 8213 G_00000084_1104_4040_8000_000000000411\n"));
    CHECK_INT(replay_ui_count(), 2);
    CHECK(row_is(0, "turn=0 now=0 kind=wear target= loc=0 aim=0 spell=0 cast=- wait=0 proto=8095 count=0"));
    CHECK(row_is(1, "turn=0 now=0 kind=wear target=G_00000084_1104_4040_8000_000000000411 loc=0 aim=0 spell=0 cast=- wait=0 proto=8213 count=0"));

    CHECK(!load_script("0 wear\n"));
    CHECK(!load_script("0 wear platemail\n"));
    CHECK(!load_script("0 wear 8095 G_X extra\n"));
}

TEST(a_row_the_parser_cannot_read_refuses_the_whole_file)
{
    // Refused, never skipped: a replay missing a row diverges from what it
    // reproduces and reads as a gameplay difference.
    CHECK(!load_script("0 object G_X 9\n")); // not a hit location
    CHECK(!load_script("0 fly G_X\n")); // no such kind
    CHECK(!load_script("0 cast 55\n")); // a cast with no target
    CHECK(!load_script("0 cast 55 object\n")); // an object with no id
    CHECK(!load_script("0 wait -5\n")); // time does not run back
    CHECK(!load_script("0 wait soon\n"));
    CHECK(!load_script("0 key combat-mode extra\n")); // nothing may trail a row
    CHECK(!load_script("zero key combat-mode\n"));
    CHECK(!load_script("0 learn\n"));
    CHECK(!load_script("0 learn harm\n"));
    // Magic MG-1: `force` is the only word that may follow.
    CHECK(load_script("0 learn 59 force\n"));
    CHECK_INT(replay_ui_count(), 1);
    CHECK(!load_script("0 learn 59 now\n"));
    CHECK_INT(replay_ui_count(), 0);
}

TEST(a_tile_step_a_cast_request_and_a_weapon_change_are_each_recorded)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_actor_moved("fighter", 1000, 1001);
    combat_observer_cast_requested("fighter", 55, "object", "virgil", 0);
    combat_observer_cast_requested("fighter", 55, "location", "NULL", 361533167198256LL);
    // The first sample is the starting weapon, not a change; the same weapon
    // again is nothing; a different one, or an empty slot, is a change.
    combat_observer_wield_observed("fighter", "sword");
    combat_observer_wield_observed("fighter", "sword");
    combat_observer_wield_observed("fighter", "bow");
    combat_observer_wield_observed("fighter", "NULL");
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"actor.moved\",\"turn\":null,\"action_id\":null,\"actor\":\"fighter\",\"from_location\":1000,\"to_location\":1001") != NULL);
    CHECK(strstr(trace, "\"spell\":55,\"target_kind\":\"object\",\"target\":\"virgil\",\"target_location\":0") != NULL);
    CHECK(strstr(trace, "\"target_kind\":\"location\",\"target\":\"NULL\",\"target_location\":361533167198256") != NULL);
    CHECK_INT(count_events(trace, "\"event\":\"wield.changed\""), 2);
    CHECK(strstr(trace, "\"weapon\":\"sword\"") == NULL);
    CHECK(strstr(trace, "\"weapon\":\"bow\"") != NULL);
    CHECK(strstr(trace, "\"weapon\":\"NULL\"") != NULL);
}

// --- card O2: the actions that change the interaction mode ---------------

static CombatObserverMode mode_of(bool combat_mode, bool turn_based,
    bool turn_based_active)
{
    CombatObserverMode mode;

    mode.combat_mode = combat_mode;
    mode.turn_based = turn_based;
    mode.turn_based_active = turn_based_active;
    return mode;
}

TEST(every_mode_state_decides_acceptance_the_same_way)
{
    int before;
    int after;

    // All 64 transitions between the three axes. Exhaustive because this is the
    // same class of judgement as command acceptance: a refused toggle recorded
    // as accepted would be re-submitted by a replay and put the run in a state
    // the recording never reached. Identity is refusal; everything else is not.
    for (before = 0; before < 8; before++) {
        for (after = 0; after < 8; after++) {
            CombatObserverMode a = mode_of((before & 1) != 0,
                (before & 2) != 0, (before & 4) != 0);
            CombatObserverMode b = mode_of((after & 1) != 0,
                (after & 2) != 0, (after & 4) != 0);

            if (before == after) {
                CHECK(!combat_observer_mode_accepted(&a, &b));
            } else {
                CHECK(combat_observer_mode_accepted(&a, &b));
            }
        }
    }
}

TEST(the_turn_based_setting_and_a_running_encounter_are_not_the_same_axis)
{
    // Pressing SPACE outside combat changes the setting and starts nothing.
    // Collapsing the two into one boolean would record that press as refused,
    // and a replay would press it again -- turning it back off.
    CombatObserverMode before = mode_of(false, false, false);
    CombatObserverMode after = mode_of(false, true, false);

    CHECK(combat_observer_mode_accepted(&before, &after));

    // And the converse: an encounter beginning while the setting already stood
    // is a change too, one no keypress caused.
    before = mode_of(true, true, false);
    after = mode_of(true, true, true);
    CHECK(combat_observer_mode_accepted(&before, &after));
}

TEST(a_mode_action_records_its_request_before_its_resolution)
{
    char trace[TRACE_SIZE];
    CombatObserverMode before = mode_of(false, true, false);
    CombatObserverMode after = mode_of(true, true, true);

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_mode_requested("fighter", "combat-mode", &before);
    combat_observer_mode_resolved("fighter", "combat-mode", true, &after);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"mode.requested\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"mode.resolved\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"mode.requested\"")
        < strstr(trace, "\"event\":\"mode.resolved\""));
    CHECK(strstr(trace, "\"action\":\"combat-mode\"") != NULL);
    // All three axes, on both events: a replay reads them to decide whether it
    // still has work to do before it can submit.
    CHECK(strstr(trace, "\"combat_mode\":false") != NULL);
    CHECK(strstr(trace, "\"combat_mode\":true") != NULL);
    CHECK(strstr(trace, "\"turn_based_active\":true") != NULL);
}

TEST(a_refused_mode_action_is_not_recorded_as_accepted)
{
    char trace[TRACE_SIZE];
    // "You cannot exit combat-mode when under attack": the toggle runs, says so
    // on the message window, and changes nothing. It returns void, so the mode
    // read either side is the only signal there is.
    CombatObserverMode unchanged = mode_of(true, true, true);

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_mode_requested("fighter", "combat-mode", &unchanged);
    combat_observer_mode_resolved("fighter", "combat-mode",
        combat_observer_mode_accepted(&unchanged, &unchanged), &unchanged);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"accepted\":false") != NULL);
    CHECK(strstr(trace, "\"accepted\":true") == NULL);
}

TEST(the_starting_mode_is_not_recorded_as_a_change)
{
    char trace[TRACE_SIZE];
    CombatObserverMode start = mode_of(false, true, false);

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    // First sight, then the same mode again for several frames.
    combat_observer_mode_observed("fighter", &start);
    combat_observer_mode_observed("fighter", &start);
    combat_observer_mode_observed("fighter", &start);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    // A transition at the head of every trace, and one per quiet frame, would
    // bury the changes that matter under thousands that do not.
    CHECK(strstr(trace, "\"event\":\"mode.changed\"") == NULL);
}

TEST(every_end_turn_state_decides_acceptance_the_same_way)
{
    int state;

    // All eight combinations of the three conditions the shipped UI tests
    // before it calls `combat_turn_based_next_subturn`. Exhaustive for the same
    // reason the other two judgements are: an end-turn recorded as taken when
    // the engine ignored the press would be re-pressed by a replay in a state
    // the recording never reached.
    //
    // Unlike the mode toggles this is a conjunction, so the interesting content
    // is that *each* condition alone is enough to refuse -- which is what the
    // loop asserts, rather than the shape of the expression.
    for (state = 0; state < 8; state++) {
        bool turn_in_progress = (state & 1) != 0;
        bool pc_turn = (state & 2) != 0;
        bool actor_ready = (state & 4) != 0;

        CHECK(combat_observer_end_turn_accepted(turn_in_progress, pc_turn,
                  actor_ready)
            == (turn_in_progress && pc_turn && actor_ready));
    }

    // Named, because these three are the refusals a replay actually meets and
    // reading any one of them wrongly is a silent divergence rather than a
    // crash.
    CHECK(!combat_observer_end_turn_accepted(false, true, true));
    CHECK(!combat_observer_end_turn_accepted(true, false, true));
    CHECK(!combat_observer_end_turn_accepted(true, true, false));
    CHECK(combat_observer_end_turn_accepted(true, true, true));
}

TEST(an_end_turn_records_its_request_before_its_resolution)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    // Thirteen action points held, then a fresh turn's worth back: the shape of
    // the press whose absence froze the 2026-09-14 replay in turn 4.
    combat_observer_end_turn_requested("fighter", 13);
    combat_observer_end_turn_resolved("fighter", true, 0, false);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"end_turn.requested\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"end_turn.resolved\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"end_turn.requested\"")
        < strstr(trace, "\"event\":\"end_turn.resolved\""));
    // The action name a replay script writes as its `key` row. Shared with the
    // mode events on purpose: one vocabulary for every named player action.
    CHECK(strstr(trace, "\"action\":\"end-turn\"") != NULL);
    // The points held at the press. A record without them made a fight that
    // was stuck look like a fight that had finished.
    CHECK(strstr(trace, "\"action_points\":13") != NULL);
    CHECK(strstr(trace, "\"pc_turn\":false") != NULL);
}

TEST(a_refused_end_turn_is_not_recorded_as_accepted)
{
    char trace[TRACE_SIZE];

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    // The press that arrives on somebody else's turn. The engine ignores it and
    // says nothing, so the recorded judgement is the only signal there is.
    combat_observer_end_turn_requested("fighter", 13);
    combat_observer_end_turn_resolved("fighter",
        combat_observer_end_turn_accepted(true, false, true), 13, false);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"accepted\":false") != NULL);
    CHECK(strstr(trace, "\"accepted\":true") == NULL);
    // Refused, so nothing was spent: the points are the same on both events.
    CHECK(strstr(trace, "\"action_points\":13") != NULL);
    CHECK(strstr(trace, "\"action_points\":0") == NULL);
}

TEST(the_terminal_latch_answers_only_after_a_critter_goes_down)
{
    int components[DAMAGE_TYPE_COUNT] = { 0 };

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    // Nothing has happened yet, so a harness waiting on this must keep waiting.
    CHECK(!combat_observer_terminal_seen());

    // A hit that hurts but does not end anything.
    combat_observer_damage_committed("fighter", "opponent", components, 0,
        5, 0, 5, 30, 30, false, false);
    CHECK(!combat_observer_terminal_seen());

    combat_observer_damage_committed("fighter", "opponent", components, 0,
        8, 5, 32000, 30, 30, true, false);
    CHECK(combat_observer_terminal_seen());
    combat_observer_close();
}

TEST(an_incapacitation_ends_the_duel_as_much_as_a_death_does)
{
    int components[DAMAGE_TYPE_COUNT] = { 0 };

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_damage_committed("opponent", "fighter", components, 0,
        9, 3, 12, 30, 0, false, true);
    // Both are terminal outcomes in the scenario, so both must stop a run that
    // is waiting for one. Reading only `dead` would hang until the frame cap.
    CHECK(combat_observer_terminal_seen());
    combat_observer_close();
}

TEST(the_latch_is_set_after_the_event_is_written)
{
    char trace[TRACE_SIZE];
    int components[DAMAGE_TYPE_COUNT] = { 0 };

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_damage_committed("fighter", "opponent", components, 0,
        8, 5, 32000, 30, 30, true, false);
    combat_observer_close();

    // The ordering that matters: a harness exits on this latch, so the event
    // that caused the exit must already be in the trace. If it were latched
    // first, an eager exit could cut its own reason out of the record -- and
    // O1 refuses a truncated run, so the saved time would buy nothing.
    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"dead\":true") != NULL);
    CHECK(combat_observer_terminal_seen());
}

TEST(the_event_counter_advances_only_when_something_is_written)
{
    unsigned int before;
    int components[DAMAGE_TYPE_COUNT] = { 0 };

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    before = combat_observer_event_count();
    combat_observer_damage_committed("fighter", "opponent", components, 0,
        1, 0, 1, 30, 30, false, false);
    // A harness watches this stop moving rather than guessing how long a death
    // animation lasts, so it has to move when -- and only when -- the trace does.
    CHECK(combat_observer_event_count() > before);

    before = combat_observer_event_count();
    CHECK(combat_observer_event_count() == before);
    combat_observer_close();
}

TEST(a_mode_change_no_player_action_claimed_is_recorded_as_uncaused)
{
    char trace[TRACE_SIZE];
    CombatObserverMode start = mode_of(false, true, false);
    CombatObserverMode fighting = mode_of(true, true, true);

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_mode_observed("fighter", &start);
    // An NPC attacks: `anim.c` activates combat mode for both sides and the
    // encounter begins. No key was pressed, so the recorder that only watches
    // keys and clicks sees nothing -- which is exactly why the first replay
    // could not reach the state its commands were recorded in.
    combat_observer_mode_observed("fighter", &fighting);
    combat_observer_mode_observed("fighter", &fighting);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"mode.changed\"") != NULL);
    CHECK(strstr(trace, "\"cause\":\"game\"") != NULL);
    // Once, not once per frame while the state persists.
    CHECK(count_events(trace, "\"event\":\"mode.changed\"") == 1);
}

TEST(a_mode_change_the_player_asked_for_is_not_also_recorded_as_uncaused)
{
    char trace[TRACE_SIZE];
    CombatObserverMode before = mode_of(false, true, false);
    CombatObserverMode after = mode_of(true, true, true);

    clean();
    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_mode_observed("fighter", &before);
    combat_observer_mode_requested("fighter", "combat-mode", &before);
    combat_observer_mode_resolved("fighter", "combat-mode", true, &after);
    // The watcher runs on the very next frame and sees the new mode. It must
    // recognise it as the one the player just asked for, or every keypress
    // would be recorded twice and a replay would press twice.
    combat_observer_mode_observed("fighter", &after);
    combat_observer_close();

    CHECK(read_trace(trace, sizeof(trace)));
    CHECK(strstr(trace, "\"event\":\"mode.changed\"") == NULL);
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
    const char* target, int skill, int hit_location,
    int requested_hit_location, bool aimed);

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
    const char* target, int skill, int hit_location,
    int requested_hit_location, bool aimed)
{
    if (inject_observer_draw) {
        (void)random_rand();
    }
    return __real_combat_observer_attack_started(attacker, target, skill,
        hit_location, requested_hit_location, aimed);
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
    // Card S1: the limb the blow resolved against, and whether the engine
    // considered it aimed. Without these the comparison could agree while the
    // attack landed somewhere else entirely, which is the difference the
    // observer was taught to report.
    int resolved_hit_loc;
    bool aimed;
    // Card S1, session 8: the lasting consequences. A called shot's whole
    // point is the injury it can cause - `OCF_CRIPPLED_ARMS_ONE` and its
    // neighbours live in the target's critter flags - so a comparison that
    // agreed on damage while disagreeing on whether an arm was crippled would
    // be agreeing about the wrong thing.
    unsigned int target_critter_flags;
    int target_fatigue_damage;
    unsigned int attacker_critter_flags;
    int attacker_fatigue_damage;
} ObservedAttack;

static void drive_one_attack_at(bool observe, int hit_loc, ObservedAttack* out)
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
    combat.hit_loc = hit_loc;

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
    out->resolved_hit_loc = combat.hit_loc;
    out->aimed = (combat.flags & CF_AIM) != 0;
    out->target_critter_flags = (unsigned int)obj_field_int32_get(target, OBJ_F_CRITTER_FLAGS);
    out->target_fatigue_damage = critter_fatigue_damage_get(target);
    out->attacker_critter_flags = (unsigned int)obj_field_int32_get(attacker, OBJ_F_CRITTER_FLAGS);
    out->attacker_fatigue_damage = critter_fatigue_damage_get(attacker);

    // Subsequent RNG state, which is what a hidden draw would move.
    for (index = 0; index < 4; index++) {
        out->rng_after[index] = random_rand();
    }
    stub_environment = false;

    obj_harness_destroy(attacker);
    obj_harness_destroy(target);
}

// The unaimed case, which is what every caller before card S1 meant.
static void drive_one_attack(bool observe, ObservedAttack* out)
{
    drive_one_attack_at(observe, HIT_LOC_TORSO, out);
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
    CHECK_INT(a->resolved_hit_loc, b->resolved_hit_loc);
    CHECK_INT(a->aimed ? 1 : 0, b->aimed ? 1 : 0);
    CHECK_INT((int)a->target_critter_flags, (int)b->target_critter_flags);
    CHECK_INT(a->target_fatigue_damage, b->target_fatigue_damage);
    CHECK_INT((int)a->attacker_critter_flags, (int)b->attacker_critter_flags);
    CHECK_INT(a->attacker_fatigue_damage, b->attacker_fatigue_damage);
}

// Non-aborting, for the negative controls: a control has to be able to observe
// a difference rather than die on the first one.
static bool attacks_differ(const ObservedAttack* a, const ObservedAttack* b)
{
    int index;

    if (a->rc != b->rc) return true;
    if (a->flags != b->flags) return true;
    if (a->total_dam != b->total_dam) return true;
    if (a->target_hp_damage != b->target_hp_damage) return true;
    if (a->resolved_hit_loc != b->resolved_hit_loc) return true;
    if (a->aimed != b->aimed) return true;
    if (a->target_critter_flags != b->target_critter_flags) return true;
    if (a->target_fatigue_damage != b->target_fatigue_damage) return true;
    if (a->attacker_critter_flags != b->attacker_critter_flags) return true;
    if (a->attacker_fatigue_damage != b->attacker_fatigue_damage) return true;
    for (index = 0; index < DAMAGE_TYPE_COUNT; index++) {
        if (a->dam[index] != b->dam[index]) return true;
    }
    for (index = 0; index < 4; index++) {
        if (a->rng_after[index] != b->rng_after[index]) return true;
    }
    return false;
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

TEST(observing_an_aimed_attack_changes_neither_its_outcome_nor_the_rng)
{
    ObservedAttack without;
    ObservedAttack with;

    // Card S1. The neutrality evidence above covers the UNAIMED path, which is
    // a different branch: a zero location makes `combat_attack` draw for a limb
    // through `combat_random_hit_loc`, and an aimed one does not. The observer
    // now reads `CF_AIM` and the pre-substitution location on both, so both
    // need the evidence.
    drive_one_attack_at(false, HIT_LOC_HEAD, &without);
    drive_one_attack_at(true, HIT_LOC_HEAD, &with);

    // Worth nothing unless the attack was really aimed and really happened.
    CHECK(without.aimed);
    CHECK_INT(without.resolved_hit_loc, HIT_LOC_HEAD);
    CHECK(without.flags != 0);

    check_attacks_agree(&without, &with);
}

TEST(the_aimed_path_does_not_draw_for_a_limb_and_the_unaimed_one_does)
{
    ObservedAttack aimed;
    ObservedAttack rolled;

    drive_one_attack_at(false, HIT_LOC_HEAD, &aimed);
    drive_one_attack_at(false, HIT_LOC_TORSO, &rolled);

    CHECK(aimed.aimed);
    CHECK(!rolled.aimed);
    // From one seed, the two consume the random stream differently, because
    // only the unaimed path calls `combat_random_hit_loc`. If these ever agreed
    // the aimed branch would be drawing when it should not.
    CHECK(attacks_differ(&aimed, &rolled));
}

TEST(a_wrong_hit_location_is_detected_by_the_comparison)
{
    ObservedAttack head;
    ObservedAttack arm;

    // The deliberate wrong-location control card S1's session 5 asks for. The
    // agreement checks above are only worth something if this comparison can
    // tell two aims apart - a check that cannot fail has measured nothing, and
    // six of this project's gates have been caught doing exactly that.
    drive_one_attack_at(true, HIT_LOC_HEAD, &head);
    drive_one_attack_at(true, HIT_LOC_ARM, &arm);

    CHECK_INT(head.resolved_hit_loc, HIT_LOC_HEAD);
    CHECK_INT(arm.resolved_hit_loc, HIT_LOC_ARM);
    CHECK(attacks_differ(&head, &arm));
}

// Drives one attack with a wielded ranged weapon, returning the ammunition the
// attacker had left. The attacker is a **PC**: `combat_consume_ammo` returns
// early for an NPC with no player leader and would consume nothing at all,
// which would make this test agree for the wrong reason.
static int drive_one_ranged_attack(int hit_loc, int consumption, int stock)
{
    int64_t attacker;
    int64_t target;
    int64_t weapon;
    int64_t ammo;
    CombatContext combat;
    int left;

    clean();
    CHECK(obj_harness_init());
    attacker = obj_harness_create_proto(OBJ_TYPE_PC);
    target = obj_harness_create_proto(OBJ_TYPE_NPC);
    weapon = obj_harness_create_proto(OBJ_TYPE_WEAPON);
    ammo = obj_harness_create_proto(OBJ_TYPE_AMMO);
    CHECK(attacker != OBJ_HANDLE_NULL);
    CHECK(target != OBJ_HANDLE_NULL);
    CHECK(weapon != OBJ_HANDLE_NULL);
    CHECK(ammo != OBJ_HANDLE_NULL);

    obj_field_int32_set(weapon, OBJ_F_WEAPON_AMMO_TYPE, 0);
    obj_field_int32_set(weapon, OBJ_F_WEAPON_AMMO_CONSUMPTION, consumption);
    obj_field_int32_set(ammo, OBJ_F_AMMO_TYPE, 0);
    obj_field_int32_set(ammo, OBJ_F_AMMO_QUANTITY, stock);
    item_insert(ammo, attacker, -1);

    obj_field_int32_set(target, OBJ_F_HP_ADJ, 40);
    obj_field_int32_set(target, OBJ_F_HP_DAMAGE, 0);

    memset(&combat, 0, sizeof(combat));
    combat.attacker_obj = attacker;
    combat.target_obj = target;
    combat.field_28 = target;
    combat.weapon_obj = weapon;
    combat.skill = SKILL_MELEE;
    combat.hit_loc = hit_loc;

    stub_environment = true;
    random_seed(ATTACK_SEED);
    combat_attack(&combat);
    stub_environment = false;

    left = item_ammo_quantity_get(attacker, 0);

    obj_harness_destroy(ammo);
    obj_harness_destroy(weapon);
    obj_harness_destroy(target);
    obj_harness_destroy(attacker);
    return left;
}

// Card RT1: the snapshot the engine writes for a PC wielding a bow, untrained
// and then at apprentice. Drives the engine's own reads - `item_weapon_skill`,
// the skill and training getters and `item_weapon_magic_speed` - rather than
// the writer, because the defect this closes was a question the engine could
// answer and the trace never asked.
//
// Called directly rather than through `combat_attack`: with a weapon wielded,
// an attack plays weapon effects keyed on the prototype's description, and a
// harness object is its own prototype with no description to key on.
static int trace_int(const char* trace, const char* key)
{
    const char* at = strstr(trace, key);
    CHECK(at != NULL);
    return at != NULL ? atoi(at + strlen(key)) : -9999;
}

static void observe_one_archer(bool apprentice, char* trace, size_t size)
{
    int64_t archer;
    int64_t weapon;

    clean();
    CHECK(obj_harness_init());
    archer = obj_harness_create_proto(OBJ_TYPE_PC);
    weapon = obj_harness_create_proto(OBJ_TYPE_WEAPON);
    CHECK(archer != OBJ_HANDLE_NULL);
    CHECK(weapon != OBJ_HANDLE_NULL);

    // An arrow weapon is a bow to `item_weapon_skill`.
    obj_field_int32_set(weapon, OBJ_F_WEAPON_AMMO_TYPE, TIG_ART_AMMO_TYPE_ARROW);
    // Wielded by its slot field. `item_wield_set` asks the art system whether
    // the critter can be drawn holding it, and inserting straight into the
    // slot redraws the critter; the harness has neither art nor a map.
    // `item_wield_get`, which the snapshot uses, reads only this field.
    item_insert(weapon, archer, -1);
    obj_field_int32_set(weapon, OBJ_F_ITEM_INV_LOCATION, ITEM_INV_LOC_WEAPON);
    CHECK(item_wield_get(archer, ITEM_INV_LOC_WEAPON) == weapon);

    // Dexterity is Bow's key stat and caps its level, so it is raised first.
    // Points are a quarter of the level: 3 is level 12. Checked, because the
    // setter refuses silently.
    stat_base_set(archer, STAT_DEXTERITY, 20);
    CHECK_INT(basic_skill_points_set(archer, BASIC_SKILL_BOW, 3), 3);
    if (apprentice) {
        // The trainer's own call, so the level threshold is enforced exactly
        // as in play. Checked, because a refused training would make the two
        // snapshots agree for the wrong reason.
        CHECK_INT(basic_skill_training_set(archer, BASIC_SKILL_BOW,
                      TRAINING_APPRENTICE),
            TRAINING_APPRENTICE);
    }

    CHECK(combat_observer_open(TRACE_FILE));
    combat_observer_snapshot_obj(archer);
    combat_observer_close();
    CHECK(read_trace(trace, size));

    obj_harness_destroy(weapon);
    obj_harness_destroy(archer);
}

TEST(a_real_snapshot_reads_the_wielded_weapons_skill_training_and_speed)
{
    char untrained[TRACE_SIZE];
    char trained[TRACE_SIZE];

    observe_one_archer(false, untrained, sizeof(untrained));
    observe_one_archer(true, trained, sizeof(trained));

    CHECK_INT(trace_int(untrained, "\"weapon_skill\":"), SKILL_BOW);
    CHECK_INT(trace_int(untrained, "\"weapon_skill_level\":"), 12);
    CHECK_INT(trace_int(untrained, "\"weapon_training\":"), TRAINING_NONE);
    CHECK_INT(trace_int(trained, "\"weapon_training\":"), TRAINING_APPRENTICE);

    // Apprentice is worth +5 of weapon speed, which the attack path spends as
    // 50 ms off every animation step. The snapshot has to show the lever moved.
    CHECK_INT(trace_int(trained, "\"weapon_speed\":"),
        trace_int(untrained, "\"weapon_speed\":") + 5);
}

TEST(ammunition_is_spent_by_the_weapon_not_by_where_it_is_aimed)
{
    int head;
    int leg;
    int unaimed;

    // Five bullets an attack: the Rifled Cannon's cost. A one-bullet weapon -
    // the Revolver the fixture carries, for instance - hides an off-by-one,
    // and six of the shipped bullet weapons cost more than one. See the
    // ammunition census in MODDING-NOTES.
    //
    // Done-when 1 lists ammunition, and `combat_consume_ammo` takes no hit
    // location - weapon, critter, count, destroy. This asserts the consequence
    // rather than trusting the signature.
    head = drive_one_ranged_attack(HIT_LOC_HEAD, 5, 40);
    leg = drive_one_ranged_attack(HIT_LOC_LEG, 5, 40);
    unaimed = drive_one_ranged_attack(HIT_LOC_TORSO, 5, 40);

    CHECK_INT(head, 35);
    CHECK_INT(leg, 35);
    CHECK_INT(unaimed, 35);
}

TEST(a_weapon_that_costs_more_spends_more_whatever_the_aim)
{
    // The check above would pass against a build that consumed a fixed one per
    // attack and ignored the weapon entirely, so the cost has to be shown to
    // follow the weapon.
    CHECK_INT(drive_one_ranged_attack(HIT_LOC_HEAD, 1, 40), 39);
    CHECK_INT(drive_one_ranged_attack(HIT_LOC_HEAD, 6, 40), 34);
    CHECK_INT(drive_one_ranged_attack(HIT_LOC_ARM, 6, 40), 34);
}

TEST(a_selected_aim_and_a_held_key_aim_produce_the_same_attack)
{
    ObservedAttack from_panel;
    ObservedAttack from_key;
    int panel_aim;
    int key_aim;
    int limb;

    // **Card S1, done-when 1.** The two routes are supposed to be
    // indistinguishable, and the reason they can be is that they converge:
    // `called_shot_effective_aim` answers one integer, and the dispatch writes
    // it at the one place the original writes an aim. This drives the attack
    // from each route's answer and requires the results to agree - the outcome,
    // the damage, the resolved limb, the aim flag and the subsequent random
    // stream.
    for (limb = HIT_LOC_HEAD; limb < HIT_LOC_COUNT; limb++) {
        // Kept selection, no key held.
        panel_aim = called_shot_effective_aim(limb, false, false, false);

        // No selection, the matching key held. Body's key is the absence of
        // one, so HIT_LOC_TORSO is excluded from the loop above.
        key_aim = called_shot_effective_aim(HIT_LOC_TORSO,
            limb == HIT_LOC_HEAD,
            limb == HIT_LOC_ARM,
            limb == HIT_LOC_LEG);

        CHECK_INT(panel_aim, limb);
        CHECK_INT(key_aim, limb);

        drive_one_attack_at(false, panel_aim, &from_panel);
        drive_one_attack_at(false, key_aim, &from_key);

        // Worth nothing unless the attack was really aimed and really happened.
        CHECK(from_panel.aimed);
        CHECK_INT(from_panel.resolved_hit_loc, limb);

        check_attacks_agree(&from_panel, &from_key);
    }
}

TEST(a_different_aim_does_not_produce_the_same_attack)
{
    ObservedAttack head;
    ObservedAttack leg;

    // The negative half. If the comparison above could not tell two aims
    // apart, its agreement would be the absence of a measurement - the failure
    // six of this project's gates were caught committing.
    drive_one_attack_at(false,
        called_shot_effective_aim(HIT_LOC_HEAD, false, false, false), &head);
    drive_one_attack_at(false,
        called_shot_effective_aim(HIT_LOC_TORSO, false, false, true), &leg);

    CHECK_INT(head.resolved_hit_loc, HIT_LOC_HEAD);
    CHECK_INT(leg.resolved_hit_loc, HIT_LOC_LEG);
    CHECK(attacks_differ(&head, &leg));
}

TEST(one_extra_observer_draw_is_detected_on_the_aimed_path_too)
{
    ObservedAttack without;
    ObservedAttack with;

    // The aimed agreement test is worth exactly as much as this: if a draw
    // smuggled into the observer could NOT move an aimed comparison, that
    // agreement would be the absence of a measurement rather than a result.
    drive_one_attack_at(false, HIT_LOC_HEAD, &without);

    inject_observer_draw = true;
    drive_one_attack_at(true, HIT_LOC_HEAD, &with);
    inject_observer_draw = false;

    CHECK(attacks_differ(&without, &with));
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
    RUN(observing_an_aimed_attack_changes_neither_its_outcome_nor_the_rng);
    RUN(the_aimed_path_does_not_draw_for_a_limb_and_the_unaimed_one_does);
    RUN(a_wrong_hit_location_is_detected_by_the_comparison);
    RUN(ammunition_is_spent_by_the_weapon_not_by_where_it_is_aimed);
    RUN(a_weapon_that_costs_more_spends_more_whatever_the_aim);
    RUN(a_selected_aim_and_a_held_key_aim_produce_the_same_attack);
    RUN(a_different_aim_does_not_produce_the_same_attack);
    RUN(one_extra_observer_draw_is_detected_on_the_aimed_path_too);
    RUN(a_complete_trace_has_ordered_control_and_semantic_events);
    RUN(the_release_impact_and_charge_boundaries_are_each_their_own_event);
    RUN(a_release_carries_the_inputs_that_made_its_step_delay);
    RUN(every_way_a_flight_ends_is_its_own_outcome);
    RUN(a_cast_is_identified_by_its_spell_not_by_its_cost);
    RUN(a_real_snapshot_reads_the_wielded_weapons_skill_training_and_speed);
    RUN(an_unreadable_frame_is_reported_as_unknown_not_as_a_number);
    RUN(repeated_releases_are_countable_and_carry_the_clock);
    RUN(an_ai_decision_lists_what_it_weighed);
    RUN(an_armor_aware_weapon_choice_lists_its_candidates);
    RUN(every_event_carries_the_clock_the_baseline_needs);
    RUN(the_trace_says_which_clock_produced_its_numbers);
    RUN(the_clock_starts_at_the_stream_and_never_goes_backwards);
    RUN(a_selected_limb_is_distinguishable_from_one_that_was_rolled);
    RUN(hit_miss_critical_and_terminal_values_are_not_conflated);
    RUN(the_bound_marks_the_trace_incomplete_once);
    RUN(an_output_failure_disables_recording_and_leaves_no_false_terminal);
    RUN(observation_does_not_advance_the_combat_rng);
    RUN(a_command_records_its_request_before_its_resolution);
    RUN(a_command_waits_for_the_mode_it_was_recorded_in);
    RUN(every_existing_row_form_still_loads_as_it_did);
    RUN(the_real_time_rows_load_into_what_they_say);
    RUN(the_throw_rows_load_into_what_they_say);
    RUN(the_called_shot_rows_load_into_what_they_say);
    RUN(the_persistence_rows_load_into_what_they_say);
    RUN(the_jam_row_loads_alone);
    RUN(the_display_and_creature_rows_load_into_what_they_say);
    RUN(the_await_death_row_names_the_critter_it_waits_for);
    RUN(the_remove_row_names_the_object_it_takes_off_the_map);
    RUN(the_afflict_row_names_a_debuff_critical);
    RUN(the_poison_row_sets_a_level_on_the_player_or_a_critter);
    RUN(the_use_row_names_a_prototype_and_perhaps_a_target);
    RUN(the_xp_train_and_inspect_xp_rows_load);
    RUN(the_wear_row_loads_with_or_without_a_wearer);
    RUN(a_row_the_parser_cannot_read_refuses_the_whole_file);
    RUN(a_tile_step_a_cast_request_and_a_weapon_change_are_each_recorded);
    RUN(every_mode_state_decides_acceptance_the_same_way);
    RUN(the_turn_based_setting_and_a_running_encounter_are_not_the_same_axis);
    RUN(a_mode_action_records_its_request_before_its_resolution);
    RUN(a_refused_mode_action_is_not_recorded_as_accepted);
    RUN(the_starting_mode_is_not_recorded_as_a_change);
    RUN(a_mode_change_no_player_action_claimed_is_recorded_as_uncaused);
    RUN(a_mode_change_the_player_asked_for_is_not_also_recorded_as_uncaused);
    RUN(every_end_turn_state_decides_acceptance_the_same_way);
    RUN(an_end_turn_records_its_request_before_its_resolution);
    RUN(a_refused_end_turn_is_not_recorded_as_accepted);
    RUN(the_terminal_latch_answers_only_after_a_critter_goes_down);
    RUN(an_incapacitation_ends_the_duel_as_much_as_a_death_does);
    RUN(the_latch_is_set_after_the_event_is_written);
    RUN(the_event_counter_advances_only_when_something_is_written);
    RUN(every_goal_state_decides_acceptance_the_same_way);
    RUN(a_refused_command_leaves_the_goal_state_untouched);
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
