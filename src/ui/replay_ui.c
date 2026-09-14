#include "ui/replay_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/debug.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/obj.h"
#include "game/obj_id.h"
#include "game/obj_pool.h"
#include "game/player.h"
#include "game/target.h"
#include "ui/intgame.h"

#define REPLAY_MAX_COMMANDS 256
#define REPLAY_IDENTITY_SIZE 64

typedef struct ReplayCommand {
    int turn;
    bool is_loc;
    int64_t loc;
    char target[REPLAY_IDENTITY_SIZE];
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

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        if (sscanf(line, "%d %31s %63s", &turn, kind, target) != 3) {
            tig_debug_printf("Replay: cannot parse '%s'; refusing the file\n", line);
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
        if (strcmp(kind, "location") == 0) {
            replay_commands[replay_count].is_loc = true;
            replay_commands[replay_count].loc = (int64_t)strtoll(target, NULL, 10);
            replay_commands[replay_count].target[0] = '\0';
        } else {
            replay_commands[replay_count].is_loc = false;
            replay_commands[replay_count].loc = 0;
            strncpy(replay_commands[replay_count].target, target,
                sizeof(replay_commands[replay_count].target) - 1);
            replay_commands[replay_count].target[
                sizeof(replay_commands[replay_count].target) - 1] = '\0';
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
    AnimID anim_id;
    int64_t pc_obj;
    int64_t target_obj;

    if (!replay_loaded || replay_next >= replay_count) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // One command at a time, and only while the actor is idle. A command
    // submitted over a running goal would be refused or would replace it, and
    // either way the replay would stop matching what it is reproducing.
    if (anim_is_busy(pc_obj, &anim_id)) {
        return;
    }

    command = &replay_commands[replay_next];

    if (!replay_mode_matches(command)) {
        // Say it once. A replay that ends with commands outstanding did not
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
    replay_stall_reported = false;

    memset(&td, 0, sizeof(td));
    if (command->is_loc) {
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

        td.is_loc = 0;
        td.obj = target_obj;
    }

    replay_next++;
    intgame_perform_action_on_target(&td);
}
