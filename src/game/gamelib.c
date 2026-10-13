#include "game/gamelib.h"

#include <stdio.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/anim_private.h"
#include "game/animfx.h"
#include "game/antiteleport.h"
#include "game/area.h"
#include "game/background.h"
#include "game/bless.h"
#include "game/brightness.h"
#include "game/broadcast.h"
#include "game/ce_mods.h"
#include "game/ci.h"
#include "game/critter.h"
#include "game/curse.h"
#include "game/description.h"
#include "game/determinism.h"
#include "game/dialog.h"
#include "game/facade.h"
#include "game/feature.h"
#include "game/gameinit.h"
#include "game/gfade.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/hrp.h"
#include "game/invensource.h"
#include "game/item.h"
#include "game/item_effect.h"
#include "game/jumppoint.h"
#include "game/level.h"
#include "game/li.h"
#include "game/light.h"
#include "game/light_scheme.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/monstergen.h"
#include "game/mt_ai.h"
#include "game/mt_item.h"
#include "game/mt_obj_node.h"
#include "game/multiplayer.h"
#include "game/name.h"
#include "game/newspaper.h"
#include "game/party.h"
#include "game/player.h"
#include "game/portrait.h"
#include "game/provenance.h"
#include "game/quest.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/reputation.h"
#include "game/roof.h"
#include "game/rumor.h"
#include "game/save_transaction.h"
#include "game/script.h"
#include "game/script_name.h"
#include "game/sector.h"
#include "game/sector_script.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/tb.h"
#include "game/tc.h"
#include "game/tech.h"
#include "game/teleport.h"
#include "game/tf.h"
#include "game/tile.h"
#include "game/tile_block.h"
#include "game/tile_script.h"
#include "game/timeevent.h"
#include "game/townmap.h"
#include "game/trap.h"
#include "game/uap.h"
#include "game/ui.h"
#include "game/wall.h"
#include "game/wallcheck.h"
#include "game/wp.h"

#define GAMELIB_LONG_VERSION_LENGTH 40
#define GAMELIB_SHORT_VERSION_LENGTH 36
#define GAMELIB_LOCALE_LENGTH 4
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 7
#define VERSION_BUILD 4

#define GAMELIB_MAX_PATCH_COUNT 10

typedef bool (*GameInitFunc)(GameInitInfo* init_info);
typedef void (*GameResetFunc)(void);
typedef bool (*GameModuleLoadFunc)(void);
typedef void (*GameModuleUnloadFunc)(void);
typedef void (*GameExitFunc)(void);
typedef void (*GamePingFunc)(unsigned int time);
typedef void (*GameUpdateViewFunc)(ViewOptions* view_options);
typedef bool (*GameSaveFunc)(TigFile* stream);
typedef bool (*GameLoadFunc)(GameLoadInfo* load_info);
typedef void (*GameResizeFunc)(GameResizeInfo* resize_info);

typedef struct GameLibModule {
    const char* name;
    GameInitFunc init_func;
    GameResetFunc reset_func;
    GameModuleLoadFunc mod_load_func;
    GameModuleUnloadFunc mod_unload_func;
    GameExitFunc exit_func;
    GamePingFunc ping_func;
    GameUpdateViewFunc update_view_func;
    GameSaveFunc save_func;
    GameLoadFunc load_func;
    GameResizeFunc resize_func;
} GameLibModule;

typedef struct GameSaveEntry {
    time_t modify_time;
    char* path;
} GameSaveEntry;

static int game_save_entry_compare_by_date(const void* va, const void* vb);
static int game_save_entry_compare_by_name(const void* va, const void* vb);
static void difficulty_changed(void);
static void gamelib_draw_game(GameDrawInfo* draw_info);
static void gamelib_draw_editor(GameDrawInfo* draw_info);
static void gamelib_logo(void);
static void gamelib_splash(tig_window_handle_t window_handle);
static void gamelib_load_data(void);
static bool gamelib_load_module_data(const char* module_name);
static void gamelib_unload_module_data(void);

// 0x59A330
static GameLibModule gamelib_modules[] = {
    { "HighResolution", hrp_init, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, hrp_resize },
    { "Description", description_init, NULL, description_mod_load, description_mod_unload, description_exit, NULL, NULL, NULL, NULL, NULL },
    { "Item-Effect", item_effect_init, NULL, item_effect_mod_load, item_effect_mod_unload, item_effect_exit, NULL, NULL, NULL, NULL, NULL },
    { "Teleport", teleport_init, teleport_reset, NULL, NULL, teleport_exit, teleport_ping, NULL, NULL, NULL, NULL },
    { "Sector", sector_history_init, sector_history_reset, NULL, NULL, sector_history_exit, NULL, NULL, sector_history_save, sector_history_load, NULL },
    { "Random", random_init, NULL, NULL, NULL, random_exit, NULL, NULL, NULL, NULL, NULL },
    { "Critter", critter_init, NULL, NULL, NULL, critter_exit, NULL, NULL, NULL, NULL, NULL },
    { "Name", name_init, NULL, NULL, NULL, name_exit, NULL, NULL, NULL, NULL, NULL },
    { "ScriptName", script_name_init, NULL, script_name_mod_load, script_name_mod_unload, script_name_exit, NULL, NULL, NULL, NULL, NULL },
    { "Portait", portrait_init, NULL, NULL, NULL, portrait_exit, NULL, NULL, NULL, NULL, NULL },
    { "AnimFX", animfx_init, NULL, NULL, NULL, animfx_exit, NULL, NULL, NULL, NULL, NULL },
    { "Script", script_init, script_reset, script_mod_load, script_mod_unload, script_exit, NULL, NULL, script_save, script_load, NULL },
    { "MagicTech", magictech_init, magictech_reset, NULL, NULL, magictech_exit, NULL, NULL, NULL, NULL, NULL },
    { "MT-AT", mt_ai_init, mt_ai_reset, NULL, NULL, mt_ai_exit, NULL, NULL, NULL, NULL, NULL },
    { "MT-Item", mt_item_init, NULL, NULL, NULL, mt_item_exit, NULL, NULL, NULL, NULL, NULL },
    { "Spell", spell_init, NULL, NULL, NULL, spell_exit, NULL, NULL, NULL, NULL, NULL },
    { "Stat", stat_init, NULL, NULL, NULL, stat_exit, NULL, NULL, NULL, NULL, NULL },
    { "Level", level_init, NULL, NULL, NULL, level_exit, NULL, NULL, NULL, NULL, NULL },
    { "Map", map_init, map_reset, map_mod_load, map_mod_unload, map_exit, map_ping, map_update_view, map_save, map_load, map_resize },
    { "LightScheme", light_scheme_init, light_scheme_reset, light_scheme_mod_load, light_scheme_mod_unload, light_scheme_exit, NULL, NULL, light_scheme_save, light_scheme_load, NULL },
    { "MagicTech-Post", magictech_post_init, NULL, NULL, NULL, NULL, NULL, NULL, magictech_post_save, magictech_post_load, NULL },
    { "Player", player_init, player_reset, 0, NULL, player_exit, NULL, NULL, player_save, player_load, NULL },
    { "Area", area_init, area_reset, area_mod_load, area_mod_unload, area_exit, NULL, NULL, area_save, area_load, NULL },
    { "Facade", facade_init, NULL, NULL, NULL, facade_exit, NULL, facade_update_view, NULL, NULL, facade_resize },
    { "TC", tc_init, NULL, NULL, NULL, tc_exit, NULL, NULL, NULL, NULL, tc_resize },
    { "Dialog", dialog_init, NULL, NULL, NULL, dialog_exit, NULL, NULL, NULL, NULL, NULL },
    { "Skill", skill_init, NULL, NULL, NULL, skill_exit, NULL, NULL, skill_save, skill_load, NULL },
    { "SoundGame", gsound_init, gsound_reset, gsound_mod_load, gsound_mod_unload, gsound_exit, gsound_ping, NULL, gsound_save, gsound_load, NULL },
    { "Item", item_init, NULL, NULL, NULL, item_exit, NULL, NULL, NULL, NULL, item_resize },
    { "Combat", combat_init, combat_reset, 0, NULL, combat_exit, NULL, NULL, combat_save, combat_load, NULL },
    { "TimeEvent", timeevent_init, timeevent_reset, NULL, NULL, timeevent_exit, timeevent_ping, NULL, timeevent_save, timeevent_load, NULL },
    { "Rumor", rumor_init, rumor_reset, rumor_mod_load, rumor_mod_unload, rumor_exit, NULL, NULL, rumor_save, rumor_load, NULL },
    { "Quest", quest_init, quest_reset, quest_mod_load, quest_mod_unload, quest_exit, NULL, NULL, quest_save, quest_load, NULL },
    { "Bless", NULL, NULL, bless_mod_load, bless_mod_unload, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Curse", NULL, NULL, curse_mod_load, curse_mod_unload, NULL, NULL, NULL, NULL, NULL, NULL },
    { "AI", ai_init, NULL, ai_mod_load, ai_mod_unload, ai_exit, NULL, NULL, NULL, NULL, NULL },
    { "Broadcast", broadcast_init, NULL, NULL, NULL, broadcast_exit, NULL, NULL, NULL, NULL, NULL },
    { "Anim", anim_init, anim_reset, NULL, NULL, anim_exit, NULL, NULL, anim_save, anim_load, NULL },
    { "Anim-Private", anim_private_init, anim_private_reset, NULL, NULL, anim_private_exit, NULL, NULL, NULL, NULL, NULL },
    { "Multiplayer", multiplayer_init, multiplayer_reset, multiplayer_mod_load, multiplayer_mod_unload, multiplayer_exit, multiplayer_ping, NULL, multiplayer_save, mutliplayer_load, NULL },
    { "Tech", tech_init, NULL, NULL, NULL, tech_exit, NULL, NULL, NULL, NULL, NULL },
    { "Background", background_init, NULL, NULL, NULL, background_exit, NULL, NULL, NULL, NULL, NULL },
    { "Reputation", reputation_init, NULL, reputation_mod_load, reputation_mod_unload, reputation_exit, NULL, 0, NULL, NULL, NULL },
    { "Reaction", reaction_init, NULL, NULL, NULL, reaction_exit, NULL, NULL, NULL, NULL, NULL },
    { "Tile-Script", tile_script_init, tile_script_reset, NULL, NULL, tile_script_exit, 0, tile_script_update_view, NULL, NULL, tile_script_resize },
    { "Sector-Script", sector_script_init, sector_script_reset, NULL, NULL, sector_script_exit, NULL, NULL, NULL, NULL, NULL },
    { "WP", wp_init, NULL, NULL, NULL, wp_exit, NULL, wp_update_view, NULL, NULL, wp_resize },
    { "Inven-Source", invensource_init, NULL, NULL, NULL, invensource_exit, NULL, NULL, NULL, NULL, NULL },
    { "Newspaper", newspaper_init, newspaper_reset, NULL, NULL, newspaper_exit, NULL, NULL, newspaper_save, newspaper_load, NULL },
    { "TownMap", NULL, townmap_reset, townmap_mod_load, townmap_mod_unload, NULL, NULL, NULL, NULL, NULL, NULL },
    { "GMovie", NULL, NULL, gmovie_mod_load, gmovie_mod_unload, NULL, NULL, NULL, NULL, NULL, NULL },
    { "Brightness", brightness_init, NULL, NULL, NULL, brightness_exit, NULL, NULL, NULL, NULL, NULL },
    { "GFade", gfade_init, NULL, NULL, NULL, gfade_exit, NULL, NULL, NULL, NULL, gfade_resize },
    { "Anti-Teleport", antiteleport_init, NULL, antiteleport_mod_load, antiteleport_mod_unload, antiteleport_exit, NULL, NULL, NULL, NULL, NULL },
    { "Trap", trap_init, NULL, NULL, NULL, trap_exit, NULL, NULL, NULL, NULL, NULL },
    { "WallCheck", wallcheck_init, wallcheck_reset, NULL, NULL, wallcheck_exit, wallcheck_ping, NULL, NULL, NULL, NULL },
    { "LI", li_init, NULL, NULL, NULL, li_exit, NULL, NULL, NULL, NULL, li_resize },
    { "CI", ci_init, NULL, NULL, NULL, ci_exit, NULL, NULL, NULL, NULL, NULL },
    { "TileBlock", tileblock_init, NULL, NULL, NULL, tileblock_exit, NULL, tileblock_update_view, NULL, NULL, tileblock_resize },
    { "MT-Obj-Node", mt_obj_node_init, NULL, NULL, NULL, mt_obj_node_exit, NULL, NULL, NULL, NULL, NULL },
    { "MonsterGen", monstergen_init, monstergen_reset, NULL, NULL, monstergen_exit, NULL, NULL, monstergen_save, monstergen_load, monstergen_resize },
    { "Party", party_init, party_reset, NULL, 0, party_exit, NULL, NULL, NULL, NULL, NULL },
    { "gameinit", gameinit_init, gameinit_reset, gameinit_mod_load, gameinit_mod_unload, gameinit_exit, NULL, NULL, NULL, NULL, NULL },
};

#define MODULE_COUNT ((int)SDL_arraysize(gamelib_modules))

// 0x59ADD8
static int gamelib_renderlock_cnt = 1;

// 0x59ADDC
static char gamelib_current_module_name[TIG_MAX_PATH] = "Arcanum";

// 0x59AEDC
static char gamelib_default_module_name[TIG_MAX_PATH] = "Arcanum";

// 0x5CFF08
static char gamelib_mod_dir_path[TIG_MAX_PATH];

// CE (`ce save folder`, release 1, R2): CE's own save folder.
#define GAMELIB_CE_SAVE_DIR "SaveCE"

// CE (release 1, R3): CE's own loose data layer.
#define GAMELIB_CE_DATA_DIR "ce-data"

// CE (release 1, R4): CE's own settings file, when present.
#define GAMELIB_CE_CONFIG "arcanum-ce.cfg"

// 0x5D000C
static TigRectListNode* gamelib_pending_dirty_rects_head;

// 0x5D0010
static bool gamelib_dirty;

// 0x5D0018
static TigRect gamelib_iso_content_rect;

// 0x5D0028
static char gamelib_mod_patch_paths[GAMELIB_MAX_PATCH_COUNT][TIG_MAX_PATH];

// NOTE: Verified - original allocation is 256 bytes (0x5D0A50..0x5D0B50); the 260-byte claim is incorrect.
//
// 0x5D0A50
static char byte_5D0A50[256];

// 0x5D0B50
static ViewOptions gamelib_view_options;

// 0x5D0B58
static char byte_5D0B58[TIG_MAX_PATH];

// 0x5D0D60
static TigRect gamelib_iso_content_rect_ex;

// 0x5D0D74
static bool in_draw;

// 0x5D0D78
static int gamelib_window_rect_x;

// 0x5D0D7C
static int gamelib_window_rect_y;

// 0x5D0D80
static int gamelib_thumbnail_height;

// 0x5D0E88
static GameInitInfo gamelib_init_info;

// 0x5D0E98
static TigRectListNode* gamelib_dirty_rects_head;

// 0x5D0E9C
static int gamelib_game_difficulty;

// 0x5D0EA0
static bool gamelib_mod_loaded;

// 0x5D0EA4
static char byte_5D0EA4[TIG_MAX_PATH];

// 0x5D0FA8
static char gamelib_mod_dat_path[TIG_MAX_PATH];

// 0x5D10AC
static void (*gamelib_draw_func)(GameDrawInfo* draw_info);

// 0x5D10B0
static TigGuid gamelib_mod_guid;

// 0x5D10C0
static int gamelib_thumbnail_width;

// 0x5D10C4
static bool dword_5D10C4;

// 0x5D10D4
static GameExtraSaveFunc gamelib_extra_save_func;

// 0x5D10D8
static GameExtraLoadFunc gamelib_extra_load_func;

// 0x5D10DC
static bool gamelib_savelist_sort_check_autosave;

// 0x5D10E0
static bool in_save;

// 0x5D10E4
static bool in_load;

// 0x5D10E8
static bool in_reset;

// 0x5D10EC
static int gamelib_cheat_level;

// 0x739E60
unsigned int gamelib_ping_time;

// 0x739E70
Settings settings;

// 0x739E7C
TigVideoBuffer* gamelib_scratch_video_buffer;

// 0x4020F0
bool gamelib_init(GameInitInfo* init_info)
{
    char version[40];
    TigWindowData window_data;
    TigVideoBufferCreateInfo vb_create_info;
    int index;

    gamelib_copy_version(version, NULL, NULL);
    tig_debug_printf("\n%s\n", version);

    if (init_info->editor) {
        settings_init(&settings, "worlded.cfg");
    } else {
        // CE (release 1, R4, 2026-10-13): a release keeps CE's settings in
        // their own file, so the original's `arcanum.cfg` is never touched.
        // Without `arcanum-ce.cfg`, CE reads and writes `arcanum.cfg` as ever.
        FILE* ce_config = fopen(GAMELIB_CE_CONFIG, "rt");
        if (ce_config != NULL) {
            fclose(ce_config);
            settings_init(&settings, GAMELIB_CE_CONFIG);
            tig_debug_printf("gamelib: settings from %s\n", GAMELIB_CE_CONFIG);
        } else {
            settings_init(&settings, "arcanum.cfg");
        }
    }

    settings_load(&settings);

    settings_register(&settings, DIFFICULTY_KEY, "1", difficulty_changed);
    difficulty_changed();

    // Before any module that attaches a change callback, and after
    // `settings_load` so a value already in the config wins over the default.
    feature_init();

    // CE (release 1, R4): enabled mod bundles set their switches for this
    // session, before anything reads a switch (`ce_mods.h`).
    ce_mods_init();

    // CE (`ce save folder`; release 1, R2, zsogg 2026-10-13): CE's saves go to
    // their own folder, `SaveCE` beside the original's `Save` in the module,
    // because the original cannot load a save CE wrote (the area count, card
    // H4). Every relative `save\...` path is redirected in the file layer
    // (`tig_file_set_save_dir`), so the game's paths stay as written. Read
    // once, here, before the first save path is touched; changing it takes
    // effect at the next start.
    tig_file_set_save_dir(feature_enabled(FEATURE_CE_SAVE_FOLDER) ? GAMELIB_CE_SAVE_DIR : NULL);

    uap_init();

    gamelib_mod_loaded = false;
    gamelib_load_data();

    if (!init_info->editor) {
        if (highres_config_get()->logos) {
            gamelib_logo();
        }

        gamelib_splash(init_info->iso_window_handle);
    }

    init_info->invalidate_rect_func = gamelib_invalidate_rect;
    init_info->draw_func = gamelib_draw;

    gamelib_init_info = *init_info;

    tig_window_data(init_info->iso_window_handle, &window_data);

    gamelib_window_rect_x = window_data.rect.x;
    gamelib_window_rect_y = window_data.rect.y;
    gamelib_thumbnail_width = window_data.rect.width / 4;
    gamelib_thumbnail_height = window_data.rect.height / 4;

    gamelib_iso_content_rect.x = 0;
    gamelib_iso_content_rect.y = 0;
    gamelib_iso_content_rect.width = window_data.rect.width;
    gamelib_iso_content_rect.height = window_data.rect.height;

    gamelib_iso_content_rect_ex.x = -256;
    gamelib_iso_content_rect_ex.y = -256;
    gamelib_iso_content_rect_ex.width = window_data.rect.width + 512;
    gamelib_iso_content_rect_ex.height = window_data.rect.height + 512;

    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_COLOR_KEY | TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = window_data.rect.width;
    vb_create_info.height = window_data.rect.height;
    vb_create_info.color_key = tig_color_make(0, 255, 0);
    vb_create_info.background_color = vb_create_info.color_key;
    if (tig_video_buffer_create(&vb_create_info, &gamelib_scratch_video_buffer) != TIG_OK) {
        return false;
    }

    if (init_info->editor) {
        gamelib_draw_func = gamelib_draw_editor;
    } else {
        gamelib_draw_func = gamelib_draw_game;
    }

    gamelib_view_options.type = VIEW_TYPE_ISOMETRIC;

    for (index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].init_func != NULL) {
            if (!gamelib_modules[index].init_func(init_info)) {
                tig_debug_printf("gamelib_init(): init function %d (%s) failed\n",
                    index,
                    gamelib_modules[index].name);

                while (--index >= 0) {
                    if (gamelib_modules[index].exit_func != NULL) {
                        gamelib_modules[index].exit_func();
                    }
                }

                return false;
            }
        }
    }

    // CE: after the module loop, deliberately. `random_init` seeds the RNG
    // from `time(NULL)`, so pinning the seed has to happen once every module
    // that touches the clock has had its turn. No-op unless the run asked for
    // reproducible mode.
    determinism_init();

    return true;
}

// 0x402380
void gamelib_reset(void)
{
    tig_timestamp_t reset_started_at;
    tig_timestamp_t module_started_at;
    tig_duration_t duration;
    TigRectListNode* node;
    TigRectListNode* next;
    int index;

    tig_debug_printf("gamelib_reset: Resetting.\n");
    tig_timer_now(&reset_started_at);

    in_reset = true;
    strcpy(gamelib_current_module_name, "Arcanum");
    sector_art_cache_disable();

    if (tig_file_is_directory("Save\\Current")) {
        tig_debug_printf("gamelib_reset: Begin Removing Files...");
        tig_timer_now(&module_started_at);

        if (!tig_file_empty_directory("Save\\Current")) {
            tig_debug_printf("gamelib_init(): error emptying folder %s\n", "Save\\Current");
        }

        duration = tig_timer_elapsed(module_started_at);
        tig_debug_printf("done. Time (ms): %d\n", duration);
    }

    node = gamelib_dirty_rects_head;
    while (node != NULL) {
        next = node->next;
        tig_rect_node_destroy(node);
        node = next;
    }
    gamelib_dirty_rects_head = NULL;

    for (index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].reset_func != NULL) {
            tig_debug_printf("gamelib_reset: Processing Reset Function: %d", index);
            tig_timer_now(&module_started_at);

            gamelib_modules[index].reset_func();

            duration = tig_timer_elapsed(module_started_at);
            tig_debug_printf(" done. Time (ms): %d.\n", duration);
        }
    }

    sector_art_cache_enable();
    in_reset = false;

    duration = tig_timer_elapsed(reset_started_at);
    tig_debug_printf("gamelib_reset(): Done.  Total time: %d ms.\n", duration);
}

// 0x4024D0
void gamelib_exit(void)
{
    settings_save(&settings);

    for (int index = MODULE_COUNT - 1; index >= 0; index--) {
        if (gamelib_modules[index].exit_func != NULL) {
            gamelib_modules[index].exit_func();
        }
    }

    mes_stats();

    TigRectListNode* node = gamelib_dirty_rects_head;
    while (node != NULL) {
        TigRectListNode* next = node->next;
        tig_rect_node_destroy(node);
        node = next;
    }

    if (gamelib_scratch_video_buffer != NULL) {
        tig_video_buffer_destroy(gamelib_scratch_video_buffer);
        gamelib_scratch_video_buffer = NULL;
    }

    if (tig_file_is_directory("Save\\Current")) {
        if (!tig_file_empty_directory("Save\\Current")) {
            // NOTE: Verified - string at 0x59B32C literally reads "gamelib_init(): error emptying folder %s\n"; CE matches.

            tig_debug_printf("gamelib_init(): error emptying folder %s\n", "Save\\Current");
        }
    }

    settings_exit(&settings);
}

// 0x402580
void gamelib_ping(void)
{
    int index;

    tig_timer_now(&gamelib_ping_time);

    for (index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].ping_func != NULL) {
            gamelib_modules[index].ping_func(gamelib_ping_time);
        }
    }
}

// 0x4025C0
void gamelib_resize(GameResizeInfo* resize_info)
{
    TigVideoBufferCreateInfo vb_create_info;
    int index;
    TigRect bounds;
    TigRect frame;

    gamelib_init_info.iso_window_handle = resize_info->window_handle;
    gamelib_iso_content_rect = resize_info->content_rect;

    gamelib_window_rect_x = resize_info->window_rect.x;
    gamelib_window_rect_y = resize_info->window_rect.y;

    gamelib_iso_content_rect_ex.x = gamelib_iso_content_rect.x - 256;
    gamelib_iso_content_rect_ex.y = gamelib_iso_content_rect.y - 256;
    gamelib_iso_content_rect_ex.width = gamelib_iso_content_rect.width + 512;
    gamelib_iso_content_rect_ex.height = gamelib_iso_content_rect.height + 512;

    if (gamelib_scratch_video_buffer != NULL) {
        tig_video_buffer_destroy(gamelib_scratch_video_buffer);
        gamelib_scratch_video_buffer = NULL;
    }

    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_COLOR_KEY | TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = gamelib_iso_content_rect.width;
    vb_create_info.height = gamelib_iso_content_rect.height;
    vb_create_info.color_key = tig_color_make(0, 255, 0);
    vb_create_info.background_color = vb_create_info.color_key;
    if (tig_video_buffer_create(&vb_create_info, &gamelib_scratch_video_buffer) != TIG_OK) {
        tig_debug_printf("gamelib_resize: ERROR: Failed to rebuild scratch buffer!\n");
        return;
    }

    for (index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].resize_func != NULL) {
            gamelib_modules[index].resize_func(resize_info);
        }
    }

    if (gamelib_dirty_rects_head != NULL) {
        bounds = resize_info->window_rect;
        bounds.x = 0;
        bounds.y = 0;
        if (tig_rect_intersection(&(gamelib_dirty_rects_head->rect), &bounds, &frame) == TIG_OK) {
            gamelib_dirty_rects_head->rect = frame;
        }
    }
}

// 0x402780
void gamelib_default_module_name_set(const char* name)
{
    strcpy(gamelib_default_module_name, name);
}

// 0x4027B0
const char* gamelib_default_module_name_get(void)
{
    return gamelib_default_module_name;
}

// 0x4027C0
void gamelib_modlist_create(GameModuleList* module_list, int type)
{
    TigFileList file_list;
    unsigned int file_index;
    char* dot;
    bool is_file;
    bool cont;
    bool found;
    unsigned int module_index;
    bool have_mp;
    char path[TIG_MAX_PATH];

    module_list->count = 0;
    module_list->selected = 0;
    module_list->paths = NULL;

    if (type != 0 && type != 1) {
        tig_debug_printf("Invalid type passed into gamelib_modlist_create()\n");
        return;
    }

    tig_file_list_create(&file_list, "Modules\\*.*");

    for (file_index = 0; file_index < file_list.count; file_index++) {
        cont = false;
        is_file = false;
        dot = strrchr(file_list.entries[file_index].path, '.');
        if (dot != NULL) {
            if (SDL_strcasecmp(dot, ".dat") == 0
                && (file_list.entries[file_index].attributes & TIG_FILE_ATTRIBUTE_SUBDIR) == 0) {
                *dot = '\0';
                is_file = true;
                cont = true;
            }
        } else if ((file_list.entries[file_index].attributes & TIG_FILE_ATTRIBUTE_SUBDIR) != 0) {
            cont = true;
        }

        if (cont) {
            found = false;
            module_index = module_list->count;
            while (module_index > 0) {
                if (SDL_strcasecmp(file_list.entries[file_index].path, module_list->paths[module_index - 1]) == 0) {
                    found = true;
                    break;
                }
                module_index--;
            }

            if (!found) {
                strcpy(path, "Modules\\");
                strcat(path, file_list.entries[file_index].path);
                if (is_file) {
                    strcat(path, ".dat");
                }

                tig_file_repository_add(path);
                have_mp = tig_file_exists_in_path(path, "mp.txt", NULL);
                tig_file_repository_remove(path);

                if ((have_mp && type == 1) || type == 0) {
                    module_list->paths = (char**)REALLOC(module_list->paths, sizeof(module_list->paths) * (module_list->count + 1));
                    module_list->paths[module_list->count] = STRDUP(file_list.entries[file_index].path);

                    if (SDL_strcasecmp(byte_5D0EA4, file_list.entries[file_index].path) == 0) {
                        module_list->selected = module_list->count;
                    }

                    module_list->count++;
                }
            }
        }
    }

    tig_file_list_destroy(&file_list);
}

// 0x402A10
void gamelib_modlist_destroy(GameModuleList* module_list)
{
    unsigned int index;

    for (index = 0; index < module_list->count; index++) {
        FREE(module_list->paths[index]);
    }
    FREE(module_list->paths);

    module_list->count = 0;
    module_list->selected = 0;
    module_list->paths = NULL;
}

// CE (`ce save folder`, R2): the save folder's name for an explicit path.
static const char* gamelib_save_dir_name(void)
{
    const char* name = tig_file_save_dir();
    return name[0] != '\0' ? name : "save";
}

// CE (`ce save folder`; release 1, R2): the first time a module is loaded with
// CE's own save folder and that folder does not exist, the original's saved
// games are copied into it - one way: CE reads the original's saves, and the
// original's folder is only ever read. The slot files only; `Current` and
// CE's transaction folder are working state. Later saves made by the original
// are not copied again.
static void gamelib_save_import(void)
{
    char ce_dir[TIG_MAX_PATH];
    char pattern[TIG_MAX_PATH];
    char src[TIG_MAX_PATH];
    char dst[TIG_MAX_PATH];
    TigFileList file_list;
    unsigned int index;
    int copied = 0;

    if (tig_file_save_dir()[0] == '\0' || gamelib_mod_dir_path[0] == '\0') {
        return;
    }

    snprintf(ce_dir, sizeof(ce_dir), "%s\\%s", gamelib_mod_dir_path, tig_file_save_dir());
    if (tig_file_is_directory(ce_dir)) {
        return;
    }
    if (!tig_file_mkdir(ce_dir)) {
        tig_debug_printf("gamelib: could not create %s\n", ce_dir);
        return;
    }

    snprintf(pattern, sizeof(pattern), "%s\\save\\slot*.*", gamelib_mod_dir_path);
    tig_file_list_create(&file_list, pattern);
    for (index = 0; index < file_list.count; index++) {
        if ((file_list.entries[index].attributes & TIG_FILE_ATTRIBUTE_SUBDIR) != 0) {
            continue;
        }
        snprintf(src, sizeof(src), "%s\\save\\%s", gamelib_mod_dir_path, file_list.entries[index].path);
        snprintf(dst, sizeof(dst), "%s\\%s", ce_dir, file_list.entries[index].path);
        if (tig_file_copy(src, dst)) {
            copied++;
        } else {
            tig_debug_printf("gamelib: could not copy %s into %s\n", src, ce_dir);
        }
    }
    tig_file_list_destroy(&file_list);
    tig_debug_printf("gamelib: CE's save folder %s created; %d file(s) of the original's saves copied\n",
        ce_dir, copied);
}

// 0x402A50
bool gamelib_mod_load(const char* path)
{
    TigFileList file_list;
    unsigned int file_index;
    int module_index;

    gamelib_mod_unload();

    if (!gamelib_load_module_data(path)) {
        return false;
    }

    if (gamelib_mod_dat_path[0] != '\0') {
        tig_file_repository_guid(gamelib_mod_dat_path, &gamelib_mod_guid);
    }

    dword_5D10C4 = true;

    gamelib_save_import();

    if (tig_file_is_directory("Save\\Current")) {
        if (!tig_file_is_empty_directory("Save\\Current")) {
            if (!tig_file_empty_directory("Save\\Current")) {
                tig_debug_printf("gamelib_mod_load(): error emptying folder %s\n", "Save\\Current");
                gamelib_unload_module_data();
                return false;
            }
        }
    }

    if (!gamelib_init_info.editor) {
        tig_file_list_create(&file_list, "maps\\*.*");

        for (file_index = 0; file_index < file_list.count; file_index++) {
            if ((file_list.entries[file_index].attributes & TIG_FILE_ATTRIBUTE_SUBDIR) != 0
                && file_list.entries[file_index].path[0] != '.') {
                if (!map_preprocess_mobile(file_list.entries[file_index].path)) {
                    tig_debug_printf("gamelib_mod_load(): error preprocessing mobile object data for map %s\n",
                        file_list.entries[file_index].path);
                    gamelib_unload_module_data();
                    return false;
                }
            }
        }

        tig_file_list_destroy(&file_list);
    }

    for (module_index = 0; module_index < MODULE_COUNT; module_index++) {
        if (gamelib_modules[module_index].mod_load_func != NULL) {
            if (!gamelib_modules[module_index].mod_load_func()) {
                tig_debug_printf("gamelib_load(): mod load function %d (%s) failed\n",
                    module_index,
                    gamelib_modules[module_index].name);

                while (--module_index >= 0) {
                    if (gamelib_modules[module_index].mod_unload_func != NULL) {
                        gamelib_modules[module_index].mod_unload_func();
                    }
                }

                gamelib_unload_module_data();

                return false;
            }
        }
    }

    strcpy(byte_5D0EA4, path);
    gamelib_mod_loaded = true;

    return true;
}

// 0x402C20
bool gamelib_mod_guid_get(TigGuid* guid_ptr)
{
    if (!dword_5D10C4) {
        return false;
    }

    *guid_ptr = gamelib_mod_guid;

    return true;
}

// 0x402C60
void gamelib_mod_unload(void)
{
    int index;

    if (gamelib_mod_loaded) {
        for (index = MODULE_COUNT - 1; index >= 0; index--) {
            if (gamelib_modules[index].mod_unload_func != NULL) {
                gamelib_modules[index].mod_unload_func();
            }
        }
        gamelib_mod_loaded = false;
    }
}

// 0x402CA0
bool gamelib_in_reset(void)
{
    return in_reset;
}

// 0x402CB0
int gamelib_cheat_level_get(void)
{
    // UAP (0x402CB0, 0x402CC6): the release build stubbed this getter to
    // `xor eax, eax` and the setter to store a hardcoded 0. UAP restores the
    // global read and makes the setter store 3. CE already implements the
    // unstubbed pair, so there is nothing to port; see main.c's `-cheatmode`
    // note.
    return gamelib_cheat_level;
}

// 0x402CC0
void gamelib_cheat_level_set(int level)
{
    gamelib_cheat_level = level;
}

// 0x402CD0
void gamelib_update_view(ViewOptions* view_options)
{
    for (int index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].update_view_func != NULL) {
            gamelib_modules[index].update_view_func(view_options);
        }
    }

    gamelib_view_options = *view_options;

    gamelib_invalidate_rect(NULL);
}

// 0x402D10
void gamelib_get_view_options(ViewOptions* view_options)
{
    *view_options = gamelib_view_options;
}

// 0x402D30
void gamelib_invalidate_rect(TigRect* rect)
{
    TigRect dirty_rect;

    if (rect != NULL) {
        dirty_rect = *rect;

        if (tig_rect_intersection(&dirty_rect, &gamelib_iso_content_rect, &dirty_rect) != TIG_OK) {
            return;
        }
    } else {
        dirty_rect = gamelib_iso_content_rect;
    }

    if (in_draw) {
        if (gamelib_pending_dirty_rects_head != NULL) {
            tig_rect_list_add_disjoint(&gamelib_pending_dirty_rects_head, &dirty_rect);
        } else {
            gamelib_pending_dirty_rects_head = tig_rect_node_create();
            gamelib_pending_dirty_rects_head->rect = dirty_rect;
        }
    } else {
        if (gamelib_dirty_rects_head != NULL) {
            tig_rect_list_add_disjoint(&gamelib_dirty_rects_head, &dirty_rect);
        } else {
            gamelib_dirty_rects_head = tig_rect_node_create();
            gamelib_dirty_rects_head->rect = dirty_rect;
        }

        gamelib_dirty = true;
    }
}

// 0x402E50
bool gamelib_draw(void)
{
    bool ret = false;
    TigRectListNode* node;
    TigRectListNode* next;
    TigRect rect;
    LocRect loc_rect;
    SectorRect sector_rect;
    SectorListNode* sectors;
    GameDrawInfo draw_info;

    if (gamelib_renderlock_cnt <= 0) {
        return false;
    }

    if (!gamelib_dirty) {
        return false;
    }

    in_draw = true;

    if (location_screen_rect_to_loc_rect(&gamelib_iso_content_rect_ex, &loc_rect)) {
        if (gamelib_view_options.type == VIEW_TYPE_ISOMETRIC) {
            sector_rect_from_loc_rect(&loc_rect, &sector_rect);
        }

        sectors = sector_list_create(&loc_rect);
        draw_info.screen_rect = &gamelib_iso_content_rect_ex;
        draw_info.loc_rect = &loc_rect;
        draw_info.sector_rect = &sector_rect;
        draw_info.sectors = sectors;
        draw_info.rects = &gamelib_dirty_rects_head;
        gamelib_draw_func(&draw_info);
        sector_list_destroy(sectors);

        node = gamelib_dirty_rects_head;
        while (node != NULL) {
            next = node->next;
            rect = node->rect;
            rect.x += gamelib_window_rect_x;
            rect.y += gamelib_window_rect_y;
            tig_window_invalidate_rect(&rect);
            tig_rect_node_destroy(node);
            node = next;
        }
        ret = true;
    }

    gamelib_dirty_rects_head = gamelib_pending_dirty_rects_head;
    gamelib_pending_dirty_rects_head = NULL;

    if (gamelib_dirty_rects_head == NULL) {
        gamelib_dirty = false;
    }

    in_draw = false;

    return ret;
}

// 0x402F90
void gamelib_renderlock_acquire(void)
{
    gamelib_renderlock_cnt--;
}

// 0x402FA0
void gamelib_renderlock_release(void)
{
    gamelib_renderlock_cnt++;
}

// 0x402FC0
void gamelib_clear_screen(void)
{
    tig_window_fill(gamelib_init_info.iso_window_handle, NULL, 0);
    tig_window_display();
    gamelib_invalidate_rect(NULL);
}

// 0x402FE0
const char* gamelib_current_module_name_get(void)
{
    return gamelib_current_module_name;
}

// 0x402FF0
void gamelib_current_mode_name_set(const char* name)
{
    if (name != NULL && *name != '\0') {
        strcpy(gamelib_current_module_name, name);
    }
}

// CE: recovery returns a verdict. Callers must not read a partly restored slot.
//
// The verdict is also remembered, because refusing to enumerate makes the save
// list arrive **empty**, and empty is what a player with no saved games sees.
// Those two readings are not close: one says "retry or inspect Save", the other
// says "start over". `gamelib_slot_recovery_blocked` lets the UI tell them
// apart without teaching it anything about the transaction.
static bool gamelib_slot_recovery_blocked;

bool gamelib_slot_recover(void)
{
    if (!save_transaction_recover()) {
        tig_debug_printf("gamelib: save recovery incomplete; files retained, save/list/load blocked\n");
        gamelib_slot_recovery_blocked = true;
        return false;
    }
    gamelib_slot_recovery_blocked = false;
    return true;
}

// CE: did the last recovery attempt leave saves unreadable?
bool gamelib_slot_recovery_blocked_get(void)
{
    return gamelib_slot_recovery_blocked;
}

// 0x403030
bool gamelib_save(const char* name, const char* description)
{
    int start_pos = 0;
    int pos;
    tig_timestamp_t start_time;
    tig_timestamp_t time;
    tig_duration_t duration;
    char path[TIG_MAX_PATH];
    TigFile* stream;
    int index;
    unsigned int sentinel = 0xBEEFCAFE;
    int version;
    GameSaveInfo save_info;

    tig_debug_printf("\ngamelib_save(): Saving to File: %s.\n", name);
    tig_timer_now(&start_time);

    if (!save_transaction_begin()) {
        return false;
    }

    in_save = true;

    ui_progressbar_init(MODULE_COUNT + 2);

    if (!tig_file_is_directory("Save\\Current")) {
        tig_debug_printf("gamelib_save(): Error finding folder %s\n", "Save\\Current");
        in_save = false;
        return false;
    }

    strcpy(path, "Save\\Current");
    strcat(path, "\\data.sav");

    stream = tig_file_fopen(path, "wb");
    if (stream == NULL) {
        tig_debug_printf("gamelib_save(): Error creating %s\n", path);
        in_save = false;
        return false;
    }

    version = 25;
    if (tig_file_fwrite(&version, sizeof(version), 1, stream) != 1) {
        tig_debug_printf("gamelib_save(): Error writing version\n");
        tig_file_fclose(stream);
        tig_file_remove(path);
        in_save = false;
        return false;
    }

    tig_file_fgetpos(stream, &start_pos);
    tig_debug_printf("gamelib_save: Start Pos: %lu\n", (unsigned long)start_pos);

    ui_progressbar_update(1);

    for (index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].save_func != NULL) {
            tig_debug_printf("gamelib_save: Function %d (%s)", index, gamelib_modules[index].name);
            tig_timer_now(&time);

            if (!gamelib_modules[index].save_func(stream)) {
                tig_debug_printf("gamelib_save(): save function %d (%s) failed\n", index, gamelib_modules[index].name);
                break;
            }

            duration = tig_timer_elapsed(time);
            tig_file_fgetpos(stream, &pos);
            tig_debug_printf(" wrote to: %lu, Total: (%lu), Time (ms): %d\n",
                (unsigned long)pos,
                (unsigned long)(pos - start_pos),
                duration);
            start_pos = pos;

            if (tig_file_fwrite(&sentinel, sizeof(sentinel), 1, stream) != 1) {
                tig_debug_printf("gamelib_save(): ERROR: Sentinel Failed to Save!\n");
                break;
            }

            ui_progressbar_update(index + 1);
        }
    }

    // CE: the buffered data.sav flush is part of staging, too.
    int close_result = tig_file_fclose(stream);

    if (index < MODULE_COUNT || close_result != 0) {
        tig_file_remove(path);
        in_save = false;
        return false;
    }

    if (gamelib_extra_save_func != NULL) {
        tig_debug_printf("gamelib_save: Begin gamelib_extra_save_func()...");
        tig_timer_now(&time);
        if (!gamelib_extra_save_func()) {
            tig_file_remove(path);
            in_save = false;
            return false;
        }
        duration = tig_timer_elapsed(time);
        tig_debug_printf("done. Time (ms): %d\n", duration);
    }

    if (!gamelib_saveinfo_init(name, description, &save_info)) {
        tig_debug_printf("gamelib_save(): error creating saveinfo\n");
        in_save = false;
        return false;
    }

    // CE: a diagnostic record of the configuration that wrote this save,
    // written into the folder so the archive below carries it (card H9). It
    // is **never** allowed to fail a save: a save the player asked for
    // matters and a diagnostic does not, so a failure is a log line.
    if (!provenance_save()) {
        tig_debug_printf("gamelib_save: no provenance record written\n");
    }

    // CE: stage every required file before the transaction can touch a live
    // slot. save_transaction.c owns the commit/recovery protocol; the .gsi
    // remains the UI's discovery file, but is NOT a transaction marker.
    tig_timer_now(&time);
    if (!tig_file_archive(SAVE_TRANSACTION_STAGE, "Save\\Current")
        || !gamelib_saveinfo_save(&save_info)
        || !save_transaction_commit(name, description)) {
        tig_debug_printf("gamelib_save(): staging or commit failed\n");
        gamelib_slot_recover();
        gamelib_saveinfo_exit(&save_info);
        in_save = false;
        return false;
    }
    duration = tig_timer_elapsed(time);
    tig_debug_printf("gamelib_save: slot transaction done. Time (ms): %d\n", duration);
    gamelib_saveinfo_exit(&save_info);

    ui_progressbar_update(MODULE_COUNT + 2);

    in_save = false;

    duration = tig_timer_elapsed(start_time);
    tig_debug_printf("gamelib_save(): Save Complete.  Total time: %d ms.\n", duration);

    return true;
}

// 0x403410
bool gamelib_load(const char* name)
{
    int start_pos = 0;
    int pos;
    tig_timestamp_t start_time;
    tig_timestamp_t time;
    tig_duration_t duration;
    char path[TIG_MAX_PATH];
    TigFile* stream;
    GameLoadInfo load_info;
    int index;
    unsigned int sentinel;

    tig_debug_printf("\ngamelib_load: Loading from File: %s.\n", name);
    tig_timer_now(&start_time);

    // CE (card R2): the other way a caller reaches a save. An interrupted
    // replacement is rolled back to its previous generation before the load
    // reads anything, so a load never sees a half-written slot.
    if (!gamelib_slot_recover()) {
        return false;
    }

    in_load = true;

    ui_progressbar_init(MODULE_COUNT + 2);

    if (!tig_file_is_directory("Save\\Current")) {
        tig_debug_printf("gamelib_load(): Error finding folder %s\n", "Save\\Current");
        in_load = false;
        return false;
    }

    snprintf(path, sizeof(path), "save\\%s", name);

    tig_debug_printf("gamelib_load: begin removing files...");
    tig_timer_now(&time);
    if (!tig_file_empty_directory("Save\\Current")) {
        tig_debug_printf("gamelib_load(): Error clearing folder %s\n", "Save\\Current");
        in_load = false;
        return false;
    }
    duration = tig_timer_elapsed(time);
    tig_debug_printf("done.  Time (ms): %d\n", duration);

    tig_debug_printf("gamelib_load: begin restoring folder archive...");
    tig_timer_now(&time);
    if (!tig_file_unarchive(path, "Save\\Current")) {
        tig_debug_printf("gamelib_load(): error restoring archive %s to save\\test\n", path);
        in_load = false;
        return false;
    }
    duration = tig_timer_elapsed(time);
    tig_debug_printf("done.  Time (ms): %d\n", duration);

    stream = tig_file_fopen("Save\\Current\\data.sav", "rb");
    if (stream == NULL) {
        tig_debug_printf("gamelib_load(): Error reading data.sav\n");
        in_load = false;
        return false;
    }

    if (tig_file_fread(&(load_info.version), sizeof(load_info.version), 1, stream) != 1) {
        tig_debug_printf("gamelib_load(): Error reading version\n");
        in_load = false;
        return false;
    }

    load_info.stream = stream;

    tig_file_fgetpos(stream, &start_pos);
    tig_debug_printf("gamelib_load: Start Pos: %lu\n", (unsigned long)start_pos);

    ui_progressbar_update(1);

    for (index = 0; index < MODULE_COUNT; index++) {
        if (gamelib_modules[index].load_func != NULL) {
            tig_debug_printf("gamelib_load: Function %d (%s)", index, gamelib_modules[index].name);
            tig_timer_now(&time);

            if (!gamelib_modules[index].load_func(&load_info)) {
                tig_debug_printf("gamelib_load(): load function %d (%s) failed\n", index, gamelib_modules[index].name);
                break;
            }

            duration = tig_timer_elapsed(time);
            tig_file_fgetpos(stream, &pos);
            tig_debug_printf(" read to: %lu, Total: (%lu), Time (ms): %d\n",
                (unsigned long)pos,
                (unsigned long)(pos - start_pos),
                duration);
            start_pos = pos;

            if (tig_file_fread(&sentinel, sizeof(sentinel), 1, load_info.stream) != 1) {
                tig_debug_printf("gamelib_load(): ERROR: Load Sentinel Failed to Load!\n");
                break;
            }

            if (sentinel != 0xBEEFCAFE) {
                tig_debug_printf("gamelib_load(): ERROR: Load Sentinel Failed to Match!\n");
                break;
            }

            ui_progressbar_update(index + 1);
        }
    }

    tig_file_fclose(stream);

    if (index < MODULE_COUNT) {
        in_load = false;
        return false;
    }

    if (gamelib_extra_load_func != NULL) {
        tig_debug_printf("gamelib_load: Begin gamelib_extra_load_func()...");
        tig_timer_now(&time);
        if (!gamelib_extra_load_func()) {
            in_load = false;
            return false;
        }
        duration = tig_timer_elapsed(time);
        tig_debug_printf("done. Time (ms): %d\n", duration);
    }

    ui_progressbar_update(MODULE_COUNT + 2);

    in_load = false;

    duration = tig_timer_elapsed(start_time);
    tig_debug_printf("gamelib_load: Load Complete.  Total time: %d ms.\n", duration);

    return true;
}

// 0x403790
bool gamelib_delete(const char* name)
{
    char path[TIG_MAX_PATH];

    if (SDL_strcasecmp(name, "SlotAutoAuto-Save") == 0) {
        return false;
    }

    snprintf(path, sizeof(path), "save\\%s.gsi", name);
    tig_file_remove(path);

    snprintf(&(path[13]), sizeof(path) - 13, ".bmp");
    tig_file_remove(path);

    snprintf(&(path[13]), sizeof(path) - 13, ".tfaf");
    tig_file_remove(path);

    snprintf(&(path[13]), sizeof(path) - 13, ".tfai");
    tig_file_remove(path);

    return true;
}

// 0x403850
const char* gamelib_last_save_name(void)
{
    // 0x5D0D88
    static char byte_5D0D88[TIG_MAX_PATH];

    // 0x5D10C8
    static GameSaveList save_list;

    byte_5D0D88[0] = '\0';
    gamelib_savelist_create(&save_list);
    gamelib_savelist_sort(&save_list, GAME_SAVE_LIST_ORDER_DATE, true);

    if (save_list.count > 0) {
        strcpy(byte_5D0D88, save_list.names[0]);
    }

    // FIX: Memory leak.
    gamelib_savelist_destroy(&save_list);

    return byte_5D0D88;
}

// CE: resolve an exact eight-character slot identity (for example
// `Slot0001`) to the full .gsi base name (`Slot0001My Save`). The load screen
// stores the description after the first eight characters, while the archive
// pair itself uses only those eight. Selecting by slot therefore has to find
// the .gsi name without falling back to the newest file's timestamp.
//
// Refuse an ambiguous directory rather than letting its iteration order pick
// one of two descriptions for the same archive pair.
bool gamelib_save_name_resolve(const char* slot, char* name, size_t size)
{
    GameSaveList save_list;
    const char* match = NULL;
    unsigned int index;
    bool valid;

    if (slot == NULL || name == NULL || size == 0 || strlen(slot) != 8
        || SDL_strncasecmp(slot, "Slot", 4) != 0) {
        return false;
    }

    valid = SDL_strcasecmp(slot, "SlotAuto") == 0;
    if (!valid) {
        valid = true;
        for (index = 4; index < 8; index++) {
            if (slot[index] < '0' || slot[index] > '9') {
                valid = false;
                break;
            }
        }
    }
    if (!valid) {
        return false;
    }

    gamelib_savelist_create(&save_list);
    for (index = 0; index < save_list.count; index++) {
        if (SDL_strncasecmp(save_list.names[index], slot, 8) != 0) {
            continue;
        }
        if (match != NULL || strlen(save_list.names[index]) >= size) {
            gamelib_savelist_destroy(&save_list);
            return false;
        }
        match = save_list.names[index];
    }

    if (match != NULL) {
        strcpy(name, match);
    }
    gamelib_savelist_destroy(&save_list);
    return match != NULL;
}

// 0x4038C0
bool gamelib_in_save(void)
{
    return in_save;
}

// 0x4038D0
bool gamelib_in_load(void)
{
    return in_load;
}

// 0x4038E0
void gamelib_set_extra_save_func(GameExtraSaveFunc func)
{
    gamelib_extra_save_func = func;
}

// 0x4038F0
void gamelib_set_extra_load_func(GameExtraLoadFunc func)
{
    gamelib_extra_load_func = func;
}

// 0x403900
void gamelib_savelist_create(GameSaveList* save_list)
{
    TigFileList file_list;
    unsigned int index;
    char fname[COMPAT_MAX_FNAME];
    char ext[COMPAT_MAX_EXT];

    save_list->count = 0;
    save_list->names = NULL;
    save_list->module = NULL;

    // CE (card R2): before anything is listed, put back any slot an
    // interrupted save left mid-replacement. A caller reaches a save through
    // here or through `gamelib_load`, and both recover first.
    if (!gamelib_slot_recover()) {
        return;
    }

    tig_file_list_create(&file_list, "save\\slot*.*");

    for (index = 0; index < file_list.count; index++) {
        compat_splitpath(file_list.entries[index].path, NULL, NULL, fname, ext);
        if (SDL_strcasecmp(ext, ".gsi") == 0) {
            save_list->names = (char**)REALLOC(save_list->names, sizeof(save_list->names) * (save_list->count + 1));
            save_list->names[save_list->count++] = STRDUP(fname);
        }
    }

    tig_file_list_destroy(&file_list);
}

// 0x4039E0
void gamelib_savelist_create_module(const char* module, GameSaveList* save_list)
{
    TigFileList file_list;
    unsigned int index;
    char fname[COMPAT_MAX_FNAME];
    char ext[COMPAT_MAX_EXT];
    char path[TIG_MAX_PATH];

    save_list->count = 0;
    save_list->names = NULL;
    save_list->module = STRDUP(module);

    // CE (`ce save folder`, R2): this path is explicit, so the file layer
    // leaves it alone; it names the save folder itself.
    snprintf(path, sizeof(path), ".\\Modules\\%s\\%s\\slot*.*", module, gamelib_save_dir_name());
    tig_file_list_create(&file_list, path);

    for (index = 0; index < file_list.count; index++) {
        compat_splitpath(file_list.entries[index].path, NULL, NULL, fname, ext);
        if (SDL_strcasecmp(ext, ".gsi") == 0) {
            save_list->names = (char**)REALLOC(save_list->names, sizeof(save_list->names) * (save_list->count + 1));
            save_list->names[save_list->count++] = STRDUP(fname);
        }
    }

    tig_file_list_destroy(&file_list);
}

// 0x403BB0
void gamelib_savelist_destroy(GameSaveList* save_list)
{
    unsigned int index;

    for (index = 0; index < save_list->count; index++) {
        FREE(save_list->names[index]);
    }

    if (save_list->names != NULL) {
        FREE(save_list->names);
    }

    save_list->count = 0;
    save_list->names = NULL;

    if (save_list->module != NULL) {
        FREE(save_list->module);
    }

    save_list->module = NULL;
}

// 0x403C10
void gamelib_savelist_sort(GameSaveList* save_list, GameSaveListOrder order, bool a3)
{
    GameSaveEntry* entries;
    unsigned int index;
    char path[TIG_MAX_PATH];
    TigFileInfo file_info;

    gamelib_savelist_sort_check_autosave = !a3;

    entries = (GameSaveEntry*)MALLOC(sizeof(*entries) * save_list->count);

    for (index = 0; index < save_list->count; index++) {
        entries[index].path = save_list->names[index];
        if (save_list->module != NULL) {
            snprintf(path, sizeof(path),
                ".\\Modules\\%s\\%s\\%s.gsi",
                save_list->module,
                gamelib_save_dir_name(),
                save_list->names[index]);
        } else {
            snprintf(path, sizeof(path),
                "save\\%s.gsi",
                save_list->names[index]);
        }

        if (!tig_file_exists(path, &file_info)) {
            tig_debug_printf("GameLib: : ERROR: Couldn't find file!\n");
            // FIX: Memory leak.
            FREE(entries);
            return;
        }

        entries[index].modify_time = file_info.modify_time;
    }

    switch (order) {
    case GAME_SAVE_LIST_ORDER_DATE:
        qsort(entries, save_list->count, sizeof(*entries), game_save_entry_compare_by_date);
        break;
    case GAME_SAVE_LIST_ORDER_NAME:
        qsort(entries, save_list->count, sizeof(*entries), game_save_entry_compare_by_name);
        break;
    }

    for (index = 0; index < save_list->count; index++) {
        save_list->names[index] = entries[index].path;
    }

    FREE(entries);
}

// 0x403D40
int game_save_entry_compare_by_date(const void* va, const void* vb)
{
    const GameSaveEntry* a = (const GameSaveEntry*)va;
    const GameSaveEntry* b = (const GameSaveEntry*)vb;

    if (gamelib_savelist_sort_check_autosave) {
        if (SDL_toupper(a->path[4]) == 'A') {
            return -1;
        }

        if (SDL_toupper(b->path[4]) == 'A') {
            return 1;
        }
    }

    if (a->modify_time < b->modify_time) {
        return 1;
    } else if (a->modify_time > b->modify_time) {
        return -1;
    } else {
        return 0;
    }
}

// 0x403DB0
int game_save_entry_compare_by_name(const void* va, const void* vb)
{
    const GameSaveEntry* a = (const GameSaveEntry*)va;
    const GameSaveEntry* b = (const GameSaveEntry*)vb;

    return -SDL_strncasecmp(a->path, b->path, 8);
}

// 0x403DD0
bool gamelib_saveinfo_init(const char* name, const char* description, GameSaveInfo* save_info)
{
    int64_t pc_obj;
    TigVideoBufferCreateInfo vb_create_info;
    TigWindowBlitInfo win_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    char* pc_name;

    if (save_info == NULL) {
        return false;
    }

    pc_obj = player_get_local_pc_obj();

    save_info->version = 25;
    strcpy(save_info->name, name);
    strcpy(save_info->module_name, gamelib_current_module_name_get());

    save_info->thumbnail_video_buffer = NULL;
    vb_create_info.width = gamelib_thumbnail_width;
    vb_create_info.height = gamelib_thumbnail_height;
    vb_create_info.flags = 0;
    vb_create_info.background_color = 0;
    if (tig_video_buffer_create(&vb_create_info, &(save_info->thumbnail_video_buffer)) != TIG_OK) {
        return false;
    }

    // CE: Source rect for thumbnails is always centered 800x600 regardless of
    // the actual window size. This makes thubnails taken at any resolution show
    // the same area.
    src_rect.x = (gamelib_iso_content_rect.width - 800) / 2;
    src_rect.y = (gamelib_iso_content_rect.height - 600) / 2;
    src_rect.width = 800;
    src_rect.height = 600;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = gamelib_thumbnail_width;
    dst_rect.height = gamelib_thumbnail_height;

    win_blit_info.type = TIG_WINDOW_BLT_WINDOW_TO_VIDEO_BUFFER;
    win_blit_info.src_window_handle = gamelib_init_info.iso_window_handle;
    win_blit_info.src_rect = &src_rect;
    win_blit_info.dst_video_buffer = save_info->thumbnail_video_buffer;
    win_blit_info.dst_rect = &dst_rect;
    win_blit_info.vb_blit_flags = TIG_VIDEO_BUFFER_BLIT_SCALE_LINEAR;
    if (tig_window_blit(&win_blit_info) != TIG_OK) {
        tig_debug_printf("gamelib: ERROR: Build thumbnail FAILED to Blit!\n");
        if (tig_video_buffer_destroy(save_info->thumbnail_video_buffer) != TIG_OK) {
            tig_debug_printf("gamelib: ERROR: Build thumbnail FAILED to destroy VBid!\n");
        }
        save_info->thumbnail_video_buffer = NULL;
        return false;
    }

    obj_field_string_get(pc_obj, OBJ_F_PC_PLAYER_NAME, &pc_name);
    strcpy(save_info->pc_name, pc_name);
    FREE(pc_name);

    save_info->map = map_current_map();
    save_info->pc_portrait = portrait_get(pc_obj);
    save_info->pc_level = stat_level_get(pc_obj, STAT_LEVEL);
    save_info->pc_location = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
    save_info->story_state = script_story_state_get();
    strcpy(save_info->description, description);
    save_info->datetime = timeevent_current_datetime();

    return true;
}

// 0x403FF0
void gamelib_saveinfo_exit(GameSaveInfo* save_info)
{
    if (save_info->thumbnail_video_buffer != NULL) {
        tig_video_buffer_destroy(save_info->thumbnail_video_buffer);
    }
}

// 0x404010
bool gamelib_saveinfo_save(GameSaveInfo* save_info)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    TigVideoBufferSaveToBmpInfo to_bmp_info;
    TigRect rect;
    bool success = false;
    int size;

    // CE: this writer only produces staging; save_transaction_commit publishes it.
    snprintf(path, sizeof(path), SAVE_TRANSACTION_STAGE ".gsi");

    stream = tig_file_fopen(path, "wb");
    if (stream == NULL) {
        return false;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = gamelib_thumbnail_width;
    rect.height = gamelib_thumbnail_height;

    to_bmp_info.flags = 0;
    to_bmp_info.video_buffer = save_info->thumbnail_video_buffer;
    to_bmp_info.rect = &rect;
    snprintf(to_bmp_info.path, sizeof(to_bmp_info.path), SAVE_TRANSACTION_STAGE ".bmp");

    if (tig_video_buffer_save_to_bmp(&to_bmp_info) == TIG_OK) {
        do {
            if (tig_file_fwrite(&(save_info->version), sizeof(save_info->version), 1, stream) != 1) break;

            size = (int)strlen(save_info->module_name);
            if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) break;
            if (tig_file_fwrite(save_info->module_name, size, 1, stream) != 1) break;

            size = (int)strlen(save_info->pc_name);
            if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) break;
            if (tig_file_fwrite(save_info->pc_name, size, 1, stream) != 1) break;

            if (tig_file_fwrite(&(save_info->map), sizeof(save_info->map), 1, stream) != 1) break;
            if (tig_file_fwrite(&(save_info->datetime), sizeof(save_info->datetime), 1, stream) != 1) break;
            if (tig_file_fwrite(&(save_info->pc_portrait), sizeof(save_info->pc_portrait), 1, stream) != 1) break;
            if (tig_file_fwrite(&(save_info->pc_level), sizeof(save_info->pc_level), 1, stream) != 1) break;
            if (tig_file_fwrite(&(save_info->pc_location), sizeof(save_info->pc_location), 1, stream) != 1) break;
            if (tig_file_fwrite(&(save_info->story_state), sizeof(save_info->story_state), 1, stream) != 1) break;

            size = (int)strlen(save_info->description);
            if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) break;
            if (size != 0 && tig_file_fwrite(save_info->description, size, 1, stream) != 1) break;

            success = true;
        } while (0);
    }

    if (tig_file_fclose(stream) != 0) {
        success = false;
    }

    return success;
}

// 0x404270
bool gamelib_saveinfo_load(const char* name, GameSaveInfo* save_info)
{
    TigFile* stream;
    int size;
    bool success = false;

    snprintf(byte_5D0A50, sizeof(byte_5D0A50), "save\\%s.gsi", name);

    stream = tig_file_fopen(byte_5D0A50, "rb");
    if (stream == NULL) {
        return false;
    }

    save_info->thumbnail_video_buffer = NULL;

    strcpy(byte_5D0A50, "save\\");
    strncpy(&(byte_5D0A50[5]), name, 8);
    strcpy(&(byte_5D0A50[13]), ".bmp");

    do {
        if (tig_video_buffer_load_from_bmp(byte_5D0A50, &save_info->thumbnail_video_buffer, 0x1) != TIG_OK) break;
        if (tig_file_fread(&(save_info->version), sizeof(save_info->version), 1, stream) != 1) break;

        if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) break;
        if (size < 0 || (size_t)size >= sizeof(save_info->module_name)) break;
        if (tig_file_fread(save_info->module_name, size, 1, stream) != 1) break;
        save_info->module_name[size] = '\0';

        if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) break;
        if (tig_file_fread(save_info->pc_name, size, 1, stream) != 1) break;

        if (tig_file_fread(&(save_info->map), sizeof(save_info->map), 1, stream) != 1) break;
        if (tig_file_fread(&(save_info->datetime), sizeof(save_info->datetime), 1, stream) != 1) break;
        if (tig_file_fread(&(save_info->pc_portrait), sizeof(save_info->pc_portrait), 1, stream) != 1) break;
        if (tig_file_fread(&(save_info->pc_level), sizeof(save_info->pc_level), 1, stream) != 1) break;
        if (tig_file_fread(&(save_info->pc_location), sizeof(save_info->pc_location), 1, stream) != 1) break;
        if (tig_file_fread(&(save_info->story_state), sizeof(save_info->story_state), 1, stream) != 1) break;

        if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) break;
        if (size != 0 && tig_file_fread(save_info->description, size, 1, stream) != 1) break;

        success = true;
    } while (0);

    tig_file_fclose(stream);

    return success;
}

// 0x4044A0
void gamelib_thumbnail_size_set(int width, int height)
{
    gamelib_thumbnail_width = width;
    gamelib_thumbnail_height = height;
}

// 0x404570
void difficulty_changed(void)
{
    gamelib_game_difficulty = settings_get_value(&settings, DIFFICULTY_KEY);
}

// 0x404590
int gamelib_game_difficulty_get(void)
{
    return gamelib_game_difficulty;
}

// 0x4045A0
void gamelib_redraw(void)
{
    li_redraw();
    ci_redraw();
    gamelib_invalidate_rect(NULL);
    gamelib_draw();
    tig_window_invalidate_rect(NULL);
}

// 0x4045D0
bool gamelib_copy_version(char* long_version, char* short_version, char* locale)
{
    if (long_version != NULL) {
        // UAP (0x59BC6C): the format string is overwritten in place with the
        // literal "Unofficial Patch 2.0.2" - exactly as long as
        // "Arcanum %d.%d.%d.%d %s", which is why it fits - leaving no
        // conversions, so the version numbers and the locale are discarded.
        //
        // NOTE: `gamelib_init` logs this string at 0x402103, before
        // `settings_init` and so before the switch can possibly be known, so
        // the startup log line always reads "Arcanum 1.0.7.4 en" here where
        // the patched binary's reads "Unofficial Patch 2.0.2". Unavoidable
        // without moving the log call, which would change the faithful path.
        // Only the main-menu banner differs in practice.
        if (uap_fixes_enabled()) {
            snprintf(long_version,
                GAMELIB_LONG_VERSION_LENGTH - 1,
                "Unofficial Patch 2.0.2");
        } else {
            snprintf(long_version,
                GAMELIB_LONG_VERSION_LENGTH - 1,
                "Arcanum %d.%d.%d.%d %s",
                VERSION_MAJOR,
                VERSION_MINOR,
                VERSION_PATCH,
                VERSION_BUILD,
                gamelib_get_locale());
        }
    }

    if (short_version != NULL) {
        snprintf(short_version,
            GAMELIB_SHORT_VERSION_LENGTH - 1,
            "%d.%d.%d.%d",
            VERSION_MAJOR,
            VERSION_MINOR,
            VERSION_PATCH,
            VERSION_BUILD);
    }

    if (locale != NULL) {
        strncpy(locale, gamelib_get_locale(), GAMELIB_LOCALE_LENGTH - 1);
    }

    return true;
}

// 0x404640
void gamelib_patch_lvl_set(const char* patch_lvl)
{
    (void)patch_lvl;
}

// 0x404650
const char* gamelib_get_locale(void)
{
    // 0x5D0D84
    static char locale[GAMELIB_LOCALE_LENGTH];

    mes_file_handle_t mes_file;
    MesFileEntry mes_file_entry;

    if (mes_load("mes\\language.mes", &mes_file)) {
        mes_file_entry.num = 1;
        if (mes_search(mes_file, &mes_file_entry)) {
            mes_get_msg(mes_file, &mes_file_entry);
            strcpy(locale, mes_file_entry.str);
            mes_unload(mes_file);
            return locale;
        }

        mes_unload(mes_file);
    }

    return "en";
}

// 0x4046F0
void gamelib_draw_game(GameDrawInfo* draw_info)
{
    if (tig_video_3d_begin_scene() == TIG_OK) {
        light_draw(draw_info);
        tile_draw(draw_info);
        object_hover_overlay_draw(draw_info);
        object_draw(draw_info);
        roof_draw(draw_info);
        tb_draw(draw_info);
        tf_draw(draw_info);
        tc_draw(draw_info);
        tig_video_3d_end_scene();
    }
}

// 0x404740
void gamelib_draw_editor(GameDrawInfo* draw_info)
{
    TigRectListNode* node;
    tig_color_t color;

    color = tig_color_make(0, 0, 255);
    node = *draw_info->rects;
    while (node != NULL) {
        tig_window_fill(gamelib_init_info.iso_window_handle, &(node->rect), color);
        node = node->next;
    }

    if (tig_video_3d_begin_scene() == TIG_OK) {
        light_draw(draw_info);
        tile_draw(draw_info);
        facade_draw(draw_info);
        jumppoint_draw(draw_info);
        tile_script_draw(draw_info);
        tileblock_draw(draw_info);
        object_draw(draw_info);
        sector_draw(draw_info);
        wall_draw(draw_info);
        wp_draw(draw_info);
        roof_draw(draw_info);
        tb_draw(draw_info);
        tig_video_3d_end_scene();
    }
}

// 0x404810
void gamelib_logo(void)
{
    gmovie_play_path("movies\\SierraLogo.bik", 0, 0);
    gmovie_play_path("movies\\TroikaLogo.bik", 0, 0);
}

// 0x404830
void gamelib_splash(tig_window_handle_t window_handle)
{
    int splash;
    TigVideoBuffer* video_buffer;
    TigVideoBufferData vb_data;
    TigFileList file_list;
    TigWindowData window_data;
    int value;
    char path[TIG_MAX_PATH];
    int index;
    TigRect src_rect;
    TigRect dst_rect;

    settings_register(&settings, SPLASH_KEY, "0", NULL);
    splash = settings_get_value(&settings, SPLASH_KEY);

    tig_file_list_create(&file_list, "art\\splash\\*.bmp");

    if (file_list.count != 0) {
        for (index = 0; index < 3; index++) {
            value = (index + splash) % file_list.count;
            snprintf(path, sizeof(path), "art\\splash\\%s", file_list.entries[value].path);
            if (tig_video_buffer_load_from_bmp(path, &video_buffer, 0x1) == TIG_OK) {
                break;
            }
        }

        if (video_buffer != NULL
            && tig_window_data(window_handle, &window_data) == TIG_OK
            && tig_video_buffer_data(video_buffer, &vb_data) == TIG_OK) {
            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = vb_data.width;
            src_rect.height = vb_data.height;

            dst_rect.x = (window_data.rect.width - vb_data.width) / 2;
            dst_rect.y = (window_data.rect.height - vb_data.height) / 2;
            dst_rect.width = vb_data.width;
            dst_rect.height = vb_data.height;

            tig_window_copy_from_vbuffer(window_handle, &dst_rect, video_buffer, &src_rect);
        }

        if (video_buffer != NULL) {
            tig_video_buffer_destroy(video_buffer);
        }

        tig_window_invalidate_rect(NULL);
        tig_window_display();

        // CE: macOS and iOS require dequeuing events for the window to be
        // presented by the window manager. I'm not sure whether there is a
        // specific event to wait for or if it's just a matter of time.
        for (int i = 0; i < 3; i++) {
            SDL_PumpEvents();
        }

        settings_set_value(&settings, SPLASH_KEY, value + 1);
    }

    tig_file_list_destroy(&file_list);
}

// 0x404A20
void gamelib_load_data(void)
{
    TigFileList file_list;
    unsigned int index;

    tig_file_list_create(&file_list, "arcanum*.dat");

    for (index = 0; index < file_list.count; index++) {
        tig_file_repository_add(file_list.entries[index].path);
    }

    tig_file_list_destroy(&file_list);

    tig_file_mkdir("data");
    tig_file_repository_add("data");

    // CE (release 1, R3, 2026-10-13): CE's own loose layer, mounted just above
    // `data` when it exists, so a file a mod installs there is read by CE and
    // never by the original exe, which mounts `data` and stops. The Combat
    // Rebalance's rules files go here in a release, leaving the original
    // playable unchanged from the same installation. Without the folder
    // nothing changes; CE does not create it.
    if (tig_file_is_directory(GAMELIB_CE_DATA_DIR)) {
        tig_file_repository_add(GAMELIB_CE_DATA_DIR);
        tig_debug_printf("gamelib: CE's own data layer %s mounted above data\n", GAMELIB_CE_DATA_DIR);
    }

    // CE (release 1, R4): the enabled mod bundles, above `ce-data`.
    ce_mods_mount();
}

// 0x404C10
bool gamelib_load_module_data(const char* module_name)
{
    char path1[TIG_MAX_PATH];
    char path2[TIG_MAX_PATH];
    size_t end;
    int index;

    if (module_name[0] == '\\' || module_name[0] == '.' || module_name[1] == ':') {
        path1[0] = '\0';
    } else {
        strcpy(path1, ".\\Modules\\");
    }

    strcat(path1, module_name);
    end = strlen(path1);

    strcat(path1, ".dat");
    if (tig_file_exists(path1, NULL)) {
        if (!tig_file_repository_add(path1)) {
            return false;
        }

        strcpy(gamelib_mod_dat_path, path1);

        path1[end] = '\0';

        for (index = 0; index < GAMELIB_MAX_PATCH_COUNT; index++) {
            snprintf(path2, TIG_MAX_PATH, "%s.patch%d", path1, index);
            if (tig_file_exists(path2, NULL)) {
                if (!tig_file_repository_add(path2)) {
                    return false;
                }

                strcpy(gamelib_mod_patch_paths[index], path2);
            }
        }
    }

    path1[end] = '\0';

    if (tig_file_is_directory(path1)) {
        tig_file_repository_add(path1);
        strcpy(gamelib_mod_dir_path, path1);
        return true;
    }

    if (tig_file_mkdir(path1)) {
        if (gamelib_init_info.editor) {
            if (gamelib_mod_dat_path[0] == '\0') {
                tig_file_copy_directory(path1, "Module template");
            }
        }

        tig_file_repository_add(path1);
        strcpy(gamelib_mod_dir_path, path1);
        return true;
    }

    tig_debug_printf("gamelib_mod_load(): error creating folder %s\n", path1);

    if (gamelib_mod_dat_path[0] != '\0') {
        tig_file_repository_remove(gamelib_mod_dat_path);
    }

    return false;
}

// 0x405070
void gamelib_unload_module_data(void)
{
    int index;

    tig_file_repository_remove(gamelib_mod_dir_path);

    if (byte_5D0B58[0] != '\0') {
        tig_file_repository_remove(byte_5D0B58);
    }

    for (index = GAMELIB_MAX_PATCH_COUNT - 1; index >= 0; index--) {
        if (gamelib_mod_patch_paths[index][0] != '\0') {
            tig_file_repository_remove(gamelib_mod_patch_paths[index]);
            gamelib_mod_patch_paths[index][0] = '\0';
        }
    }

    if (gamelib_mod_dat_path[0] != '\0') {
        tig_file_repository_remove(gamelib_mod_dat_path);
    }

    gamelib_mod_dir_path[0] = '\0';
    byte_5D0B58[0] = '\0';
    gamelib_mod_dat_path[0] = '\0';
    byte_5D0EA4[0] = '\0';

    memset(&gamelib_mod_guid, 0, sizeof(gamelib_mod_guid));

    dword_5D10C4 = false;
}
