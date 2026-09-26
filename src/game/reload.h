#ifndef ARCANUM_GAME_RELOAD_H_
#define ARCANUM_GAME_RELOAD_H_

#include <stdbool.h>
#include <stdint.h>

// CE (turn-based rebalance, reload prototype session 2): firearm reloading,
// behind `firearm reloading`, default off.
//
// A gun listed in `rules/firearm_reload.mes` holds up to its capacity in
// **ready shots**. Firing uses one loaded shot and no spare ammunition; a gun
// with nothing loaded does not fire. One Reload command (Q) works toward one
// more shot: in turn-based it spends the turn's AP toward the shot's loading
// cost, keeping unfinished work for a later command, and never draws on
// fatigue; in real time it loads the shot at once and the real-time budget
// then times it as a `reload` action. When a shot's loading completes, that
// shot's spare bullets move into the gun.
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

// One listed weapon: `{<prototype>}{Capacity: 1, LoadAP: 4, LoadAPApprentice: 3}`.
typedef struct ReloadRule {
    int prototype;
    int capacity;
    int load_ap;
    // At Apprentice training or better in the weapon's skill.
    int load_ap_apprentice;
} ReloadRule;

// The keys a rule may name, and their clamps. `LoadAPApprentice` defaults to
// `LoadAP` when absent.
#define RELOAD_CAPACITY_MIN 1
#define RELOAD_CAPACITY_MAX 100
#define RELOAD_LOAD_AP_MIN 1
#define RELOAD_LOAD_AP_MAX 20

// Pure: reads one entry's text into `rule`. False, with `rule` untouched, for
// anything malformed - an unknown key, a value that is not a number, a
// missing Capacity or LoadAP - so a bad entry lists nothing rather than a
// guessed weapon. Values outside their range are clamped.
bool reload_rule_parse(int prototype, const char* str, ReloadRule* rule);

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

void reload_init(void);
void reload_exit(void);
// Reads `rules/firearm_reload.mes` again. Called at init.
void reload_rules_load(void);

// The rule for this weapon, or NULL when it is not listed.
const ReloadRule* reload_rule_for(int64_t weapon_obj);
// True when reloading governs this critter's use of this weapon: the feature
// is on, the weapon is listed, and - in the first build - the critter is the
// local player.
bool reload_applies(int64_t critter_obj, int64_t weapon_obj);
// True when the critter's wielded weapon is governed and has nothing loaded.
bool reload_wielded_is_empty(int64_t critter_obj);

int reload_loaded_get(int64_t weapon_obj);
int reload_progress_get(int64_t weapon_obj);

// Fires: uses one loaded shot. False, changing nothing, when none is loaded.
bool reload_fire(int64_t critter_obj, int64_t weapon_obj);

// The Reload command (Q), for the local player. Reports what it did over the
// player's head; returns true if it did any work.
bool reload_command(int64_t critter_obj);

#endif /* ARCANUM_GAME_RELOAD_H_ */
