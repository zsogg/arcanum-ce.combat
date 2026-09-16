#ifndef ARCANUM_UI_REPLAY_UI_H_
#define ARCANUM_UI_REPLAY_UI_H_

#include <stdbool.h>

// CE (card O2): re-submit a recorded run's commands through the ordinary
// validated action path.
//
// **Not a simulation and not a policy.** It replays what a player actually did,
// command by command, through `intgame_perform_action_on_target` - the same
// entry point a mouse click uses - so action eligibility, movement, animation
// and AP scheduling all happen exactly as they do in play. Driving combat by
// calling `combat_attack` in a loop would reproduce itself perfectly while
// testing something that is not the game; see the observatory plan, section 7.
//
// The command file is deliberately plain text rather than the JSONL event
// stream, because parsing JSON in the engine to drive the engine is a cost with
// no matching benefit. `observatory.py replay-script` derives it from a run.
//
//     # turn  kind      target
//     0       key       combat-mode
//     0       object    G_FA62E3B5_9BA4_49A2_89C4_228E9F58BE12
//     2       object    G_00000083_0001_4040_8000_000000000001
//     0       location  362014203537285
//
// `key` is one of the two actions that change the interaction mode. It is in
// the file because the mode is the other half of a command: a script of clicks
// alone could never reach the state its clicks were recorded in.
//
// Loads the file. False means nothing will be replayed, and says why on the
// debug log rather than proceeding silently with an empty list.
bool replay_ui_load(const char* path);

// Submit the next command if the actor is idle. Call once per frame; a no-op
// when no file was loaded or the list is exhausted.
void replay_ui_tick(void);

// Whether the game's mode matches the one a command was recorded in. Pure, so
// the judgement that decides between submitting and stalling can be tested
// without a running game. A command recorded at turn 0 was issued before any
// turn started; one at turn 1 or later was issued inside turn-based combat.
bool replay_ui_mode_matches(int recorded_turn, bool combat_active);

// Whether a command file was loaded, and how many commands have not been
// submitted. A replay that ends with commands outstanding did not reproduce
// its recording, and the run record should say so rather than look complete.
bool replay_ui_active(void);
int replay_ui_remaining(void);

#endif /* ARCANUM_UI_REPLAY_UI_H_ */
