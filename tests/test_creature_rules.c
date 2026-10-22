// Creature profiles (zsogg, 2026-10-21): the creature sheet's entries and the
// profiles' rules, pure. The engine side - which creature a critical meets,
// the phantom's ward and the switch - is exercised in scripted runs.

#include "game/creature_rules.h"

#include "game/combat.h"
#include "game/crit_effects.h"
#include "game/criticals.h"
#include "game/obj_flags.h"

#include "test_harness.h"

TEST(an_entry_takes_any_of_its_keys)
{
    CreatureRule rule;

    CHECK(creature_rule_parse(28341, "Profile: skeletal, FirearmScale: 50", &rule));
    CHECK_INT(rule.id, 28341);
    CHECK_INT(rule.profile, CREATURE_PROFILE_SKELETAL);
    CHECK_INT(rule.firearm_scale, 50);
    CHECK_INT(rule.chart, CREATURE_RULE_UNSET);
    CHECK_INT(rule.natural, CREATURE_RULE_UNSET);

    // Spaces around keys and values, as the generator writes them.
    CHECK(creature_rule_parse(28330, " Profile : spirit , Natural : 55 ", &rule));
    CHECK_INT(rule.profile, CREATURE_PROFILE_SPIRIT);
    CHECK_INT(rule.natural, 55);

    CHECK(creature_rule_parse(27384, "Chart: snake", &rule));
    CHECK_INT(rule.chart, CRIT_BODY_TYPE_SNAKE);
    CHECK_INT(rule.profile, CREATURE_RULE_UNSET);
}

TEST(every_profile_and_chart_has_its_name)
{
    static const char* profiles[] = { "living", "skeletal", "fleshy", "mechanical", "spirit", "phantom" };
    static const char* charts[] = { "biped", "quadruped", "amorphous", "insect", "snake", "avian" };
    CreatureRule rule;
    char entry[64];
    int index;

    for (index = 0; index < CREATURE_PROFILE_COUNT; index++) {
        snprintf(entry, sizeof(entry), "Profile: %s", profiles[index]);
        CHECK(creature_rule_parse(30000, entry, &rule));
        CHECK_INT(rule.profile, index);
        CHECK_STR(creature_profile_name((CreatureProfile)index), profiles[index]);
    }
    CHECK(creature_profile_name(CREATURE_PROFILE_NONE) == NULL);

    for (index = 0; index < CRIT_BODY_TYPE_COUNT; index++) {
        snprintf(entry, sizeof(entry), "Chart: %s", charts[index]);
        CHECK(creature_rule_parse(30000, entry, &rule));
        CHECK_INT(rule.chart, index);
    }
}

TEST(only_a_phantom_says_what_hurts_it_when_examined)
{
    int index;

    // 2026-10-22: the examine panel's line in place of the reaction.
    CHECK_STR(creature_profile_examine_hint(CREATURE_PROFILE_PHANTOM),
        "Only criticals, magic or explosives hurt it");
    for (index = CREATURE_PROFILE_NONE; index < CREATURE_PROFILE_COUNT; index++) {
        if (index != CREATURE_PROFILE_PHANTOM) {
            CHECK(creature_profile_examine_hint((CreatureProfile)index) == NULL);
        }
    }
}

TEST(malformed_entries_are_refused_and_change_nothing)
{
    CreatureRule rule = { 7, 1, 2, 3, 4 };

    CHECK(!creature_rule_parse(28341, "", &rule));
    CHECK(!creature_rule_parse(28341, "Profile: ghost", &rule));
    CHECK(!creature_rule_parse(28341, "Profile: Skeletal", &rule));
    CHECK(!creature_rule_parse(28341, "Chart: centaur", &rule));
    CHECK(!creature_rule_parse(28341, "Natural: lots", &rule));
    CHECK(!creature_rule_parse(28341, "Protection: 20", &rule));
    CHECK(!creature_rule_parse(28341, "Profile skeletal", &rule));
    CHECK(!creature_rule_parse(0, "Profile: living", &rule));
    CHECK(!creature_rule_parse(28341, NULL, &rule));
    CHECK_INT(rule.id, 7);
    CHECK_INT(rule.profile, 1);
    CHECK_INT(rule.chart, 2);
    CHECK_INT(rule.natural, 3);
    CHECK_INT(rule.firearm_scale, 4);
}

TEST(values_are_clamped)
{
    CreatureRule rule;

    CHECK(creature_rule_parse(28310, "Natural: 5000, FirearmScale: 250", &rule));
    CHECK_INT(rule.natural, CREATURE_RULE_VALUE_MAX);
    CHECK_INT(rule.firearm_scale, CREATURE_RULE_SCALE_MAX);
    CHECK(creature_rule_parse(28310, "Natural: -3, FirearmScale: -1", &rule));
    CHECK_INT(rule.natural, 0);
    CHECK_INT(rule.firearm_scale, 0);
}

TEST(an_unlisted_creature_takes_the_profile_its_flags_give)
{
    CHECK_INT(creature_profile_default(0, 0, CRIT_BODY_TYPE_BIPED), CREATURE_PROFILE_LIVING);
    CHECK_INT(creature_profile_default(0, 0, CRIT_BODY_TYPE_QUADRUPED), CREATURE_PROFILE_LIVING);
    CHECK_INT(creature_profile_default(OCF_UNDEAD, 0, CRIT_BODY_TYPE_BIPED), CREATURE_PROFILE_FLESHY);
    CHECK_INT(creature_profile_default(0, 0, CRIT_BODY_TYPE_AMORPHOUS), CREATURE_PROFILE_SPIRIT);
    CHECK_INT(creature_profile_default(0, OCF2_ELEMENTAL, CRIT_BODY_TYPE_BIPED), CREATURE_PROFILE_SPIRIT);
    // An undead spirit is a spirit; a machine is a machine whatever its chart.
    CHECK_INT(creature_profile_default(OCF_UNDEAD, 0, CRIT_BODY_TYPE_AMORPHOUS), CREATURE_PROFILE_SPIRIT);
    CHECK_INT(creature_profile_default(OCF_MECHANICAL, OCF2_ELEMENTAL, CRIT_BODY_TYPE_AMORPHOUS),
        CREATURE_PROFILE_MECHANICAL);
}

TEST(only_the_living_suffer_bodily_effects_and_spirits_are_never_knocked_down)
{
    CHECK(creature_profile_bodily(CREATURE_PROFILE_LIVING));
    CHECK(!creature_profile_bodily(CREATURE_PROFILE_SKELETAL));
    CHECK(!creature_profile_bodily(CREATURE_PROFILE_FLESHY));
    CHECK(!creature_profile_bodily(CREATURE_PROFILE_MECHANICAL));
    CHECK(!creature_profile_bodily(CREATURE_PROFILE_SPIRIT));
    CHECK(!creature_profile_bodily(CREATURE_PROFILE_PHANTOM));

    CHECK(creature_profile_knockdown(CREATURE_PROFILE_LIVING));
    CHECK(creature_profile_knockdown(CREATURE_PROFILE_SKELETAL));
    CHECK(creature_profile_knockdown(CREATURE_PROFILE_FLESHY));
    CHECK(creature_profile_knockdown(CREATURE_PROFILE_MECHANICAL));
    CHECK(!creature_profile_knockdown(CREATURE_PROFILE_SPIRIT));
    CHECK(!creature_profile_knockdown(CREATURE_PROFILE_PHANTOM));

    CHECK_INT(creature_profile_knockdown_bonus(CREATURE_PROFILE_SKELETAL, CRIT_HIT_TYPE_CRUSHING), 10);
    CHECK_INT(creature_profile_knockdown_bonus(CREATURE_PROFILE_SKELETAL, CRIT_HIT_TYPE_CUTTING), 0);
    CHECK_INT(creature_profile_knockdown_bonus(CREATURE_PROFILE_FLESHY, CRIT_HIT_TYPE_CRUSHING), 0);
}

TEST(each_profile_takes_the_temporary_effects_that_suit_it)
{
    int loc;

    // The living, by location, as before.
    for (loc = HIT_LOC_TORSO; loc < HIT_LOC_COUNT; loc++) {
        CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_LIVING, loc, CRIT_HIT_TYPE_CUTTING),
            crit_effects_for_location(loc));
    }

    // Flesh: a limb loosens, the head staggers, the torso nothing.
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_FLESHY, HIT_LOC_ARM, CRIT_HIT_TYPE_CUTTING), CRIT_EFFECT_NUMBED_ARM);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_FLESHY, HIT_LOC_LEG, CRIT_HIT_TYPE_IMPALING), CRIT_EFFECT_HOBBLED);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_FLESHY, HIT_LOC_HEAD, CRIT_HIT_TYPE_CRUSHING), CRIT_EFFECT_STAGGERED);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_FLESHY, HIT_LOC_TORSO, CRIT_HIT_TYPE_CRUSHING), 0);

    // Bone: only a crushing critical, then as flesh.
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_SKELETAL, HIT_LOC_ARM, CRIT_HIT_TYPE_CRUSHING), CRIT_EFFECT_NUMBED_ARM);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_SKELETAL, HIT_LOC_LEG, CRIT_HIT_TYPE_CRUSHING), CRIT_EFFECT_HOBBLED);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_SKELETAL, HIT_LOC_HEAD, CRIT_HIT_TYPE_CRUSHING), CRIT_EFFECT_STAGGERED);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_SKELETAL, HIT_LOC_ARM, CRIT_HIT_TYPE_CUTTING), 0);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_SKELETAL, HIT_LOC_HEAD, CRIT_HIT_TYPE_IMPALING), 0);

    // A machine: malfunction, a jammed arm, a hobbled leg; electricity
    // malfunctions it anywhere.
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_MECHANICAL, HIT_LOC_HEAD, CRIT_HIT_TYPE_CUTTING), CRIT_EFFECT_MALFUNCTION);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_MECHANICAL, HIT_LOC_TORSO, CRIT_HIT_TYPE_IMPALING), CRIT_EFFECT_MALFUNCTION);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_MECHANICAL, HIT_LOC_ARM, CRIT_HIT_TYPE_CRUSHING), CRIT_EFFECT_JAMMED_ARM);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_MECHANICAL, HIT_LOC_LEG, CRIT_HIT_TYPE_CUTTING), CRIT_EFFECT_HOBBLED);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_MECHANICAL, HIT_LOC_ARM, CRIT_HIT_TYPE_ELECTRICAL), CRIT_EFFECT_MALFUNCTION);
    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_MECHANICAL, HIT_LOC_LEG, CRIT_HIT_TYPE_ELECTRICAL), CRIT_EFFECT_MALFUNCTION);

    // Spirits and phantoms: disrupted, wherever it lands.
    for (loc = HIT_LOC_TORSO; loc < HIT_LOC_COUNT; loc++) {
        CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_SPIRIT, loc, CRIT_HIT_TYPE_FIRE), CRIT_EFFECT_DISRUPTED);
        CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_PHANTOM, loc, CRIT_HIT_TYPE_CUTTING), CRIT_EFFECT_DISRUPTED);
    }

    CHECK_INT(creature_profile_temporary(CREATURE_PROFILE_NONE, HIT_LOC_ARM, CRIT_HIT_TYPE_CUTTING), 0);
}

TEST(the_new_effects_work_as_the_ones_they_copy)
{
    CritEffectEntry entry = { 0 };

    // Malfunction and Disrupted halve the next turn's AP, as Staggered.
    crit_effects_entry_apply(&entry, CRIT_EFFECT_MALFUNCTION, 1000);
    CHECK_INT(crit_effects_entry_turn_action_points(&entry, 11), 5);
    CHECK_INT(crit_effects_entry_to_hit_penalty(&entry), 0);

    entry = (CritEffectEntry) { 0 };
    crit_effects_entry_apply(&entry, CRIT_EFFECT_DISRUPTED, 1000);
    CHECK_INT(crit_effects_entry_turn_action_points(&entry, 11), 5);
    crit_effects_entry_turn_started(&entry);
    CHECK_INT(crit_effects_entry_turn_action_points(&entry, 11), 11);

    // A jammed arm is -20 to hit, as Numbed arm, and leaves AP alone.
    entry = (CritEffectEntry) { 0 };
    crit_effects_entry_apply(&entry, CRIT_EFFECT_JAMMED_ARM, 1000);
    CHECK_INT(crit_effects_entry_to_hit_penalty(&entry), CRIT_EFFECT_TO_HIT_PENALTY);
    CHECK_INT(crit_effects_entry_turn_action_points(&entry, 11), 11);
}

TEST(the_new_effects_have_their_names)
{
    char names[128];

    crit_effects_names(CRIT_EFFECT_MALFUNCTION | CRIT_EFFECT_DISRUPTED | CRIT_EFFECT_JAMMED_ARM,
        names, sizeof(names));
    CHECK_STR(names, "\"malfunction\",\"disrupted\",\"jammed_arm\"");
}

void test_creature_rules_suite(void)
{
    RUN(an_entry_takes_any_of_its_keys);
    RUN(every_profile_and_chart_has_its_name);
    RUN(only_a_phantom_says_what_hurts_it_when_examined);
    RUN(malformed_entries_are_refused_and_change_nothing);
    RUN(values_are_clamped);
    RUN(an_unlisted_creature_takes_the_profile_its_flags_give);
    RUN(only_the_living_suffer_bodily_effects_and_spirits_are_never_knocked_down);
    RUN(each_profile_takes_the_temporary_effects_that_suit_it);
    RUN(the_new_effects_work_as_the_ones_they_copy);
    RUN(the_new_effects_have_their_names);
}
