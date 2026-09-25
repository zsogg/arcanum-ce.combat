#include "ui/replay_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/debug.h>
#include <tig/message.h>
#include <tig/timer.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/item.h"
#include "game/obj.h"
#include "game/obj_id.h"
#include "game/obj_pool.h"
#include "game/object.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/target.h"
#include "ui/called_shot.h"
#include "ui/combat_ui.h"
#include "ui/intgame.h"
#include "ui/skill_ui.h"
#include "ui/spell_ui.h"

#define REPLAY_MAX_COMMANDS 256
#define REPLAY_IDENTITY_SIZE 64

typedef enum ReplayKind {
    REPLAY_KIND_OBJECT,
    REPLAY_KIND_LOCATION,
    // CE (card O2): a named player action with no target. Not a target at all -
    // it carries the action's name and goes through `replay_key_action` rather
    // than the command dispatch. Three so far: the two mode toggles and ending
    // a turn, each one found by a replay that could not reproduce its
    // recording without it.
    REPLAY_KIND_KEY,
    // Real-time session A: what a real-time run needs and a click cannot say.
    //
    //   cast <spell> self | object <id> | location <loc>
    //       the spell button, then the target click - `spell_ui_activate`, then
    //       `intgame_apply_mode_to_target`, the path a click in spell mode takes.
    //   wield <id> | wield <prototype>
    //       the hotkey bar's equip, the one equip route with one function. A
    //       bare number names a prototype in the player's pack (session C), for
    //       a carried item whose identity a script cannot know in advance.
    //   wait <ms>
    //       hold the next row this long, so a script can space its commands.
    REPLAY_KIND_CAST,
    REPLAY_KIND_WIELD,
    REPLAY_KIND_WAIT,
    // `learn <spell>`: the character screen's learn button, `spell_ui_add`, which
    // spends points and checks level and Intelligence as it does for a player.
    // Script-only, like `key character-points`: they build a test character, and
    // nothing records them.
    REPLAY_KIND_LEARN,
    // Real-time session C: the two a throw needs.
    //
    //   give <prototype> <count>
    //       that many new items of that prototype into the player's inventory.
    //       Script-only, like `learn`: it stocks a test character, and nothing
    //       records it.
    //   throw <prototype> <location>
    //       one item of that prototype from the inventory to the tile, through
    //       `anim_goal_throw_item` - what dragging it onto the map calls.
    REPLAY_KIND_GIVE,
    REPLAY_KIND_THROW,
    //   skill <skill> <id>
    //       a skill used on an object, through `skill_use` - what the skill
    //       buttons reach for every skill but pickpocketing and repair.
    REPLAY_KIND_SKILL,
    //   raise <stat>
    //       one point into a primary stat (0 Strength ... 4 Intelligence ...
    //       7 Charisma), as the character screen's plus button spends it.
    //       Script-only, like `learn`.
    REPLAY_KIND_RAISE,
    // Called shots in real time and while prowling (2026-09-25):
    //
    //   prowl on | prowl off
    //       the Prowling skill button, `skill_ui_begin(pc, 0)`, pressed only
    //       when the player is not already in the asked state.
    //   panel <limb>
    //       a left click at the centre of the called-shot panel's control for
    //       that limb, enqueued as a mouse message so it meets the same
    //       buttons and window filters a real click does. Logged, and nothing
    //       sent, when the panel is not shown.
    REPLAY_KIND_PROWL,
    REPLAY_KIND_PANEL,
} ReplayKind;

// A cast's target, which is a third shape: the caster, an object or a tile.
typedef enum ReplayCastTarget {
    REPLAY_CAST_SELF,
    REPLAY_CAST_OBJECT,
    REPLAY_CAST_LOCATION,
} ReplayCastTarget;

typedef struct ReplayCommand {
    int turn;
    ReplayKind kind;
    int64_t loc;
    char target[REPLAY_IDENTITY_SIZE];
    // CE (card S1): the aim this command was recorded with. A fourth field on
    // the line, absent in every record written before it existed - and absent
    // means `HIT_LOC_TORSO`, an ordinary attack, which is exactly what those
    // records meant when they were written.
    int aim;
    // Real-time session A. `now` submits without waiting for the actor to be
    // free, which is the only way a script can click during a wind-up or a
    // recovery - what cancelling and queueing are about.
    bool now;
    int spell;
    ReplayCastTarget cast_target;
    int wait_ms;
    int proto;
    int count;
    int skill;
    int stat;
    // `prowl`: 1 on, 0 off.
    int prowl;
} ReplayCommand;

static ReplayCommand replay_commands[REPLAY_MAX_COMMANDS];
static int replay_count;
static int replay_next;
static bool replay_loaded;
static bool replay_stall_reported;
static bool replay_waiting;
static tig_timestamp_t replay_wait_started;

// CE (card O2): does the game's mode match the one this command was recorded
// in?
//
// Measured 2026-09-14: the same click is an attack inside combat mode and
// something else outside it, because `intgame.c` gates the whole attack branch
// on `combat_critter_is_combat_mode_active`. A replay that ignores the mode
// therefore does not fail - it quietly performs a *different* action and
// reports it accepted, which is the worst of the available outcomes.
//
// The recorded turn is the proxy: a command recorded at turn 0 was issued
// before any turn started, and one at turn 1 or later was issued inside
// turn-based combat. Holding the command until the mode agrees turns a silent
// substitution into a visible stall.
bool replay_ui_mode_matches(int recorded_turn, bool combat_active)
{
    return recorded_turn >= 1 ? combat_active : !combat_active;
}

static bool replay_mode_matches(const ReplayCommand* command)
{
    return replay_ui_mode_matches(command->turn, combat_turn_based_is_active());
}

// CE (card O2): the named actions a script may press.
//
// One place, so a recorded action that has no replay entry point is a named
// failure rather than a silent no-op. Each goes through the same UI function
// the keyboard calls, so the recorder sees a replayed press exactly as it saw
// the original - which is what makes `replay-script` a fixed point.
static bool replay_key_action(const char* action)
{
    if (strcmp(action, "end-turn") == 0) {
        combat_ui_end_turn_action();
        return true;
    }

    return intgame_mode_action(action);
}

// CE (card O2): a target the click path could not have handed us.
//
// The recorder only ever sees commands that came through `target_pick`, which
// makes dead critters click-through under `TGT_OBJ_NO_ST_CRITTER_DEAD` and
// skips objects that are destroyed or switched off. A replay resolves a GUID
// straight to a handle and is bound by none of that, so it says so here.
//
// Measured 2026-09-15, and this is why it exists rather than being defensive:
// a script that outlived its duel submitted the recorded attack again while
// the guard was mid-`ag_dying`, and the engine asserted in `anim_private.c`
// line 675 -- `goal_type >= 0 && goal_type < ANIM_GOAL_MAX`. The dispatch's
// `OBJ_TYPE_PC`/`OBJ_TYPE_NPC` branch leaves its `anim` local uninitialised for
// a dead critter with no ALT held, and hands it to
// `anim_goal_data_init_no_interrupt`. That defect is the engine's and is
// recorded separately; what belongs here is that no player could have made
// this click, so no recording can contain it and no replay should submit it.
static bool replay_target_is_live(int64_t obj)
{
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && critter_is_dead(obj)) {
        return false;
    }

    return true;
}

static bool replay_parse_int(const char* token, int* value)
{
    char* end;
    long parsed;

    if (token == NULL) {
        return false;
    }
    parsed = strtol(token, &end, 10);
    if (end == token || *end != '\0') {
        return false;
    }
    *value = (int)parsed;
    return true;
}

// A prototype by the number a modder sees - the `.pro` file's name, and the
// `prototype` in a scenario file - which is its description plus 20
// (`proto_description_to_oid`). Taking the description instead was measured
// wrong on 2026-09-23: `give 15171` made an Explosive Decoy, description 15171,
// not the Smoke Grenade that is prototype 15171.
static int64_t replay_prototype(int prototype)
{
    return prototype > 20 ? proto_get_by_description(prototype - 20) : OBJ_HANDLE_NULL;
}

// The player's first usable item of a prototype, or null - saying what the
// pack does hold when there is none, so a wrong number is visible in the log.
//
// Not `item_find_by_proto`, which returns items the engine has finished with.
// Measured 2026-09-23: a thrown Smoke Grenade stays in the thrower's inventory
// list flagged `OF_DESTROYED` after its spell goes off, the search returned it
// for every later throw, and `anim_throw_item` refused it each time - five of
// six grenades released and never flew.
static int64_t replay_find_carried(int64_t pc_obj, int prototype, bool pack_only)
{
    int64_t proto_obj;
    int64_t item_obj;
    int count;
    int index;

    proto_obj = replay_prototype(prototype);
    count = obj_field_int32_get(pc_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (index = 0; index < count; index++) {
        item_obj = obj_arrayfield_handle_get(pc_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        if (proto_obj == OBJ_HANDLE_NULL
            || obj_field_handle_get(item_obj, OBJ_F_PROTOTYPE_HANDLE) != proto_obj
            || (obj_field_int32_get(item_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0
            || (pack_only && IS_WEAR_INV_LOC(item_inventory_location_get(item_obj)))) {
            continue;
        }
        return item_obj;
    }

    tig_debug_printf("Replay: no usable prototype %d in the pack; it holds:", prototype);
    for (index = 0; index < count; index++) {
        item_obj = obj_arrayfield_handle_get(pc_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        tig_debug_printf(" %d", obj_field_int32_get(item_obj, OBJ_F_DESCRIPTION) + 20);
    }
    tig_debug_printf("\n");
    return OBJ_HANDLE_NULL;
}

static void replay_copy_target(ReplayCommand* command, const char* token)
{
    strncpy(command->target, token, sizeof(command->target) - 1);
    command->target[sizeof(command->target) - 1] = '\0';
}

// One line: `<turn> [now] <kind> <arguments>`. The first three kinds keep the
// grammar every existing script was written in, so those scripts load as
// before; the rest are real-time session A's.
static bool replay_parse_line(const char* text, ReplayCommand* command)
{
    char line[512];
    char* tokens[8];
    int count = 0;
    int index = 0;
    char* token;
    const char* kind;

    snprintf(line, sizeof(line), "%s", text);
    for (token = strtok(line, " \t\r\n"); token != NULL && count < 8;
        token = strtok(NULL, " \t\r\n")) {
        tokens[count++] = token;
    }

    memset(command, 0, sizeof(*command));
    command->aim = HIT_LOC_TORSO;

    if (count < 3 || !replay_parse_int(tokens[index++], &(command->turn))) {
        return false;
    }

    if (strcmp(tokens[index], "now") == 0) {
        command->now = true;
        index++;
    }

    if (index + 1 >= count) {
        return false;
    }
    kind = tokens[index++];

    if (strcmp(kind, "object") == 0 || strcmp(kind, "location") == 0) {
        command->kind = strcmp(kind, "location") == 0
            ? REPLAY_KIND_LOCATION
            : REPLAY_KIND_OBJECT;
        if (command->kind == REPLAY_KIND_LOCATION) {
            command->loc = (int64_t)strtoll(tokens[index], NULL, 10);
        } else {
            replay_copy_target(command, tokens[index]);
        }
        index++;
        // `panel` instead of a limb: force no aim, so the command commits
        // whatever the called-shot panel holds - the one way a script can
        // test that a click on the panel reached the attack.
        if (index < count && strcmp(tokens[index], "panel") == 0) {
            command->aim = -1;
            index++;
            return index == count;
        }
        if (index < count && !replay_parse_int(tokens[index++], &(command->aim))) {
            return false;
        }
        if (command->aim < 0 || command->aim >= HIT_LOC_COUNT) {
            // Refused rather than clamped. A record naming a limb this build
            // has no entry for cannot be reproduced, and replaying it as some
            // other limb would be a wrong reproduction that reads as a
            // gameplay difference.
            tig_debug_printf("Replay: aim %d is not a hit location\n", command->aim);
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "key") == 0) {
        command->kind = REPLAY_KIND_KEY;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "wield") == 0) {
        command->kind = REPLAY_KIND_WIELD;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "learn") == 0) {
        command->kind = REPLAY_KIND_LEARN;
        if (!replay_parse_int(tokens[index++], &(command->spell))
            || command->spell < 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "give") == 0) {
        command->kind = REPLAY_KIND_GIVE;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0 || index >= count
            || !replay_parse_int(tokens[index++], &(command->count))
            || command->count <= 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "prowl") == 0) {
        command->kind = REPLAY_KIND_PROWL;
        if (strcmp(tokens[index], "on") == 0) {
            command->prowl = 1;
        } else if (strcmp(tokens[index], "off") == 0) {
            command->prowl = 0;
        } else {
            return false;
        }
        index++;
        return index == count;
    }

    if (strcmp(kind, "panel") == 0) {
        command->kind = REPLAY_KIND_PANEL;
        if (!replay_parse_int(tokens[index++], &(command->aim))
            || command->aim < 0 || command->aim >= HIT_LOC_COUNT) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "raise") == 0) {
        command->kind = REPLAY_KIND_RAISE;
        if (!replay_parse_int(tokens[index++], &(command->stat))
            || command->stat < STAT_STRENGTH || command->stat > STAT_CHARISMA) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "skill") == 0) {
        command->kind = REPLAY_KIND_SKILL;
        if (!replay_parse_int(tokens[index++], &(command->skill))
            || command->skill < 0 || command->skill >= SKILL_COUNT || index >= count) {
            return false;
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "throw") == 0) {
        command->kind = REPLAY_KIND_THROW;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0 || index >= count) {
            return false;
        }
        command->loc = (int64_t)strtoll(tokens[index++], NULL, 10);
        return index == count && command->loc != 0;
    }

    if (strcmp(kind, "wait") == 0) {
        command->kind = REPLAY_KIND_WAIT;
        if (!replay_parse_int(tokens[index++], &(command->wait_ms))
            || command->wait_ms < 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "cast") == 0) {
        command->kind = REPLAY_KIND_CAST;
        if (!replay_parse_int(tokens[index++], &(command->spell))
            || command->spell < 0 || index >= count) {
            return false;
        }
        if (strcmp(tokens[index], "self") == 0) {
            command->cast_target = REPLAY_CAST_SELF;
            index++;
        } else if (strcmp(tokens[index], "object") == 0 && index + 1 < count) {
            command->cast_target = REPLAY_CAST_OBJECT;
            replay_copy_target(command, tokens[index + 1]);
            index += 2;
        } else if (strcmp(tokens[index], "location") == 0 && index + 1 < count) {
            command->cast_target = REPLAY_CAST_LOCATION;
            command->loc = (int64_t)strtoll(tokens[index + 1], NULL, 10);
            index += 2;
        } else {
            return false;
        }
        return index == count;
    }

    return false;
}

int replay_ui_count(void)
{
    return replay_count;
}

// For the suite: one loaded row, in a fixed form, so a test can say exactly what
// a line parsed into without reaching into the table.
bool replay_ui_describe(int index, char* buffer, size_t size)
{
    const ReplayCommand* command;
    static const char* kinds[] = { "object", "location", "key", "cast", "wield", "wait", "learn", "give", "throw", "skill", "raise", "prowl", "panel" };
    int written;
    static const char* casts[] = { "self", "object", "location" };

    if (index < 0 || index >= replay_count) {
        return false;
    }
    command = &(replay_commands[index]);
    written = snprintf(buffer, size,
        "turn=%d now=%d kind=%s target=%s loc=%lld aim=%d spell=%d cast=%s wait=%d",
        command->turn, command->now ? 1 : 0, kinds[command->kind],
        command->target, (long long)command->loc, command->aim, command->spell,
        command->kind == REPLAY_KIND_CAST ? casts[command->cast_target] : "-",
        command->wait_ms);
    // Only the kinds that carry them, so every older row reads as it always did.
    if ((command->kind == REPLAY_KIND_GIVE || command->kind == REPLAY_KIND_THROW)
        && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " proto=%d count=%d",
            command->proto, command->count);
    }
    if (command->kind == REPLAY_KIND_SKILL && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " skill=%d", command->skill);
    }
    if (command->kind == REPLAY_KIND_RAISE && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " stat=%d", command->stat);
    }
    if (command->kind == REPLAY_KIND_PROWL && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " prowl=%d", command->prowl);
    }
    return true;
}

bool replay_ui_load(const char* path)
{
    FILE* stream;
    char line[512];

    replay_count = 0;
    replay_next = 0;
    replay_loaded = false;
    replay_stall_reported = false;
    replay_waiting = false;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        tig_debug_printf("Replay: cannot open '%s'; nothing will be replayed\n", path);
        return false;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        if (replay_count >= REPLAY_MAX_COMMANDS) {
            // Refuse rather than truncate: a replay missing its tail would
            // diverge from its recording and look like a gameplay difference.
            tig_debug_printf("Replay: more than %d commands; refusing the file\n",
                REPLAY_MAX_COMMANDS);
            fclose(stream);
            replay_count = 0;
            return false;
        }

        if (!replay_parse_line(line, &(replay_commands[replay_count]))) {
            tig_debug_printf("Replay: cannot parse '%s'; refusing the file\n", line);
            fclose(stream);
            replay_count = 0;
            return false;
        }
        replay_count++;
    }

    fclose(stream);
    replay_loaded = true;
    tig_debug_printf("Replay: loaded %d command(s) from '%s'\n", replay_count, path);
    return true;
}

bool replay_ui_active(void)
{
    return replay_loaded;
}

int replay_ui_remaining(void)
{
    return replay_loaded ? replay_count - replay_next : 0;
}

// An object row's identity, resolved the way the dispatch below does and with
// the same messages: unreadable, or absent from this run.
static bool replay_resolve(const ReplayCommand* command, int64_t* obj_ptr)
{
    ObjectID oid;

    if (!objid_id_from_str(&oid, command->target)) {
        tig_debug_printf("Replay: command %d names an unreadable target '%s'\n",
            replay_next, command->target);
        return false;
    }

    *obj_ptr = obj_pool_perm_lookup(oid);
    if (*obj_ptr == OBJ_HANDLE_NULL) {
        tig_debug_printf("Replay: command %d targets '%s', absent from this run\n",
            replay_next, command->target);
        return false;
    }

    return true;
}

void replay_ui_tick(void)
{
    ReplayCommand* command;
    TargetDescriptor td;
    ObjectID oid;
    int64_t pc_obj;
    int64_t target_obj;

    if (!replay_loaded || replay_next >= replay_count) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    command = &replay_commands[replay_next];

    // Real-time session A: a wait holds the queue for its time and nothing
    // else. Measured on the engine's own timer, which the deterministic clock
    // drives, so a scripted wait is the same length in every repeat.
    if (command->kind == REPLAY_KIND_WAIT) {
        if (!replay_waiting) {
            replay_waiting = true;
            tig_timer_now(&replay_wait_started);
            return;
        }
        if (tig_timer_elapsed(replay_wait_started) < command->wait_ms) {
            return;
        }
        replay_waiting = false;
        replay_next++;
        return;
    }

    // One command at a time, and only while the actor is free. A command
    // submitted over a running goal would be refused or would replace it, and
    // either way the replay would stop matching what it is reproducing.
    //
    // `combat_ui_actor_ready` rather than `anim_is_busy`, which this read until
    // 2026-09-15: a standing critter is busy with its fidget goal, so the
    // stricter test could hold a command forever in a state the engine itself
    // treats as actionable - including the end-turn press, whose own condition
    // is exactly this one. Two notions of "free" in one path is the drift this
    // card has already been bitten by twice.
    // Real-time session A: `now` is the exception - a click during a wind-up
    // or a recovery, which a player can make and cancelling and queueing are
    // about. Everything else below still holds.
    if (!command->now && !combat_ui_actor_ready(pc_obj)) {
        return;
    }

    // The UI drops every message when a turn-based encounter is running and
    // the turn is not the PC's (`intgame_process_event`, first statement). A
    // replay calls the dispatch directly and so is not bound by that gate
    // unless it holds the same line here - and acting on another critter's
    // turn is something no player could do, so no recording can contain it.
    if (combat_turn_based_is_active()
        && combat_turn_based_whos_turn_get() != pc_obj) {
        return;
    }

    if (!replay_mode_matches(command)) {
        // Say it once - the flag is cleared only when a command is actually
        // submitted, not on the way past this gate. It used to be cleared here,
        // which meant a stall the *next* check caught reported itself every
        // frame: 36 017 identical lines and a 4.3 MB log, measured 2026-09-15.
        //
        // A replay that ends with commands outstanding did not
        // reproduce its recording, and `replay_ui_remaining` is what the run
        // record reads - but a reader of the log deserves to know why it
        // stopped rather than inferring it from a count.
        if (!replay_stall_reported) {
            replay_stall_reported = true;
            tig_debug_printf("Replay: holding command %d (recorded at turn %d);"
                             " turn-based combat is %s\n",
                replay_next, command->turn,
                combat_turn_based_is_active() ? "active" : "not active");
        }
        return;
    }

    if (command->kind == REPLAY_KIND_KEY) {
        // A key action goes through the same UI entry point the keyboard uses,
        // so the recorder sees it exactly as it saw the original press.
        replay_stall_reported = false;
        replay_next++;
        if (!replay_key_action(command->target)) {
            tig_debug_printf("Replay: command %d names an unknown action '%s'\n",
                replay_next - 1, command->target);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_LEARN) {
        replay_stall_reported = false;
        replay_next++;
        spell_ui_add(pc_obj, command->spell);
        // The character screen refuses on screen - level, Intelligence,
        // Willpower, the college's order - which a script never sees. Said
        // here, because a cast of a spell never learned still plays its
        // animation and reads as a cast that did nothing (measured 2026-09-23).
        if (!spell_is_known(pc_obj, command->spell)) {
            tig_debug_printf("Replay: command %d could not learn spell %d"
                             " (level %d, Intelligence %d, Willpower %d)\n",
                replay_next - 1, command->spell,
                stat_level_get(pc_obj, STAT_LEVEL),
                stat_level_get(pc_obj, STAT_INTELLIGENCE),
                stat_level_get(pc_obj, STAT_WILLPOWER));
        }
        return;
    }

    if (command->kind == REPLAY_KIND_GIVE) {
        int64_t proto_obj;
        int64_t item_obj;
        int given;

        replay_stall_reported = false;
        replay_next++;
        proto_obj = replay_prototype(command->proto);
        if (proto_obj == OBJ_HANDLE_NULL) {
            tig_debug_printf("Replay: command %d names no prototype %d\n",
                replay_next - 1, command->proto);
            return;
        }
        for (given = 0; given < command->count; given++) {
            if (!object_create(proto_obj, obj_field_int64_get(pc_obj, OBJ_F_LOCATION), &item_obj)
                || !item_transfer(item_obj, pc_obj)) {
                tig_debug_printf("Replay: command %d gave %d of %d\n",
                    replay_next - 1, given, command->count);
                return;
            }
        }
        return;
    }

    if (command->kind == REPLAY_KIND_PROWL) {
        bool was_prowling;

        replay_stall_reported = false;
        replay_next++;
        was_prowling = critter_is_concealed(pc_obj);
        if (was_prowling != (command->prowl != 0)) {
            skill_ui_begin(pc_obj, 0);
        }
        tig_debug_printf("Replay: command %d prowl %s; the player was %s and is"
                         " %s, combat mode %s\n",
            replay_next - 1, command->prowl ? "on" : "off",
            was_prowling ? "prowling" : "not prowling",
            critter_is_concealed(pc_obj) ? "prowling" : "not prowling",
            combat_critter_is_combat_mode_active(pc_obj) ? "on" : "off");
        return;
    }

    if (command->kind == REPLAY_KIND_PANEL) {
        TigMessage click;
        int x;
        int y;

        replay_stall_reported = false;
        replay_next++;
        if (!called_shot_control_point(command->aim, &x, &y)) {
            tig_debug_printf("Replay: command %d: the called-shot panel is not"
                             " shown; no click sent\n",
                replay_next - 1);
            return;
        }
        memset(&click, 0, sizeof(click));
        click.type = TIG_MESSAGE_MOUSE;
        click.data.mouse.x = x;
        click.data.mouse.y = y;
        click.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;
        tig_message_enqueue(&click);
        click.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;
        tig_message_enqueue(&click);
        tig_debug_printf("Replay: command %d clicked the called-shot panel at"
                         " %d,%d for limb %d\n",
            replay_next - 1, x, y, command->aim);
        return;
    }

    if (command->kind == REPLAY_KIND_RAISE) {
        int value;
        int cost;
        int unspent;

        replay_stall_reported = false;
        replay_next++;
        // The plus button's rule (`charedit_ui.c`): refused at the stat's
        // maximum or without the points, and charged `stat_cost` of the new
        // value.
        value = stat_base_get(pc_obj, command->stat) + 1;
        cost = stat_cost(value);
        unspent = stat_level_get(pc_obj, STAT_UNSPENT_POINTS);
        if (stat_level_get(pc_obj, command->stat) >= stat_level_max(pc_obj, command->stat)
            || cost > unspent) {
            tig_debug_printf("Replay: command %d could not raise stat %d"
                             " (%d unspent points, cost %d)\n",
                replay_next - 1, command->stat, unspent, cost);
            return;
        }
        stat_base_set(pc_obj, command->stat, value);
        stat_base_set(pc_obj, STAT_UNSPENT_POINTS, unspent - cost);
        return;
    }

    if (command->kind == REPLAY_KIND_SKILL) {
        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &target_obj)) {
            return;
        }
        if (!skill_use(pc_obj, command->skill, target_obj, 0)) {
            tig_debug_printf("Replay: command %d could not use skill %d on '%s'\n",
                replay_next - 1, command->skill, command->target);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_THROW) {
        int64_t item_obj;

        replay_stall_reported = false;
        replay_next++;
        // A thrown item comes from the pack, not the hands.
        item_obj = replay_find_carried(pc_obj, command->proto, true);
        if (item_obj == OBJ_HANDLE_NULL) {
            return;
        }
        if (!anim_goal_throw_item(pc_obj, item_obj, command->loc)) {
            tig_debug_printf("Replay: command %d could not throw prototype %d\n",
                replay_next - 1, command->proto);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_WIELD) {
        int proto;

        replay_stall_reported = false;
        replay_next++;
        if (replay_parse_int(command->target, &proto) && proto > 0) {
            target_obj = replay_find_carried(pc_obj, proto, false);
            if (target_obj == OBJ_HANDLE_NULL) {
                return;
            }
        } else if (!replay_resolve(command, &target_obj)) {
            return;
        }
        if (!intgame_equip_weapon(target_obj)) {
            tig_debug_printf("Replay: command %d could not wield '%s'\n",
                replay_next - 1, command->target);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_CAST) {
        memset(&td, 0, sizeof(td));
        if (command->cast_target == REPLAY_CAST_LOCATION) {
            td.is_loc = 1;
            td.loc = command->loc;
        } else if (command->cast_target == REPLAY_CAST_SELF) {
            td.obj = pc_obj;
        } else {
            if (!replay_resolve(command, &target_obj)) {
                replay_next++;
                return;
            }
            td.obj = target_obj;
        }

        replay_stall_reported = false;
        replay_next++;

        // Only a spell the character knows: the spell button lists nothing
        // else, and `spell_ui_activate` does not check - measured 2026-09-23,
        // a cast of an unlearned spell played its animation and did nothing.
        if (!spell_is_known(pc_obj, command->spell)) {
            tig_debug_printf("Replay: command %d casts spell %d, which is not known\n",
                replay_next - 1, command->spell);
            return;
        }

        // The spell button, then the click. A spell whose target is the caster
        // is applied by activation itself, as the UI does; anything else is
        // applied through the click path, which checks the target the way a
        // click would. Left in spell mode - a target the click path refused -
        // the replay backs out as a right-click would.
        if (spell_ui_activate(pc_obj, command->spell) == SPELL_UI_ACTIVATE_OK
            && intgame_mode_get() == INTGAME_MODE_SPELL) {
            intgame_apply_mode_to_target(&td);
            if (intgame_mode_get() == INTGAME_MODE_SPELL) {
                spell_ui_cancel();
            }
        }
        return;
    }

    // Real-time session B: a tile is walked to, the way a click on it walks -
    // `anim_goal_move_to_tile`, which is what the UI's own tile click calls. It
    // used to go through the target dispatch below, which a real tile click
    // never reaches: that dispatch reads the target's object type before asking
    // whether it is a tile, and `TargetDescriptor` keeps `obj` and `loc` in one
    // union, so the location was read as an object handle and the game crashed
    // (measured 2026-09-22, SIGSEGV in obj_field_int32_get). No recording ever
    // contained a location row - movement is not recorded - so none replayed.
    if (command->kind == REPLAY_KIND_LOCATION) {
        replay_stall_reported = false;
        replay_next++;
        anim_goal_move_to_tile(pc_obj, command->loc);
        return;
    }

    memset(&td, 0, sizeof(td));
    if (!objid_id_from_str(&oid, command->target)) {
        tig_debug_printf("Replay: command %d names an unreadable target '%s'\n",
            replay_next, command->target);
        replay_next++;
        return;
    }

    target_obj = obj_pool_perm_lookup(oid);
    if (target_obj == OBJ_HANDLE_NULL) {
        // The recorded object is not in this run. That is a real
        // divergence - a replay cannot act on something absent - so say so
        // and move on rather than silently skipping to a different target.
        tig_debug_printf("Replay: command %d targets '%s', absent from this run\n",
            replay_next, command->target);
        replay_next++;
        return;
    }

    if (!replay_target_is_live(target_obj)) {
        // Held rather than skipped, and held rather than submitted. The
        // recording's command named a living target; this run's is dead, so
        // the replay has already diverged from what it is reproducing.
        // Leaving it outstanding is what makes that visible - the engine
        // reports `replay outstanding=N` on the way out and `compare`
        // refuses a replay that ended holding commands.
        if (!replay_stall_reported) {
            replay_stall_reported = true;
            tig_debug_printf("Replay: holding command %d; its target '%s'"
                             " is not a target the click path would give\n",
                replay_next, command->target);
        }
        return;
    }

    td.is_loc = 0;
    td.obj = target_obj;

    replay_stall_reported = false;
    replay_next++;

    // The recorded aim is in force only across this dispatch. Left set, it
    // would silently aim the player's own next click.
    if (command->aim >= 0) {
        called_shot_replay_aim_set(command->aim);
    }
    intgame_perform_action_on_target(&td);
    called_shot_replay_aim_clear();
}
