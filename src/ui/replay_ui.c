#include "ui/replay_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/debug.h>
#include <tig/timer.h>

#include "game/combat.h"
#include "game/critter.h"
#include "game/obj.h"
#include "game/obj_id.h"
#include "game/obj_pool.h"
#include "game/player.h"
#include "game/target.h"
#include "ui/called_shot.h"
#include "ui/combat_ui.h"
#include "ui/intgame.h"
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
    //   wield <id>
    //       the hotkey bar's equip, the one equip route with one function.
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
    static const char* kinds[] = { "object", "location", "key", "cast", "wield", "wait", "learn" };
    static const char* casts[] = { "self", "object", "location" };

    if (index < 0 || index >= replay_count) {
        return false;
    }
    command = &(replay_commands[index]);
    snprintf(buffer, size,
        "turn=%d now=%d kind=%s target=%s loc=%lld aim=%d spell=%d cast=%s wait=%d",
        command->turn, command->now ? 1 : 0, kinds[command->kind],
        command->target, (long long)command->loc, command->aim, command->spell,
        command->kind == REPLAY_KIND_CAST ? casts[command->cast_target] : "-",
        command->wait_ms);
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
        return;
    }

    if (command->kind == REPLAY_KIND_WIELD) {
        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &target_obj)) {
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

    memset(&td, 0, sizeof(td));
    if (command->kind == REPLAY_KIND_LOCATION) {
        td.is_loc = 1;
        td.loc = command->loc;
    } else {
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
    }

    replay_stall_reported = false;
    replay_next++;

    // The recorded aim is in force only across this dispatch. Left set, it
    // would silently aim the player's own next click.
    called_shot_replay_aim_set(command->aim);
    intgame_perform_action_on_target(&td);
    called_shot_replay_aim_clear();
}
