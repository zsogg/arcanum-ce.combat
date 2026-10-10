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
// It governs everyone who uses a listed gun (session 4; the first build was
// the player's only). An NPC's gun starts loaded to capacity, the player's
// empty; an NPC loads from the bullets it carries, so a hostile gunner's
// ammunition is finite where stock gave it unlimited. The rules, and the
// decisions behind them, are arcanum-work docs/RELOAD-PROTOTYPE.md.
//
// **Where the state lives:** the weapon's two padding fields, which nothing
// else uses. `OBJ_F_WEAPON_PAD_I_1` is the loaded shots, `OBJ_F_WEAPON_PAD_I_2`
// the loading work done toward the next shot, in AP. Set on the instance, they
// persist through the ordinary changed-field path: saves, swaps, drops and
// trades carry them with the gun. A new gun, and every gun in an existing save,
// reads 0 in both - empty, as zsogg decided (2026-09-25).
//
// **A whole-magazine reload** (2026-10-03): an entry with `WholeMagazine: 1`
// takes its LoadAP once for the magazine, by the same routes, and the
// completed work loads every shot the capacity and the pack allow.

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
// is on and the weapon's rule names a Capacity. The first time it is asked
// about a gun, it also sets how that gun starts: full for an NPC.
bool reload_applies(int64_t critter_obj, int64_t weapon_obj);
// True when the critter's wielded weapon is governed and has nothing loaded.
bool reload_wielded_is_empty(int64_t critter_obj);
// True when the player's wielded firearm cannot fire for want of ammunition:
// a gun with no magazine and less in the pack than a shot spends, or a
// governed gun with nothing loaded and nothing in the pack to load. Under
// `firearm reloading`, and for the player only (zsogg, 2026-10-03: "make empty
// guns refuse the shot with 'No ammo!'"); an NPC keeps the stock path, where
// running dry is what sends the AI for another weapon.
bool reload_wielded_out_of_ammo(int64_t critter_obj);

// Whether the gun can take another shot now: governed, not full, and the
// shot's bullets in the pack. Says nothing over anyone's head.
bool reload_can_load_quiet(int64_t critter_obj, int64_t weapon_obj);

int reload_loaded_get(int64_t weapon_obj);
int reload_progress_get(int64_t weapon_obj);

// Fires: uses one loaded shot. False, changing nothing, when none is loaded.
bool reload_fire(int64_t critter_obj, int64_t weapon_obj);

// The Reload command: Q for the player, and an NPC's reload (`reload_ai_act`).
// Reports what it did over the player's head; returns true if it did any work,
// or began a real-time reload.
bool reload_command(int64_t critter_obj);

// The real-time budget's answer to a reload begun with
// `rt_budget_begin_standing_action`: its time passed with the player standing
// (`completed`) - the shot is loaded now - or it was cancelled by a step or by
// turn-based beginning, and nothing is loaded or spent.
void reload_standing_done(int64_t critter_obj, bool completed);

// ---------------------------------------------------------------------------
// Criticals CR-4.1: jams, under `firearm fumbles`, default off (zsogg,
// 2026-10-06; arcanum-work docs/CRITICALS.md). A firearm's fumble jams the
// gun half the time, hits its wielder a fifth of the time - for half damage,
// losing 5 shots of ammunition rather than all of it - and otherwise does
// nothing. At Firearms Expert the self-hit jams instead; a Firearms Master
// never fumbles. A jammed gun does not fire. The Reload command clears it
// instead of loading, for `combat_jam_clear_cost` - the work kept across
// turns as loading's is - keeping the shots loaded and needing no ammunition.
//
// **Where the state lives:** beside loading's, in `OBJ_F_WEAPON_PAD_I_2`: a
// jammed bit and, above it, the clearing work done. A gun in an existing save
// reads neither. With the switch off the bit is ignored, as reloading ignores
// loaded shots when it is off.
//
// It governs everyone's fumbles with a governed gun (milestone CR-4.2; the
// player's alone in CR-4.1). An NPC clears a jam as it loads
// (`reload_ai_act`), "Jammed!" floating over one the player can see; and a
// wielded gun of the player's or a follower's clears itself outside combat,
// in its clearing time (`reload_prep_tick`).
// ---------------------------------------------------------------------------

typedef enum ReloadFumble {
    RELOAD_FUMBLE_JAM,
    RELOAD_FUMBLE_SELF_HIT,
    RELOAD_FUMBLE_NOTHING,
} ReloadFumble;

// Pure: what a firearm's fumble does, from a 1-100 roll and the shooter's
// Firearms training: 50% a jam, 20% a hit on itself, 30% nothing; at Expert
// that hit is a jam too, and a Master's fumble is nothing.
ReloadFumble reload_fumble_outcome(int roll, int training);

// True when `firearm fumbles` governs this critter's fumble with this
// weapon: the switch is on, the weapon is a firearm, and reloading governs
// it for this critter.
bool reload_jams_apply(int64_t critter_obj, int64_t weapon_obj);
// True when the gun is jammed and the switch is on.
bool reload_jammed(int64_t weapon_obj);
// True when the critter's wielded gun is governed and jammed.
bool reload_wielded_is_jammed(int64_t critter_obj);
// Jams the gun, saying so over its user's head.
void reload_jam(int64_t critter_obj, int64_t weapon_obj);
int reload_clear_progress_get(int64_t weapon_obj);

// Session 4, the AI (`ai_action_perform_combat`): for an NPC whose governed
// gun is empty - or whose standing reload is under way - decide instead of
// attacking. With an enemy adjacent it takes a melee backup if it carries one
// (and returns false, so the attack goes ahead with it); otherwise it reloads,
// ending its turn-based turn if no AP is left for the work; with nothing to
// load, it switches weapon. A gun with a shot loaded fires at a target in
// range, and with none in range is topped up a shot at a time first (session
// 5). True when it has acted and must not attack.
bool reload_ai_act(int64_t critter_obj, int64_t enemy_obj);

// Session 4: automatic loading outside combat, from `timeevent_ping`. The
// player's and each follower's wielded listed gun, while the player and it are
// out of combat mode, loads one shot each time its loading cost passes as time
// - the length a standing reload would take - using real ammunition. A fight
// starting keeps what is loaded and drops the time toward the next.
void reload_prep_tick(int delta_ms);

#endif /* ARCANUM_GAME_RELOAD_H_ */
