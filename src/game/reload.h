#ifndef ARCANUM_GAME_RELOAD_H_
#define ARCANUM_GAME_RELOAD_H_

#include <stdbool.h>
#include <stdint.h>

// CE (turn-based rebalance, reload prototype session 2): firearm reloading,
// behind `firearm reloading`, default off.
//
// A gun whose `rules/weapons.mes` entry names a Capacity (`weapon_rules.h`)
// holds up to that capacity in
// **ready shots**. Firing uses one loaded shot and no spare ammunition; a gun
// with nothing loaded does not fire. One Reload command (Q) works toward one
// more shot: in turn-based it spends the turn's AP toward the shot's loading
// cost, keeping unfinished work for a later command, and never draws on
// fatigue; in real time it takes its time **standing still**, timed by the
// real-time budget as a `reload` action, and a step cancels it with nothing
// loaded (zsogg, 2026-09-25, after his playtest). When a shot's loading
// completes, that shot's spare bullets move into the gun.
//
// The first build applies to the local player only; followers and hostile
// gunners keep stock ammunition. The rules, and the decisions behind them,
// are arcanum-work docs/RELOAD-PROTOTYPE.md.
//
// **Where the state lives:** the weapon's two padding fields, which nothing
// else uses. `OBJ_F_WEAPON_PAD_I_1` is the loaded shots, `OBJ_F_WEAPON_PAD_I_2`
// the loading work done toward the next shot, in AP. Set on the instance, they
// persist through the ordinary changed-field path: saves, swaps, drops and
// trades carry them with the gun. A new gun, and every gun in an existing save,
// reads 0 in both - empty, as zsogg decided (2026-09-25).

// Pure: one Reload command's turn-based work. Spends up to `available_ap`
// toward the `cost - progress` still owed, and says how much it spent and
// whether the shot is now loaded. Never spends more than it has - which is what
// keeps fatigue out of it.
typedef struct ReloadWork {
    int spent;
    int progress;
    bool completed;
} ReloadWork;
ReloadWork reload_work(int cost, int progress, int available_ap);

// True when reloading governs this critter's use of this weapon: the feature
// is on, the weapon's rule names a Capacity, and - in the first build - the
// critter is the local player.
bool reload_applies(int64_t critter_obj, int64_t weapon_obj);
// True when the critter's wielded weapon is governed and has nothing loaded.
bool reload_wielded_is_empty(int64_t critter_obj);

int reload_loaded_get(int64_t weapon_obj);
int reload_progress_get(int64_t weapon_obj);

// Fires: uses one loaded shot. False, changing nothing, when none is loaded.
bool reload_fire(int64_t critter_obj, int64_t weapon_obj);

// The Reload command (Q), for the local player. Reports what it did over the
// player's head; returns true if it did any work, or began a real-time reload.
bool reload_command(int64_t critter_obj);

// The real-time budget's answer to a reload begun with
// `rt_budget_begin_standing_action`: its time passed with the player standing
// (`completed`) - the shot is loaded now - or it was cancelled by a step or by
// turn-based beginning, and nothing is loaded or spent.
void reload_standing_done(int64_t critter_obj, bool completed);

#endif /* ARCANUM_GAME_RELOAD_H_ */
