// Tech specialists (zsogg, 2026-10-19): `schematic second discipline cap` and
// `schematic second discipline level`, against the real
// `tech_component_level_required`, which the schematic screen asks for each
// component's requirement. Its one caller, `schematic_ui_draw_component`, is
// UI code outside this binary; the screen is checked in a game.
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/settings.h"
#include "game/tech.h"
#include "test_harness.h"

static void cap_begin(void)
{
    settings_init(&settings, "unused");
    feature_init();
    // `tech_init` registers it; this suite does not run that init.
    settings_register(&settings, TECH_SECOND_DISCIPLINE_LEVEL_KEY, "35", NULL);
}

static void cap_end(void)
{
    settings_set_value(&settings, feature_key(FEATURE_SCHEMATIC_SECOND_DISCIPLINE_CAP), 0);
    settings_exit(&settings);
}

static void cap_switch(bool on)
{
    settings_set_value(&settings, feature_key(FEATURE_SCHEMATIC_SECOND_DISCIPLINE_CAP), on ? 1 : 0);
}

TEST(stock_every_component_needs_its_full_complexity)
{
    cap_begin();
    // Strong Poison (Chemistry 75) in an Explosives schematic.
    CHECK_INT(tech_component_level_required(TECH_CHEMISTRY, TECH_EXPLOSIVES, 75), 75);
    CHECK_INT(tech_component_level_required(TECH_EXPLOSIVES, TECH_EXPLOSIVES, 60), 60);
    cap_end();
}

TEST(the_cap_applies_outside_the_own_discipline_only)
{
    cap_begin();
    cap_switch(true);
    CHECK_INT(tech_component_level_required(TECH_CHEMISTRY, TECH_EXPLOSIVES, 75), 35);
    CHECK_INT(tech_component_level_required(TECH_HERBOLOGY, TECH_MECHANICAL, 85), 35);
    // Below the cap, the component's own complexity.
    CHECK_INT(tech_component_level_required(TECH_CHEMISTRY, TECH_EXPLOSIVES, 20), 20);
    // The own discipline keeps its full requirement.
    CHECK_INT(tech_component_level_required(TECH_GUN, TECH_GUN, 85), 85);
    cap_end();
}

TEST(the_cap_is_a_setting)
{
    cap_begin();
    cap_switch(true);
    settings_set_value(&settings, TECH_SECOND_DISCIPLINE_LEVEL_KEY, 50);
    CHECK_INT(tech_component_level_required(TECH_CHEMISTRY, TECH_EXPLOSIVES, 75), 50);
    // 100 or more is no cap.
    settings_set_value(&settings, TECH_SECOND_DISCIPLINE_LEVEL_KEY, 100);
    CHECK_INT(tech_component_level_required(TECH_CHEMISTRY, TECH_EXPLOSIVES, 75), 75);
    cap_end();
}

void test_schematic_cap_suite(void)
{
    RUN(stock_every_component_needs_its_full_complexity);
    RUN(the_cap_applies_outside_the_own_discipline_only);
    RUN(the_cap_is_a_setting);
}
