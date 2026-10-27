#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_WINDOWS
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <tig/tig.h>

#include "ce_version.h"
#include "game/ai.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/critter.h"
#include "game/descriptions.h"
#include "game/determinism.h"
#include "game/dialog.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/hrp.h"
#include "game/install_check.h"
#include "game/item.h"
#include "game/level.h"
#include "game/light_scheme.h"
#include "game/location.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/name.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/path.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/roof.h"
#include "game/script.h"
#include "game/scroll.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/tech.h"
#include "game/uap.h"
#include "game/wallcheck.h"
#include "ui/anim_ui.h"
#include "ui/charedit_ui.h"
#include "ui/dialog_ui.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/iso.h"
#include "ui/logbook_ui.h"
#include "ui/mainmenu_ui.h"
#include "ui/replay_ui.h"
#include "ui/textedit_ui.h"
#include "ui/wmap_rnd.h"
#include "ui/wmap_ui.h"

static void main_loop(void);
static void handle_mouse_scroll(void);
static void handle_keyboard_scroll(void);
static void build_cmd_line(char* dst, size_t size, int argc, char** argv);

// 0x59A040
static float gamma = 1.0f;

// 0x5CFF00
static int saved_light_scheme;

// CE: the reproducible-run harness. Not in the original.
//
// `-autoload` quick-loads the newest save instead of showing the menu;
// `-autoload:Slot0001` selects that exact slot identity instead. A missing or
// ambiguous exact slot is a harness error, never a timestamp fallback.
// `-screenshot=N` writes a screenshot at frame N through the shipped F12 path.
// `-runframes=N` quick-saves and quits after N frames of `main_loop`. With
// `-deterministic` that is a complete scripted run: same save in, same number
// of frames, same save out - which is what makes two CE builds comparable
// byte for byte without Wine, a patched binary, or synthetic input.
//
// Bare `-autoload` selection is by mtime, the same contract
// `mainmenu_ui_quick_load` already has and the same one
// `harness/make-save-pair.sh` already drives by touching the save it wants.
// Nothing new is exported for either form.
static bool harness_autoload;
static bool harness_autoload_exact;
static char harness_autoload_slot[TIG_MAX_PATH];
// CE (release 1, R7): `-newgame[:N]` starts a new game with pregenerated
// character N (default 1) instead of showing the menu - the release check's
// "a fresh install starts a new game" with no input at all.
static int harness_newgame;
static int harness_run_frames;
static int harness_frames_run;
// CE: `-runframes` ends the run on the player's death with this exit code and
// no save; 1 is taken by observatory's unresolved-replay failure.
#define HARNESS_EXIT_PLAYER_DIED 3
static int harness_exit_code = EXIT_SUCCESS;

// CE (card O2): `-untildone[=N]` ends the run when the work is done instead of
// at a frame count nobody can pick correctly.
//
// Measured 2026-09-15: a scripted duel on the O1 fixture finished in about two
// minutes of an eleven-minute `-runframes=40000` budget, and the rest was dead
// frames - the loop is vsync-paced, so those minutes are spent waiting rather
// than computing. The margin was paid on every run of a comparison batch.
//
// Three conditions, all of which must hold:
//
//   the duel ended       `combat_observer_terminal_seen`, so this needs
//                        `--combat-observe`; without the observer the engine
//                        has no terminal signal and the switch refuses.
//   the script is done   a replay still holding commands has not finished, and
//                        exiting would turn "it stalled" into "it completed".
//   the trace is quiet   no event written for N frames. Watching the writer
//                        stop beats guessing a death animation's length, and it
//                        is what keeps the tail of the trace intact: O1 refuses
//                        a truncated run, so an eager exit would trade eleven
//                        minutes for a run that cannot be compared at all.
//
// Unlike `-runframes` this does **not** quick-save on the way out. The save is
// what a comparison run has to undo afterwards, and nothing here needs it.
static int harness_until_done;
static int harness_quiet_frames = 120;
static unsigned int harness_last_event_count;
static int harness_last_event_frame;
// CE: `-screenshot=N` fires the game's own F12 path at a fixed FRAME.
// A wall-clock screenshot is not reproducible: the frame differential
// (roadmap P5.5) measured two shots taken seconds apart in one in-game run
// differing by 16,743 vs 16,740 distinct colours, because the world had
// simulated in between. `-deterministic` does not help - it pins the RNG,
// the GUID source, the frame clock and the pointer, none of which decides
// *when* somebody presses a key. Pinning the frame number is what makes an
// in-game frame comparable across runs.
static int harness_shot_frame;
// CE (2026-09-28): `-screenshot=N-M` shoots every frame from N through M, to
// catch an effect that lasts a handful of frames - a hit's eye candy - where
// one frame number would have to be guessed exactly.
static int harness_shot_last_frame;

// 0x401000
int main(int argc, char** argv)
{
    TigInitInfo init_info;
    TigVideoScreenshotSettings screenshotter;
    GameInitInfo game_init_info;
    const HighResConfig* highres_config;
    char* pch;
    int value;
    tig_art_id_t cursor_art_id;
    int64_t pc_starting_location;
    char msg[80];
    const char* combat_observer_path = NULL;
    const char* replay_path = NULL;

    // CE (card S1): `-features` prints what this binary was built with and
    // exits, before anything is initialised, so it needs no display and no
    // installation.
    //
    // **The toolkit reads `feature_list.h` to learn what features exist, and
    // that is the source, not the thing that will run.** A mod declaring
    // `requires_features` was therefore validated against a header rather than
    // against the engine the player has - and a profile can trivially carry an
    // older binary than the tree it was built from, which has misled this card
    // three times. This is the probe the plan asks for: identify the build, or
    // ask it what it supports.
    for (value = 1; value < argc; value++) {
        // CE (release 1, R5): the install check, before anything starts.
        if (strcmp(argv[value], "-check-install") == 0
            || strcmp(argv[value], "-check-install-fix") == 0) {
            return install_check_run(strcmp(argv[value], "-check-install-fix") == 0);
        }
        if (strcmp(argv[value], "-features") == 0) {
            printf("schema 1\n");
            for (int index = 0; index < FEATURE_COUNT; index++) {
                printf("%s\t%s\n", feature_key(index),
                    feature_default(index) ? "default-on" : "default-off");
            }
            return EXIT_SUCCESS;
        }
    }

    // CE (release 1, R9): the log's first line names the build, so a log
    // attached to a bug report says what wrote it (`log file`).
    tig_debug_printf("Arcanum CE %s\n", ARCANUM_CE_VERSION);

    // CE: the private observatory passes an absolute per-run output path.
    // Read argv directly so the legacy lower-casing of lpCmdLine below cannot
    // corrupt a case-sensitive Linux path. The capability is absent unless
    // this exact switch is present.
    //
    // CE (card O2): `-replay=` is read here for the same reason, and it was
    // not until 2026-09-15. Taken from `lpCmdLine` it could not open any path
    // containing a capital letter - the file simply did not exist under the
    // lower-cased name - and `lpCmdLine` is 260 bytes, so a long absolute path
    // was truncated as well. Neither showed up when the switch was built,
    // because the script it was tested with happened to sit under an
    // all-lowercase path.
    for (int arg = 1; arg < argc; arg++) {
        const char* prefix = "--combat-observe=";
        if (strncmp(argv[arg], prefix, strlen(prefix)) == 0) {
            combat_observer_path = argv[arg] + strlen(prefix);
        }
        prefix = "-replay=";
        if (strncmp(argv[arg], prefix, strlen(prefix)) == 0) {
            replay_path = argv[arg] + strlen(prefix);
        }
    }

#if SDL_PLATFORM_MACOS
    chdir(SDL_GetBasePath());
#elif SDL_PLATFORM_IOS
    chdir(SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS));
#elif SDL_PLATFORM_ANDROID
    chdir(SDL_GetPrefPath("com.alexbatalov", "arcanumce"));
#endif

    // Convert args array to WinMain-like lpCmdLine.
    char lpCmdLine[260];
    build_cmd_line(lpCmdLine, sizeof(lpCmdLine), argc, argv);

    highres_config_load();
    highres_config = highres_config_get();

    init_info.texture_width = 1024;
    init_info.texture_height = 1024;
    init_info.flags = 0;

    if (highres_config->show_fps) {
        init_info.flags |= TIG_INITIALIZE_FPS;
    }

    pch = lpCmdLine;
    while (*pch != '\0') {
        *pch = (char)(unsigned char)SDL_tolower(*(unsigned char*)pch);
        pch++;
    }

    if (strstr(lpCmdLine, "-fps") != NULL) {
        init_info.flags |= TIG_INITIALIZE_FPS;
    }

    if (strstr(lpCmdLine, "-nosound") != NULL) {
        init_info.flags |= TIG_INITIALIZE_NO_SOUND;
    }

    pch = strstr(lpCmdLine, "-vidfreed");
    if (pch != NULL) {
        value = atoi(pch + 8);
        if (value > 0) {
            tig_art_cache_set_video_memory_fullness(value);
        }
    }

    if (strstr(lpCmdLine, "-animcatchup") != NULL) {
        anim_catch_up_enable();
    }

    if (strstr(lpCmdLine, "-norandom") != NULL) {
        wmap_rnd_disable();
    }

    // CE: `-deterministic` / `-deterministic=SEED` pins the RNG, the GUID
    // source and the frame clock for this run, so two runs of the same save
    // can be compared byte for byte. See game/determinism.h. A harness wants
    // this per run rather than written into the player's arcanum.cfg, which
    // is why it is a switch as well as a setting.
    pch = strstr(lpCmdLine, "-deterministic");
    if (pch != NULL) {
        unsigned int seed = 0;

        if (pch[strlen("-deterministic")] == '=') {
            seed = (unsigned int)strtoul(pch + strlen("-deterministic") + 1, NULL, 10);
        }

        determinism_request(seed);
    }

    pch = strstr(lpCmdLine, "-autoload");
    if (pch != NULL) {
        harness_autoload = true;
        if (pch[strlen("-autoload")] == ':') {
            const char* slot = pch + strlen("-autoload:");
            size_t slot_len = strcspn(slot, " \t\"");

            harness_autoload_exact = true;
            if (slot_len >= sizeof(harness_autoload_slot)) {
                slot_len = sizeof(harness_autoload_slot) - 1;
            }
            memcpy(harness_autoload_slot, slot, slot_len);
            harness_autoload_slot[slot_len] = '\0';
        }
    }

    pch = strstr(lpCmdLine, "-newgame");
    if (pch != NULL) {
        harness_newgame = 1;
        if (pch[strlen("-newgame")] == ':') {
            harness_newgame = atoi(pch + strlen("-newgame:"));
        }
    }

    // CE (card O2): restore a random state captured at a previous run's Ready
    // barrier, so a replay proceeds from the state the recorded run proceeded
    // from rather than from whatever this process happened to load with.
    pch = strstr(lpCmdLine, "-rngstate=");
    if (pch != NULL) {
        determinism_restore_request(
            (unsigned int)strtoul(pch + strlen("-rngstate="), NULL, 10));
    }

    // CE (card O2): `-replay=<file>` re-submits a recorded run's commands.
    // The path comes from argv, above, so it keeps its case and its length.
    if (replay_path != NULL) {
        replay_ui_load(replay_path);
    }

    pch = strstr(lpCmdLine, "-screenshot=");
    if (pch != NULL) {
        char* end;

        harness_shot_frame = (int)strtol(pch + strlen("-screenshot="), &end, 10);
        harness_shot_last_frame = *end == '-'
            ? (int)strtol(end + 1, NULL, 10)
            : harness_shot_frame;
    }

    pch = strstr(lpCmdLine, "-runframes=");
    if (pch != NULL) {
        harness_run_frames = (int)strtol(pch + strlen("-runframes="), NULL, 10);
        anim_ui_death_ends_run = harness_run_frames > 0;
    }

    // CE (card O2): `-untildone[=N]`, N being the quiet-frame count. Two
    // seconds at sixty frames by default, which is comfortably longer than the
    // gap between events inside a turn.
    pch = strstr(lpCmdLine, "-untildone");
    if (pch != NULL) {
        harness_until_done = 1;
        if (pch[strlen("-untildone")] == '=') {
            harness_quiet_frames = (int)strtol(pch + strlen("-untildone") + 1,
                NULL, 10);
        }
    }

    mainmenu_ui_female_chars_missing_clear();

    // NOTE: These were original switches to set cheat level. They were removed
    // from Arcanum release version.
    if (strstr(lpCmdLine, "-2680") != NULL) {
        gamelib_cheat_level_set(1);
    }

    if (strstr(lpCmdLine, "-0897") != NULL) {
        gamelib_cheat_level_set(2);
    }

    if (strstr(lpCmdLine, "-4637") != NULL) {
        gamelib_cheat_level_set(3);
    }

    pch = strstr(lpCmdLine, "-pathlimit");
    if (pch != NULL) {
        value = atoi(pch + 10);
        if (value > 0) {
            path_set_limit(value);
        }
    }

    pch = strstr(lpCmdLine, "-pathtimelimit");
    if (pch != NULL) {
        value = atoi(pch + 14); // FIX: Length was wrong (10).
        if (value > 0) {
            path_set_time_limit(value);
        }
    }

    if (strstr(lpCmdLine, "-fullscreen") != NULL) {
        intgame_set_fullscreen();
        intgame_toggle_interface();
    }

    pch = strstr(lpCmdLine, "-patchlvl");
    if (pch != NULL) {
        gamelib_patch_lvl_set(pch + 9);
    }

    // CE: where the geometry came from, tracked so it can be reported below.
    const char* geometry_source = highres_config->loaded
        ? "HighRes/config.ini"
        : "built-in default";

    init_info.width = highres_config->width;
    init_info.height = highres_config->height;
    init_info.bpp = 32;
    init_info.art_file_path_resolver = name_resolve_path;
    init_info.art_id_reset_func = name_normalize_aid;
    init_info.sound_file_path_resolver = gsound_resolve_path;

    if (highres_config->windowed) {
        init_info.flags |= TIG_INITIALIZE_WINDOWED;
    }

    // NOTE: The `window` switch is borrowed from ToEE.
    if (strstr(lpCmdLine, "-window") != NULL) {
        init_info.flags |= TIG_INITIALIZE_WINDOWED;
    }

    // NOTE: The `geometry` switch is also borrowed from ToEE, but the
    // implementation is different (original implementaion is wrong).
    pch = strstr(lpCmdLine, "-geometry");
    if (pch != NULL) {
        int width;
        int height;

        if (sscanf(pch + 10, "%dx%d", &width, &height) == 2) {
            init_info.width = width;
            init_info.height = height;
            geometry_source = "-geometry";
        }
    }

    // CE: say which of the three sources won, and what it produced.
    //
    // Geometry can arrive from the built-in default, from HighRes/config.ini
    // parsed at startup, or from `-geometry` on the command line, and until
    // this line existed the only way to learn which had won was to take a
    // screenshot and read its header. That cost real time in the P5.5 frame
    // differential, where CE rendered 1280x720 against the original's 800x600
    // and neither engine said so.
    //
    // `-geometry` is the authority when present: it is applied last and
    // overrides the file. The original has no equivalent - the High Resolution
    // Patch bakes width and height into the exe with WeiDU at install time, so
    // for the original `config.ini` is documentation rather than
    // configuration, and editing it changes nothing until the patch is
    // reinstalled. CE reading the same file at runtime is deliberately NOT
    // bit-compatible with that model; matching it would mean importing an
    // ambiguity CE does not have.
    tig_debug_printf("Geometry: %dx%d %s (from %s)\n",
        init_info.width,
        init_info.height,
        (init_info.flags & TIG_INITIALIZE_WINDOWED) != 0 ? "windowed" : "fullscreen",
        geometry_source);

    // Specify window name.
    init_info.flags |= TIG_INITIALIZE_SET_WINDOW_NAME;
    init_info.window_name = "Arcanum: Of Steamworks & Magick Obscura - Community Edition";

    if (tig_init(&init_info) != TIG_OK) {
        // NOTE: Verified against original main 0x401000 - the original falls through to its final `return 0`; transcription faithful.
        return EXIT_SUCCESS;
    }

    // CE (real-time session B): `-vsyncinterval=N` presents every Nth screen
    // refresh, so N = 2 runs a 60 Hz display at 30 fps. The loop is paced by the
    // display, and today's real-time pace depends on it; the action budget is
    // meant not to, and this is how that is measured. Harness-only, like
    // `-runframes`: without it the renderer keeps its interval of 1.
    pch = strstr(lpCmdLine, "-vsyncinterval=");
    if (pch != NULL) {
        SDL_Renderer* renderer;
        int interval = (int)strtol(pch + strlen("-vsyncinterval="), NULL, 10);

        if (interval >= 1 && tig_video_renderer_get(&renderer) == TIG_OK) {
            if (SDL_SetRenderVSync(renderer, interval)) {
                tig_debug_printf("Harness: vsync interval %d\n", interval);
            } else {
                tig_debug_printf("Harness: vsync interval %d refused: %s\n",
                    interval, SDL_GetError());
            }
        }
    }

    screenshotter.key = SDL_SCANCODE_F12;
    screenshotter.field_4 = 0;
    tig_video_screenshot_set_settings(&screenshotter);

    intgame_set_iso_window_width(init_info.width);
    intgame_set_iso_window_height(init_info.height);
    if (!intgame_create_iso_window(&(game_init_info.iso_window_handle))) {
        tig_exit();
        // NOTE: Verified against original main 0x401000 - original calls tig_exit and returns 0; transcription faithful.
        return EXIT_SUCCESS;
    }

    tig_mouse_hide();

    // WorldEd's engine half is in this tree: 24 modules take `init_info->editor`
    // into a per-module static, and `sector.c` swaps in real `sector_load_editor`
    // / `sector_save_editor` implementations rather than stubs. Card A2 exists to
    // measure how much of that actually works, so the flag needs a way to be
    // turned on.
    //
    // **It cannot be an X1 feature flag, and that is a property of the ordering
    // rather than an omission.** `gamelib_init` reads this flag to decide
    // *which settings file to open* (`worlded.cfg` or `arcanum.cfg`,
    // gamelib.c:321) and only then calls `settings_load` and `feature_init`. A
    // feature is read after the load, which is after the decision it would have
    // to make; there is no ordering that breaks the circle. So this is a
    // command-line switch, which is also closer to the original, where WorldEd
    // was a separate binary and no runtime toggle existed at all.
    //
    // Off unless asked for, like every other switch here.
    game_init_info.editor = strstr(lpCmdLine, "-editor") != NULL;
    if (game_init_info.editor) {
        tig_debug_printf("main: -editor given; starting in editor mode\n");
    }
    game_init_info.invalidate_rect_func = iso_invalidate_rect;
    game_init_info.draw_func = iso_redraw;

    if (!gamelib_init(&game_init_info)) {
        tig_window_destroy(game_init_info.iso_window_handle);
        tig_exit();
        // NOTE: Verified against original main 0x401000 - original destroys the window, calls tig_exit, and returns 0; transcription faithful.
        return EXIT_SUCCESS;
    }

    // UAP (0x401118): "Added a '-cheatmode' switch, which enables Troika's
    // original cheat mode, see Documents\Cheat Mode.txt for an explanation on
    // what it does."
    //
    // The patch reuses this handler wholesale - it repoints the string at
    // 0x59A198 from "-animdebug" to "-cheatmode" and the call at 0x401118 from
    // `anim_debug_enable` to `gamelib_cheat_level_set` - so `-animdebug` is
    // gone from the patched build rather than sitting beside the new switch.
    //
    // NOTE: In the original binary `gamelib_cheat_level_set` (0x402CC0) stores
    // a hardcoded 0 and `gamelib_cheat_level_get` (0x402CB0) is `xor eax, eax`,
    // which is how Troika disabled cheat mode in the release build. UAP patches
    // both back (0x402CC0's immediate becomes 3, 0x402CB0 reads the global);
    // nothing is needed here because CE already implements the unstubbed pair,
    // which is why CE's own `-2680`/`-0897`/`-4637` switches work where
    // 1.0.7.4's do not. See HANDOFF, *UAP: what it changes, and where*.
    //
    // FIX: this sat with the other switches ~230 lines above, where
    // `uap_fixes_enabled()` is **always false** whatever `arcanum.cfg` says.
    // The feature table holds its compiled defaults until `gamelib_init` calls
    // `settings_load` and `feature_init`, so the gate was read before the file
    // that sets it. `-cheatmode` silently did nothing and `-animdebug` was
    // always the live branch. Found 2026-09-20 when `J` granted no character
    // points on a profile with `uap fixes=1`.
    //
    // Moved rather than gated differently, and it has to be after the whole of
    // `gamelib_init` rather than merely after `settings_load`. Nothing between
    // the old site and here reads either flag: `gamelib_cheat_level` is a
    // plain static nothing initialises, and `anim_debug_enable` is empty in
    // the original too (0x4232F0), so the `else` arm is a no-op wherever it
    // runs.
    if (uap_fixes_enabled()) {
        if (strstr(lpCmdLine, "-cheatmode") != NULL) {
            gamelib_cheat_level_set(3);
            tig_debug_printf("main: -cheatmode given; cheat level %d\n",
                gamelib_cheat_level_get());
        }
    } else {
        if (strstr(lpCmdLine, "-animdebug") != NULL) {
            anim_debug_enable();
        }
    }

    if (combat_observer_path != NULL
        && combat_observer_open(combat_observer_path)) {
        atexit(combat_observer_close);
    }

    // CE (card O2): `-untildone` reads the duel's end from the observer, so
    // without one it has nothing to wait for. Refused loudly rather than
    // silently degrading to "run forever", which is the failure a harness would
    // notice last.
    if (harness_until_done && !combat_observer_is_enabled()) {
        tig_debug_printf("Harness: -untildone needs --combat-observe;"
                         " ignoring it\n");
        harness_until_done = 0;
    }

    if (strstr(lpCmdLine, "-dialogcheck") != NULL) {
        dialog_check();
    }

    if (strstr(lpCmdLine, "-dialognumber") != NULL) {
        dialog_enable_numbers();
    }

    if (strstr(lpCmdLine, "-gendercheck") != NULL) {
        map_enable_gender_check();
    }

    pch = strstr(lpCmdLine, "-scrollfps:");
    if (pch != NULL) {
        value = atoi(pch + 11);
        scroll_fps_set(value);
    } else {
        scroll_fps_set(highres_config->scroll_fps);
    }

    pch = strstr(lpCmdLine, "-scrolldist:");
    if (pch != NULL) {
        value = atoi(pch + 12);
        scroll_distance_set(value);
    } else {
        scroll_distance_set(highres_config->scroll_dist);
    }

    pch = strstr(lpCmdLine, "-mod:");
    if (pch != NULL) {
        gamelib_default_module_name_set(pch + 5);
    }

    pch = strstr(lpCmdLine, "-roundtrip:");
    if (pch != NULL) {
        // 2026-08-21: headless savegame round-trip, handled by
        // mainmenu_ui_roundtrip_run() just before main_loop(); spec is
        // `SLOT[=OUTSLOT]`. See arcanum-work SAVE-ROUNDTRIP.md.
        //
        // The spec may be QUOTED, and it has to be able to be: ten of this
        // project's named saves have a space in them ("Slot0008maintain
        // spell"), and stopping at whitespace truncated the name, dropped the
        // `=OUTSLOT` half with it, and then ran the EXTRACT path instead -
        // succeeding, with a different meaning, and no complaint. Found
        // 2026-08-30 trying to round-trip the one save that exercises
        // MagicTech-Post.
        char roundtrip_spec[TIG_MAX_PATH];
        const char* roundtrip_arg = pch + 11;
        size_t roundtrip_len;

        if (*roundtrip_arg == '"') {
            roundtrip_arg++;
            roundtrip_len = strcspn(roundtrip_arg, "\"");
        } else {
            roundtrip_len = strcspn(roundtrip_arg, " \t");
        }

        if (roundtrip_len >= sizeof(roundtrip_spec)) {
            roundtrip_len = sizeof(roundtrip_spec) - 1;
        }
        memcpy(roundtrip_spec, roundtrip_arg, roundtrip_len);
        roundtrip_spec[roundtrip_len] = '\0';
        mainmenu_ui_roundtrip_set(roundtrip_spec);
    }

    tig_art_interface_id_create(0, 0, 0, 0, &cursor_art_id);
    tig_mouse_cursor_set_art_id(cursor_art_id);

    if (!gameuilib_init(&game_init_info)) {
        gameuilib_exit();
        tig_window_destroy(game_init_info.iso_window_handle);
        tig_exit();
        // NOTE: Verified against original main 0x401000 - original runs this same three-call cleanup and returns 0; transcription faithful.
        return EXIT_SUCCESS;
    }

    if (!gamelib_mod_load(gamelib_default_module_name_get())) {
        tig_debug_printf("Error loading default module %s\n",
            gamelib_default_module_name_get());
        // NOTE: Verified against original main 0x401000 - original prints this error and returns 0 without any further shutdown; transcription faithful.
        return EXIT_SUCCESS;
    }

    if (!gameuilib_mod_load()) {
        tig_debug_printf("Error loading UI module data\n");
        // NOTE: Verified against original main 0x401000 - original prints this error and returns 0 without any further shutdown; transcription faithful.
        return EXIT_SUCCESS;
    }

    if (highres_config->intro) {
        gmovie_play(8, GAME_MOVIE_NO_FINAL_FLIP, 0);
    }

    // CE: `-autoload` makes the menu load the newest save immediately instead
    // of waiting for a click. Its `:SLOT` form resolves one exact slot. The
    // faithful path is unchanged when it is off.
    mainmenu_ui_autoload_set(harness_autoload);
    if (harness_autoload_exact) {
        mainmenu_ui_autoload_slot_set(harness_autoload_slot);
    }
    mainmenu_ui_newgame_set(harness_newgame);

    // 2026-08-21: headless savegame round-trip (SAVE-ROUNDTRIP.md in
    // arcanum-work). No-op unless `-roundtrip:` was given. Runs at the exact
    // Continue-click state - module data mounted, UI modules initialized,
    // modal menu not yet entered - and never returns on a round-trip run.
    mainmenu_ui_roundtrip_run();

    if (!mainmenu_ui_handle()) {
        gameuilib_exit();
        gamelib_exit();
        tig_exit();
        tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);
        // NOTE: Verified against original main 0x401000 - original runs this same cleanup and returns 0; transcription faithful.
        return EXIT_SUCCESS;
    }

    if (strstr(lpCmdLine, "-logcheck") != NULL) {
        logbook_ui_check();
    }

    tig_debug_printf("[Beginning Game]\n");

    pc_starting_location = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    snprintf(msg, sizeof(msg),
        "Player Start Position: x: %d, y: %d",
        (int)LOCATION_GET_X(pc_starting_location),
        (int)LOCATION_GET_Y(pc_starting_location));
    tig_debug_printf("%s\n", msg);

    main_loop();

    // CE (card O2): how many frames the run actually took.
    //
    // `main_loop` has six returns and this is the one place past all of them,
    // so the count is reported once however the run ended. It is cheap and it
    // is two different things at once:
    //
    //   a budget   `-runframes` was picked by guesswork before this existed,
    //              and the margin was paid on every run of a batch.
    //   a signal   a replay that needed materially more frames than the run it
    //              reproduces has diverged, and says so here without anyone
    //              having to diff two event streams to find out.
    tig_debug_printf("Harness: frames run=%d\n", harness_frames_run);

    // CE (card O2): a replay that ends with commands outstanding did not
    // reproduce its recording. Say so on the way out, so the run record carries
    // it rather than a reader inferring it from an absent event.
    if (replay_ui_active()) {
        tig_debug_printf("Harness: replay outstanding=%d\n", replay_ui_remaining());
    }

    combat_observer_close();

    gameuilib_mod_unload();
    gamelib_mod_unload();
    gameuilib_exit();
    gamelib_exit();
    tig_exit();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);

    return harness_exit_code;
}

// 0x401560
void main_loop(void)
{
    int64_t location;
    int64_t pc_obj;
    tig_art_id_t art_id;
    bool enable_profiler = false;
    bool disable_profiler = false;
    bool output_profile_data = false;
    TigMessage message;
    int index;
    TigMouseState mouse_state;
    int64_t mouse_loc;
    char version_str[80];
    char mouse_state_str[20];
    char story_state_str[80];

    // CE: the original places the PC on the tile under screen point (400,200)
    // and returns if there is none (0x401570-0x4015bf); CE keeps the PC on
    // its own location. TODO: Figure out if this is really needed.
#if 0
    if (!location_at(400, 200, &location)) {
        return;
    }
#endif

    pc_obj = player_get_local_pc_obj();
    location = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
    object_move_to(pc_obj, location, 0, 0);
    location_origin_set(location);

    art_id = obj_field_int32_get(pc_obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_frame_set(art_id, 0);
    art_id = tig_art_id_anim_set(art_id, 0);
    object_set_current_aid(pc_obj, art_id);

    object_flags_unset(pc_obj, OF_OFF);
    anim_fidget_schedule();
    iso_interface_refresh();
    intgame_draw_bar(INTGAME_BAR_HEALTH);
    intgame_draw_bar(INTGAME_BAR_FATIGUE);

    while (1) {
        if (enable_profiler) {
            intgame_message_window_display_str(-1, "Enabling profiler...\n");
            enable_profiler = false;
        }

        if (disable_profiler) {
            intgame_message_window_display_str(-1, "Disabling profiler...\n");
            disable_profiler = false;
        }

        if (output_profile_data) {
            intgame_message_window_display_str(-1, "Outputing profile data...\n");
            output_profile_data = false;
        }

        tig_ping();
        gamelib_ping();
        iso_redraw();
        tig_window_display();

        // CE: counted after the frame is displayed, so N means "N frames ran"
        // for both switches below, and `-screenshot=N -runframes=N` shoots the
        // last frame of the run. The counter used to live inside the
        // `-runframes` branch; it is unconditional now so `-screenshot` can
        // share it. The comparison is unchanged, so `-runframes` behaves
        // exactly as before.
        harness_frames_run++;

        // CE (card O2): the interaction mode, sampled before the replay acts
        // on it, so a change the player did not ask for is recorded as such.
        intgame_mode_observe();

        // CE (card O2): `-replay=<file>`. Submit the next recorded command if
        // the actor is idle. After the frame is displayed, so the actor's goal
        // state reflects a completed frame rather than one in progress.
        replay_ui_tick();

        // CE: `-screenshot=N`. Writes modules/<module>/screenNNNN.bmp through
        // the shipped F12 path - the same `tig_video_screenshot_make` the key
        // handler calls, so this captures what the engine actually presented
        // rather than a second, harness-only rendering of it.
        if (harness_shot_frame > 0
            && harness_frames_run >= harness_shot_frame
            && harness_frames_run <= harness_shot_last_frame) {
            tig_debug_printf("Harness: screenshot at frame %d\n",
                harness_frames_run);
            tig_video_screenshot_make();
        }

        // CE (card O2): `-untildone`. The natural end of the run, as opposed
        // to `-runframes` below, which is the cap that catches a duel that
        // never finishes. Both can be given, and should be.
        if (harness_until_done) {
            unsigned int events = combat_observer_event_count();

            if (events != harness_last_event_count) {
                harness_last_event_count = events;
                harness_last_event_frame = harness_frames_run;
            }

            if (combat_observer_terminal_seen()
                && replay_ui_remaining() == 0
                && harness_frames_run - harness_last_event_frame
                    >= harness_quiet_frames) {
                tig_debug_printf("Harness: duel ended, trace quiet for %d"
                                 " frames, exiting at frame %d\n",
                    harness_quiet_frames, harness_frames_run);
                return;
            }
        }

        // CE: `-runframes`. A dead player would otherwise reset to the main
        // menu, where this loop stops and the run hangs until killed.
        if (anim_ui_player_died) {
            tig_debug_printf("Harness: the player died at frame %d,"
                             " exiting without a save\n",
                harness_frames_run);
            harness_exit_code = HARNESS_EXIT_PLAYER_DIED;
            return;
        }

        // CE: `-runframes=N`. The save is taken from the same point every time.
        if (harness_run_frames > 0) {
            if (harness_frames_run >= harness_run_frames) {
                tig_debug_printf("Harness: %d frames run, quick-saving\n",
                    harness_frames_run);
                gamelib_save("SlotAuto", "Auto-Save");
                tig_debug_printf("Harness: saved, exiting\n");
                return;
            }
        }

        pc_obj = player_get_local_pc_obj();

        while (tig_message_dequeue(&message) == TIG_OK) {
            if (message.type == TIG_MESSAGE_QUIT
                && mainmenu_ui_confirm_quit() == TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
                mainmenu_ui_reset();
                return;
            }

            if (message.type == TIG_MESSAGE_REDRAW) {
                gamelib_redraw();
            }

            intgame_process_event(&message);

            if (gameuilib_wants_mainmenu()) {
                mainmenu_ui_start(MM_TYPE_DEFAULT);
                if (!mainmenu_ui_handle()) {
                    return;
                }
            }

            if (intgame_mode_supports_scrolling(intgame_mode_get())) {
                handle_keyboard_scroll();
            }

            if (message.type == TIG_MESSAGE_KEYBOARD) {
                // CE: Toggle highlight mode when Left Alt is pressed.
                if (message.data.keyboard.scancode == SDL_SCANCODE_LALT) {
                    object_highlight_mode_set(message.data.keyboard.pressed);
                }

                if (!message.data.keyboard.pressed) {
                    switch (message.data.keyboard.scancode) {
                    case SDL_SCANCODE_ESCAPE: {
                        // CE: outside the main, dialog and barter modes Escape
                        // returns to the main mode; the original opens the
                        // menu from any mode (0x401705-0x401752).
                        IntgameMode esc_mode = intgame_mode_get();
                        if (esc_mode != INTGAME_MODE_MAIN
                            && esc_mode != INTGAME_MODE_DIALOG
                            && esc_mode != INTGAME_MODE_BARTER) {
                            intgame_mode_set(INTGAME_MODE_MAIN);
                            break;
                        }
                        if (dialog_ui_is_local_pc_in_dialog()
                            || wmap_ui_is_created()
                            || (combat_turn_based_is_active()
                                && player_get_local_pc_obj() != combat_turn_based_whos_turn_get())) {
                            mainmenu_ui_start(MM_TYPE_IN_PLAY_LOCKED);
                        } else {
                            mainmenu_ui_start(MM_TYPE_IN_PLAY);
                        }
                        if (!mainmenu_ui_handle()) {
                            return;
                        }
                        break;
                    }
                    case SDL_SCANCODE_F10:
                        intgame_toggle_interface();
                        tig_debug_printf("iso_redraw...");
                        iso_redraw();
                        tig_debug_printf("tig_window_display...");
                        tig_window_display();
                        tig_debug_printf("completed.\n");
                        break;
                    case SDL_SCANCODE_O:
                        if (!textedit_ui_is_focused()) {
                            mainmenu_ui_start(MM_TYPE_OPTIONS);
                            if (!mainmenu_ui_handle()) {
                                return;
                            }
                        }
                        break;
                    case SDL_SCANCODE_F7:
                        if (!critter_is_dead(player_get_local_pc_obj())) {
                            if (wmap_ui_is_created()) {
                                wmap_ui_close();
                                tig_ping();
                                gamelib_ping();
                                iso_redraw();
                                tig_window_display();
                            }

                            if (!combat_turn_based_is_active() || player_get_local_pc_obj() == combat_turn_based_whos_turn_get()) {
                                intgame_mode_set(INTGAME_MODE_MAIN);
                                intgame_mode_set(INTGAME_MODE_MAIN);
                                mainmenu_ui_feedback_saving();
                                gamelib_save("SlotAuto", "Auto-Save");
                                mainmenu_ui_feedback_saving_completed();
                            } else {
                                mainmenu_ui_feedback_cannot_save_in_tb();
                            }
                        }
                        break;
                    case SDL_SCANCODE_F8:
                    case SDL_SCANCODE_F9: {
                        // UAP (0x401863): "moved Quick Load from F8 to F9 to
                        // prevent accidental presses since Quick Save is F7."
                        // The patch changes the single scancode this case
                        // compares against, so exactly one of the two keys is
                        // live at a time.
                        SDL_Scancode quick_load_key = uap_fixes_enabled()
                            ? SDL_SCANCODE_F9
                            : SDL_SCANCODE_F8;
                        if (message.data.keyboard.scancode != quick_load_key) {
                            break;
                        }

                        mainmenu_ui_feedback_loading();
                        mainmenu_ui_quick_load();
                        mainmenu_ui_feedback_loading_completed();
                        break;
                    }
                    case SDL_SCANCODE_F11:
                        if (gamelib_cheat_level_get() >= 3) {
                            for (index = 0; index < SPELL_COUNT; index++) {
                                spell_add(pc_obj, index, true);
                            }

                            stat_base_set(pc_obj, STAT_INTELLIGENCE, 20);
                            stat_base_set(pc_obj, STAT_WILLPOWER, 20);

                            for (index = 0; index < SPELL_COUNT; index++) {
                                spell_add(pc_obj, index, true);
                            }

                            magictech_cheat_mode_on();

                            critter_fatigue_damage_set(pc_obj, 0);
                            iso_interface_refresh();
                            intgame_draw_bar(INTGAME_BAR_HEALTH);
                            intgame_draw_bar(INTGAME_BAR_FATIGUE);
                        }
                        break;
                    default:
                        break;
                    }

                    if (!textedit_ui_is_focused()) {
                        if (gamelib_cheat_level_get() >= 3) {
                            switch (message.data.keyboard.scancode) {
                            case SDL_SCANCODE_H:
                                timeevent_inc_milliseconds(3600000);
                                break;
                            case SDL_SCANCODE_N:
                                object_hp_damage_set(pc_obj, 0);
                                critter_fatigue_damage_set(pc_obj, 0);
                                object_flags_unset(pc_obj, OF_NO_BLOCK | OF_FLAT);
                                critter_decay_timeevent_cancel(pc_obj);
                                break;
                            case SDL_SCANCODE_GRAVE:
                                stat_base_set(pc_obj, STAT_INTELLIGENCE, 20);
                                for (index = 0; index < 8; index++) {
                                    tech_degree_inc(pc_obj, index);
                                }
                                charedit_refresh();
                                break;
                            case SDL_SCANCODE_P:
                                ai_npc_fighting_toggle();
                                break;
                            case SDL_SCANCODE_J:
                                // CE: twenty unspent character points, for
                                // building a test character without a
                                // playthrough.
                                //
                                // **Points, not skills.** Granting firearms
                                // directly would make a character no rule in
                                // the game can produce; these are spent
                                // through the ordinary character screen, so
                                // what comes out is a legitimate build that
                                // merely arrived early. Firearms accuracy is
                                // `5 * level + 25` capped by Perception
                                // (`skill.c`), and no existing cheat touches
                                // either - F11 gives spells and INT/WIL, the
                                // backtick gives tech *degrees*, which are the
                                // crafting disciplines rather than the skill.
                                //
                                // Behind its own flag, default off, so the
                                // faithful path is unchanged whatever the
                                // cheat level. UAP already lifts the unspent
                                // cap from 56 to 10000 (`stat.c`). The body is
                                // `intgame_debug_character_points`, which a
                                // replay's `key character-points` also calls.
                                intgame_debug_character_points();
                                break;
                            default:
                                break;
                            }
                        }

                        if (gamelib_cheat_level_get() >= 2) {
                            switch (message.data.keyboard.scancode) {
                            case SDL_SCANCODE_D:
                                if (light_scheme_get() == LIGHT_SCHEME_DEFAULT_LIGHTING) {
                                    light_scheme_set(saved_light_scheme, light_scheme_get_hour());
                                } else {
                                    saved_light_scheme = light_scheme_get();
                                    light_scheme_set(LIGHT_SCHEME_DEFAULT_LIGHTING, light_scheme_get_hour());
                                }
                                break;
                            case SDL_SCANCODE_Y:
                                critter_give_xp(pc_obj, level_experience_points_to_next_level(pc_obj));
                                break;
                            case SDL_SCANCODE_4:
                                if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                                    UiMessage ui_message;

                                    ui_message.type = UI_MSG_TYPE_FEEDBACK;
                                    ui_message.str = "Cheater! Here's $1000!";
                                    ui_display_msg(&ui_message);

                                    int64_t gold_obj;
                                    object_create(proto_get_by_description(BP_GOLD),
                                        obj_field_int64_get(pc_obj, OBJ_F_LOCATION),
                                        &gold_obj);
                                    obj_field_int32_set(gold_obj, OBJ_F_GOLD_QUANTITY, 1000);
                                    item_transfer(gold_obj, pc_obj);
                                    ui_refresh_counters();
                                }
                                break;
                            default:
                                break;
                            }
                        }

                        if (gamelib_cheat_level_get() >= 1) {
                            switch (message.data.keyboard.scancode) {
                            case SDL_SCANCODE_V:
                                gamelib_copy_version(version_str, NULL, NULL);
                                if (tig_video_3d_check_hardware() == TIG_OK) {
                                    strcat(version_str, " [hardware renderer");
                                } else {
                                    strcat(version_str, " [software renderer");
                                }

                                if (tig_video_check_gamma_control() == TIG_OK) {
                                    strcat(version_str, " with gamma support]");
                                } else {
                                    strcat(version_str, " without gamma support]");
                                }
                                intgame_message_window_display_str(-1, version_str);
                                break;
                            case SDL_SCANCODE_E:
                                critter_debug_obj(player_get_local_pc_obj());
                                timeevent_debug_lists();
                                magictech_debug_lists();
                                anim_stats();
                                break;
                            case SDL_SCANCODE_X:
                                tig_mouse_get_state(&mouse_state);
                                location_at(mouse_state.x, mouse_state.y, &mouse_loc);
                                snprintf(mouse_state_str, sizeof(mouse_state_str),
                                    "x: %d, y: %d",
                                    (int)LOCATION_GET_X(mouse_loc),
                                    (int)LOCATION_GET_Y(mouse_loc));
                                tig_debug_printf("%s\n", mouse_state_str);
                                intgame_message_window_display_str(-1, mouse_state_str);
                                break;
                            case SDL_SCANCODE_U:
                                snprintf(story_state_str, sizeof(story_state_str),
                                    "Current Story State: %d",
                                    script_story_state_get());
                                intgame_message_window_display_str(-1, story_state_str);
                                break;
                            case SDL_SCANCODE_LEFTBRACKET:
                                switch (object_blit_flags_get()) {
                                case 0:
                                    object_blit_flags_set(TIG_ART_BLT_BLEND_ALPHA_CONST);
                                    break;
                                case TIG_ART_BLT_BLEND_ALPHA_CONST:
                                    object_blit_flags_set(TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S);
                                    break;
                                case TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S:
                                    object_blit_flags_set(0);
                                    break;
                                }
                                break;
                            case SDL_SCANCODE_RIGHTBRACKET:
                                switch (roof_blit_flags_get()) {
                                case 0:
                                    roof_blit_flags_set(TIG_ART_BLT_BLEND_ALPHA_CONST);
                                    break;
                                case TIG_ART_BLT_BLEND_ALPHA_CONST:
                                    roof_blit_flags_set(TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S);
                                    break;
                                case TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S:
                                    roof_blit_flags_set(0);
                                    break;
                                }
                                break;
                            case SDL_SCANCODE_APOSTROPHE:
                                settings_set_value(&settings, SHADOWS_KEY, 1 - settings_get_value(&settings, SHADOWS_KEY));
                                break;
                            case SDL_SCANCODE_BACKSLASH:
                                wallcheck_set_enabled(!wallcheck_is_enabled());
                                break;
                            case SDL_SCANCODE_KP_7:
                                if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                                    disable_profiler = true;
                                }
                                break;
                            case SDL_SCANCODE_KP_8:
                                if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                                    enable_profiler = true;
                                }
                                break;
                            case SDL_SCANCODE_KP_9:
                                if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                                    output_profile_data = true;
                                }
                                break;
                            case SDL_SCANCODE_G:
                                if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                                    gamma = 1.0f;
                                    tig_video_set_gamma(gamma);
                                } else if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                                    if (gamma > 0.1f) {
                                        gamma -= 0.1f;
                                        tig_video_set_gamma(gamma);
                                    }
                                } else {
                                    if (gamma < 1.9f) {
                                        gamma += 0.1f;
                                        tig_video_set_gamma(gamma);
                                    }
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            } else {
                if (mainmenu_ui_is_active()) {
                    if (!mainmenu_ui_handle()) {
                        return;
                    }
                }
            }
        }

        if (intgame_mode_supports_scrolling(intgame_mode_get())) {
            handle_mouse_scroll();
        }
    }
}

// 0x401F50
void handle_mouse_scroll(void)
{
    TigMouseState mouse_state;
    int width;
    int height;
    int tolerance;

    if (!tig_get_active()) {
        scroll_stop();
        return;
    }

    // CE: an 8-pixel band at the iso window's edges starts a scroll; the
    // original scrolls only on the edge pixels themselves, x 0 or 799 and y 0
    // or 599 (0x401f64, 0x401fa1, 0x401f6c, 0x401f7e).
    tig_mouse_get_state(&mouse_state);
    width = hrp_iso_window_width_get();
    height = hrp_iso_window_height_get();
    tolerance = 8;

    if (mouse_state.x < tolerance) {
        if (mouse_state.y < tolerance) {
            scroll_start(SCROLL_DIRECTION_UP_LEFT);
        } else if (mouse_state.y >= height - tolerance) {
            scroll_start(SCROLL_DIRECTION_DOWN_LEFT);
        } else {
            scroll_start(SCROLL_DIRECTION_LEFT);
        }
    } else if (mouse_state.x >= width - tolerance) {
        if (mouse_state.y < tolerance) {
            scroll_start(SCROLL_DIRECTION_UP_RIGHT);
        } else if (mouse_state.y >= height - tolerance) {
            scroll_start(SCROLL_DIRECTION_DOWN_RIGHT);
        } else {
            scroll_start(SCROLL_DIRECTION_RIGHT);
        }
    } else {
        if (mouse_state.y < tolerance) {
            scroll_start(SCROLL_DIRECTION_UP);
        } else if (mouse_state.y >= height - tolerance) {
            scroll_start(SCROLL_DIRECTION_DOWN);
        } else {
            scroll_stop();
        }
    }
}

// 0x402010
void handle_keyboard_scroll(void)
{
    if (tig_kb_is_key_pressed(SDL_SCANCODE_UP)) {
        if (tig_kb_is_key_pressed(SDL_SCANCODE_LEFT)) {
            scroll_start(SCROLL_DIRECTION_UP_LEFT);
        } else if (tig_kb_is_key_pressed(SDL_SCANCODE_RIGHT)) {
            scroll_start(SCROLL_DIRECTION_UP_RIGHT);
        } else {
            scroll_start(SCROLL_DIRECTION_UP);
        }
    } else if (tig_kb_is_key_pressed(SDL_SCANCODE_DOWN)) {
        if (tig_kb_is_key_pressed(SDL_SCANCODE_LEFT)) {
            scroll_start(SCROLL_DIRECTION_DOWN_LEFT);
        } else if (tig_kb_is_key_pressed(SDL_SCANCODE_RIGHT)) {
            scroll_start(SCROLL_DIRECTION_DOWN_RIGHT);
        } else {
            scroll_start(SCROLL_DIRECTION_DOWN);
        }
    } else if (tig_kb_is_key_pressed(SDL_SCANCODE_LEFT)) {
        scroll_start(SCROLL_DIRECTION_LEFT);
    } else if (tig_kb_is_key_pressed(SDL_SCANCODE_RIGHT)) {
        scroll_start(SCROLL_DIRECTION_RIGHT);
    }
}

void build_cmd_line(char* dst, size_t size, int argc, char** argv)
{
    int idx;
    char* src;

    for (idx = 1; idx < argc && size > 1; idx++) {
        if (idx != 1) {
            *dst++ = ' ';
            size--;
        }

        src = argv[idx];
        while (*src != '\0' && size > 1) {
            *dst++ = *src++;
            size--;
        }
    }

    *dst = '\0';
}
