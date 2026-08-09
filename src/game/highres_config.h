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

    bool logos;
    bool intro;
} HighResConfig;

#define HIGHRES_MENU_POSITION_TOP 0
#define HIGHRES_MENU_POSITION_CENTER 1
#define HIGHRES_MENU_POSITION_BOTTOM 2

void highres_config_load(void);
const HighResConfig* highres_config_get(void);

#endif /* GAME_HIGHRES_CONFIG_H_ */
