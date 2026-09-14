// CE: bounded, opt-in semantic combat trace for the private observatory.
#include "game/combat_observer.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/damage_type.h"
#include "game/obj_id.h"
#include "game/obj_pool.h"

#define COMBAT_OBSERVER_SCHEMA 1
#define COMBAT_OBSERVER_DEFAULT_LIMIT 10000U

static FILE* combat_observer_stream;
static bool combat_observer_complete;
static bool combat_observer_truncated;
static unsigned int combat_observer_sequence;
static unsigned int combat_observer_semantic_count;
static unsigned int combat_observer_limit = COMBAT_OBSERVER_DEFAULT_LIMIT;
static unsigned int combat_observer_turn;
static unsigned long long combat_observer_next_action;
static unsigned long long combat_observer_current_action;
static int combat_observer_fail_after = -1;
static int combat_observer_lines_written;

static bool combat_observer_write(const char* line)
{
    if (combat_observer_stream == NULL) {
        return false;
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
            "{\"schema\":1,\"sequence\":%u,\"event\":\"observer.truncated\",\"turn\":%u,\"action_id\":null,\"limit\":%u}",
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
    snprintf(line, sizeof(line),
        "{\"schema\":%d,\"sequence\":%u,\"event\":\"run.started\",\"turn\":null,\"action_id\":null}",
        COMBAT_OBSERVER_SCHEMA,
        combat_observer_sequence++);
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
        "{\"schema\":1,\"sequence\":%u,\"event\":\"turn.started\",\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"action_points\":%d}",
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
    const char* target, int64_t target_loc)
{
    char line[512];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":1,\"sequence\":%u,\"event\":\"command.requested\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"target_kind\":\"%s\",\"target\":\"%s\","
        "\"target_location\":%lld}",
        combat_observer_sequence++, combat_observer_turn, actor, target_kind,
        target, (long long)target_loc);
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
        "{\"schema\":1,\"sequence\":%u,\"event\":\"command.resolved\",\"turn\":%u,"
        "\"action_id\":null,\"actor\":\"%s\",\"accepted\":%s}",
        combat_observer_sequence++, combat_observer_turn, actor,
        accepted ? "true" : "false");
    combat_observer_write(line);
}

void combat_observer_turn_ended(const char* actor, int action_points)
{
    char line[384];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":1,\"sequence\":%u,\"event\":\"turn.ended\",\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"action_points\":%d}",
        combat_observer_sequence++, combat_observer_turn, actor, action_points);
    combat_observer_write(line);
}

void combat_observer_actor_snapshot(const char* actor, int level,
    int strength, int dexterity, int speed, int melee, int dodge,
    int hp_current, int hp_max, int fatigue_current, int fatigue_max,
    int weapon_description, int armor_description)
{
    char line[768];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":1,\"sequence\":%u,\"event\":\"actor.snapshot\",\"turn\":%u,\"action_id\":null,\"actor\":\"%s\",\"level\":%d,\"strength\":%d,\"dexterity\":%d,\"speed\":%d,\"melee\":%d,\"dodge\":%d,\"hp_current\":%d,\"hp_max\":%d,\"fatigue_current\":%d,\"fatigue_max\":%d,\"weapon_description\":%d,\"armor_description\":%d}",
        combat_observer_sequence++, combat_observer_turn, actor, level,
        strength, dexterity, speed, melee, dodge, hp_current, hp_max,
        fatigue_current, fatigue_max, weapon_description, armor_description);
    combat_observer_write(line);
}

void combat_observer_ap_checked(const char* actor, int requested,
    int before, bool accepted)
{
    char line[448];
    char action[32];

    if (!combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":1,\"sequence\":%u,\"event\":\"ap.checked\",\"turn\":%u,\"action_id\":%s,\"actor\":\"%s\",\"requested\":%d,\"before\":%d,\"accepted\":%s}",
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
        "{\"schema\":1,\"sequence\":%u,\"event\":\"ap.committed\",\"turn\":%u,\"action_id\":%s,\"actor\":\"%s\",\"requested\":%d,\"before\":%d,\"after\":%d,\"accepted\":%s,\"fatigue_before\":%d,\"fatigue_after\":%d}",
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), actor,
        requested, before, after, accepted ? "true" : "false",
        fatigue_before, fatigue_after);
    combat_observer_write(line);
}

unsigned long long combat_observer_attack_started(const char* attacker,
    const char* target, int skill, int hit_location)
{
    char line[512];
    unsigned long long action_id;

    if (!combat_observer_begin_semantic()) return 0;
    action_id = ++combat_observer_next_action;
    combat_observer_current_action = action_id;
    snprintf(line, sizeof(line),
        "{\"schema\":1,\"sequence\":%u,\"event\":\"attack.started\",\"turn\":%u,\"action_id\":%llu,\"attacker\":\"%s\",\"target\":\"%s\",\"skill\":%d,\"hit_location\":%d}",
        combat_observer_sequence++, combat_observer_turn, action_id,
        attacker, target, skill, hit_location);
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
            "{\"schema\":1,\"sequence\":%u,\"event\":\"attack.resolved\",\"turn\":%u,\"action_id\":%llu,\"attacker\":\"%s\",\"target\":\"%s\",\"skill\":%d,\"hit_location\":%d,\"hit\":%s,\"critical\":%s,\"flags\":%u,\"result\":%d}",
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

void combat_observer_skill_roll(const char* source, const char* target,
    int skill, int effectiveness, int difficulty, int roll,
    int critical_roll, bool success, bool critical)
{
    char line[640];
    char action[32];

    if (combat_observer_current_action == 0
        || !combat_observer_begin_semantic()) return;
    snprintf(line, sizeof(line),
        "{\"schema\":1,\"sequence\":%u,\"event\":\"skill.roll\",\"turn\":%u,\"action_id\":%s,\"source\":\"%s\",\"target\":\"%s\",\"skill\":%d,\"effectiveness\":%d,\"difficulty\":%d,\"roll\":%d,\"critical_roll\":%d,\"success\":%s,\"critical\":%s}",
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), source, target,
        skill, effectiveness, difficulty, roll, critical_roll,
        success ? "true" : "false", critical ? "true" : "false");
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
        "{\"schema\":1,\"sequence\":%u,\"event\":\"damage.committed\",\"turn\":%u,\"action_id\":%s,\"attacker\":\"%s\",\"target\":\"%s\",\"normal\":%d,\"fatigue\":%d,\"poison\":%d,\"electrical\":%d,\"fire\":%d,\"damage_flags\":%u,\"hp_amount\":%d,\"hp_before\":%d,\"hp_after\":%d,\"fatigue_before\":%d,\"fatigue_after\":%d,\"dead\":%s,\"unconscious\":%s}",
        combat_observer_sequence++, combat_observer_turn,
        combat_observer_action_json(action, sizeof(action)), attacker, target,
        components[DAMAGE_TYPE_NORMAL], components[DAMAGE_TYPE_FATIGUE],
        components[DAMAGE_TYPE_POISON], components[DAMAGE_TYPE_ELECTRICAL],
        components[DAMAGE_TYPE_FIRE], damage_flags, hp_amount, hp_before,
        hp_after, fatigue_before, fatigue_after, dead ? "true" : "false",
        unconscious ? "true" : "false");
    combat_observer_write(line);
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
}
