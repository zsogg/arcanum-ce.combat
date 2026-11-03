// Card FB1: the combat feedback floats, pure. One case per row of the card's
// table (arcanum-work tasks/FB1-combat-feedback.md) and per Floats level.

#include "game/combat_feedback.h"
#include "game/tf.h"

#include "test_harness.h"

static const char* text(CombatFeedbackArmor armor, int hp, int fatigue, int level)
{
    static char buffer[80];

    combat_feedback_hit_text(armor, hp, fatigue, level, "HT", "FT", buffer, sizeof(buffer));
    return buffer;
}

TEST(armor_stopped_glancing_or_nothing)
{
    CHECK_INT(combat_feedback_armor(12, 0), COMBAT_FEEDBACK_ARMOR_STOPPED);
    CHECK_INT(combat_feedback_armor(12, 5), COMBAT_FEEDBACK_ARMOR_GLANCING); // 7 of 12 stopped
    CHECK_INT(combat_feedback_armor(12, 6), COMBAT_FEEDBACK_ARMOR_NONE); // half, not more
    CHECK_INT(combat_feedback_armor(12, 12), COMBAT_FEEDBACK_ARMOR_NONE);
    CHECK_INT(combat_feedback_armor(1, 0), COMBAT_FEEDBACK_ARMOR_STOPPED);
    CHECK_INT(combat_feedback_armor(12, -1), COMBAT_FEEDBACK_ARMOR_NONE); // the rules did not apply
    CHECK_INT(combat_feedback_armor(0, 0), COMBAT_FEEDBACK_ARMOR_NONE); // no normal damage to stop
}

TEST(the_damage_line_is_verbose_only)
{
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_NONE, 4, 7, TF_LEVEL_VERBOSE), "4 HT, 7 FT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_NONE, 4, 0, TF_LEVEL_VERBOSE), "4 HT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_NONE, 0, 7, TF_LEVEL_VERBOSE), "7 FT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_NONE, 0, 0, TF_LEVEL_VERBOSE), "");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_NONE, 4, 7, TF_LEVEL_MINIMAL), "");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_NONE, 4, 7, TF_LEVEL_NEVER), "");
}

TEST(stopped_and_glancing_show_at_minimal)
{
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_STOPPED, 0, 0, TF_LEVEL_MINIMAL), "Stopped");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_STOPPED, 0, 2, TF_LEVEL_MINIMAL), "Stopped, 2 FT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_GLANCING, 3, 0, TF_LEVEL_MINIMAL), "Glancing");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_GLANCING, 3, 5, TF_LEVEL_MINIMAL), "Glancing, 5 FT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_STOPPED, 0, 2, TF_LEVEL_NEVER), "");
}

TEST(stopped_and_glancing_lead_the_damage_line_at_verbose)
{
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_STOPPED, 0, 2, TF_LEVEL_VERBOSE), "Stopped, 2 FT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_STOPPED, 0, 0, TF_LEVEL_VERBOSE), "Stopped");
    // Fire on a stopped blow still burns: the HP is the hit's whole damage.
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_STOPPED, 3, 2, TF_LEVEL_VERBOSE), "Stopped, 3 HT, 2 FT");
    CHECK_STR(text(COMBAT_FEEDBACK_ARMOR_GLANCING, 3, 5, TF_LEVEL_VERBOSE), "Glancing, 3 HT, 5 FT");
}

TEST(a_short_buffer_is_cut_not_overrun)
{
    char buffer[6];

    combat_feedback_hit_text(COMBAT_FEEDBACK_ARMOR_GLANCING, 30, 50, TF_LEVEL_VERBOSE, "HT", "FT",
        buffer, sizeof(buffer));
    CHECK_STR(buffer, "Glanc");
}

void test_combat_feedback_suite(void)
{
    RUN(armor_stopped_glancing_or_nothing);
    RUN(the_damage_line_is_verbose_only);
    RUN(stopped_and_glancing_show_at_minimal);
    RUN(stopped_and_glancing_lead_the_damage_line_at_verbose);
    RUN(a_short_buffer_is_cut_not_overrun);
}
