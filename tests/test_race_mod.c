// UAP's optional Race Mod (card RM1): the pure parts of its engine changes.
//
// The expected values come from the Race Mod exe the UAP installer ships
// (`arcanum-work/uap/Arcanum-UAP-race.exe`) against UAP base: the reaction
// cave at 0x4B9E02, the portrait table at 0x5B7C0C, and the two art setters
// at 0x503ED0 and 0x503FE0. The race arrows, the pregenerated characters and
// the starting clothes take objects and the main menu; RM1's scripted runs
// cover them.

#include "game/feature.h"
#include "game/gamelib.h"
#include "game/portrait.h"
#include "game/proto.h"
#include "game/reaction.h"
#include "game/settings.h"
#include "game/stat.h"
#include "game/uap.h"
#include "test_harness.h"
#include "tig/art.h"

TEST(race_mod_maps_a_new_race_pc_onto_its_parent_column)
{
    int npc_race;

    for (npc_race = 0; npc_race < RACE_COUNT; npc_race++) {
        CHECK_INT(reaction_race_modifier(npc_race, RACE_DARK_ELF, true),
            reaction_race_modifier(npc_race, RACE_ELF, true));
        CHECK_INT(reaction_race_modifier(npc_race, RACE_OGRE, true),
            reaction_race_modifier(npc_race, RACE_HALF_OGRE, true));
        CHECK_INT(reaction_race_modifier(npc_race, RACE_ORC, true),
            reaction_race_modifier(npc_race, RACE_HALF_ORC, true));
    }

    // Cells of the table at 0x5B684C, so a mapping onto the wrong column
    // cannot pass by also being wrong in the comparison: a Human NPC likes
    // an Elf (5) and dislikes a Half-Orc (-5); a Dwarf NPC likes a Dwarf (10)
    // and dislikes an Elf (-10).
    CHECK_INT(reaction_race_modifier(RACE_HUMAN, RACE_DARK_ELF, true), 5);
    CHECK_INT(reaction_race_modifier(RACE_HUMAN, RACE_ORC, true), -5);
    CHECK_INT(reaction_race_modifier(RACE_DWARF, RACE_DARK_ELF, true), -10);
}

TEST(race_mod_leaves_the_stock_races_and_npc_races_alone)
{
    int npc_race;
    int pc_race;

    for (npc_race = 0; npc_race < RACE_COUNT; npc_race++) {
        for (pc_race = 0; pc_race < 8; pc_race++) {
            CHECK_INT(reaction_race_modifier(npc_race, pc_race, true),
                reaction_race_modifier(npc_race, pc_race, false));
        }
    }

    CHECK_INT(reaction_race_modifier(RACE_HUMAN, RACE_HUMAN, false), 0);
    CHECK_INT(reaction_race_modifier(RACE_HUMAN, RACE_ELF, false), 5);
    CHECK_INT(reaction_race_modifier(RACE_DWARF, RACE_DWARF, false), 10);
}

TEST(without_the_race_mod_a_new_race_pc_reads_the_table_flat)
{
    int npc_race;

    // As the original and CE before it: column 8 is the next row's first.
    for (npc_race = 0; npc_race < RACE_COUNT - 1; npc_race++) {
        CHECK_INT(reaction_race_modifier(npc_race, RACE_DARK_ELF, false),
            reaction_race_modifier(npc_race + 1, RACE_HUMAN, false));
        CHECK_INT(reaction_race_modifier(npc_race, RACE_ORC, false),
            reaction_race_modifier(npc_race + 1, RACE_ELF, false));
    }

    // The last row is real, and a comparison of the function with itself
    // cannot show it: Orc NPC cells from the table, and an Ogre NPC's
    // Dark Elf column landing on the first of them.
    CHECK_INT(reaction_race_modifier(RACE_ORC, RACE_HUMAN, false), -20);
    CHECK_INT(reaction_race_modifier(RACE_ORC, RACE_ELF, false), -40);
    CHECK_INT(reaction_race_modifier(RACE_ORC, RACE_HALF_ORC, false), 10);
    CHECK_INT(reaction_race_modifier(RACE_OGRE, RACE_DARK_ELF, false), -20);
    CHECK_INT(reaction_race_modifier(RACE_ORC, RACE_ORC, true), 10);

    // Past the whole table: no modifier.
    CHECK_INT(reaction_race_modifier(RACE_ORC, RACE_DARK_ELF, false), 0);
    CHECK_INT(reaction_race_modifier(RACE_ORC, RACE_ORC, false), 0);
}

TEST(race_mod_portrait_prefixes)
{
    static const char* stock[RACE_COUNT] = {
        "HU", "DW", "EL", "HE", "GN", "HA", "HO", "HG", "EL", "HG", "HO"
    };
    static const char* race_mod[RACE_COUNT] = {
        "HU", "DW", "EL", "HE", "GN", "HA", "HO", "HG", "DE", "OG", "OC"
    };
    int race;

    for (race = 0; race < RACE_COUNT; race++) {
        CHECK_STR(portrait_race_specifier(race, false), stock[race]);
        CHECK_STR(portrait_race_specifier(race, true), race_mod[race]);
    }
}

TEST(race_mod_body_types)
{
    CHECK_INT(proto_race_mod_body_type(RACE_HUMAN), TIG_ART_CRITTER_BODY_TYPE_HUMAN);
    CHECK_INT(proto_race_mod_body_type(RACE_HALF_ELF), TIG_ART_CRITTER_BODY_TYPE_ELF);
    CHECK_INT(proto_race_mod_body_type(RACE_HALF_ORC), TIG_ART_CRITTER_BODY_TYPE_HUMAN);
    CHECK_INT(proto_race_mod_body_type(RACE_DARK_ELF), TIG_ART_CRITTER_BODY_TYPE_ELF);
    CHECK_INT(proto_race_mod_body_type(RACE_OGRE), TIG_ART_CRITTER_BODY_TYPE_HALF_OGRE);
    CHECK_INT(proto_race_mod_body_type(RACE_ORC), TIG_ART_CRITTER_BODY_TYPE_UAP_ORC);
    CHECK_INT(TIG_ART_CRITTER_BODY_TYPE_UAP_ORC, 5);
}

// A critter art id with every field set, so a setter that keeps or drops a
// field shows it.
static tig_art_id_t dressed_critter(int gender, int body_type)
{
    tig_art_id_t art_id = 0;

    CHECK(tig_art_critter_id_create(gender, body_type, 3, 1, 2, 6, 5, 4, 2, &art_id) == TIG_OK);
    return art_id;
}

TEST(stock_body_type_set_keeps_the_rest_of_the_art)
{
    tig_art_id_t art_id = dressed_critter(TIG_ART_CRITTER_GENDER_FEMALE, TIG_ART_CRITTER_BODY_TYPE_HUMAN);
    tig_art_id_t monster = 0;

    tig_art_uap_race_mod_set(false);
    CHECK_INT(tig_art_critter_id_body_type_set(art_id, TIG_ART_CRITTER_BODY_TYPE_ELF),
        dressed_critter(TIG_ART_CRITTER_GENDER_FEMALE, TIG_ART_CRITTER_BODY_TYPE_ELF));
    // Out of range: body type 0.
    CHECK_INT(tig_art_critter_id_body_type_set(art_id, TIG_ART_CRITTER_BODY_TYPE_UAP_ORC), art_id);

    // Stock sets the gender bit on monster art too, which is the specie's top
    // bit: a male orc becomes specie 18.
    CHECK(tig_art_monster_id_create(TIG_ART_MONSTER_SPECIE_ORC, 0, 0, 0, 4, 0, 0, 0, &monster) == TIG_OK);
    CHECK_INT(tig_art_monster_id_specie_get(tig_art_critter_id_gender_set(monster, TIG_ART_CRITTER_GENDER_MALE)),
        TIG_ART_MONSTER_SPECIE_ORC + 16);
}

TEST(race_mod_body_type_set_gives_the_new_critter_art)
{
    tig_art_id_t art_id = dressed_critter(TIG_ART_CRITTER_GENDER_FEMALE, TIG_ART_CRITTER_BODY_TYPE_HUMAN);
    tig_art_id_t expected = 0;
    tig_art_id_t monster = 0;
    tig_art_id_t portal = 0;
    int body_type;

    tig_art_uap_race_mod_set(true);

    // A male critter facing 4, the rest 0, as `proto_load_critter_stats`
    // creates them; the gender setter runs after it in the game.
    for (body_type = 0; body_type < TIG_ART_CRITTER_BODY_TYPE_COUNT; body_type++) {
        CHECK(tig_art_critter_id_create(TIG_ART_CRITTER_GENDER_MALE, body_type, 0, 0, 0, 4, 0, 0, 0, &expected) == TIG_OK);
        CHECK_INT(tig_art_critter_id_body_type_set(art_id, body_type), expected);
    }
    CHECK_INT(tig_art_critter_id_body_type_set(art_id, TIG_ART_CRITTER_BODY_TYPE_HUMAN), 0x28002000);

    // Body type 5: the orc monster, as stock orc NPCs are made.
    CHECK(tig_art_monster_id_create(TIG_ART_MONSTER_SPECIE_ORC, 0, 0, 0, 4, 0, 0, 0, &monster) == TIG_OK);
    CHECK_INT(tig_art_critter_id_body_type_set(art_id, TIG_ART_CRITTER_BODY_TYPE_UAP_ORC), monster);
    CHECK_INT(monster, 0xC1002000);

    // Other art types are untouched, as in stock.
    CHECK(tig_art_portal_id_create(5, 0, 0, 0, 0, 0, &portal) == TIG_OK);
    CHECK_INT(tig_art_critter_id_body_type_set(portal, TIG_ART_CRITTER_BODY_TYPE_ELF), portal);

    tig_art_uap_race_mod_set(false);
}

TEST(race_mod_gender_set_leaves_monster_art_alone)
{
    tig_art_id_t monster = 0;
    tig_art_id_t critter = dressed_critter(TIG_ART_CRITTER_GENDER_MALE, TIG_ART_CRITTER_BODY_TYPE_ELF);

    tig_art_uap_race_mod_set(true);

    CHECK(tig_art_monster_id_create(TIG_ART_MONSTER_SPECIE_ORC, 0, 0, 0, 4, 0, 0, 0, &monster) == TIG_OK);
    CHECK_INT(tig_art_critter_id_gender_set(monster, TIG_ART_CRITTER_GENDER_MALE), monster);
    CHECK_INT(tig_art_critter_id_gender_set(monster, TIG_ART_CRITTER_GENDER_FEMALE), monster);

    // Critter art still takes the gender.
    CHECK_INT(tig_art_critter_id_gender_set(critter, TIG_ART_CRITTER_GENDER_FEMALE),
        dressed_critter(TIG_ART_CRITTER_GENDER_FEMALE, TIG_ART_CRITTER_BODY_TYPE_ELF));

    tig_art_uap_race_mod_set(false);
}

TEST(race_mod_needs_its_switch_uap_fixes_and_the_archive)
{
    int race_mod;
    int fixes;
    int archive;

    settings_init(&settings, "unused");
    feature_init();

    for (race_mod = 0; race_mod < 2; race_mod++) {
        for (fixes = 0; fixes < 2; fixes++) {
            for (archive = 0; archive < 2; archive++) {
                settings_set_value(&settings, feature_key(FEATURE_UAP_RACE_MOD), race_mod);
                settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), fixes);
                uap_race_mod_archive_set(archive != 0);
                CHECK(uap_race_mod_enabled() == (race_mod && fixes && archive));
            }
        }
    }

    // `uap_race_mod_archive_set` pushes the result to TIG: with all three
    // on, the body-type setter gives the new critter art.
    CHECK_INT(tig_art_critter_id_body_type_set(dressed_critter(TIG_ART_CRITTER_GENDER_FEMALE, 0), 0), 0x28002000);

    settings_set_value(&settings, feature_key(FEATURE_UAP_RACE_MOD), 0);
    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 0);
    uap_race_mod_archive_set(false);
    CHECK(!uap_race_mod_enabled());
    settings_exit(&settings);
}

void test_race_mod_suite(void)
{
    RUN(race_mod_maps_a_new_race_pc_onto_its_parent_column);
    RUN(race_mod_leaves_the_stock_races_and_npc_races_alone);
    RUN(without_the_race_mod_a_new_race_pc_reads_the_table_flat);
    RUN(race_mod_portrait_prefixes);
    RUN(race_mod_body_types);
    RUN(stock_body_type_set_keeps_the_rest_of_the_art);
    RUN(race_mod_body_type_set_gives_the_new_critter_art);
    RUN(race_mod_gender_set_leaves_monster_art_alone);
    RUN(race_mod_needs_its_switch_uap_fixes_and_the_archive);
}
