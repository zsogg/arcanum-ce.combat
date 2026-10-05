// Entry point for the unit tests.
//
// These cover CE's pure-logic code only: no assets, no display, no network, so
// the binary runs anywhere and needs no game install. That is the whole point -
// the existing smoke gate proves the tree builds and reaches `player_create`,
// but it exercises nothing that a rename actually touches.
//
//     cmake --preset linux-x64-debug -DARCANUM_BUILD_TESTS=ON
//     cmake --build --preset linux-x64-debug --target arcanum-tests
//     ./out/build/linux-x64-debug/tests/arcanum-tests
//
// Exit status is 0 when every check passes, 1 otherwise, so it drops straight
// into `verify-build.sh`.

#include "test_harness.h"

void test_str_parse_suite(void);
void test_rect_suite(void);
void test_idxtable_suite(void);
void test_art_id_suite(void);
void test_color_suite(void);
void test_called_shot_suite(void);
void test_combat_observer_suite(void);
void test_armor_rules_suite(void);
void test_penetration_suite(void);
void test_ai_targeting_suite(void);
void test_reload_suite(void);
void test_rt_budget_suite(void);
void test_weapon_rules_suite(void);
void test_bsearch_suite(void);
void test_palette_suite(void);
void test_mes_suite(void);
void test_database_suite(void);
void test_file_suite(void);
void test_sound_suite(void);
void test_find_file_suite(void);
void test_memory_suite(void);
void test_file_cache_suite(void);
void test_message_suite(void);
void test_timer_suite(void);
void test_bmp_suite(void);
void test_core_suite(void);
void test_debug_suite(void);
void test_draw_suite(void);
void test_kb_suite(void);
void test_guid_suite(void);
void test_dialog_shutdown_suite(void);
void test_dialog_options_suite(void);
void test_random_suite(void);
void test_stat_suite(void);
void test_skill_suite(void);
void test_location_suite(void);
void test_obj_harness_suite(void);
void test_obj_file_suite(void);
void test_highres_config_suite(void);
void test_settings_suite(void);
void test_feature_suite(void);
void test_obj_id_suite(void);
void test_reaction_suite(void);
void test_compat_suite(void);
void test_magitech_suite(void);
void test_determinism_suite(void);
void test_script_suite(void);
void test_save_slot_suite(void);
void test_proto_suite(void);
void test_fatigue_suite(void);
void test_area_suite(void);
void test_provenance_suite(void);
void test_eye_candy_sound_suite(void);

int main(void)
{
    test_str_parse_suite();
    test_rect_suite();
    test_idxtable_suite();
    test_art_id_suite();
    test_color_suite();
    test_called_shot_suite();
    test_combat_observer_suite();
    test_armor_rules_suite();
    test_penetration_suite();
    test_ai_targeting_suite();
    test_reload_suite();
    test_rt_budget_suite();
    test_weapon_rules_suite();
    test_bsearch_suite();
    test_palette_suite();
    test_mes_suite();
    test_database_suite();
    // `find_file` before `file`, deliberately. `tig_file_list_create_native`
    // walks the iterator with an unbounded `do/while` (file.c:1013), so an
    // iterator whose cursor stops advancing hangs that loop instead of
    // failing a check - measured 2026-08-25 while mutation-testing this
    // module. Running the direct, bounded checks first turns that hang into
    // a named failure. The engine would hang the same way; that is a
    // property of the caller, not something these tests can fix.
    test_find_file_suite();
    test_file_suite();
    test_sound_suite();
    test_memory_suite();
    test_file_cache_suite();
    test_message_suite();
    test_timer_suite();
    test_bmp_suite();
    test_core_suite();
    test_debug_suite();
    test_draw_suite();
    test_kb_suite();
    test_guid_suite();
    test_dialog_shutdown_suite();
    test_dialog_options_suite();
    test_random_suite();
    test_stat_suite();
    test_skill_suite();
    test_location_suite();
    test_obj_harness_suite();
    test_obj_file_suite();
    test_highres_config_suite();
    test_settings_suite();
    test_feature_suite();
    test_obj_id_suite();
    test_reaction_suite();
    test_compat_suite();
    test_magitech_suite();
    test_determinism_suite();
    test_script_suite();
    test_save_slot_suite();
    test_proto_suite();
    test_fatigue_suite();
    test_eye_candy_sound_suite();
    test_area_suite();
    test_provenance_suite();

    return test_report();
}
