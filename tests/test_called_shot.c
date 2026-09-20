#include <stdio.h>
#include <string.h>

#include "game/combat.h"
#include "test_harness.h"
#include "ui/called_shot.h"

// The resolver is pure, so the whole decision can be walked rather than
// sampled: four selections by eight key combinations is 32 cases, and 32 is
// small enough to assert every one of them.
#define KEY_COMMA 0x1
#define KEY_PERIOD 0x2
#define KEY_SLASH 0x4

static int aim_with(int selection, int keys)
{
    return called_shot_effective_aim(selection,
        (keys & KEY_COMMA) != 0,
        (keys & KEY_PERIOD) != 0,
        (keys & KEY_SLASH) != 0);
}

TEST(with_no_key_held_the_kept_selection_is_what_an_attack_asks_for)
{
    CHECK_INT(aim_with(HIT_LOC_TORSO, 0), HIT_LOC_TORSO);
    CHECK_INT(aim_with(HIT_LOC_HEAD, 0), HIT_LOC_HEAD);
    CHECK_INT(aim_with(HIT_LOC_ARM, 0), HIT_LOC_ARM);
    CHECK_INT(aim_with(HIT_LOC_LEG, 0), HIT_LOC_LEG);
}

TEST(a_held_key_overrides_every_selection_and_the_selection_survives_it)
{
    int selection;

    // The contract agreed in session 1: a held key is temporary. It wins while
    // down, and because the resolver is pure it cannot do anything else - the
    // selection it was given is still there when the key comes up.
    for (selection = 0; selection < HIT_LOC_COUNT; selection++) {
        CHECK_INT(aim_with(selection, KEY_COMMA), HIT_LOC_HEAD);
        CHECK_INT(aim_with(selection, KEY_PERIOD), HIT_LOC_ARM);
        CHECK_INT(aim_with(selection, KEY_SLASH), HIT_LOC_LEG);
        CHECK_INT(aim_with(selection, 0), selection);
    }
}

TEST(the_originals_key_precedence_is_preserved_exactly)
{
    int selection;

    // `intgame_perform_action_on_target_inner` tests comma, then period, then
    // slash, and two keys down has always meant the first of them. A selector
    // that reordered this would change a mechanic while claiming to add a
    // control, so every combination is asserted rather than the obvious three.
    for (selection = 0; selection < HIT_LOC_COUNT; selection++) {
        CHECK_INT(aim_with(selection, KEY_COMMA | KEY_PERIOD), HIT_LOC_HEAD);
        CHECK_INT(aim_with(selection, KEY_COMMA | KEY_SLASH), HIT_LOC_HEAD);
        CHECK_INT(aim_with(selection, KEY_PERIOD | KEY_SLASH), HIT_LOC_ARM);
        CHECK_INT(aim_with(selection, KEY_COMMA | KEY_PERIOD | KEY_SLASH),
            HIT_LOC_HEAD);
    }
}

TEST(every_selection_and_key_combination_answers_a_real_hit_location)
{
    int selection;
    int keys;
    int aim;

    // Whatever it returns indexes `hit_loc_penalties[]`, which has exactly
    // HIT_LOC_COUNT entries. A resolver that can answer outside that range is
    // a read off the end of a table.
    for (selection = -3; selection < HIT_LOC_COUNT + 3; selection++) {
        for (keys = 0; keys < 8; keys++) {
            aim = aim_with(selection, keys);
            CHECK(aim >= 0);
            CHECK(aim < HIT_LOC_COUNT);
        }
    }
}

TEST(a_selection_outside_the_table_is_treated_as_an_ordinary_attack)
{
    // Not clamped to the nearest limb: a caller who asks for something that
    // does not exist gets the shipped behaviour, not a different aim.
    CHECK_INT(aim_with(-1, 0), HIT_LOC_TORSO);
    CHECK_INT(aim_with(HIT_LOC_COUNT, 0), HIT_LOC_TORSO);
    CHECK_INT(aim_with(9999, 0), HIT_LOC_TORSO);
}

TEST(with_the_feature_off_the_resolver_is_the_shipped_behaviour)
{
    int keys;

    // The whole flag-off promise, stated as a property rather than trusted:
    // with no selection kept, the answer is exactly what the three keys said
    // before this module existed.
    for (keys = 0; keys < 8; keys++) {
        int expected = (keys & KEY_COMMA) != 0 ? HIT_LOC_HEAD
            : (keys & KEY_PERIOD) != 0         ? HIT_LOC_ARM
            : (keys & KEY_SLASH) != 0          ? HIT_LOC_LEG
                                               : HIT_LOC_TORSO;
        CHECK_INT(aim_with(HIT_LOC_TORSO, keys), expected);
    }
}

TEST(a_refused_selection_does_not_disturb_the_one_already_kept)
{
    called_shot_reset();
    called_shot_select(HIT_LOC_LEG);
    called_shot_select(HIT_LOC_COUNT + 4);

    // Refusing has to leave the previous choice intact; replacing it with a
    // default would lose a selection the player made because of a caller's bug.
    // Read through `called_shot_effective_aim` so the feature flag, which is
    // off in this suite, does not hide the stored value.
    CHECK_INT(called_shot_effective_aim(HIT_LOC_LEG, false, false, false),
        HIT_LOC_LEG);

    called_shot_reset();
    CHECK_INT(called_shot_selection(), HIT_LOC_TORSO);
}

TEST(the_selection_is_off_unless_the_feature_is_on)
{
    // `called shot ui` defaults off, and this suite does not switch it on, so
    // a selection made here must not reach an attack.
    called_shot_reset();
    called_shot_select(HIT_LOC_HEAD);
    CHECK(!called_shot_enabled());
    CHECK_INT(called_shot_selection(), HIT_LOC_TORSO);
    CHECK_INT(called_shot_aim_now(), HIT_LOC_TORSO);
    called_shot_reset();
}

TEST(each_control_shows_the_limb_it_selects_not_the_one_its_key_raises)
{
    // Quirk 48. `interface.mes` names art 819 Cursor-Called-Leg and 820
    // Cursor-Called-Arm, while the key dispatch raises 819 for an ARM shot and
    // 820 for a LEG one - Troika's crossing, faithfully kept. The panel is
    // where a player reads what they are aiming at, so it maps the art to the
    // limb it depicts.
    CHECK_INT(called_shot_art_for(HIT_LOC_ARM), 820);
    CHECK_INT(called_shot_art_for(HIT_LOC_LEG), 819);

    CHECK_INT(called_shot_art_for(HIT_LOC_HEAD), 818);
    // Body is the ordinary combat cursor, because Body IS an ordinary attack:
    // the location is rolled, not aimed at the torso.
    CHECK_INT(called_shot_art_for(HIT_LOC_TORSO), 353);
}

TEST(no_two_controls_show_the_same_art)
{
    int a;
    int b;

    // Four distinct limbs need four distinct pictures; two controls sharing one
    // would be unreadable in exactly the way the crossing above already is.
    for (a = 0; a < HIT_LOC_COUNT; a++) {
        for (b = a + 1; b < HIT_LOC_COUNT; b++) {
            CHECK(called_shot_art_for(a) != called_shot_art_for(b));
        }
    }
}

TEST(a_limb_outside_the_table_has_no_art)
{
    CHECK_INT(called_shot_art_for(-1), -1);
    CHECK_INT(called_shot_art_for(HIT_LOC_COUNT), -1);
}

TEST(a_click_on_each_control_lands_on_the_limb_it_shows)
{
    TigRect panel = { 100, 200, 136, 37 };
    int index;
    int x;

    // Every control, hit at its own centre. The first panel used tig_button,
    // which rejects a click on a colour-key pixel - correct for drawn button
    // art and wrong for cursor art, which is mostly transparent. It drew
    // perfectly and accepted nothing.
    for (index = 0; index < HIT_LOC_COUNT; index++) {
        x = panel.x + index * 34 + 16;
        CHECK_INT(called_shot_control_index_at(&panel, x, panel.y + 18), index);
    }
}

TEST(a_click_outside_the_panel_belongs_to_nobody)
{
    TigRect panel = { 100, 200, 136, 37 };

    // Returning a control for a click that missed would make the panel eat
    // mouse events meant for the map.
    CHECK_INT(called_shot_control_index_at(&panel, panel.x - 1, panel.y + 18), -1);
    CHECK_INT(called_shot_control_index_at(&panel, panel.x + 1, panel.y - 1), -1);
    CHECK_INT(called_shot_control_index_at(&panel, panel.x + 1, panel.y + 37), -1);
    CHECK_INT(called_shot_control_index_at(&panel,
                  panel.x + 4 * 34, panel.y + 18),
        -1);
}

TEST(the_gaps_between_controls_select_nothing)
{
    TigRect panel = { 0, 0, 136, 37 };

    // Two pixels between each control. A gap that selected its neighbour would
    // make the edge of one control silently choose another limb.
    CHECK_INT(called_shot_control_index_at(&panel, 32, 18), -1);
    CHECK_INT(called_shot_control_index_at(&panel, 33, 18), -1);
    CHECK_INT(called_shot_control_index_at(&panel, 31, 18), 0);
    CHECK_INT(called_shot_control_index_at(&panel, 34, 18), 1);
}

TEST(the_hit_test_follows_the_panel_wherever_it_is_placed)
{
    TigRect a = { 0, 0, 136, 37 };
    TigRect b = { 517, 640, 136, 37 };

    // The panel is mapped by hrp_apply onto whatever the screen is, so the
    // geometry has to be relative to where it landed and not to the design
    // space it was authored in.
    CHECK_INT(called_shot_control_index_at(&a, 16, 18), HIT_LOC_TORSO);
    CHECK_INT(called_shot_control_index_at(&b, 16, 18), -1);
    CHECK_INT(called_shot_control_index_at(&b, 517 + 16, 640 + 18), HIT_LOC_TORSO);
}

TEST(a_missing_panel_rectangle_selects_nothing)
{
    CHECK_INT(called_shot_control_index_at(NULL, 10, 10), -1);
}

void test_called_shot_suite(void)
{
    RUN(with_no_key_held_the_kept_selection_is_what_an_attack_asks_for);
    RUN(a_held_key_overrides_every_selection_and_the_selection_survives_it);
    RUN(the_originals_key_precedence_is_preserved_exactly);
    RUN(every_selection_and_key_combination_answers_a_real_hit_location);
    RUN(a_selection_outside_the_table_is_treated_as_an_ordinary_attack);
    RUN(with_the_feature_off_the_resolver_is_the_shipped_behaviour);
    RUN(a_refused_selection_does_not_disturb_the_one_already_kept);
    RUN(the_selection_is_off_unless_the_feature_is_on);
    RUN(each_control_shows_the_limb_it_selects_not_the_one_its_key_raises);
    RUN(no_two_controls_show_the_same_art);
    RUN(a_limb_outside_the_table_has_no_art);
    RUN(a_click_on_each_control_lands_on_the_limb_it_shows);
    RUN(a_click_outside_the_panel_belongs_to_nobody);
    RUN(the_gaps_between_controls_select_nothing);
    RUN(the_hit_test_follows_the_panel_wherever_it_is_placed);
    RUN(a_missing_panel_rectangle_selects_nothing);
}
