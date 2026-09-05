// Fatigue-field tests.
//
// F5 from FATIGUE-AND-AI-SPELLS.md: the original `critter_fatigue_adj_set`
// writes the fatigue *points* field, not the adjustment field. CE now
// deliberately writes ADJ. These tests also pin the max-fatigue formula and
// the damage setter's clamp.

#include "game/ai.h"
#include "game/critter.h"
#include "game/stat.h"

#include <stdio.h>
#include <string.h>

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




// --------------------------------------------------------------------------
// `rules/ai_fatigue.mes` parsing - card X7, 2026-09-05
//
// The comment above `ai_fatigue_config_load` promised that "an absent file, an
// absent key, or a malformed value leaves the stock default in place". Two of
// those three were true. `tig_str_parse_named_value` reads the value with
// `atoi`, which cannot fail, so `MinCastPctLeaderless: banana` returned
// *success* carrying 0 - and the AI that casts at 0% is a different AI from
// the one that casts at 20%, with nothing said on any log.
//
// These drive `ai_fatigue_config_apply_entry`, which is the judgement the
// loader applies to each line, so the file layers do not have to be stood up
// to test what the file means.

TEST(fatigue_config_defaults_are_the_documented_ones)
{
    const AiFatigueConfig* cfg;

    ai_fatigue_config_reset();
    cfg = ai_fatigue_config_get();

    CHECK_INT(cfg->min_cast_pct_leaderless, 20);
    CHECK_INT(cfg->min_cast_pct_follower, 0);
    CHECK_INT(cfg->reserve_after_cast, 1);
    CHECK_INT(cfg->item_first_pct, 50);
    CHECK_INT(cfg->recover_below_pct, 20);
    CHECK_INT(cfg->white_necro_item_skip_pct, 80);
    CHECK(!cfg->use_real_charge_cost);
}

TEST(fatigue_config_reads_a_good_value)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("MinCastPctLeaderless: 35"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->min_cast_pct_leaderless, 35);
}

// The review's exact reproduction, and the one the card is named for.
TEST(fatigue_config_keeps_the_default_for_a_malformed_value)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("MinCastPctLeaderless: banana"),
        AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_get()->min_cast_pct_leaderless, 20);

    CHECK_INT(ai_fatigue_config_apply_entry("ReserveAfterCast: nonsense"),
        AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_get()->reserve_after_cast, 1);
}

// Every key, because the defect was in the shared parse and not in one branch.
TEST(fatigue_config_keeps_every_default_for_a_malformed_value)
{
    const AiFatigueConfig* cfg;

    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("MinCastPctLeaderless: x"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("MinCastPctFollower: x"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("ReserveAfterCast: x"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("ItemFirstPct: x"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("RecoverBelowPct: x"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("WhiteNecroItemSkipPct: x"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("UseRealChargeCost: x"), AI_FATIGUE_ENTRY_BAD_VALUE);

    cfg = ai_fatigue_config_get();
    CHECK_INT(cfg->min_cast_pct_leaderless, 20);
    CHECK_INT(cfg->min_cast_pct_follower, 0);
    CHECK_INT(cfg->reserve_after_cast, 1);
    CHECK_INT(cfg->item_first_pct, 50);
    CHECK_INT(cfg->recover_below_pct, 20);
    CHECK_INT(cfg->white_necro_item_skip_pct, 80);
    CHECK(!cfg->use_real_charge_cost);
}

TEST(fatigue_config_refuses_trailing_junk)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("ItemFirstPct: 40 percent"),
        AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_get()->item_first_pct, 50);

    // Trailing *blank* is not junk: a mod author's stray space is not an error.
    CHECK_INT(ai_fatigue_config_apply_entry("ItemFirstPct: 40  "),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->item_first_pct, 40);
}

TEST(fatigue_config_refuses_an_empty_value)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("RecoverBelowPct:"),
        AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_apply_entry("RecoverBelowPct:   "),
        AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_fatigue_config_get()->recover_below_pct, 20);
}

TEST(fatigue_config_still_clamps_a_readable_value)
{
    ai_fatigue_config_reset();

    CHECK_INT(ai_fatigue_config_apply_entry("MinCastPctFollower: 500"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->min_cast_pct_follower, 100);

    CHECK_INT(ai_fatigue_config_apply_entry("MinCastPctFollower: -7"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->min_cast_pct_follower, 0);

    // The reserve clamps to 1000, not 100. A shared clamp would lose that.
    CHECK_INT(ai_fatigue_config_apply_entry("ReserveAfterCast: 5000"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->reserve_after_cast, 1000);
}

TEST(fatigue_config_reads_a_negative_value)
{
    // `-7` is readable and then clamped; it must not be mistaken for junk.
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("ReserveAfterCast: -7"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->reserve_after_cast, 0);
}

TEST(fatigue_config_ignores_an_entry_about_something_else)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("SomeOtherKey: 3"),
        AI_FATIGUE_ENTRY_UNKNOWN);
    CHECK_INT(ai_fatigue_config_apply_entry(""), AI_FATIGUE_ENTRY_UNKNOWN);
    CHECK_INT(ai_fatigue_config_get()->min_cast_pct_leaderless, 20);
}

TEST(fatigue_config_tolerates_leading_space)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("   ItemFirstPct: 12"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_fatigue_config_get()->item_first_pct, 12);
}

TEST(fatigue_config_bool_key_takes_any_number)
{
    ai_fatigue_config_reset();
    CHECK_INT(ai_fatigue_config_apply_entry("UseRealChargeCost: 1"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK(ai_fatigue_config_get()->use_real_charge_cost);

    CHECK_INT(ai_fatigue_config_apply_entry("UseRealChargeCost: 0"),
        AI_FATIGUE_ENTRY_APPLIED);
    CHECK(!ai_fatigue_config_get()->use_real_charge_cost);
}

// Through the real loader, not just the judgement. The isolated cases above
// were green while `cmd_install` was writing files to the wrong name and
// while a mod's own output was being captured as the player's original -
// twice this week the end-to-end run found what the unit tests could not, so
// one case here reads an actual malformed file off disk.
TEST(ai_fatigue_mes_keeps_defaults_for_malformed_values)
{
    static const char* fixture = "rules/ai_fatigue.mes";
    const AiFatigueConfig* cfg;
    FILE* stream;

    CHECK(obj_harness_init());

    stream = fopen(fixture, "wb");
    CHECK(stream != NULL);
    fputs(
        "{1}{MinCastPctLeaderless: banana}\n"
        "{2}{ReserveAfterCast: nonsense}\n"
        "{3}{ItemFirstPct: 40 percent}\n"
        "{4}{RecoverBelowPct: 15}\n",
        stream);
    fclose(stream);

    ai_fatigue_config_load();
    cfg = ai_fatigue_config_get();

    // The three unreadable ones keep their documented defaults...
    CHECK_INT(cfg->min_cast_pct_leaderless, 20);
    CHECK_INT(cfg->reserve_after_cast, 1);
    CHECK_INT(cfg->item_first_pct, 50);
    // ...and a good line in the same file still takes effect.
    CHECK_INT(cfg->recover_below_pct, 15);

    remove(fixture);
}

void test_fatigue_suite(void)
{
    RUN(fatigue_adj_set_writes_the_adjustment_field);
    RUN(fatigue_max_uses_the_documented_formula);
    RUN(fatigue_damage_set_clamps_to_zero);
    RUN(ai_fatigue_mes_overrides_the_stock_defaults);

    // Card X7, 2026-09-05.
    RUN(fatigue_config_defaults_are_the_documented_ones);
    RUN(fatigue_config_reads_a_good_value);
    RUN(fatigue_config_keeps_the_default_for_a_malformed_value);
    RUN(fatigue_config_keeps_every_default_for_a_malformed_value);
    RUN(fatigue_config_refuses_trailing_junk);
    RUN(fatigue_config_refuses_an_empty_value);
    RUN(fatigue_config_still_clamps_a_readable_value);
    RUN(fatigue_config_reads_a_negative_value);
    RUN(fatigue_config_ignores_an_entry_about_something_else);
    RUN(fatigue_config_tolerates_leading_space);
    RUN(fatigue_config_bool_key_takes_any_number);
    RUN(ai_fatigue_mes_keeps_defaults_for_malformed_values);

    obj_harness_exit();
}
