#ifndef ARCANUM_UI_CALLED_SHOT_H_
#define ARCANUM_UI_CALLED_SHOT_H_

#include <stdbool.h>

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

/** `called_shot_effective_aim` against the live selection and keyboard. */
int called_shot_aim_now(void);

#endif /* ARCANUM_UI_CALLED_SHOT_H_ */
