#ifndef ARCANUM_UI_CALLED_SHOT_H_
#define ARCANUM_UI_CALLED_SHOT_H_

#include <stdbool.h>

#include <tig/tig.h>

#include "game/combat.h"

/**
 * The local player's called-shot preference (card S1).
 *
 * Arcanum has always had called shots: hold comma, period or slash while
 * attacking and the blow is aimed at head, arms or legs. What it has never had
 * is a way to *choose* one and keep it, so this module owns exactly one thing —
 * which limb the next attack should ask for, and nothing about how an attack is
 * resolved.
 *
 * **Local input preference only.** It is never consulted for an NPC: an AI's
 * aim is its own, and a UI preference that reached one would change the fight
 * rather than the player's view of it.
 *
 * Off unless `called shot ui` is switched on, and off is byte-for-byte the
 * behaviour that shipped: the resolver answers `HIT_LOC_TORSO` for the
 * persistent half, leaving the held keys as the only source of aim.
 */

/** Whether the feature is switched on. */
bool called_shot_enabled(void);

/**
 * Whether aimed throws are switched on (card S2).
 *
 * A separate row from `called shot ui`, and deliberately so. The selector
 * gives a player a way to choose an aim the original already accepted from the
 * keyboard; an aimed *throw* is an aim the original never accepted at all, so
 * it is a new mechanic and carries its own flag and its own acceptance.
 */
bool called_shot_throws_enabled(void);

/**
 * The aim a throw should carry, given the aim its command committed. **Pure.**
 *
 * `enabled` is passed rather than read so the whole decision can be tested
 * without a settings file. Off answers `HIT_LOC_TORSO` — which is not "aim at
 * the torso" but *unaimed*: `combat_attack` rolls the location at 70/15/10/5,
 * exactly as a thrown weapon has always behaved.
 */
int called_shot_throw_aim_for(bool enabled, int committed_aim);

/** `called_shot_throw_aim_for` over the live flag. */
int called_shot_throw_aim(int committed_aim);

/** The kept selection; `HIT_LOC_TORSO` means an ordinary attack. */
int called_shot_selection(void);

/**
 * Keeps `hit_loc` as the selection for subsequent attacks.
 *
 * Out-of-range values are refused rather than stored, because this value
 * indexes `hit_loc_penalties` further down and a bad index there reads off the
 * end of a four-element table.
 */
void called_shot_select(int hit_loc);

/** Back to an ordinary attack. Combat exit, new game, load, PC replacement. */
void called_shot_reset(void);

/**
 * The aim an attack committed right now would carry. **Pure.**
 *
 * `selection` is the kept choice and the three flags are the keys held at this
 * instant. Separated from the keyboard so the whole decision can be tested
 * without one, the way the command-acceptance judgement is.
 *
 * A held key is a **temporary override**: it wins while down and the selection
 * returns when it is released. The original's precedence is preserved exactly —
 * comma, then period, then slash — because two keys down has always meant the
 * first of those and a selector must not quietly reorder it.
 */
int called_shot_effective_aim(int selection, bool comma, bool period, bool slash);

/**
 * The aim a command committed **now** would carry: the replayed aim when one
 * is in force, otherwise `called_shot_effective_aim` over the live selection
 * and keyboard.
 */
int called_shot_aim_now(void);

/**
 * Forces the next committed aim, for replaying a recorded command.
 *
 * A replayed attack goes through the same dispatch a click does, and that
 * dispatch *pulls* the aim from the panel and keyboard. So without this a
 * replay would aim wherever the panel happens to sit at replay time rather
 * than where the recording aimed — the card's "never infer command aim later
 * from the current widget or keyboard state", and a divergence that would read
 * as a gameplay difference rather than a lost input.
 *
 * Pushed around the dispatch rather than written into the goal directly, so
 * the aim still reaches execution through the one write site session 6
 * established. Two write sites is how a selector and a key stop being the same
 * thing.
 */
void called_shot_replay_aim_set(int hit_loc);

/** Releases the forced aim. Always paired with the call above. */
void called_shot_replay_aim_clear(void);

/* -- the panel ---------------------------------------------------------- */

/**
 * Creates the four-control panel, or does nothing when the feature is off.
 *
 * The controls are the shipped called-shot cursors, which are already limb
 * imagery: `battlecur` for Body because that is the ordinary combat cursor,
 * then head, arm and leg. **The arm and leg art are mapped by what they
 * depict, not by which key raises them** — the key dispatch raises the leg art
 * for an arm shot and the arm art for a leg shot
 * ([quirk 48](../../MODDING-NOTES.md)), and a panel that inherited that
 * crossing would be wrong in the one place a player checks.
 */
bool called_shot_ui_create(void);

/** Destroys the panel and forgets its handles. */
void called_shot_ui_destroy(void);

/**
 * Shows the panel while the local player is in combat mode, turn-based or real
 * time, and hides it everywhere else.
 *
 * Session 1's decision: hidden entirely rather than shown disabled, so no inert
 * control sits on screen. Real time was added on 2026-09-25 (zsogg): its attack
 * already commits the aim by the same route, so only the panel was missing.
 */
void called_shot_ui_refresh(void);

/**
 * The interface art a limb's control shows; -1 outside the table.
 *
 * Exposed so the mapping can be asserted without a display, because this is
 * the one place quirk 48 sets a trap: the key dispatch raises the **leg** art
 * for an arm shot and the **arm** art for a leg shot, and a panel that copied
 * that would mislabel two of its four controls.
 */
int called_shot_art_for(int hit_loc);

/**
 * Which control a screen position falls in, or -1. **Pure.**
 *
 * Rectangles, not art pixels. `tig_button` hit-tests by sampling the button's
 * art and rejecting a colour-key pixel, which is right for a drawn button and
 * wrong for these: the limb pictures are cursors and mostly transparent, so
 * most of each control was a hole that swallowed the click. The first panel
 * drew perfectly and accepted nothing.
 *
 * Exposed so the geometry can be asserted without a screen, which is the only
 * way this particular mistake was ever going to be caught early.
 */
int called_shot_control_index_at(const TigRect* panel, int x, int y);

/**
 * The screen point at the centre of `hit_loc`'s control, for a scripted click
 * that has to travel the same route as a real one. False when the panel is
 * not shown, which is itself the answer a test is often after.
 */
bool called_shot_control_point(int hit_loc, int* x, int* y);

/** Handles a click on one of the four controls. True when it was ours. */
bool called_shot_ui_handle_button(tig_button_handle_t button_handle);

#endif /* ARCANUM_UI_CALLED_SHOT_H_ */
