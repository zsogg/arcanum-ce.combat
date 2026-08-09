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

    bool logos;
    bool intro;
} HighResConfig;

void highres_config_load(void);
const HighResConfig* highres_config_get(void);

#endif /* GAME_HIGHRES_CONFIG_H_ */
