#ifndef ARCANUM_GAME_COMBAT_FEEDBACK_H_
#define ARCANUM_GAME_COMBAT_FEEDBACK_H_

#include <stddef.h>

// CE (turn-based rebalance, card FB1; zsogg, 2026-11-02): the floating text a
// weapon hit or miss shows under `combat feedback`, pure. arcanum-work
// tasks/FB1-combat-feedback.md owns the rules; `combat.c` calls these with
// what the rules decided, so the text says only that.
//
//   a plain miss           "Miss"                    Minimal and Verbose
//   armor took all of a    "Stopped", ", 2 FT"       Minimal and Verbose; at
//   weapon hit's normal    when fatigue got through  Verbose, the damage line
//   damage                                           after it
//   armor took more than   "Glancing"                as "Stopped"
//   half of it
//   any damage             "4 HT, 7 FT", a part      Verbose, in place of the
//                          that is 0 left out        stock damage float

typedef enum CombatFeedbackArmor {
    COMBAT_FEEDBACK_ARMOR_NONE,
    COMBAT_FEEDBACK_ARMOR_STOPPED,
    COMBAT_FEEDBACK_ARMOR_GLANCING,
} CombatFeedbackArmor;

#define COMBAT_FEEDBACK_MISS "Miss"

// What armor did to a weapon hit's normal damage: `before` it met the armor
// rules and `after` (negative when the rules did not apply).
CombatFeedbackArmor combat_feedback_armor(int before, int after);

// The float for a hit that got `hp` damage and `fatigue` through, at the
// Floats `level` (`TF_LEVEL_*`), with the game's own words for HP and fatigue.
// Writes "" when nothing is shown.
void combat_feedback_hit_text(CombatFeedbackArmor armor, int hp, int fatigue, int level,
    const char* hp_word, const char* fatigue_word, char* buffer, size_t size);

#endif /* ARCANUM_GAME_COMBAT_FEEDBACK_H_ */
