#ifndef ARCANUM_GAME_DODGE_RULES_H_
#define ARCANUM_GAME_DODGE_RULES_H_

// CE (turn-based rebalance, card DG1; zsogg, 2026-11-05): the Dodge rules
// under `dodge rules`, pure. arcanum-work tasks/DG1-dodge-rules.md owns them.
//
//   the chance    stock effectiveness, less the body armor's DodgePenalty
//                 (`rules/armor.mes`) and 2 a level of encumbrance, 16 at
//                 severe; never below 0; then against a weapon with a
//                 DodgeShare (`rules/weapons.mes`) that percent of itself,
//                 rounded down
//   training      no critical dodge: a dodge makes the attacker fumble 0, 5,
//                 10 or 20% of the time by the dodger's Dodge training, and
//                 that fumble is always a stumble

// The dodge chance from the stock `effectiveness`, the body armor's
// `armor_penalty`, the dodger's `EncumbranceLevel` and the weapon's `share`
// (100 is stock).
int dodge_rules_chance(int effectiveness, int armor_penalty, int encumbrance_level, int share);

// What `EncumbranceLevel` takes from the chance: the speed penalty's shape.
int dodge_rules_encumbrance_penalty(int encumbrance_level);

// The percent chance a dodge makes the attacker fumble, by the dodger's
// `Training`.
int dodge_rules_fumble_chance(int training);

#endif /* ARCANUM_GAME_DODGE_RULES_H_ */
