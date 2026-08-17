// Tests for the high-resolution patch's pure-logic pair: `highres_config.c`
// and `hrp.c`.
//
// `highres_config.c` is the parser for `HighRes/config.ini`. Every one of its
// bounds is player-visible - MenuPosition moves panels, DialogFont changes
// dialogue geometry, the two limits change AI/broadcast radii - so the parser
// is exactly the kind of small, isolated logic a suite should own.
//
// `hrp.c` is the 800x600-design-to-window gravity translation every HRP
// layout call funnels through. `hrp_apply` and the two mouse-unprojection
// sites in the UI use the same constants, so a wrong dx/dy here is a
// draw/click split, not just a visual blemish.
//
// The parser reads a fixed relative path (`HighRes/config.ini`), so the
// fixture tests chdir into a temporary directory. That is the same trick
// `test_mes.c` uses for its fixtures, one directory deeper.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/highres_config.h"
#include "game/hrp.h"

#include "test_harness.h"

#ifndef _WIN32
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

static char highres_old_cwd[PATH_MAX];
static char highres_fixture_dir[] = "./arcanum_highres_test_XXXXXX";
static bool highres_in_fixture;

static bool highres_fixture_enter(const char* contents)
{
    if (getcwd(highres_old_cwd, sizeof(highres_old_cwd)) == NULL) {
        return false;
    }

    strcpy(highres_fixture_dir, "./arcanum_highres_test_XXXXXX");
    if (mkdtemp(highres_fixture_dir) == NULL) {
        return false;
    }

    if (chdir(highres_fixture_dir) != 0
        || mkdir("HighRes", 0755) != 0) {
        return false;
    }

    if (contents != NULL) {
        FILE* stream = fopen("HighRes/config.ini", "wb");

        if (stream == NULL) {
            return false;
        }
        fwrite(contents, 1, strlen(contents), stream);
        fclose(stream);
    }

    highres_in_fixture = true;
    return true;
}

static void highres_fixture_leave(void)
{
    if (!highres_in_fixture) {
        return;
    }

    remove("HighRes/config.ini");
    rmdir("HighRes");
    chdir(highres_old_cwd);
    rmdir(highres_fixture_dir);
    highres_in_fixture = false;
}
#else
static bool highres_fixture_enter(const char* contents)
{
    (void)contents;
    return false;
}

static void highres_fixture_leave(void)
{
}
#endif

TEST(defaults_apply_when_there_is_no_config_file)
{
#ifndef _WIN32
    const HighResConfig* cfg;

    bool entered = highres_fixture_enter(NULL);

    CHECK(entered);
    if (!entered) {
        return;
    }

    highres_config_load();

    cfg = highres_config_get();
    CHECK_INT(cfg->width, 800);
    CHECK_INT(cfg->height, 600);
    CHECK(!cfg->windowed);
    CHECK(!cfg->show_fps);
    CHECK_INT(cfg->scroll_fps, 35);
    CHECK_INT(cfg->scroll_dist, 10);
    CHECK_INT(cfg->preload_limit, 30);
    CHECK_INT(cfg->broadcast_limit, 10);
    CHECK_INT(cfg->menu_position, HIGHRES_MENU_POSITION_CENTER);
    CHECK(!cfg->options_compact);
    CHECK(cfg->sprite_mirroring);
    CHECK_INT(cfg->logbook_font, 0);
    CHECK_INT(cfg->dialog_font, 0);
    CHECK(cfg->logos);
    CHECK(cfg->intro);

    CHECK_INT(highres_dialog_metrics()->font_art, 229);
    CHECK_INT(highres_dialog_metrics()->bubble_size, 200);

    highres_fixture_leave();
#endif
}

TEST(every_documented_key_parses)
{
#ifndef _WIN32
    const HighResConfig* cfg;
    static const char* contents = "//Arcanum High Resolution Patch Settings\n"
                                  "Width = 1280 // original: 800\n"
                                  "Height = 720 // original: 600\n"
                                  "Windowed = 1\n"
                                  "ShowFPS = 1\n"
                                  "ScrollFPS = 60\n"
                                  "ScrollDist = 30\n"
                                  "PreloadLimit = 60\n"
                                  "BroadcastLimit = 20\n"
                                  "MenuPosition = 2\n"
                                  "Options = 1\n"
                                  "SpriteMirroring = 0\n"
                                  "LogbookFont = 1\n"
                                  "DialogFont = 2\n"
                                  "Logos = 0\n"
                                  "Intro = 0\n";

    bool entered = highres_fixture_enter(contents);

    CHECK(entered);
    if (!entered) {
        return;
    }

    highres_config_load();

    cfg = highres_config_get();
    CHECK(cfg->loaded);
    CHECK_INT(cfg->width, 1280);
    CHECK_INT(cfg->height, 720);
    CHECK(cfg->windowed);
    CHECK(cfg->show_fps);
    CHECK_INT(cfg->scroll_fps, 60);
    CHECK_INT(cfg->scroll_dist, 30);
    CHECK_INT(cfg->preload_limit, 60);
    CHECK_INT(cfg->broadcast_limit, 20);
    CHECK_INT(cfg->menu_position, HIGHRES_MENU_POSITION_BOTTOM);
    CHECK(cfg->options_compact);
    CHECK(!cfg->sprite_mirroring);
    CHECK_INT(cfg->logbook_font, 1);
    CHECK_INT(cfg->dialog_font, 2);
    CHECK(!cfg->logos);
    CHECK(!cfg->intro);

    CHECK_INT(highres_dialog_metrics()->font_art, 133);
    CHECK_INT(highres_dialog_metrics()->bubble_size, 300);

    highres_fixture_leave();
#endif
}

TEST(out_of_range_values_are_ignored_not_clamped)
{
#ifndef _WIN32
    const HighResConfig* cfg;
    static const char* contents = "Width = 799\n"
                                  "Height = 599\n"
                                  "ScrollFPS = 0\n"
                                  "ScrollDist = -1\n"
                                  "PreloadLimit = 0\n"
                                  "PreloadLimit = 256\n"
                                  "BroadcastLimit = 256\n"
                                  "MenuPosition = 3\n"
                                  "LogbookFont = 2\n"
                                  "DialogFont = 3\n";

    bool entered = highres_fixture_enter(contents);

    CHECK(entered);
    if (!entered) {
        return;
    }

    highres_config_load();

    cfg = highres_config_get();
    CHECK_INT(cfg->width, 800);
    CHECK_INT(cfg->height, 600);
    CHECK_INT(cfg->scroll_fps, 35);
    CHECK_INT(cfg->scroll_dist, 10);
    CHECK_INT(cfg->preload_limit, 30);
    CHECK_INT(cfg->broadcast_limit, 10);
    CHECK_INT(cfg->menu_position, HIGHRES_MENU_POSITION_CENTER);
    CHECK_INT(cfg->logbook_font, 0);
    CHECK_INT(cfg->dialog_font, 0);

    highres_fixture_leave();
#endif
}

TEST(dialog_metrics_track_the_three_documented_fonts)
{
#ifndef _WIN32
    static const int font_arts[] = { 229, 230, 133 };
    static const int bubbles[] = { 200, 235, 300 };
    char contents[64];
    int value;

    for (value = 0; value <= 2; value++) {
        snprintf(contents, sizeof(contents), "DialogFont = %d\n", value);
        bool entered = highres_fixture_enter(contents);

        CHECK(entered);
        if (!entered) {
            break;
        }

        highres_config_load();
        CHECK_INT(highres_dialog_metrics()->font_art, font_arts[value]);
        CHECK_INT(highres_dialog_metrics()->bubble_size, bubbles[value]);

        highres_fixture_leave();
    }
#endif
}

TEST(menu_gravity_tracks_menu_position)
{
#ifndef _WIN32
    char contents[64];
    int value;
    Gravity expected[] = {
        GRAVITY_TOP,
        GRAVITY_CENTER_VERTICAL,
        GRAVITY_BOTTOM,
    };

    for (value = 0; value <= 2; value++) {
        snprintf(contents, sizeof(contents), "MenuPosition = %d\n", value);
        bool entered = highres_fixture_enter(contents);

        CHECK(entered);
        if (!entered) {
            break;
        }

        highres_config_load();
        CHECK_INT(hrp_menu_gravity(), expected[value]);

        highres_fixture_leave();
    }
#endif
}

TEST(hrp_apply_translates_the_800x600_design_onto_the_window)
{
    GameResizeInfo resize_info;
    TigRect rect;

    memset(&resize_info, 0, sizeof(resize_info));
    resize_info.window_rect.width = 1280;
    resize_info.window_rect.height = 720;
    hrp_resize(&resize_info);

    rect = (TigRect) { 10, 20, 30, 40 };
    hrp_apply(&rect, GRAVITY_LEFT | GRAVITY_TOP);
    CHECK_INT(rect.x, 10);
    CHECK_INT(rect.y, 20);

    rect = (TigRect) { 10, 20, 30, 40 };
    hrp_apply(&rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);
    CHECK_INT(rect.x, 250);
    CHECK_INT(rect.y, 80);

    rect = (TigRect) { 10, 20, 30, 40 };
    hrp_apply(&rect, GRAVITY_RIGHT | GRAVITY_BOTTOM);
    CHECK_INT(rect.x, 490);
    CHECK_INT(rect.y, 140);
}

TEST(hrp_apply_shrinks_back_down_below_the_design_resolution)
{
    GameResizeInfo resize_info;
    TigRect rect;

    memset(&resize_info, 0, sizeof(resize_info));
    resize_info.window_rect.width = 640;
    resize_info.window_rect.height = 480;
    hrp_resize(&resize_info);

    rect = (TigRect) { 10, 20, 30, 40 };
    hrp_apply(&rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);
    CHECK_INT(rect.x, -70);
    CHECK_INT(rect.y, -40);
}

TEST(hrp_center_moves_a_point_by_the_same_delta)
{
    GameResizeInfo resize_info;
    int x = 100;
    int y = 50;

    memset(&resize_info, 0, sizeof(resize_info));
    resize_info.window_rect.width = 1280;
    resize_info.window_rect.height = 720;
    hrp_resize(&resize_info);

    hrp_center(&x, &y);
    CHECK_INT(x, 340);
    CHECK_INT(y, 110);
}

void test_highres_config_suite(void)
{
    RUN(defaults_apply_when_there_is_no_config_file);
    RUN(every_documented_key_parses);
    RUN(out_of_range_values_are_ignored_not_clamped);
    RUN(dialog_metrics_track_the_three_documented_fonts);
    RUN(menu_gravity_tracks_menu_position);
    RUN(hrp_apply_translates_the_800x600_design_onto_the_window);
    RUN(hrp_apply_shrinks_back_down_below_the_design_resolution);
    RUN(hrp_center_moves_a_point_by_the_same_delta);
}
