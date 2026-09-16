#ifndef ARCANUM_GAME_COMBAT_OBSERVER_H_
#define ARCANUM_GAME_COMBAT_OBSERVER_H_

#include "game/context.h"

#define COMBAT_OBSERVER_IDENTITY_SIZE 40

bool combat_observer_open(const char* path);
void combat_observer_close(void);
bool combat_observer_is_enabled(void);
bool combat_observer_is_complete(void);

void combat_observer_object_id(int64_t obj,
    char identity[COMBAT_OBSERVER_IDENTITY_SIZE]);
void combat_observer_turn_started(const char* actor, int action_points);
void combat_observer_turn_ended(const char* actor, int action_points);

// CE (card O2): the player's command, and whether a goal was queued for it.
// Request first so that anything the command causes follows it in the stream.
void combat_observer_command_requested(const char* actor, const char* target_kind,
    const char* target, int64_t target_loc);
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
    const char* target, int skill, int hit_location);
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

/* Pure writer fault seams used by the unit suite. */
void combat_observer_test_set_event_limit(unsigned int limit);
void combat_observer_test_fail_after(int lines);
void combat_observer_test_reset(void);

#endif /* ARCANUM_GAME_COMBAT_OBSERVER_H_ */
