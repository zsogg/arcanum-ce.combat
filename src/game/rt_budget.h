#ifndef ARCANUM_GAME_RT_BUDGET_H_
#define ARCANUM_GAME_RT_BUDGET_H_

#include <stdbool.h>
#include <stdint.h>

#include "game/anim.h"

// CE (real-time session B): the real-time action budget.
//
// Turn-based combat limits actions with action points; real time has nothing
// in their place, so an animation's own length decides how often an actor
// acts - a bow rewinds three frames and fires again, Harm has no casting
// animation at all. This gives every real-time attack, throw and ordinary cast
// a length of `R * AP / max(5, Speed)`, the same proportions turn-based sets.
//
// The rules are the session-2 rule sheet in arcanum-work's
// docs/REAL-TIME-COMBAT-OVERHAUL-PLAN.md (section 7a); the numbers below name
// them. Behind `real time budget`, default off: with it off nothing here runs.
//
// One action at a time per actor, in three states:
//
//   ready       free to act.
//   accepted    an action has started (rule 6: its wind-up plays as shipped)
//               and has not released. Cancelled at no cost if its animation
//               ends first (rule 7).
//   released    its effect has happened; recovery runs until the action's
//               time has elapsed, counted from acceptance.
//
// "Holding" is not a state here but what the animation does when an attempt
// finds its actor not ready: it does not advance a frame, and asks again on
// the next step. A newer command replaces the held attempt the way it replaces
// any animation (rule 4).
//
// The clock (rule 2) is real milliseconds, advanced from `timeevent_ping` only
// while the game is not paused (rule 8), only in real time, and - with the
// movement setting on (rule 12) - not while the actor's current goal is a
// movement goal. Nothing is saved (rule 3): a load or a new game starts every
// actor ready.

#define RT_BUDGET_DEFAULT_PACE_MS 2000

typedef enum RtBudgetCategory {
    RT_BUDGET_ATTACK,
    RT_BUDGET_CAST,
    // Session C: the throw-item goal - thrown weapons, grenades, anything
    // dragged onto the map. Costs what the throw goal charges in turn-based.
    RT_BUDGET_THROW,
} RtBudgetCategory;

typedef enum RtBudgetState {
    RT_BUDGET_READY,
    RT_BUDGET_ACCEPTED,
    RT_BUDGET_RELEASED,
} RtBudgetState;

// One actor's action. Pure data, so the rules can be tested without the
// animation system; the engine side keeps a table of these.
typedef struct RtBudgetEntry {
    int64_t obj;
    RtBudgetState state;
    RtBudgetCategory category;
    AnimID run;
    // Which action within `run` was accepted. A run is not an action: the
    // game's own real-time spell queue adds a cast to the run already casting,
    // and an attack loop replaces one attempt with the next in the same run.
    // So a cast is keyed by its spell run and an attack by a count of attempts
    // begun (AGbeginAnimAttack); measured 2026-09-22, keying by run alone let
    // three queued Harm casts release on one acceptance.
    int action_key;
    int attempts_begun;
    AnimID attempt_run;
    int action_points;
    int duration_ms;
    int elapsed_ms;
    // A held attempt whose "action frame reached" flag (run flag 0x04) was
    // already raised when it began - Harm's casting effect has its action frame
    // at 0. Held with the flag down, or it would release without being
    // accepted; raised again on acceptance.
    AnimID held_run;
    bool held_action_flag;
    // While a run holds, it asks again after the recovery left rather than
    // after a whole animation step - 108 ms for a guard's sword - so it starts
    // within a frame of being ready. The run's own step delay is kept here and
    // put back when the action is accepted.
    bool held_pause_saved;
    unsigned int held_pause_ms;
} RtBudgetEntry;

// Rule 1: `pace_ms * action_points / max(5, speed)`, rounded to the
// millisecond. A non-positive cost or pace is no time at all.
int rt_budget_duration_ms(int pace_ms, int action_points, int speed);

// The rules on one entry.
void rt_budget_entry_accept(RtBudgetEntry* entry, RtBudgetCategory category,
    AnimID run, int action_points, int duration_ms);
void rt_budget_entry_release(RtBudgetEntry* entry);
// Advances the clock. Returns true when this call made the actor ready.
// `counts` is false while the game should not count this actor's time - the
// caller decides that from pausing and movement.
bool rt_budget_entry_tick(RtBudgetEntry* entry, int delta_ms, bool counts);
// Rule 5: turn-based began. An action still winding up is cancelled - it
// never released - and a recovery starts again from zero for when real time
// resumes. Returns true if an unreleased action was cancelled.
bool rt_budget_entry_turn_based_began(RtBudgetEntry* entry);
bool rt_budget_entry_ready(const RtBudgetEntry* entry);

// The engine side.
void rt_budget_init(void);
void rt_budget_reset(void);
bool rt_budget_active(void);

// Called at the top of each attack and spell-attempt update. True means hold:
// do not advance a frame this step.
bool rt_budget_hold(AnimRunInfo* run_info);
// Called where a bow or gun would rewind for another shot. True means hold the
// drawn pose instead of rewinding; false means rewind - a new action has been
// accepted if the budget is active.
bool rt_budget_hold_repeat(AnimRunInfo* run_info);
// Called where an attempt reaches its action frame.
void rt_budget_released(AnimRunInfo* run_info);
// Called where an attack attempt begins, so the next hold knows it is a new
// action and not the rest of the last one.
void rt_budget_attack_begun(AnimRunInfo* run_info);
// True when the casting run `anim_id` already has a cast waiting - one it holds
// that the budget has not accepted, whether queued at the bottom of its goal
// stack or current and held. The game's own real-time spell queue asks before
// adding another: rule 4 allows one request waiting.
bool rt_budget_cast_waiting(AnimID* anim_id);
// Called from `timeevent_ping` with the real milliseconds since the last ping.
void rt_budget_tick(int delta_ms);

#endif /* ARCANUM_GAME_RT_BUDGET_H_ */
