#include "game/dodge_rules.h"

#include "game/critter.h"
#include "game/skill.h"

// A first setting; zsogg expects to balance it (2026-11-05).
static const int dodge_rules_fumble_tbl[TRAINING_COUNT] = {
    /*        TRAINING_NONE */ 0,
    /*  TRAINING_APPRENTICE */ 5,
    /*      TRAINING_EXPERT */ 10,
    /*      TRAINING_MASTER */ 20,
};

int dodge_rules_encumbrance_penalty(int encumbrance_level)
{
    if (encumbrance_level <= ENCUMBRANCE_LEVEL_NONE) {
        return 0;
    }
    // Severe adds two encumbrance effects beyond its level, as for speed.
    if (encumbrance_level >= ENCUMBRANCE_LEVEL_SEVERE) {
        return 2 * (ENCUMBRANCE_LEVEL_SEVERE + 2);
    }
    return 2 * encumbrance_level;
}

int dodge_rules_chance(int effectiveness, int armor_penalty, int encumbrance_level, int share)
{
    int chance;

    chance = effectiveness - armor_penalty - dodge_rules_encumbrance_penalty(encumbrance_level);
    if (chance < 0) {
        chance = 0;
    }
    if (share < 0) {
        share = 0;
    }
    return chance * share / 100;
}

int dodge_rules_fumble_chance(int training)
{
    if (training < TRAINING_NONE || training >= TRAINING_COUNT) {
        return 0;
    }
    return dodge_rules_fumble_tbl[training];
}
