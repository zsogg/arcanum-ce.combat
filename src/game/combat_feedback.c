#include "game/combat_feedback.h"

#include <stdio.h>

#include "game/tf.h"

CombatFeedbackArmor combat_feedback_armor(int before, int after)
{
    if (before <= 0 || after < 0) {
        return COMBAT_FEEDBACK_ARMOR_NONE;
    }
    if (after == 0) {
        return COMBAT_FEEDBACK_ARMOR_STOPPED;
    }
    if (after * 2 < before) {
        return COMBAT_FEEDBACK_ARMOR_GLANCING;
    }
    return COMBAT_FEEDBACK_ARMOR_NONE;
}

void combat_feedback_hit_text(CombatFeedbackArmor armor, int hp, int fatigue, int level,
    const char* hp_word, const char* fatigue_word, char* buffer, size_t size)
{
    size_t pos = 0;
    int written;

    buffer[0] = '\0';

    if (armor == COMBAT_FEEDBACK_ARMOR_STOPPED || armor == COMBAT_FEEDBACK_ARMOR_GLANCING) {
        if (level == TF_LEVEL_NEVER) {
            return;
        }
        written = snprintf(buffer, size, "%s",
            armor == COMBAT_FEEDBACK_ARMOR_STOPPED ? "Stopped" : "Glancing");
        pos = written > 0 ? (size_t)written : 0;
        if (level != TF_LEVEL_VERBOSE) {
            // Minimal: only the fatigue that got through.
            if (fatigue > 0 && pos < size) {
                snprintf(buffer + pos, size - pos, ", %d %s", fatigue, fatigue_word);
            }
            return;
        }
    } else if (level != TF_LEVEL_VERBOSE) {
        return;
    }

    if (hp > 0 && pos < size) {
        written = snprintf(buffer + pos, size - pos, "%s%d %s", pos != 0 ? ", " : "", hp, hp_word);
        pos += written > 0 ? (size_t)written : 0;
    }
    if (fatigue > 0 && pos < size) {
        snprintf(buffer + pos, size - pos, "%s%d %s", pos != 0 ? ", " : "", fatigue, fatigue_word);
    }
}
