// Fatigue-field tests.
//
// F5 from FATIGUE-AND-AI-SPELLS.md: the original `critter_fatigue_adj_set`
// writes the fatigue *points* field, not the adjustment field. CE now
// deliberately writes ADJ. These tests also pin the max-fatigue formula and
// the damage setter's clamp.

#include "game/ai.h"
#include "game/critter.h"
#include "game/stat.h"

#include "obj_harness.h"
#include "test_harness.h"

TEST(fatigue_adj_set_writes_the_adjustment_field)
{
    int64_t obj;

    CHECK(obj_harness_init());

    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);

    critter_fatigue_pts_set(obj, 3);
    CHECK_INT(critter_fatigue_adj_set(obj, 4), 4);

    CHECK_INT(critter_fatigue_pts_get(obj), 3);
    CHECK_INT(critter_fatigue_adj_get(obj), 4);

    obj_harness_destroy(obj);
}

TEST(fatigue_max_uses_the_documented_formula)
{
    int64_t obj;
    int level;
    int constitution = 6;
    int willpower = 7;
    int expected;

    CHECK(obj_harness_init());

    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);

    stat_base_set(obj, STAT_CONSTITUTION, constitution);
    stat_base_set(obj, STAT_WILLPOWER, willpower);
    critter_fatigue_pts_set(obj, 3);
    critter_fatigue_adj_set(obj, 4);

    level = stat_level_get(obj, STAT_LEVEL);
    expected = 3 * 4 + 4 + 2 * (level + constitution) + willpower + 4;

    CHECK_INT(critter_fatigue_max(obj), expected);
    CHECK_INT(critter_fatigue_current(obj), expected);

    obj_harness_destroy(obj);
}

TEST(fatigue_damage_set_clamps_to_zero)
{
    int64_t obj;

    CHECK(obj_harness_init());

    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);

    CHECK_INT(critter_fatigue_damage_set(obj, -5), 0);
    CHECK_INT(critter_fatigue_damage_get(obj), 0);

    obj_harness_destroy(obj);
}

TEST(ai_fatigue_mes_overrides_the_stock_defaults)
{
    static const char* fixture = "rules/ai_fatigue.mes";
    const AiFatigueConfig* cfg;
    FILE* stream;

    CHECK(obj_harness_init());

    stream = fopen(fixture, "wb");
    CHECK(stream != NULL);
    fputs(
        "{1}{MinCastPctLeaderless: 11}\n"
        "{2}{MinCastPctFollower: 12}\n"
        "{3}{ReserveAfterCast: 13}\n"
        "{4}{ItemFirstPct: 14}\n"
        "{5}{RecoverBelowPct: 15}\n"
        "{6}{WhiteNecroItemSkipPct: 16}\n"
        "{7}{UseRealChargeCost: 1}\n",
        stream);
    fclose(stream);

    ai_fatigue_config_load();
    cfg = ai_fatigue_config_get();

    CHECK_INT(cfg->min_cast_pct_leaderless, 11);
    CHECK_INT(cfg->min_cast_pct_follower, 12);
    CHECK_INT(cfg->reserve_after_cast, 13);
    CHECK_INT(cfg->item_first_pct, 14);
    CHECK_INT(cfg->recover_below_pct, 15);
    CHECK_INT(cfg->white_necro_item_skip_pct, 16);
    CHECK(cfg->use_real_charge_cost);

    remove(fixture);
}

void test_fatigue_suite(void)
{
    RUN(fatigue_adj_set_writes_the_adjustment_field);
    RUN(fatigue_max_uses_the_documented_formula);
    RUN(fatigue_damage_set_clamps_to_zero);
    RUN(ai_fatigue_mes_overrides_the_stock_defaults);

    obj_harness_exit();
}
