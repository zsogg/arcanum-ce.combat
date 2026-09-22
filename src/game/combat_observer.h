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
#define COMBAT_OBSERVER_SCHEMA 4

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

void combat_observer_actor_snapshot(const char* actor, int level,
    int strength, int dexterity, int speed, int melee, int dodge,
    int hp_current, int hp_max, int fatigue_current, int fatigue_max,
    int weapon_description, int armor_description);
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
void combat_observer_skill_roll(const char* source, const char* target,
    int skill, int effectiveness, int difficulty, int roll,
    int critical_roll, bool success, bool critical);
void combat_observer_damage_committed(const char* attacker,
    const char* target, const int* components, unsigned int damage_flags,
    int hp_amount, int hp_before, int hp_after, int fatigue_before,
    int fatigue_after, bool dead, bool unconscious);

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
