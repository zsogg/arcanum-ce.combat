#ifndef ARCANUM_GAME_PENETRATION_H_
#define ARCANUM_GAME_PENETRATION_H_

#include <stdbool.h>

// CE (turn-based rebalance, armor milestone 1): armor penetration, pure.
//
// The rules zsogg agreed on 2026-09-20 and 2026-09-24 (arcanum-work
// COMBAT-REBALANCE-DESIGN-DISCUSSION.md, "Armor penetration - current design
// direction"), as arithmetic with no game objects. For a connected physical
// hit at a location, in this order:
//
//   1. location protection: head the helmet, torso the body armor, arms
//      0.4 body + 0.6 gloves, legs 0.4 body + 0.6 boots, plus the creature's
//      natural protection. An empty slot gives 0 and its weight is not moved.
//   2. a shield block makes protection the higher of the shield's rating and
//      step 1, and no gap is possible;
//   3. a gap (an unblocked critical that found one) multiplies step 1's
//      protection by 0.35;
//   4. magical protection is added last, never cut;
//   5. penetration, varied by a per-hit percentage (80-120), against that
//      protection gives the share of normal damage passed: none at a ratio of
//      0.5 or less, one third at 1, all at 1.5 or more, joined by straight
//      lines, and all when there is no protection.
//
// Ratings are whole numbers on one common scale. The arithmetic is done in
// hundredths so the weights, the gap and the variation never round; the only
// rounding is the damage passed, to the nearest point. Rounding and the roll's
// distribution were left open by the design and are decided outside this file.

// Locations, as `HIT_LOC_*` (combat.h): torso 0, head 1, arm 2, leg 3.
typedef struct PenetrationWorn {
    int helmet;
    int body;
    int gloves;
    int boots;
} PenetrationWorn;

typedef struct PenetrationHit {
    int hit_loc;
    PenetrationWorn worn;
    // The creature's own protection, cut by a gap like worn armor.
    int natural;
    // Added last; no gap or block changes it.
    int magic;
    // A shield blocked the hit, and its rating.
    bool blocked;
    int shield;
    // An unblocked critical found a gap. Ignored when `blocked`.
    bool gap;
    int penetration;
    // The per-hit variation, 80 to 120: penetration x this / 100.
    int variation_percent;
    // The hit's normal damage, any critical bonus included.
    int damage;
} PenetrationHit;

typedef struct PenetrationResult {
    // Effective protection and penetration, in hundredths.
    int protection_centi;
    int penetration_centi;
    // The share of damage passed, as a fraction.
    int passed_num;
    int passed_den;
    // The normal damage that gets through.
    int damage;
} PenetrationResult;

#define PENETRATION_VARIATION_MIN 80
#define PENETRATION_VARIATION_MAX 120

// Step 1, in hundredths: the worn protection at `hit_loc` plus `natural`.
int penetration_location_protection_centi(int hit_loc, const PenetrationWorn* worn, int natural);

// The chance, in percent, that a critical finds a gap: 50, plus the weapon's
// critical bonus, plus 10 for an aimed limb or 20 for an aimed head.
int penetration_gap_chance(int crit_bonus, int hit_loc, bool aimed);

// Steps 1 to 5 for one hit.
PenetrationResult penetration_resolve(const PenetrationHit* hit);

// CE (combat AI, milestone AI-1): the share of normal damage expected to pass,
// in thousandths, for `hit` without its location, block, gap or variation:
// averaged over the unaimed hit locations (torso 70, leg 15, arm 10, head 5,
// as `combat_random_hit_loc` rolls them), every variation 80-120% equally,
// and a block on `block_chance` percent. No gap: only a critical finds one.
int penetration_expected_permille(const PenetrationHit* hit, int block_chance);

#endif /* ARCANUM_GAME_PENETRATION_H_ */
