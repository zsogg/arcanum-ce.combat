// Tests for the object harness, and the first tests of code that needs a
// critter to exist.
//
// Two jobs. The first half checks the harness itself does what it claims -
// objects come out distinct, fields round-trip, defaults are applied. The
// second half is the point of building it: `stat.c` and `skill.c` were covered
// only as far as their tables and arithmetic, because everything else takes an
// `int64_t obj`. These are the parts that were out of reach.
//
// The most valuable of them is the **racial stat cap**. `stat_level_max` reads
// the critter's race and returns a per-race maximum, so the human path was all
// the earlier suite could reach with `OBJ_HANDLE_NULL`; an ogre's strength cap
// of 26 and a gnome's willpower cap of 22 were simply untested. They are the
// numbers that make the races feel different, and nothing else would notice if
// one were wrong.
//
// Also worth knowing: a critter built here has **no effects on it**. The
// harness installs a fixture `effect.mes` with nothing in the parsed range, so
// every effect entry is zeroed and contributes no modifier. That is deliberate
// - it means these tests see base values, not values filtered through Troika's
// racial and equipment modifiers, which is what makes them readable.

#include "game/item.h"
#include "game/obj.h"
#include "game/skill.h"
#include "game/stat.h"

#include "obj_harness.h"
#include "test_harness.h"

// A critter with a chosen race, at the harness's defaults otherwise.
static int64_t make_critter(int race)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);

    if (obj != OBJ_HANDLE_NULL) {
        stat_base_set(obj, STAT_RACE, race);
    }

    return obj;
}

TEST(the_harness_creates_a_usable_object)
{
    int64_t obj;

    CHECK(obj_harness_init());

    obj = obj_harness_create_proto(OBJ_TYPE_NPC);
    CHECK(obj != OBJ_HANDLE_NULL);
    CHECK_INT(obj_field_int32_get(obj, OBJ_F_TYPE), OBJ_TYPE_NPC);

    obj_harness_destroy(obj);
}

TEST(objects_are_distinct_and_independent)
{
    int64_t a = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t b = obj_harness_create_proto(OBJ_TYPE_NPC);

    CHECK(a != OBJ_HANDLE_NULL);
    CHECK(b != OBJ_HANDLE_NULL);
    CHECK(a != b);

    obj_field_int32_set(a, OBJ_F_HP_DAMAGE, 11);
    obj_field_int32_set(b, OBJ_F_HP_DAMAGE, 22);

    CHECK_INT(obj_field_int32_get(a, OBJ_F_HP_DAMAGE), 11);
    CHECK_INT(obj_field_int32_get(b, OBJ_F_HP_DAMAGE), 22);

    obj_harness_destroy(b);
    obj_harness_destroy(a);
}

TEST(fields_round_trip_through_the_object)
{
    int64_t obj = obj_harness_create_proto(OBJ_TYPE_NPC);

    obj_field_int32_set(obj, OBJ_F_HP_DAMAGE, 42);
    CHECK_INT(obj_field_int32_get(obj, OBJ_F_HP_DAMAGE), 42);

    obj_field_int32_set(obj, OBJ_F_CRITTER_FATIGUE_DAMAGE, 3);
    CHECK_INT(obj_field_int32_get(obj, OBJ_F_CRITTER_FATIGUE_DAMAGE), 3);

    // Array fields work too - this is how skills, inventories and overlays are
    // stored, so it is the shape most game code actually reaches for.
    obj_arrayfield_int32_set(obj, OBJ_F_CRITTER_BASIC_SKILL_IDX, BASIC_SKILL_HEAL, 9);
    CHECK_INT(obj_arrayfield_int32_get(obj, OBJ_F_CRITTER_BASIC_SKILL_IDX, BASIC_SKILL_HEAL), 9);

    // NOTE: `OBJ_F_LOCATION` is deliberately *not* exercised. Writing it calls
    // `obj_find_move` to reindex the object spatially, which needs the sector
    // system - the boundary this harness does not cross. Found by writing that
    // test first and segfaulting in `obj_find_move`, which is a tidy
    // demonstration of where the line is.

    obj_harness_destroy(obj);
}

TEST(objects_of_different_types_can_be_made)
{
    // Each `OBJ_TYPE_*` has its own field count, so this exercises the
    // per-type field layout rather than just one shape.
    static const int types[] = {
        OBJ_TYPE_NPC, OBJ_TYPE_PC, OBJ_TYPE_WEAPON, OBJ_TYPE_ARMOR,
        OBJ_TYPE_KEY, OBJ_TYPE_KEY_RING, OBJ_TYPE_CONTAINER, OBJ_TYPE_SCENERY,
    };
    int64_t obj;
    int i;

    for (i = 0; i < (int)(sizeof(types) / sizeof(types[0])); i++) {
        obj = obj_harness_create_proto(types[i]);
        CHECK(obj != OBJ_HANDLE_NULL);
        CHECK_INT(obj_field_int32_get(obj, OBJ_F_TYPE), types[i]);
        obj_harness_destroy(obj);
    }
}

TEST(a_new_object_carries_the_engines_field_defaults)
{
    // `obj_create_proto` runs `obj_set_defaults`, so a fresh object is not
    // simply zeroed. If defaults ever stopped being applied, code reading them
    // would see 0 and quietly misbehave.
    int64_t npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t weapon = obj_harness_create_proto(OBJ_TYPE_WEAPON);

    CHECK_INT(obj_field_int32_get(npc, OBJ_F_BLIT_ALPHA), 255);
    CHECK_INT(obj_field_int32_get(npc, OBJ_F_BLIT_SCALE), 100);

    // The art id is *not* left invalid. `obj_set_defaults` writes
    // TIG_ART_ID_INVALID early and then overwrites it in the per-type switch -
    // a critter gets a default critter appearance, an item a default item one.
    // So a valid id here is what proves the type-specific branch ran, and
    // expecting the invalid one (as this test did at first) reads the function
    // only halfway.
    CHECK(obj_field_int32_get(npc, OBJ_F_AID) != (int)TIG_ART_ID_INVALID);
    CHECK(obj_field_int32_get(weapon, OBJ_F_AID) != (int)TIG_ART_ID_INVALID);
    CHECK(obj_field_int32_get(npc, OBJ_F_AID) != obj_field_int32_get(weapon, OBJ_F_AID));

    // And the current art id starts equal to it - the last two lines of
    // `obj_set_defaults` set both from the same value.
    CHECK_INT(obj_field_int32_get(npc, OBJ_F_CURRENT_AID),
        obj_field_int32_get(npc, OBJ_F_AID));

    obj_harness_destroy(weapon);
    obj_harness_destroy(npc);
}

TEST(stat_base_round_trips_on_a_critter)
{
    // `stat_base_get`/`stat_base_set` were untestable before this harness.
    int64_t obj = make_critter(RACE_HUMAN);
    int stat;
    int wrong = 0;

    for (stat = STAT_STRENGTH; stat <= STAT_CHARISMA; stat++) {
        stat_base_set(obj, stat, 12);
        if (stat_base_get(obj, stat) != 12) {
            wrong++;
        }
    }

    CHECK_INT(wrong, 0);

    obj_harness_destroy(obj);
}

TEST(stat_level_equals_base_when_nothing_modifies_it)
{
    // With no effects installed, effective level is the base. That is what
    // makes every other reading in this file interpretable.
    int64_t obj = make_critter(RACE_HUMAN);

    stat_base_set(obj, STAT_STRENGTH, 14);
    CHECK_INT(stat_level_get(obj, STAT_STRENGTH), 14);

    stat_base_set(obj, STAT_DEXTERITY, 9);
    CHECK_INT(stat_level_get(obj, STAT_DEXTERITY), 9);

    obj_harness_destroy(obj);
}

TEST(each_race_gets_its_own_stat_caps)
{
    // The table the earlier stat.c suite could not reach: `stat_level_max`
    // needs a critter to read a race from, so `OBJ_HANDLE_NULL` only ever gave
    // the human column. These are the numbers that differentiate the races.
    struct {
        int race;
        int stat;
        int expected;
    } static const cases[] = {
        // Humans take the base table.
        { RACE_HUMAN, STAT_STRENGTH, 20 },
        { RACE_HUMAN, STAT_DEXTERITY, 20 },
        { RACE_HUMAN, STAT_WILLPOWER, 20 },

        { RACE_DWARF, STAT_STRENGTH, 21 },
        { RACE_DWARF, STAT_CONSTITUTION, 21 },
        { RACE_DWARF, STAT_DEXTERITY, 20 },

        { RACE_ELF, STAT_DEXTERITY, 21 },
        { RACE_ELF, STAT_BEAUTY, 21 },
        { RACE_ELF, STAT_WILLPOWER, 21 },
        { RACE_ELF, STAT_STRENGTH, 20 },

        { RACE_DARK_ELF, STAT_DEXTERITY, 21 },
        { RACE_DARK_ELF, STAT_BEAUTY, 21 },

        { RACE_GNOME, STAT_WILLPOWER, 22 },
        { RACE_HALFLING, STAT_DEXTERITY, 22 },
        { RACE_HALF_OGRE, STAT_STRENGTH, 24 },
        { RACE_OGRE, STAT_STRENGTH, 26 },
        { RACE_ORC, STAT_STRENGTH, 22 },
        { RACE_ORC, STAT_CONSTITUTION, 22 },
        { RACE_HALF_ORC, STAT_STRENGTH, 21 },
        { RACE_HALF_ORC, STAT_CONSTITUTION, 21 },
    };
    int64_t obj;
    int i;
    int wrong = 0;

    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        obj = make_critter(cases[i].race);
        if (stat_level_max(obj, cases[i].stat) != cases[i].expected) {
            wrong++;
            CHECK_INT(stat_level_max(obj, cases[i].stat), cases[i].expected);
        }
        obj_harness_destroy(obj);
    }

    CHECK_INT(wrong, 0);
}

TEST(a_half_elf_inherits_the_elven_dexterity_cap_but_not_the_beauty_one)
{
    // The faithful behaviour, and precisely what UAP 0x4B107C changes: with the
    // switch off a half-elf caps dexterity at 21 like an elf, but beauty stays
    // at the base 20. Reachable only with a real critter, so this is the first
    // time a UAP-gated site has been checked on its original path with the
    // condition it actually depends on.
    int64_t obj = make_critter(RACE_HALF_ELF);

    CHECK_INT(stat_level_max(obj, STAT_DEXTERITY), 21);
    CHECK_INT(stat_level_max(obj, STAT_BEAUTY), 20);

    obj_harness_destroy(obj);
}

TEST(a_skill_cannot_be_raised_past_what_its_governing_stat_allows)
{
    // This is the interaction the two earlier suites could each only see half
    // of: `basic_skill_points_set` refuses a value whose resulting skill level
    // would exceed `max_skill_for_stat_level` of the *governing* stat. Melee is
    // governed by dexterity, and one point of skill is four levels.
    int64_t obj = make_critter(RACE_HUMAN);

    // Dexterity 1 permits skill level 3, so one point (4 levels) is too much.
    stat_base_set(obj, STAT_DEXTERITY, 1);
    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_MELEE, 1), 0);
    CHECK_INT(basic_skill_points_get(obj, BASIC_SKILL_MELEE), 0);

    // Dexterity 6 permits skill level 7, so one point fits and two do not.
    stat_base_set(obj, STAT_DEXTERITY, 6);
    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_MELEE, 1), 1);
    CHECK_INT(basic_skill_points_get(obj, BASIC_SKILL_MELEE), 1);
    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_MELEE, 2), 1);

    // Dexterity 20 permits skill level 20, so five points fit exactly.
    stat_base_set(obj, STAT_DEXTERITY, 20);
    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_MELEE, 5), 5);
    CHECK_INT(basic_skill_points_get(obj, BASIC_SKILL_MELEE), 5);
    // And six do not - 24 levels is past the cap.
    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_MELEE, 6), 5);

    obj_harness_destroy(obj);
}

TEST(raising_a_skill_is_governed_by_its_own_stat_not_a_shared_one)
{
    // Prowling is governed by perception, melee by dexterity. A high dexterity
    // must not license a prowling skill.
    int64_t obj = make_critter(RACE_HUMAN);

    stat_base_set(obj, STAT_DEXTERITY, 20);
    stat_base_set(obj, STAT_PERCEPTION, 1);

    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_MELEE, 5), 5);
    CHECK_INT(basic_skill_points_set(obj, BASIC_SKILL_PROWLING, 5), 0);

    obj_harness_destroy(obj);
}

TEST(item_is_item_distinguishes_items_from_critters)
{
    int64_t npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    int64_t weapon = obj_harness_create_proto(OBJ_TYPE_WEAPON);
    int64_t armor = obj_harness_create_proto(OBJ_TYPE_ARMOR);
    int64_t scenery = obj_harness_create_proto(OBJ_TYPE_SCENERY);

    CHECK(!item_is_item(npc));
    CHECK(item_is_item(weapon));
    CHECK(item_is_item(armor));
    CHECK(!item_is_item(scenery));

    obj_harness_destroy(scenery);
    obj_harness_destroy(armor);
    obj_harness_destroy(weapon);
    obj_harness_destroy(npc);
}

void test_obj_harness_suite(void)
{
    RUN(the_harness_creates_a_usable_object);
    RUN(objects_are_distinct_and_independent);
    RUN(fields_round_trip_through_the_object);
    RUN(objects_of_different_types_can_be_made);
    RUN(a_new_object_carries_the_engines_field_defaults);

    RUN(stat_base_round_trips_on_a_critter);
    RUN(stat_level_equals_base_when_nothing_modifies_it);
    RUN(each_race_gets_its_own_stat_caps);
    RUN(a_half_elf_inherits_the_elven_dexterity_cap_but_not_the_beauty_one);

    RUN(a_skill_cannot_be_raised_past_what_its_governing_stat_allows);
    RUN(raising_a_skill_is_governed_by_its_own_stat_not_a_shared_one);

    RUN(item_is_item_distinguishes_items_from_critters);

    obj_harness_exit();
}
