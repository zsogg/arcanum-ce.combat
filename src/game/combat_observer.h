#ifndef ARCANUM_GAME_COMBAT_OBSERVER_H_
#define ARCANUM_GAME_COMBAT_OBSERVER_H_

#include "game/context.h"

#define COMBAT_OBSERVER_IDENTITY_SIZE 40

bool combat_observer_open(const char* path);
void combat_observer_close(void);
// 2 (card S1, 2026-09-18): `attack.started` gained `requested_hit_location`
// and `aimed`. Every event line now reports this macro; before the bump
// fifteen of the seventeen hardcoded `1` and only `run.started`/`run.ended`
// used it, so a version change would have been announced by two events out of
// seventeen.
//
// 3 (card S1, 2026-09-19): `command.requested` gained `aim`, the limb the
// command is about to commit. Recorded because a replay goes through the same
// dispatch a click does, and that dispatch reads the aim from the panel - so
// without it on the record a replay aims wherever the selector happens to sit.
//
// 4 (real-time combat session 1, 2026-09-21): every event gained `ms`, and
// `run.started` gained `virtual_clock`. The observer could say what happened
// and in what order and never how often, which made a timing baseline
// impossible - the session's own acceptance forbids reporting an event count
// as a firing rate without clock evidence, and there was none to have. `ms` is
// elapsed since the stream opened. It counts real milliseconds normally and
// fixed steps per frame under the deterministic virtual clock, which is why
// the file records which one was running rather than leaving a reader to
// assume.
//
// 5 (card RT1, 2026-09-22): the four questions session 1 could not answer.
// `projectile.impact` is replaced by `projectile.ended`, which carries an
// `outcome`: the old event sat on the animation-cleanup path, fired on 3 of 34
// releases and always reported a null target. `resource.charged` gained
// `spell`, and casts gained `spell.cast`, because a fatigue cost of 5 is Harm
// and Minor Healing alike. `actor.snapshot` gained the wielded weapon's skill,
// skill level, training and effective speed. `action.released` gained the goal
// that released and the timing inputs behind the step delay.
//
// 7 (real-time session B, 2026-09-22): the action budget's own events -
// `action.accepted`, `action.cancelled` and `actor.ready` - so its rules can be
// read off a trace rather than inferred from releases.
//
// 6 (real-time session A, 2026-09-22): what a real-time replay has to drive and
// check. `actor.moved` for every tile a critter changes, `cast.requested` when
// the player applies a spell, and `wield.changed` when the player's weapon slot
// changes by any route. None existed, so a recorded run could not say that the
// player cast, equipped or moved, and a replay could not reproduce it.
//
// 8 (real-time session F, 2026-09-24): `action.cancelled` gained `reason`, so
// a trace says why an action cost nothing - a whiff, turn-based beginning, or
// an animation that ended before its action frame.
#define COMBAT_OBSERVER_SCHEMA 8

bool combat_observer_is_enabled(void);
bool combat_observer_is_complete(void);

void combat_observer_object_id(int64_t obj,
    char identity[COMBAT_OBSERVER_IDENTITY_SIZE]);
void combat_observer_turn_started(const char* actor, int action_points);
void combat_observer_turn_ended(const char* actor, int action_points);

// CE (card O2): the player's command, and whether a goal was queued for it.
// Request first so that anything the command causes follows it in the stream.
// CE (card S1): `aim` is the limb the command will commit, taken before the
// dispatch runs. Recorded here because a replay must reproduce the aim the run
// had, not read the panel at replay time.
void combat_observer_command_requested(const char* actor, const char* target_kind,
    const char* target, int64_t target_loc, int aim);
void combat_observer_command_resolved(const char* actor, bool accepted);

// CE (card O2): whether a command was taken, from the actor's goal state before
// and after the dispatch. Pure, so the judgement a replay trusts can be tested
// without standing up a UI.
bool combat_observer_command_accepted(bool busy_before, bool busy_after,
    bool same_goal);

// CE (card O2): the interaction mode, which is the other half of a command.
//
// `intgame.c` gates its entire attack branch on combat mode, so the same click
// is an attack inside it and something else outside it. A record of clicks
// alone is therefore not a replay input: measured 2026-09-14, a replay that
// ignored the mode did not fail, it quietly performed a different action and
// reported it accepted.
//
// Three axes, because they move independently and a replay needs all three:
//
//   combat_mode        the PC's own combat stance (`R`, or the combat button).
//   turn_based         the turn-based *setting* (`SPACE`), which the player can
//                      change at any time.
//   turn_based_active  whether a turn-based encounter is actually running,
//                      which is what `replay_ui_mode_matches` reads. Turning the
//                      setting on outside combat moves the second and not the
//                      third.
typedef struct CombatObserverMode {
    bool combat_mode;
    bool turn_based;
    bool turn_based_active;
} CombatObserverMode;

// A mode change the player asked for, recorded around the action the same way a
// command is: the request first, so anything the change causes follows it.
void combat_observer_mode_requested(const char* actor, const char* action,
    const CombatObserverMode* before);
void combat_observer_mode_resolved(const char* actor, const char* action,
    bool accepted, const CombatObserverMode* after);

// Did the action move the mode? Pure, for the same reason the command
// judgement is: a refused toggle re-submitted by a replay would put the run in
// a state the recording never reached. Every one of these toggles can be
// refused - dead, stoned, paralysed, or "you cannot exit combat-mode when under
// attack" - and none of them says so in a return value.
bool combat_observer_mode_accepted(const CombatObserverMode* before,
    const CombatObserverMode* after);

// The mode as it stands, sampled once per frame. Emits `mode.changed` only when
// the mode differs from the last one recorded, so a change no player action
// claimed - an NPC attacking, dialogue, sleep, a map transition - is recorded as
// having no player cause rather than silently not recorded at all. That
// omission is what made the first replay unreproducible: combat began in the
// recording with nothing in the record to say how.
void combat_observer_mode_observed(const char* actor,
    const CombatObserverMode* mode);

// CE (card O2): ending a turn, the third player input a replay needs.
//
// Measured 2026-09-14: a replay that could enter combat and attack still froze
// in turn 4 holding action points, because nothing ended the turn. Ending one is
// a button (`combat_ui_end_turn_button_info`) and the `E` key, and the recorder -
// whose boundary was `intgame_perform_action_on_target` - saw neither. That is
// the same finding as the mode gap, one level in.
//
// Recorded as its own pair rather than as a mode action: the three mode axes do
// not move when a turn ends, so reporting it as a mode change would be false.
// The convention the two share is what a replay reads - a named action with no
// target, submitted as a `key` row.
void combat_observer_end_turn_requested(const char* actor, int action_points);
void combat_observer_end_turn_resolved(const char* actor, bool accepted,
    int action_points, bool pc_turn);

// Was the end-turn taken? Pure, like the other two judgements, and for the same
// reason: a refusal recorded as an acceptance is re-submitted by a replay and
// diverges silently.
//
// Unlike the mode toggles, this decision is the UI's own - `combat_ui`
// evaluates all three conditions and only then calls
// `combat_turn_based_next_subturn`, which returns nothing. So the conditions
// *are* the judgement, and holding them here keeps the button and the `E` key
// from drifting apart; they carried byte-identical copies of this test until
// this card deduplicated them.
//
//   turn_in_progress   false  nobody has a turn, so there is none to end.
//                             Deliberately *this* and not
//                             `combat_turn_based_is_active`: the shipped UI
//                             tests `combat_turn_based_whos_turn_get() !=
//                             OBJ_HANDLE_NULL`, and the two are separate
//                             variables in `combat.c`. Substituting the flag
//                             would be a behaviour change dressed as a
//                             refactor.
//   pc_turn            false  it is someone else's turn; the press is ignored.
//   actor_ready        false  the PC is mid-animation. Note this is not simply
//                             "not busy": a critter standing around is busy
//                             with its fidget goal, and refusing the press then
//                             would make ending a turn depend on animation
//                             timing.
bool combat_observer_end_turn_accepted(bool turn_in_progress, bool pc_turn,
    bool actor_ready);

// CE (card RT1): `weapon_skill` is `item_weapon_skill` of the wielded weapon
// (melee when unarmed), with its level and training degree beside it, and
// `weapon_speed` is `item_weapon_magic_speed` - the value the attack path
// subtracts from its step delay, 10 ms a point above 10. Apprentice training
// is +5 of it. Without these a cadence cannot be interpreted: on 2026-09-21
// the only sign that training had been attempted was a Strength change.
void combat_observer_actor_snapshot(const char* actor, int level,
    int strength, int dexterity, int speed, int melee, int dodge,
    int hp_current, int hp_max, int fatigue_current, int fatigue_max,
    int weapon_description, int armor_description, int weapon_skill,
    int weapon_skill_level, int weapon_training, int weapon_speed);
void combat_observer_ap_checked(const char* actor, int requested,
    int before, bool accepted);
void combat_observer_ap_committed(const char* actor, int requested,
    int before, int after, bool accepted, int fatigue_before,
    int fatigue_after);
unsigned long long combat_observer_attack_started(const char* attacker,
    const char* target, int skill, int hit_location,
    int requested_hit_location, bool aimed);
void combat_observer_attack_resolved(unsigned long long action_id,
    const char* attacker, const char* target, int skill, int hit_location,
    unsigned int flags, int result);
// Real-time combat session 1: the boundaries a timing baseline needs and the
// observer did not have. `attack.started`/`attack.resolved` bracket a resolved
// attack, which is not the same question as "when did the animation let this
// actor act again" - and in real time that is the question, because the ranged
// repeat path rewinds the animation rather than restarting the attack.
//
// `anim_reached_action_frame` is the release: its own comment calls it "the
// moment the blow lands or the item changes hands", and flag 0x08 latches so
// it fires once per action. The bow rewind clears 0x0C, which re-arms it - so
// one release event per arrow, and the spacing between them IS the firing
// rate the session is trying to measure.
//
// CE (card RT1): the timing inputs, so a cadence can be explained and not only
// restated. `goal` is the animation goal that released, which separates a bow
// shot from a cast. `art_fps` is the art's own rate; `speed_fps` is
// `anim_get_animation_fps` at the actor's Speed, which is what the attack path
// divides into 1000 and clamps to 30..800 ms; `weapon_speed` is subtracted from
// that 10 ms a point above 10, floored at 30 ms; `step_ms` is the pause the
// animation is actually running at, after all of it. -1 means the art could
// not be read, as for `frame`.
void combat_observer_action_released(const char* actor, int goal, int frame,
    int action_frame, int weapon_type, int art_fps, int speed_fps,
    int weapon_speed, int step_ms);

// CE (card RT1): how a projectile's flight ended, one event per ending.
//
//   struck        it met an object: the intended target on a hit, or a
//                 bystander or obstacle that intercepted it. `struck` says
//                 which, and the attack against it follows in the stream.
//   tile          it reached its target tile with no object to strike there.
//   returned      a boomerang arriving back after its outbound flight.
//   out_of_range  it flew past its weapon's range.
//   abandoned     its animation was torn down while it still existed.
//
// The event this replaces sat only on the last of these, which is why it fired
// on 3 of 34 releases and never named a target. Releases and endings are
// counted separately on purpose: an arrow that reaches nothing commits no
// damage and would otherwise be invisible.
void combat_observer_projectile_ended(const char* source, const char* target,
    const char* struck, const char* outcome);

// CE (card RT1): a spell that has begun, after its upkeep and fatigue checks
// passed. `caster` is the critter the spell is attributed to and `source` what
// cast it - the same critter, or an item such as a grenade or a scroll.
// `free` is `MAGICTECH_RUN_FREE`: no fatigue was asked of the caster.
void combat_observer_spell_cast(const char* caster, const char* source,
    const char* target, int spell, bool free);

// Ammunition and fatigue, the two things an ordinary action spends outside
// turn-based AP. One event with a `resource` field rather than two events,
// because the question asked of both is the same - was this charged once per
// accepted action, or once per release?
// CE (card RT1): `spell` is the spell a fatigue charge paid for, or -1 (JSON
// null) for ammunition and for a charge made on no spell's behalf.
void combat_observer_resource_charged(const char* actor, const char* resource,
    int spell, int cost, int before, int after);

// Real-time session A: a critter left one tile for another. One event per
// tile, from `object_move_to`, which every movement route passes through -
// walking, running, knockback, teleports - so the record is what happened and
// not what was intended. Rule 12's "time spent moving" is measured from these.
void combat_observer_actor_moved(const char* actor, int64_t from_loc,
    int64_t to_loc);

// Real-time session A: the player applied a spell to a target. The request,
// before its fatigue and line-of-sight checks: `spell.cast` follows only if the
// spell began, so a refused cast is a request with no cast after it.
// `target_kind` is "object" or "location"; `target_loc` is 0 for an object.
void combat_observer_cast_requested(const char* actor, int spell,
    const char* target_kind, const char* target, int64_t target_loc);

// Real-time session A: the player's weapon slot, sampled once a frame like the
// mode. Equipping has three routes - dragging to the paper doll, the hotkey
// bar, double-clicking - and none shares a function a hook could sit in, so the
// slot is watched instead. The first sample is the starting weapon, not a
// change. `weapon` is "NULL" for an empty slot.
void combat_observer_wield_observed(const char* actor, const char* weapon);

// Real-time session B: the action budget (rt_budget.c). `event` is
// "action.accepted" when an action starts and takes its time, "action.cancelled"
// when one ends before releasing, and "actor.ready" when an actor may act
// again. `category` is "attack", "cast", "throw" or "skill"; `elapsed_ms` is
// the time counted so far, which for a ready actor is the whole duration.
// `reason` (schema 8, session F) is written only when it is not NULL, which is
// on "action.cancelled": "whiff" (a melee strike found no one in reach),
// "turn_based" (turn-based began during the wind-up), "unreleased" (the
// animation ended before its action frame - a new order, an interruption, a
// target gone) or "moved" (a standing action - a reload - was cancelled by a
// step, from 2026-09-25).
void combat_observer_budget(const char* event, const char* actor,
    const char* category, int action_points, int duration_ms, int elapsed_ms,
    const char* reason);

void combat_observer_skill_roll(const char* source, const char* target,
    int skill, int effectiveness, int difficulty, int roll,
    int critical_roll, bool success, bool critical);
void combat_observer_damage_committed(const char* attacker,
    const char* target, const int* components, unsigned int damage_flags,
    int hp_amount, int hp_before, int hp_after, int fatigue_before,
    int fatigue_after, bool dead, bool unconscious);

// CE (combat AI, milestone AI-0): one combat decision by an NPC - what it
// meant to do this pass and why. `danger` is none, combat, flee or surrender;
// `origin` is the branch that decided (heal, pickup or decide); `action` what
// it did (attack, cast, item, skill, grenade, reload, back_off, flee,
// surrender). `cast_chance` and `cast_roll` are the spell roll when it was
// made this pass, else -1; `considered` lists each spell weighed as [spell,
// code], code 0 for castable and `ai_check_use_spell`'s refusal otherwise (9:
// the target is already under it). In real time only a changed decision is
// written. `desperate` (milestone AI-2) is true when the NPC's desperation
// roll for this turn passed, letting it cast below its fatigue floor.
#define COMBAT_OBSERVER_AI_CONSIDERED_MAX 8
void combat_observer_ai_decided(const char* actor, const char* target,
    const char* danger, const char* origin, const char* action, int spell,
    const char* item, const char* weapon, int cast_chance, int cast_roll,
    const int considered[][2], int considered_count, bool desperate);

// CE (combat AI, milestone AI-1): an NPC choosing its weapon under `ai armor
// aware`. `candidates` lists each weapon weighed as [description, score,
// expected_permille]: the score is its average damage times skill
// effectiveness with the normal part at the share expected through the
// target's armor; `kept` is true when the weapon already in hand stayed for
// commitment though another scored higher.
#define COMBAT_OBSERVER_AI_WEAPONS_MAX 8
void combat_observer_ai_weapon_chosen(const char* actor, const char* target,
    const char* previous, const char* chosen, bool kept,
    const int candidates[][3], int candidate_count);

// CE (armor milestone 3): one weapon hit's normal damage resolved against
// armor under `armor penetration`. `worn` is the location's worn protection
// and `natural` the creature's own, before block, gap and magic; the
// effective figures are in hundredths, as `penetration.h` computes them;
// `passed_num / passed_den` is the share of damage passed. `fatigue_added`
// is the fatigue a monster's natural attack gains from the damage armor
// stopped, before fatigue resistance (melee, bow and thrown pass, 2026-10-04).
void combat_observer_armor_resolved(const char* attacker, const char* target,
    int hit_loc, int worn_centi, int natural, int magic, bool blocked,
    bool gap, int penetration, int variation_percent, int protection_centi,
    int penetration_centi, int passed_num, int passed_den, int damage_before,
    int damage_after, int fatigue_added);

// CE (card O2): the two facts a harness needs to know the duel is over.
//
// `combat_observer_terminal_seen` latches when a `damage.committed` reports a
// death or an incapacitation. It is deliberately narrower than O1's notion of a
// *valid* terminal outcome, which is a scenario property compared against
// `valid_outcomes` in the run record: the engine knows only that a critter went
// down. For the O1 fixture the two coincide, and a scenario whose ending is
// survival or escape would need its own signal rather than this one.
//
// `combat_observer_event_count` is the write counter. A harness watches it stop
// moving rather than guessing how long a death animation lasts, so the trace is
// quiet by measurement instead of by a constant nobody can justify.
bool combat_observer_terminal_seen(void);
unsigned int combat_observer_event_count(void);

/* Pure writer fault seams used by the unit suite. */
void combat_observer_test_set_event_limit(unsigned int limit);
void combat_observer_test_fail_after(int lines);
void combat_observer_test_reset(void);

#endif /* ARCANUM_GAME_COMBAT_OBSERVER_H_ */
