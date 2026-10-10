// CE: bounded, opt-in semantic combat trace for the private observatory.
#include "game/combat_observer.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/combat.h"
#include "game/crit_effects.h"
#include "game/damage_type.h"
#include "game/obj_id.h"
#include "game/obj_pool.h"

#define COMBAT_OBSERVER_DEFAULT_LIMIT 10000U

static FILE* combat_observer_stream;
static bool combat_observer_complete;
static bool combat_observer_terminal;
static bool combat_observer_truncated;
static unsigned int combat_observer_sequence;
static unsigned int combat_observer_semantic_count;
static unsigned int combat_observer_limit = COMBAT_OBSERVER_DEFAULT_LIMIT;
static unsigned int combat_observer_turn;
static unsigned long long combat_observer_next_action;
static unsigned long long combat_observer_current_action;
static int combat_observer_fail_after = -1;
static int combat_observer_lines_written;
static CombatObserverMode combat_observer_mode;
static bool combat_observer_mode_known;
static char combat_observer_weapon[COMBAT_OBSERVER_IDENTITY_SIZE];
static bool combat_observer_weapon_known;

// Card RT1 (session 1): the run-relative clock, injected here rather than at
// each of the nineteen call sites.
//
// A timing baseline was impossible before this: every event carried `sequence`
// and nothing carried time, so an observer file could say what happened and in
// what order but never how often. The plan's own acceptance forbids presenting
// an event count as a firing rate without clock evidence, and the instrument
// could not supply any.
//
// Injected centrally because the alternative is nineteen `snprintf` calls that
// each have to remember, and the twentieth event somebody adds is the one that
// forgets. `line` always arrives as a complete object starting `{"schema"`, so
// the field goes in immediately after the brace.
//
// **`ms` is not always milliseconds, and the file says so.** `tig_timer_now`
// reads `SDL_GetTicks()` normally, but a deterministic run turns on the
// virtual clock, where it advances a fixed step once per frame instead - so
// the same field counts real time in one run and frames in another. Rather
// than pick one and mislead the other, `run.started` records which clock was
// running, and a reader that ignores it is reading frame counts as if they
// were a rate. That is the distinction `tig/timer.h` exists to draw.
static tig_timestamp_t combat_observer_started_at;

static long long combat_observer_ms(void)
{
    if (combat_observer_stream == NULL) {
        return 0;
    }
    return (long long)tig_timer_elapsed(combat_observer_started_at);
}

static bool combat_observer_write(const char* line)
{
    char stamped[1024];

    if (combat_observer_stream == NULL) {
        return false;
    }

    // `run.started` stamps itself, because the baseline is taken as the stream
    // opens and elapsing against an unset start is not zero, it is nonsense.
    if (line[0] == '{' && strstr(line, "\"ms\":") == NULL) {
        snprintf(stamped, sizeof(stamped), "{\"ms\":%lld,%s",
            combat_observer_ms(), line + 1);
        line = stamped;
    }

    if (combat_observer_fail_after >= 0
        && combat_observer_lines_written >= combat_observer_fail_after) {
        combat_observer_complete = false;
        fclose(combat_observer_stream);
        combat_observer_stream = NULL;
        tig_debug_printf("Combat observer: injected output failure\n");
        return false;
    }

    if (fputs(line, combat_observer_stream) == EOF
        || fputc('\n', combat_observer_stream) == EOF
        || fflush(combat_observer_stream) != 0) {
        combat_observer_complete = false;
        fclose(combat_observer_stream);
        combat_observer_stream = NULL;
        tig_debug_printf("Combat observer: output failure; trace is incomplete\n");
        return false;
    }

    combat_observer_lines_written++;
    return true;
}

static bool combat_observer_begin_semantic(void)
{
    char line[256];

    if (combat_observer_stream == NULL) {
        return false;
    }

    if (combat_observer_semantic_count < combat_observer_limit) {
        combat_observer_semantic_count++;
        return true;
    }

    if (!combat_observer_truncated) {
        combat_observer_truncated = true;
        combat_observer_complete = false;
        snprintf(line, sizeof(line),
            "{\"schema\":%d,\"sequence\":%u,\"event\":\"observer.truncated\",\"turn\":%u,\"action_id\":null,\"limit\":%u}",
            COMBAT_OBSERVER_SCHEMA,
            combat_observer_sequence++,
            combat_observer_turn,
            combat_observer_limit);
        combat_observer_write(line);
    }

    return false;
}

static const char* combat_observer_action_json(char* buffer, size_t size)
{
    if (combat_observer_current_action == 0) {
        return "null";
    }
    snprintf(buffer, size, "%llu", combat_observer_current_action);
    return buffer;
}

bool combat_observer_open(const char* path)
{
    char line[256];

    combat_observer_test_reset();
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    combat_observer_stream = fopen(path, "wb");
    if (combat_observer_stream == NULL) {
        combat_observer_complete = false;
        tig_debug_printf("Combat observer: cannot open %s\n", path);
        return false;
    }

    combat_observer_complete = true;
    // Before the first line, so every `ms` is relative to a real baseline.
    tig_timer_now(&combat_observer_started_at);
    // The clock this file was recorded against, stated once by the file
    // itself. A reader that sees `virtual_clock: true` is looking at frame
    // steps, not milliseconds, and must not report a rate from them.
    snprintf(line, sizeof(line),
        "{\"ms\":0,\"schema\":%d,\"sequence\":%u,\"event\":\"run.started\","
        "\"turn\":null,\"action_id\":null,\"virtual_clock\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++,
        tig_timer_virtual_enabled() ? "true" : "false");
    if (!combat_observer_write(line)) {
        return false;
    }

    return true;
}

void combat_observer_close(void)
{
    char line[256];

    if (combat_observer_stream == NULL) {
        return;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"run.ended\",\"turn\":%u,\"action_id\":null,\"complete\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++,
        combat_observer_turn,
        combat_observer_complete ? "true" : "false");
    combat_observer_write(line);
    if (combat_observer_stream != NULL) {
        fclose(combat_observer_stream);
        combat_observer_stream = NULL;
    }
}

bool combat_observer_is_enabled(void)
{
    return combat_observer_stream != NULL;
}

bool combat_observer_is_complete(void)
{
    return combat_observer_complete && !combat_observer_truncated;
}

void combat_observer_object_id(int64_t obj,
    char identity[COMBAT_OBSERVER_IDENTITY_SIZE])
{
    ObjectID oid;

    if (obj == OBJ_HANDLE_NULL) {
        strcpy(identity, "NULL");
        return;
    }

    oid = obj_pool_perm_reverse_lookup(obj);
    if (oid.type == OID_TYPE_NULL) {
        strcpy(identity, "NULL");
        return;
    }

    objid_id_to_str(identity, oid);
}

void combat_observer_turn_started(const char* actor, int action_points)
{
    char line[384];

    if (!combat_observer_begin_semantic()) return;
    combat_observer_turn++;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"turn.started\",\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"action_points\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, action_points);
    combat_observer_write(line);
}

// CE (card O2): what the player asked for, and whether the engine took it.
//
// Two events rather than one field, because ordering carries meaning: the
// request is written *before* the dispatch runs, so any combat the command
// causes appears after it in the stream rather than before the thing that
// caused it. The resolution follows once the outcome is known.
//
// "Accepted" means a goal was queued for the actor. That is the property a
// replay needs: a request the engine refused is a click that changed nothing,
// and re-submitting it would silently diverge from the run being reproduced.
void combat_observer_command_requested(const char* actor, const char* target_kind,
    const char* target, int64_t target_loc, int aim)
{
    char line[512];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"command.requested\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"target_kind\":\"%s\",\"target\":\"%s\","
        "\"target_location\":%lld,\"aim\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, target_kind,
        target, (long long)target_loc, aim);
    combat_observer_write(line);
}

// CE (card O2): did the engine take the command?
//
// The dispatch has around twenty early returns and no return value, so the only
// honest signal is what it left behind: a goal queued for the actor that was not
// queued before. Kept as a pure decision over three booleans, separate from the
// UI code that gathers them, because this is the judgement a replay trusts - a
// refusal recorded as an acceptance would be re-submitted and diverge silently -
// and a judgement worth trusting is worth testing exhaustively.
//
//   busy_after  false  nothing is queued, so nothing was taken.
//   busy_before false  and now busy: a goal appeared. Taken.
//   same_goal   true   busy before and after with the same goal: the dispatch
//                      refused, or re-issued what was already running. Either
//                      way nothing new was accepted.
//   otherwise          busy before and after with a different goal: the old one
//                      was replaced, which is an acceptance.
bool combat_observer_command_accepted(bool busy_before, bool busy_after,
    bool same_goal)
{
    if (!busy_after) {
        return false;
    }
    return !(busy_before && same_goal);
}

void combat_observer_command_resolved(const char* actor, bool accepted)
{
    char line[384];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"command.resolved\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"accepted\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor,
        accepted ? "true" : "false");
    combat_observer_write(line);
}

// CE (card O2): the mode fields every mode event carries, written once here so
// the three axes cannot drift apart between the three events.
static int combat_observer_mode_fields(char* buffer, size_t size,
    const CombatObserverMode* mode)
{
    return snprintf(buffer, size,
        "\"combat_mode\":%s,\"turn_based\":%s,\"turn_based_active\":%s",
        mode->combat_mode ? "true" : "false",
        mode->turn_based ? "true" : "false",
        mode->turn_based_active ? "true" : "false");
}

void combat_observer_mode_requested(const char* actor, const char* action,
    const CombatObserverMode* before)
{
    char line[512];
    char fields[160];

    if (!combat_observer_begin_semantic()) return;
    combat_observer_mode_fields(fields, sizeof(fields), before);
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"mode.requested\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"action\":\"%s\",%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, action, fields);
    combat_observer_write(line);
}

// See the header for why refusal is the interesting case. Any axis moving is an
// acceptance; the toggles are independent and one action never moves two.
bool combat_observer_mode_accepted(const CombatObserverMode* before,
    const CombatObserverMode* after)
{
    return before->combat_mode != after->combat_mode
        || before->turn_based != after->turn_based
        || before->turn_based_active != after->turn_based_active;
}

void combat_observer_mode_resolved(const char* actor, const char* action,
    bool accepted, const CombatObserverMode* after)
{
    char line[512];
    char fields[160];

    if (!combat_observer_begin_semantic()) return;
    combat_observer_mode_fields(fields, sizeof(fields), after);
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"mode.resolved\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"action\":\"%s\",\"accepted\":%s,%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, action,
        accepted ? "true" : "false", fields);
    combat_observer_write(line);

    // The player path owns the change it just made, so the frame watcher below
    // does not report it a second time as uncaused.
    combat_observer_mode = *after;
    combat_observer_mode_known = true;
}

void combat_observer_mode_observed(const char* actor,
    const CombatObserverMode* mode)
{
    char line[512];
    char fields[160];

    if (!combat_observer_is_enabled()) return;

    if (!combat_observer_mode_known) {
        // First sight is the run's starting mode, not a change. Recording it as
        // one would put a spurious transition at the head of every trace.
        combat_observer_mode = *mode;
        combat_observer_mode_known = true;
        return;
    }

    if (!combat_observer_mode_accepted(&combat_observer_mode, mode)) {
        return;
    }

    combat_observer_mode = *mode;
    if (!combat_observer_begin_semantic()) return;
    combat_observer_mode_fields(fields, sizeof(fields), mode);
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"mode.changed\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"cause\":\"game\",%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, fields);
    combat_observer_write(line);
}

// CE (card O2): ending a turn, recorded the same way a command is - the request
// first, so whatever the end of the turn sets in motion lands after it.
//
// The action points are carried because they are what makes an unreproduced
// replay visible: the run that named this gap sat in turn 4 holding thirteen of
// them, and a record that did not say so looked like a fight that had simply
// finished.
void combat_observer_end_turn_requested(const char* actor, int action_points)
{
    char line[384];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"end_turn.requested\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"action\":\"end-turn\","
        "\"action_points\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, action_points);
    combat_observer_write(line);
}

// See the header for why the conditions are the judgement here rather than a
// before/after reading of state.
bool combat_observer_end_turn_accepted(bool turn_in_progress, bool pc_turn,
    bool actor_ready)
{
    return turn_in_progress && pc_turn && actor_ready;
}

// `pc_turn` is the state *after*: false means the turn passed to someone else,
// true after an acceptance means a fresh turn came back around - which happens
// when the PC is the only critter left with a turn to take.
void combat_observer_end_turn_resolved(const char* actor, bool accepted,
    int action_points, bool pc_turn)
{
    char line[384];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"end_turn.resolved\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"action\":\"end-turn\","
        "\"accepted\":%s,\"action_points\":%d,\"pc_turn\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor,
        accepted ? "true" : "false", action_points,
        pc_turn ? "true" : "false");
    combat_observer_write(line);
}

void combat_observer_turn_ended(const char* actor, int action_points)
{
    char line[384];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"turn.ended\",\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"action_points\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, action_points);
    combat_observer_write(line);
}

void combat_observer_actor_snapshot(const char* actor, int level,
    int strength, int dexterity, int speed, int melee, int dodge,
    int hp_current, int hp_max, int fatigue_current, int fatigue_max,
    int weapon_description, int armor_description, int weapon_skill,
    int weapon_skill_level, int weapon_training, int weapon_speed)
{
    char line[896];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"actor.snapshot\",\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"level\":%d,\"strength\":%d,\"dexterity\":%d,\"speed\":%d,\"melee\":%d,\"dodge\":%d,\"hp_current\":%d,\"hp_max\":%d,\"fatigue_current\":%d,\"fatigue_max\":%d,\"weapon_description\":%d,\"armor_description\":%d,\"weapon_skill\":%d,\"weapon_skill_level\":%d,\"weapon_training\":%d,\"weapon_speed\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, actor, level,
        strength, dexterity, speed, melee, dodge, hp_current, hp_max,
        fatigue_current, fatigue_max, weapon_description, armor_description,
        weapon_skill, weapon_skill_level, weapon_training, weapon_speed);
    combat_observer_write(line);
}

void combat_observer_ap_checked(const char* actor, int requested,
    int before, bool accepted)
{
    char line[448];
    char action[32];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"ap.checked\",\"turn\":%u,\"action_id\":%s,\"actor\":\"%s\",\"requested\":%d,\"before\":%d,\"accepted\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), actor,
        requested, before, accepted ? "true" : "false");
    combat_observer_write(line);
}

void combat_observer_ap_committed(const char* actor, int requested,
    int before, int after, bool accepted, int fatigue_before,
    int fatigue_after)
{
    char line[512];
    char action[32];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"ap.committed\",\"turn\":%u,\"action_id\":%s,\"actor\":\"%s\",\"requested\":%d,\"before\":%d,\"after\":%d,\"accepted\":%s,\"fatigue_before\":%d,\"fatigue_after\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), actor,
        requested, before, after, accepted ? "true" : "false",
        fatigue_before, fatigue_after);
    combat_observer_write(line);
}

// CE (card S1): `requested_hit_location` and `aimed` are the halves the stream
// could not previously tell apart.
//
// `combat_attack` replaces a zero hit location with `combat_random_hit_loc()`
// before it reports anything, so by the time this is called `hit_location`
// holds the *resolved* limb and a selected head shot is indistinguishable from
// an ordinary attack that rolled a head. Acceptance for the called-shot
// selector is "the selector and the held key produce the same attack", which
// cannot be judged by an instrument that cannot see the difference.
//
// Both new values are read, not recomputed: `requested` is the caller's own
// copy taken before the substitution, and `aimed` is the engine's `CF_AIM`
// after it. No second calculation, no extra draw.
unsigned long long combat_observer_attack_started(const char* attacker,
    const char* target, int skill, int hit_location,
    int requested_hit_location, bool aimed)
{
    char line[576];
    unsigned long long action_id;

    if (!combat_observer_begin_semantic()) return 0;
    action_id = ++combat_observer_next_action;
    combat_observer_current_action = action_id;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"attack.started\",\"turn\":%u,\"action_id\":%llu,\"attacker\":\"%s\",\"target\":\"%s\",\"skill\":%d,\"hit_location\":%d,\"requested_hit_location\":%d,\"aimed\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn, action_id,
        attacker, target, skill, hit_location,
        requested_hit_location, aimed ? "true" : "false");
    combat_observer_write(line);
    return action_id;
}

void combat_observer_attack_resolved(unsigned long long action_id,
    const char* attacker, const char* target, int skill, int hit_location,
    unsigned int flags, int result)
{
    char line[576];

    if (combat_observer_begin_semantic()) {
        snprintf(line, sizeof(line),
            "{\"schema\":%d,\"sequence\":%u,\"event\":\"attack.resolved\",\"turn\":%u,\"action_id\":%llu,\"attacker\":\"%s\",\"target\":\"%s\",\"skill\":%d,\"hit_location\":%d,\"hit\":%s,\"critical\":%s,\"flags\":%u,\"result\":%d}",
            COMBAT_OBSERVER_SCHEMA,
            combat_observer_sequence++, combat_observer_turn, action_id,
            attacker, target, skill, hit_location,
            (flags & 0x2) != 0 ? "true" : "false",
            (flags & 0x4) != 0 ? "true" : "false", flags, result);
        combat_observer_write(line);
    }
    if (combat_observer_current_action == action_id) {
        combat_observer_current_action = 0;
    }
}

void combat_observer_action_released(const char* actor, int goal, int frame,
    int action_frame, int weapon_type, int art_fps, int speed_fps,
    int weapon_speed, int step_ms)
{
    char line[384];

    if (combat_observer_stream == NULL) {
        return;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"action.released\","
        "\"turn\":null,\"action_id\":null,\"actor\":\"%s\",\"goal\":%d,"
        "\"frame\":%d,\"action_frame\":%d,\"weapon_type\":%d,"
        "\"art_fps\":%d,\"speed_fps\":%d,\"weapon_speed\":%d,"
        "\"step_ms\":%d}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, actor, goal,
        frame, action_frame, weapon_type, art_fps, speed_fps, weapon_speed,
        step_ms);
    combat_observer_write(line);
}

void combat_observer_projectile_ended(const char* source, const char* target,
    const char* struck, const char* outcome)
{
    char line[384];

    if (combat_observer_stream == NULL) {
        return;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"projectile.ended\","
        "\"turn\":null,\"action_id\":null,\"source\":\"%s\","
        "\"target\":\"%s\",\"struck\":\"%s\",\"outcome\":\"%s\"}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, source, target,
        struck, outcome);
    combat_observer_write(line);
}

void combat_observer_spell_cast(const char* caster, const char* source,
    const char* target, int spell, bool free)
{
    char line[384];

    if (combat_observer_stream == NULL) {
        return;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"spell.cast\","
        "\"turn\":null,\"action_id\":null,\"caster\":\"%s\","
        "\"source\":\"%s\",\"target\":\"%s\",\"spell\":%d,"
        "\"free\":%s}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, caster, source,
        target, spell, free ? "true" : "false");
    combat_observer_write(line);
}

void combat_observer_resource_charged(const char* actor, const char* resource,
    int spell, int cost, int before, int after)
{
    char line[320];
    char spell_json[16];

    if (combat_observer_stream == NULL) {
        return;
    }

    if (spell < 0) {
        snprintf(spell_json, sizeof(spell_json), "null");
    } else {
        snprintf(spell_json, sizeof(spell_json), "%d", spell);
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"resource.charged\","
        "\"turn\":null,\"action_id\":null,\"actor\":\"%s\","
        "\"resource\":\"%s\",\"spell\":%s,\"cost\":%d,\"before\":%d,"
        "\"after\":%d}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, actor, resource,
        spell_json, cost, before, after);
    combat_observer_write(line);
}

void combat_observer_actor_moved(const char* actor, int64_t from_loc,
    int64_t to_loc)
{
    char line[320];

    if (combat_observer_stream == NULL) {
        return;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"actor.moved\","
        "\"turn\":null,\"action_id\":null,\"actor\":\"%s\","
        "\"from_location\":%lld,\"to_location\":%lld}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, actor,
        (long long)from_loc, (long long)to_loc);
    combat_observer_write(line);
}

void combat_observer_cast_requested(const char* actor, int spell,
    const char* target_kind, const char* target, int64_t target_loc)
{
    char line[384];

    // A player command, counted against the bound like `command.requested`.
    if (!combat_observer_begin_semantic()) return;

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"cast.requested\","
        "\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"spell\":%d,"
        "\"target_kind\":\"%s\",\"target\":\"%s\","
        "\"target_location\":%lld}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, combat_observer_turn,
        actor, spell, target_kind, target, (long long)target_loc);
    combat_observer_write(line);
}

void combat_observer_wield_observed(const char* actor, const char* weapon)
{
    char line[320];

    if (combat_observer_stream == NULL) {
        return;
    }

    if (!combat_observer_weapon_known) {
        snprintf(combat_observer_weapon, sizeof(combat_observer_weapon), "%s",
            weapon);
        combat_observer_weapon_known = true;
        return;
    }

    if (strcmp(combat_observer_weapon, weapon) == 0) {
        return;
    }

    snprintf(combat_observer_weapon, sizeof(combat_observer_weapon), "%s",
        weapon);
    // A change of state, counted against the bound like `mode.changed`.
    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"wield.changed\","
        "\"turn\":%u,\"action_id\":null,\"actor\":\"%s\","
        "\"weapon\":\"%s\"}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, combat_observer_turn,
        actor, weapon);
    combat_observer_write(line);
}

void combat_observer_budget(const char* event, const char* actor,
    const char* category, int action_points, int duration_ms, int elapsed_ms,
    const char* reason)
{
    char line[384];
    char reason_field[48];

    if (combat_observer_stream == NULL) {
        return;
    }

    reason_field[0] = '\0';
    if (reason != NULL) {
        snprintf(reason_field, sizeof(reason_field), ",\"reason\":\"%s\"", reason);
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"%s\","
        "\"turn\":null,\"action_id\":null,\"actor\":\"%s\","
        "\"category\":\"%s\",\"action_points\":%d,\"duration_ms\":%d,"
        "\"elapsed_ms\":%d%s}",
        COMBAT_OBSERVER_SCHEMA, combat_observer_sequence++, event, actor,
        category, action_points, duration_ms, elapsed_ms, reason_field);
    combat_observer_write(line);
}

void combat_observer_skill_roll(const char* source, const char* target,
    int skill, int effectiveness, int difficulty, int roll,
    int critical_roll, int critical_chance, bool success, bool critical)
{
    char line[704];
    char action[32];

    if (combat_observer_current_action == 0
        || !combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"skill.roll\",\"turn\":%u,\"action_id\":%s,\"source\":\"%s\",\"target\":\"%s\",\"skill\":%d,\"effectiveness\":%d,\"difficulty\":%d,\"roll\":%d,\"critical_roll\":%d,\"critical_chance\":%d,\"success\":%s,\"critical\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), source, target,
        skill, effectiveness, difficulty, roll, critical_roll, critical_chance,
        success ? "true" : "false", critical ? "true" : "false");
    combat_observer_write(line);
}

// CE (criticals, milestone CR-0): the effects a critical rolled, by name, in
// the order of their flags.
static const struct {
    unsigned int flag;
    const char* name;
} combat_observer_critical_effects[] = {
    { CDF_BONUS_DAM_50, "bonus_50" },
    { CDF_BONUS_DAM_100, "bonus_100" },
    { CDF_BONUS_DAM_200, "bonus_200" },
    { CDF_STUN, "stun" },
    { CDF_KNOCKDOWN, "knockdown" },
    { CDF_KNOCKOUT, "knockout" },
    { CDF_BLIND, "blind" },
    { CDF_CRIPPLE_ARM, "cripple_arm" },
    { CDF_CRIPPLE_LEG, "cripple_leg" },
    { CDF_SCAR, "scar" },
    { CDF_DROP_WEAPON, "drop_weapon" },
    { CDF_DAMAGE_WEAPON, "damage_weapon" },
    { CDF_DESTROY_WEAPON, "destroy_weapon" },
    { CDF_EXPLODE_WEAPON, "explode_weapon" },
    { CDF_LOST_AMMO, "lost_ammo" },
    { CDF_DESTROY_AMMO, "destroy_ammo" },
    { CDF_DROP_HELMET, "drop_helmet" },
    { CDF_DAMAGE_ARMOR, "damage_armor" },
};

// `flags`' effects as a JSON list's contents, in the table's order.
static void combat_observer_critical_effect_names(unsigned int flags, char* list, size_t size)
{
    size_t used = 0;
    size_t index;

    list[0] = '\0';
    for (index = 0; index < sizeof(combat_observer_critical_effects) / sizeof(combat_observer_critical_effects[0]); index++) {
        if ((flags & combat_observer_critical_effects[index].flag) != 0) {
            int written = snprintf(list + used, size - used, "%s\"%s\"",
                used > 0 ? "," : "", combat_observer_critical_effects[index].name);
            if (written < 0 || (size_t)written >= size - used) {
                break;
            }
            used += (size_t)written;
        }
    }
}

void combat_observer_critical_rolled(const char* attacker, const char* target,
    bool hit, int hit_location, int effect_bonus, bool aimed,
    bool player_spared, int weapon_chart, int body_chart,
    unsigned int effects, unsigned int resisted, bool target_down,
    unsigned int temporary, const char* outcome, int winded_fatigue,
    int winded_from)
{
    char line[1280];
    char outcome_json[32];
    char action[32];
    char list[512];
    char resisted_list[128];
    char temporary_list[128];

    if (!combat_observer_begin_semantic()) return;

    combat_observer_critical_effect_names(effects, list, sizeof(list));
    combat_observer_critical_effect_names(resisted, resisted_list, sizeof(resisted_list));
    crit_effects_names(temporary, temporary_list, (int)sizeof(temporary_list));
    if (outcome != NULL) {
        snprintf(outcome_json, sizeof(outcome_json), "\"%s\"", outcome);
    } else {
        snprintf(outcome_json, sizeof(outcome_json), "null");
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"critical.rolled\",\"turn\":%u,\"action_id\":%s,\"attacker\":\"%s\",\"target\":\"%s\",\"kind\":\"%s\",\"hit_location\":%d,\"effect_bonus\":%d,\"aimed\":%s,\"player_spared\":%s,\"weapon_chart\":%d,\"body_chart\":%d,\"effects\":[%s],\"resisted\":[%s],\"target_down\":%s,\"temporary\":[%s],\"outcome\":%s,\"winded_fatigue\":%d,\"winded_from\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), attacker, target,
        hit ? "hit" : "miss", hit_location, effect_bonus,
        aimed ? "true" : "false", player_spared ? "true" : "false",
        weapon_chart, body_chart, list, resisted_list,
        target_down ? "true" : "false", temporary_list, outcome_json,
        winded_fatigue, winded_from);
    combat_observer_write(line);
}

void combat_observer_ai_decided(const char* actor, const char* target,
    const char* danger, const char* origin, const char* action, int spell,
    const char* item, const char* weapon, int cast_chance, int cast_roll,
    const int considered[][2], int considered_count, bool desperate)
{
    char line[2048];
    char action_id[32];
    char list[512];
    size_t used = 0;
    int index;

    if (!combat_observer_begin_semantic()) return;

    list[0] = '\0';
    for (index = 0; index < considered_count && index < COMBAT_OBSERVER_AI_CONSIDERED_MAX; index++) {
        int written = snprintf(list + used, sizeof(list) - used, "%s[%d,%d]",
            index > 0 ? "," : "", considered[index][0], considered[index][1]);
        if (written < 0 || (size_t)written >= sizeof(list) - used) {
            break;
        }
        used += (size_t)written;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"ai.decided\",\"turn\":%u,\"action_id\":%s,\"actor\":\"%s\",\"target\":\"%s\",\"danger\":\"%s\",\"origin\":\"%s\",\"action\":\"%s\",\"spell\":%d,\"item\":\"%s\",\"weapon\":\"%s\",\"cast_chance\":%d,\"cast_roll\":%d,\"considered\":[%s],\"desperate\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action_id, sizeof(action_id)), actor, target,
        danger, origin, action, spell, item, weapon, cast_chance, cast_roll, list,
        desperate ? "true" : "false");
    combat_observer_write(line);
}

void combat_observer_ai_weapon_chosen(const char* actor, const char* target,
    const char* previous, const char* chosen, bool kept,
    const int candidates[][3], int candidate_count,
    const int unaffordable[][2], int unaffordable_count)
{
    char line[2048];
    char action_id[32];
    char list[640];
    char passed[320];
    size_t used = 0;
    int index;

    if (!combat_observer_begin_semantic()) return;

    list[0] = '\0';
    for (index = 0; index < candidate_count && index < COMBAT_OBSERVER_AI_WEAPONS_MAX; index++) {
        int written = snprintf(list + used, sizeof(list) - used, "%s[%d,%d,%d]",
            index > 0 ? "," : "", candidates[index][0], candidates[index][1], candidates[index][2]);
        if (written < 0 || (size_t)written >= sizeof(list) - used) {
            break;
        }
        used += (size_t)written;
    }

    passed[0] = '\0';
    used = 0;
    for (index = 0; index < unaffordable_count && index < COMBAT_OBSERVER_AI_WEAPONS_MAX; index++) {
        int written = snprintf(passed + used, sizeof(passed) - used, "%s[%d,%d]",
            index > 0 ? "," : "", unaffordable[index][0], unaffordable[index][1]);
        if (written < 0 || (size_t)written >= sizeof(passed) - used) {
            break;
        }
        used += (size_t)written;
    }

    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"ai.weapon_chosen\",\"turn\":%u,\"action_id\":%s,\"actor\":\"%s\",\"target\":\"%s\",\"previous\":\"%s\",\"chosen\":\"%s\",\"kept\":%s,\"candidates\":[%s],\"unaffordable\":[%s]}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action_id, sizeof(action_id)), actor, target,
        previous, chosen, kept ? "true" : "false", list, passed);
    combat_observer_write(line);
}

void combat_observer_armor_resolved(const char* attacker, const char* target,
    int hit_loc, int worn_centi, int natural, int magic, bool blocked,
    bool gap, int penetration, int variation_percent, int protection_centi,
    int penetration_centi, int passed_num, int passed_den, int damage_before,
    int damage_after, int fatigue_added)
{
    char line[1024];
    char action[32];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"armor.resolved\",\"turn\":%u,\"action_id\":%s,\"attacker\":\"%s\",\"target\":\"%s\",\"hit_location\":%d,\"worn_centi\":%d,\"natural\":%d,\"magic\":%d,\"blocked\":%s,\"gap\":%s,\"penetration\":%d,\"variation_percent\":%d,\"protection_centi\":%d,\"penetration_centi\":%d,\"passed_num\":%d,\"passed_den\":%d,\"damage_before\":%d,\"damage_after\":%d,\"fatigue_added\":%d}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), attacker, target,
        hit_loc, worn_centi, natural, magic, blocked ? "true" : "false",
        gap ? "true" : "false", penetration, variation_percent,
        protection_centi, penetration_centi, passed_num, passed_den,
        damage_before, damage_after, fatigue_added);
    combat_observer_write(line);
}

void combat_observer_damage_committed(const char* attacker,
    const char* target, const int* components, unsigned int damage_flags,
    int hp_amount, int hp_before, int hp_after, int fatigue_before,
    int fatigue_after, bool dead, bool unconscious)
{
    char line[1024];
    char action[32];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"damage.committed\",\"turn\":%u,\"action_id\":%s,\"attacker\":\"%s\",\"target\":\"%s\",\"normal\":%d,\"fatigue\":%d,\"poison\":%d,\"electrical\":%d,\"fire\":%d,\"damage_flags\":%u,\"hp_amount\":%d,\"hp_before\":%d,\"hp_after\":%d,\"fatigue_before\":%d,\"fatigue_after\":%d,\"dead\":%s,\"unconscious\":%s}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), attacker, target,
        components[DAMAGE_TYPE_NORMAL], components[DAMAGE_TYPE_FATIGUE],
        components[DAMAGE_TYPE_POISON], components[DAMAGE_TYPE_ELECTRICAL],
        components[DAMAGE_TYPE_FIRE], damage_flags, hp_amount, hp_before,
        hp_after, fatigue_before, fatigue_after, dead ? "true" : "false",
        unconscious ? "true" : "false");
    combat_observer_write(line);

    // Latched after the write, so a harness that exits on this can never cut
    // the event that caused it out of the trace.
    if (dead || unconscious) {
        combat_observer_terminal = true;
    }
}

bool combat_observer_terminal_seen(void)
{
    return combat_observer_terminal;
}

unsigned int combat_observer_event_count(void)
{
    return combat_observer_sequence;
}

void combat_observer_test_set_event_limit(unsigned int limit)
{
    combat_observer_limit = limit;
}

void combat_observer_test_fail_after(int lines)
{
    combat_observer_fail_after = lines;
}

void combat_observer_test_reset(void)
{
    if (combat_observer_stream != NULL) {
        fclose(combat_observer_stream);
    }
    combat_observer_stream = NULL;
    combat_observer_complete = false;
    combat_observer_truncated = false;
    combat_observer_sequence = 0;
    combat_observer_semantic_count = 0;
    combat_observer_limit = COMBAT_OBSERVER_DEFAULT_LIMIT;
    combat_observer_turn = 0;
    combat_observer_next_action = 0;
    combat_observer_current_action = 0;
    combat_observer_fail_after = -1;
    combat_observer_lines_written = 0;
    memset(&combat_observer_mode, 0, sizeof(combat_observer_mode));
    combat_observer_mode_known = false;
    combat_observer_weapon[0] = '\0';
    combat_observer_weapon_known = false;
    combat_observer_terminal = false;
}
