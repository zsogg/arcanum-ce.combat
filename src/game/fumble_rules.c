#include "game/fumble_rules.h"

#include "game/skill.h"

FumbleOutcome fumble_rules_outcome(int roll, int training, bool armed)
{
    FumbleOutcome outcome;

    if (training >= TRAINING_MASTER) {
        return FUMBLE_NONE;
    }

    if (roll <= 50) {
        outcome = FUMBLE_STUMBLE;
    } else if (roll <= 80) {
        outcome = FUMBLE_SELF_HIT;
    } else if (roll <= 95) {
        outcome = FUMBLE_DROP;
    } else {
        outcome = FUMBLE_CRITICAL_SELF_HIT;
    }

    if (outcome == FUMBLE_DROP && !armed) {
        return FUMBLE_STUMBLE;
    }
    if ((outcome == FUMBLE_SELF_HIT || outcome == FUMBLE_CRITICAL_SELF_HIT)
        && training >= TRAINING_EXPERT) {
        return FUMBLE_STUMBLE;
    }
    return outcome;
}

const char* fumble_rules_outcome_name(FumbleOutcome outcome)
{
    switch (outcome) {
    case FUMBLE_STUMBLE:
        return "stumble";
    case FUMBLE_SELF_HIT:
        return "self_hit";
    case FUMBLE_DROP:
        return "drop";
    case FUMBLE_CRITICAL_SELF_HIT:
        return "critical_self_hit";
    default:
        return "none";
    }
}
