// Card AP1, `aptitude balance` (zsogg, 2026-11-12), decisions 4 and 5, against
// the real `item_effective_power` and `item_aptitude_crit_failure_chance` on
// harness objects: a critter whose aptitude comes from its magick and tech
// points, and an item whose complexity is set. Decisions 1, 3 and 6 and tech
// melee are measured in the game (audits/ap1-rules-20261114).
//
// - decision 5: a technologist keeps the neutral half of a magick item's
//   power at aptitude 0, falling evenly to none at -100; stock takes it away
//   as fast as the aptitude grows past the item's complexity.
// - decision 4: a magician's critical-failure chance with a tech item is
//   complexity x aptitude over 250, not 100.
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/obj.h"
#include "game/settings.h"
#include "game/stat.h"
#include "obj_harness.h"
#include "test_harness.h"
#include "tig/file.h"

static int64_t apt_owner;
static int64_t apt_item;

static bool apt_begin(bool on)
{
    settings_init(&settings, "unused");
    feature_init();
    settings_set_value(&settings, feature_key(FEATURE_APTITUDE_BALANCE), on ? 1 : 0);
    if (!obj_harness_init()) {
        return false;
    }
    apt_owner = obj_harness_create_proto(OBJ_TYPE_PC);
    apt_item = obj_harness_create_proto(OBJ_TYPE_WEAPON);
    return apt_owner != OBJ_HANDLE_NULL && apt_item != OBJ_HANDLE_NULL;
}

static void apt_end(void)
{
    obj_harness_destroy(apt_item);
    obj_harness_destroy(apt_owner);
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

// Aptitude is (50 x magick - 55 x tech points) / 10.
static void apt_set(int magick_points, int tech_points, int complexity)
{
    stat_base_set(apt_owner, STAT_MAGICK_POINTS, magick_points);
    stat_base_set(apt_owner, STAT_TECH_POINTS, tech_points);
    obj_field_int32_set(apt_item, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY, complexity);
}

static int apt_power(bool on, int tech_points, int complexity)
{
    int power = -1;

    if (apt_begin(on)) {
        apt_set(0, tech_points, complexity);
        power = item_effective_power(apt_item, apt_owner);
    }
    apt_end();
    return power;
}

static int apt_crit(bool on, int magick_points, int complexity)
{
    int chance = -1;

    if (apt_begin(on)) {
        apt_set(magick_points, 0, complexity);
        chance = item_aptitude_crit_failure_chance(apt_item, apt_owner);
    }
    apt_end();
    return chance;
}

TEST(a_technologist_keeps_a_magick_items_power_on_an_even_fall)
{
    // Complexity 50. Tech 6 is aptitude -33, tech 9 -49, tech 19 below -100.
    CHECK_INT(apt_power(false, 0, 50), 25);
    CHECK_INT(apt_power(true, 0, 50), 25);
    CHECK_INT(apt_power(false, 6, 50), 8);
    CHECK_INT(apt_power(true, 6, 50), 50 * 67 / 200);
    CHECK_INT(apt_power(false, 9, 50), 0);
    CHECK_INT(apt_power(true, 9, 50), 50 * 51 / 200);
    CHECK_INT(apt_power(true, 19, 50), 0);
}

TEST(a_tech_item_is_untouched_by_the_magick_item_rule)
{
    // Complexity -40 with tech 6 (aptitude -33): stock either way.
    CHECK_INT(apt_power(true, 6, -40), apt_power(false, 6, -40));
}

TEST(a_magicians_critical_failure_chance_divides_by_250)
{
    // Complexity -80 with magick 16, aptitude 80: 64 stock, 25 on.
    CHECK_INT(apt_crit(false, 16, -80), 80 * 80 / 100);
    CHECK_INT(apt_crit(true, 16, -80), 80 * 80 / 250);
    // A technologist has no such chance either way.
    CHECK_INT(apt_crit(true, 0, -80), 0);
}

void test_aptitude_balance_suite(void)
{
    RUN(a_technologist_keeps_a_magick_items_power_on_an_even_fall);
    RUN(a_tech_item_is_untouched_by_the_magick_item_rule);
    RUN(a_magicians_critical_failure_chance_divides_by_250);
}
