#include "ui/replay_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/debug.h>

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
} ReplayKind;

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
} ReplayCommand;

static ReplayCommand replay_commands[REPLAY_MAX_COMMANDS];
static int replay_count;
static int replay_next;
static bool replay_loaded;
static bool replay_stall_reported;

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

bool replay_ui_load(const char* path)
{
    FILE* stream;
    char line[512];

    replay_count = 0;
    replay_next = 0;
    replay_loaded = false;
    replay_stall_reported = false;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        tig_debug_printf("Replay: cannot open '%s'; nothing will be replayed\n", path);
        return false;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        char kind[32];
        char target[REPLAY_IDENTITY_SIZE];
        int turn;
        int aim;
        int fields;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        aim = HIT_LOC_TORSO;
        fields = sscanf(line, "%d %31s %63s %d", &turn, kind, target, &aim);
        if (fields != 3 && fields != 4) {
            tig_debug_printf("Replay: cannot parse '%s'; refusing the file\n", line);
            fclose(stream);
            replay_count = 0;
            return false;
        }

        if (aim < 0 || aim >= HIT_LOC_COUNT) {
            // Refused rather than clamped. A record naming a limb this build
            // has no entry for cannot be reproduced, and replaying it as some
            // other limb would be a wrong reproduction that reads as a
            // gameplay difference.
            tig_debug_printf("Replay: aim %d is not a hit location; refusing"
                             " the file\n",
                aim);
            fclose(stream);
            replay_count = 0;
            return false;
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

        replay_commands[replay_count].turn = turn;
        replay_commands[replay_count].aim = aim;
        replay_commands[replay_count].loc = 0;
        replay_commands[replay_count].target[0] = '\0';
        if (strcmp(kind, "location") == 0) {
            replay_commands[replay_count].kind = REPLAY_KIND_LOCATION;
            replay_commands[replay_count].loc = (int64_t)strtoll(target, NULL, 10);
        } else {
            replay_commands[replay_count].kind = strcmp(kind, "key") == 0
                ? REPLAY_KIND_KEY
                : REPLAY_KIND_OBJECT;
            strncpy(replay_commands[replay_count].target, target,
                sizeof(replay_commands[replay_count].target) - 1);
            replay_commands[replay_count].target[sizeof(replay_commands[replay_count].target) - 1] = '\0';
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
    if (!combat_ui_actor_ready(pc_obj)) {
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

    command = &replay_commands[replay_next];

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
