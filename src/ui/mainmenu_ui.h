#ifndef ARCANUM_UI_MAINMENU_UI_H_
#define ARCANUM_UI_MAINMENU_UI_H_

#include "game/context.h"
#include "game/mes.h"
#include "game/timeevent.h"
#include "ui/types.h"

typedef enum MainMenuFont {
    MM_FONT_FLARE12,
    MM_FONT_ARIAL10,
    MM_FONT_MORPH15,
    MM_FONT_MPICONS,
    MM_FONT_COUNT,
} MainMenuFont;

typedef enum MainMenuColor {
    MM_COLOR_WHITE,
    MM_COLOR_RED,
    MM_COLOR_BLUE,
    MM_COLOR_GOLD,
    MM_COLOR_BROWN,
    MM_COLOR_GREEN,
    MM_COLOR_PURPLE,
    MM_COLOR_LIGHTBLUE,
    MM_COLOR_ORANGE,
    MM_COLOR_GRAY,
    MM_COLOR_COUNT,
} MainMenuColor;

typedef enum MainMenuType {
    MM_TYPE_DEFAULT,
    MM_TYPE_1,
    MM_TYPE_IN_PLAY,
    MM_TYPE_IN_PLAY_LOCKED,
    MM_TYPE_OPTIONS,
    MM_TYPE_5,
} MainMenuType;

/**
 * NOTE: This enum is a *compressed* renumbering of the original's, not a
 * transcription of it. The values agree up to `MM_WINDOW_OPTIONS` (6), but the
 * credits window is 25 in the original where it is 16 here - visible as the one
 * differing byte in `mainmenu_ui_mainmenu_no_multiplayer_buttons` against the
 * exe. `MM_WINDOW_26` keeps the original's number in its *name* while holding
 * the value 17.
 *
 * `main_menu_window_info` is compressed to match, so CE is self-consistent; the
 * only cost is that window numbers here cannot be compared against the binary.
 * Recorded because `arcanum-work/scripts/verify_tables.py` flags the resulting
 * byte difference on every run.
 */
typedef enum MainMenuWindowType {
    MM_WINDOW_0,
    MM_WINDOW_1,
    MM_WINDOW_MAINMENU,
    MM_WINDOW_MAINMENU_IN_PLAY,
    MM_WINDOW_MAINMENU_IN_PLAY_LOCKED,
    MM_WINDOW_SINGLE_PLAYER,
    MM_WINDOW_OPTIONS,
    MM_WINDOW_LOAD_GAME,
    MM_WINDOW_SAVE_GAME,
    MM_WINDOW_LAST_SAVE_GAME,
    MM_WINDOW_INTRO,
    MM_WINDOW_PICK_NEW_OR_PREGEN,
    MM_WINDOW_NEW_CHAR,
    MM_WINDOW_PREGEN_CHAR,
    MM_WINDOW_CHAREDIT,
    MM_WINDOW_SHOP,
    MM_WINDOW_CREDITS,
    MM_WINDOW_26,
    MM_WINDOW_COUNT,
} MainMenuWindowType;

extern int mainmenu_ui_mp_char_idx;
extern int mainmenu_ui_mp_chars_count;
extern int64_t* mainmenu_ui_mp_chars;

bool mainmenu_ui_init(GameInitInfo* init_info);
void mainmenu_ui_exit(void);
void mainmenu_ui_start(MainMenuType type);
void mainmenu_ui_dismiss(void);
bool mainmenu_ui_handle(void);
bool mainmenu_ui_is_active(void);
TigWindowModalDialogChoice mainmenu_ui_confirm_quit(void);
void mainmenu_ui_reset(void);

// 2026-08-21: headless savegame round-trip driver (documented in
// arcanum-work SAVE-ROUNDTRIP.md). `spec` is `SLOT[=OUTSLOT]`: with OUTSLOT
// the process loads SLOT then saves OUTSLOT and exits 0; without it the
// process loads SLOT and exits, leaving the restored Save\Current in place
// for the harness to snapshot. Exits 20-23 on failure. A verification seam,
// deliberately unreachable from any menu.
void mainmenu_ui_roundtrip_set(const char* spec);
void mainmenu_ui_roundtrip_frames_set(int frames);
void mainmenu_ui_roundtrip_run(void);

void mainmenu_ui_open(void);
void mainmenu_ui_close(bool back);
MainMenuWindowType mainmenu_ui_pop_window_stack(void);
void mainmenu_ui_push_window_stack(MainMenuWindowType window_type);
bool mainmenu_ui_quick_load(void);

/**
 * CE: arms the reproducible-run harness's autoload.
 *
 * `mainmenu_ui_quick_load` cannot be used for this: it refuses while the menu
 * is active, and the menu is active for the whole of `mainmenu_ui_handle`.
 * The load has to happen from inside that function, after the menu is set up
 * and before its message loop starts - which is where
 * `harness/huddbg-capture.patch` has been patching it in by hand. This makes
 * the same hook a permanent, buildable one.
 *
 * Set by `main` from `-autoload`, optionally with an exact `:SLOT`. Off by
 * default; the menu behaves normally.
 */
void mainmenu_ui_autoload_set(bool enabled);
void mainmenu_ui_autoload_slot_set(const char* slot);
void mainmenu_ui_create_window(void);
void mainmenu_ui_create_window_func(bool should_display);
bool mainmenu_ui_process_callback(TimeEvent* timeevent);
bool mainmenu_ui_return_to_mainmenu(tig_button_handle_t button_handle);
void mainmenu_ui_textedit_focus(char* buffer, int size);
char* mainmenu_ui_textedit_focused_buffer(void);
void mainmenu_ui_exit_game(void);
void mainmenu_ui_progressbar_init(int max_value);
void mainmenu_ui_progressbar_update(int value);
MainMenuWindowInfo* mainmenu_ui_window_info_get(int index);
MainMenuWindowType mainmenu_ui_window_type_get(void);
void mainmenu_ui_feedback_saving(void);
void mainmenu_ui_feedback_saving_completed(void);
void mainmenu_ui_feedback_cannot_save_in_tb(void);
void mainmenu_ui_feedback_loading(void);
void mainmenu_ui_feedback_loading_completed(void);
tig_window_handle_t mainmenu_ui_window_handle_get(void);
void mainmenu_ui_window_type_set(MainMenuWindowType window_type);
mes_file_handle_t mainmenu_ui_mes_file(void);
tig_font_handle_t mainmenu_ui_font(MainMenuFont font, MainMenuColor color);
void mainmenu_ui_button_texts_refresh(void);
void mainmenu_ui_window_stack_set(int* a1, int num);
void mainmenu_ui_key_shortcuts_disable(void);
void mainmenu_ui_key_shortcuts_enable(void);
int mainmenu_ui_key_shortcuts_disable_count_get(void);
void mainmenu_ui_female_chars_missing_clear(void);

#endif /* ARCANUM_UI_MAINMENU_UI_H_ */
