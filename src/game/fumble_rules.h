#ifndef ARCANUM_GAME_FUMBLE_RULES_H_
#define ARCANUM_GAME_FUMBLE_RULES_H_

#include <stdbool.h>

// CE (`weapon fumbles`, card FU1; zsogg, 2026-10-27): what a fumble with a
// melee weapon, a bow, a thrown weapon or bare hands does, revisiting CR-4's
// decision 6. Firearms keep `firearm fumbles` (`reload_fumble_outcome`).
// arcanum-work `tasks/FU1-weapon-fumbles.md` has the card.

typedef enum FumbleOutcome {
    // A Master's: no fumble at all.
    FUMBLE_NONE,
    // The wielder stumbles and falls; getting up costs
    // `FUMBLE_GET_UP_COST` in turn-based.
    FUMBLE_STUMBLE,
    // The wielder's own weapon strikes them for its ordinary damage.
    FUMBLE_SELF_HIT,
    // The weapon is dropped (a thrown one lands at the wielder's feet).
    FUMBLE_DROP,
    // The self-hit, critical.
    FUMBLE_CRITICAL_SELF_HIT,
} FumbleOutcome;

// Getting up from a fumble's fall, in AP; any other knockdown costs stock's 5.
#define FUMBLE_GET_UP_COST 6

// Pure: the outcome of a fumble rolled `roll` (1-100) by a wielder of
// `training` in the weapon's skill: 50 stumble, 30 self-hit, 15 drop, 5
// critical self-hit. An Expert never hits themselves - a self-hit becomes a
// stumble - and a Master never fumbles. With nothing to drop (`armed`
// false: bare hands), the drop is a stumble too.
FumbleOutcome fumble_rules_outcome(int roll, int training, bool armed);

// A fumble outcome's name for `critical.rolled`.
const char* fumble_rules_outcome_name(FumbleOutcome outcome);

#endif /* ARCANUM_GAME_FUMBLE_RULES_H_ */
