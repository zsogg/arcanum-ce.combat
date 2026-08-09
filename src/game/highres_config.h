#ifndef GAME_HIGHRES_CONFIG_H_
#define GAME_HIGHRES_CONFIG_H_

#include <stdbool.h>

typedef struct HighResConfig {
    bool loaded;
    int width;
    int height;
    bool windowed;
    bool show_fps;
    int scroll_fps;
    int scroll_dist;

    /**
     * How far from the local pc, in tiles, an npc still gets its
     * SAP_HEARTBEAT script run each tick. `PreloadLimit` in config.ini, which
     * is the high resolution patch's name for it; the patch pokes the constant
     * at 0x4AD4B8, inside what is now `ai_should_run_heartbeat`.
     *
     * Stock 30, and the patch's own default is 60.
     */
    int preload_limit;

    /**
     * How far a broadcast command carries, in tiles. `BroadcastLimit` in
     * config.ini; the patch pokes the constant at 0x4C339E, inside
     * `broadcast_msg_client`.
     *
     * Stock 10, and the patch's own default is 20.
     */
    int broadcast_limit;

    /**
     * Where the shared "big window" panel and the character-editor windows sit
     * vertically: 0 top, 1 centre, 2 bottom. `MenuPosition` in config.ini.
     *
     * The patch spells this as Base.tph's `HeightBonus`, which is 0,
     * (Height - 600) / 2 or (Height - 600); `hrp_menu_gravity` maps those onto
     * GRAVITY_TOP, GRAVITY_CENTER_VERTICAL and GRAVITY_BOTTOM. Default 1, which
     * is what CE did unconditionally before and is also the patch's default.
     */
    int menu_position;

    /**
     * Draw the main menu's options screen in the patch's compact layout:
     * background art 669 in place of 556, and the lens, the four option
     * buttons and the eight controls all 41 pixels higher. `Options` in
     * config.ini, where 0 is normal and 1 is compact.
     *
     * Art 669 lives in the patch's own `ArcanumZHighRes.dat`, so this only
     * looks right on an install that has those archives mounted. Default off.
     */
    bool options_compact;

    /**
     * Mirror left-facing critter frames instead of storing them, which is what
     * the engine does by default. `SpriteMirroring` in config.ini, which writes
     * the flag at 0x5BEA14 - CE's `tig_art_mirroring_enabled`.
     *
     * Defaults to true, the stock behaviour, rather than to the patch's own
     * config default of 0. Ignored while `uap fixes` is on, because those
     * archives ship the mirrored frames already; see `uap_fixes_changed`.
     */
    bool sprite_mirroring;

    /**
     * Logbook text size: 0 keeps the stock size-12 font (interface art 229), 1
     * uses the patch's size-14 (art 230) and the roomier page rects that go
     * with it. `LogbookFont` in config.ini.
     */
    int logbook_font;

    /**
     * Dialogue text size: 0 stock size-12 (interface art 229), 1 the patch's
     * size-14 (art 230), 2 its size-18 (art 133). Larger text also needs a
     * roomier dialogue window and text bubble, so `highres_dialog_metrics`
     * carries those together. `DialogFont` in config.ini.
     */
    int dialog_font;

    bool logos;
    bool intro;
} HighResConfig;

/**
 * Everything `DialogFont` changes, kept in one place so tc.c and tb.c cannot
 * drift apart. The patch spreads the same numbers over Dialog.tph.
 */
typedef struct HighResDialogMetrics {
    int font_art; /* the interface art the dialogue fonts are built from */
    int window_width; /* tc_content_rect */
    int window_height; /* tc_content_rect, and the scratch buffer it is drawn in */
    int bubble_size; /* text bubble, square in both axes */
} HighResDialogMetrics;

const HighResDialogMetrics* highres_dialog_metrics(void);

#define HIGHRES_MENU_POSITION_TOP 0
#define HIGHRES_MENU_POSITION_CENTER 1
#define HIGHRES_MENU_POSITION_BOTTOM 2

void highres_config_load(void);
const HighResConfig* highres_config_get(void);

#endif /* GAME_HIGHRES_CONFIG_H_ */
