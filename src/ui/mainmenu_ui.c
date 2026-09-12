#include "ui/mainmenu_ui.h"
#include <errno.h>
#include <unistd.h>

#include <stdio.h>

#include "game/area.h"
#include "game/background.h"
#include "game/critter.h"
#include "game/descriptions.h"
#include "game/gamelib.h"
#include "game/gfade.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/hrp.h"
#include "game/item.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/player.h"
#include "game/portrait.h"
#include "game/proto.h"
#include "game/reaction.h"
#include "game/script.h"
#include "game/snd.h"
#include "game/stat.h"
#include "game/teleport.h"
#include "game/timeevent.h"
#include "game/uap.h"
#include "ui/broadcast_ui.h"
#include "ui/charedit_ui.h"
#include "ui/fate_ui.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/inven_ui.h"
#include "ui/iso.h"
#include "ui/logbook_ui.h"
#include "ui/options_ui.h"
#include "ui/schematic_ui.h"
#include "ui/scrollbar_ui.h"
#include "ui/sleep_ui.h"
#include "ui/slide_ui.h"
#include "ui/spell_ui.h"
#include "ui/textedit_ui.h"
#include "ui/wmap_rnd.h"
#include "ui/wmap_ui.h"

typedef enum MainMenuUiNewCharHoverMode {
    MMUI_NEW_CHAR_HOVER_MODE_BACKGROUND,
    MMUI_NEW_CHAR_HOVER_MODE_GENDER,
    MMUI_NEW_CHAR_HOVER_MODE_RACE,
} MainMenuUiNewCharHoverMode;

typedef struct MainMenuAnim {
    /* 0000 */ tig_art_id_t art_id;
    /* 0004 */ int max_frame;
    /* 0008 */ int fps;
    /* 000C */ int x;
    /* 0010 */ int y;
} MainMenuAnim;

static void mainmenu_ui_dismiss_ex(bool exiting);
static TigWindowModalDialogChoice mainmenu_ui_confirm(int num);
static void mainmenu_ui_expand_tabs(char* dst, const char* src);
static void mainmenu_ui_draw_text(char* str, TigRect* rect, tig_font_handle_t font, unsigned int flags);
static void mainmenu_ui_create_mainmenu(void);
static void mainmenu_ui_draw_version(void);
static bool mainmenu_ui_press_mainmenu_in_play(tig_button_handle_t button_handle);
static bool mainmenu_ui_press_mainmenu_in_play_locked(tig_button_handle_t button_handle);
static void mainmenu_ui_create_options(void);
static void mainmenu_ui_options_close_game_screens(void);
static void mainmenu_ui_destroy_options(void);
static bool mainmenu_ui_press_options(tig_button_handle_t button_handle);
static void mainmenu_ui_options_tab_switch(int a1);
static void mainmenu_ui_load_game_create(void);
static void mainmenu_ui_close_game_screens(void);
static void mainmenu_ui_load_game_destroy(void);
static void mainmenu_ui_scrollbar_value_changed(int a1);
static void mainmenu_ui_scrollbar_refresh_rect(TigRect* rect);
static bool mainmenu_ui_load_game_execute(int btn);
static bool mainmenu_ui_load_game_button_pressed(tig_button_handle_t button_handle);
static bool mainmenu_ui_load_game_button_released(tig_button_handle_t button_handle);
static void mainmenu_ui_load_game_mouse_up(int a1, int a2);
static void mainmenu_ui_load_saveinfo_reload(void);
static void mainmenu_ui_load_game_refresh(TigRect* rect);
static void mainmenu_ui_draw_text_ellipsized(char* str, TigRect* rect, tig_font_handle_t font);
static void mainmenu_ui_draw_centered_text(char* str, TigRect* rect, tig_font_handle_t font);
static void mainmenu_ui_draw_right_aligned_text(char* str, TigRect* rect, tig_font_handle_t font);
static void mmUITextWriteCenteredToArray(char* str, TigRect* rects, int cnt, tig_font_handle_t font);
static char* mainmenu_ui_savelist_display_name(int index);
static void mainmenu_ui_load_select_prev(void);
static void mainmenu_ui_load_select_next(void);
static bool mainmenu_ui_load_game_handle_delete(void);
static bool mainmenu_ui_load_game_by_name(const char* name);
static void mainmenu_ui_save_game_create(void);
static void mainmenu_ui_save_game_destroy(void);
static bool mainmenu_ui_save_game_execute(int btn);
static bool mainmenu_ui_save_game_button_pressed(tig_button_handle_t button_handle);
static bool mainmenu_ui_save_game_button_released(tig_button_handle_t button_handle);
static void mainmenu_ui_save_game_mouse_up(int x, int y);
static void mainmenu_ui_save_game_refresh(TigRect* rect);
static void mainmenu_ui_textedit_draw(const char* str, TigRect* rect, tig_font_handle_t font);
static void mainmenu_ui_save_select_prev(void);
static void mainmenu_ui_save_select_next(void);
static void mainmenu_ui_save_saveinfo_reload(void);
static bool mainmenu_ui_save_game_handle_delete(void);
static void mainmenu_ui_last_save_create(void);
static void mainmenu_ui_intro_create(void);
static void mainmenu_ui_credits_create(void);
static void mainmenu_ui_last_save_refresh(TigRect* rect);
static void mainmenu_ui_create_single_player(void);
static void mainmenu_ui_pick_new_or_pregen_create(void);
static void mainmenu_ui_new_char_create(void);
static void mainmenu_ui_new_char_refresh(TigRect* rect);
static void mmUISharedCharRefreshFunc(int64_t obj, TigRect* rect);
static bool mainmenu_ui_new_char_button_released(tig_button_handle_t button_handle);
static bool mainmenu_ui_new_char_next_background(int64_t obj, int* background_ptr);
static bool mainmenu_ui_new_char_prev_background(int64_t obj, int* background_ptr);
static bool mainmenu_ui_new_char_prev_gender(int64_t obj);
static bool mainmenu_ui_new_char_set_gender(int64_t obj, int gender);
static bool mainmenu_ui_new_char_next_gender(int64_t obj);
static bool mainmenu_ui_new_char_prev_race(int64_t obj);
static void mainmenu_ui_new_char_set_race(int64_t obj, int race);
static bool mainmenu_ui_new_char_next_race(int64_t obj);
static bool mainmenu_ui_new_char_button_hover(tig_button_handle_t button_handle);
static bool mainmenu_ui_new_char_button_leave(tig_button_handle_t button_handle);
static void mainmenu_ui_new_char_mouse_idle(int x, int y);
static bool mainmenu_ui_new_char_execute(int btn);
static void mainmenu_ui_pregen_char_create(void);
static void mainmenu_ui_pregen_char_refresh(TigRect* rect);
static bool mainmenu_ui_pregen_char_button_released(tig_button_handle_t button_handle);
static bool mainmenu_ui_pregen_char_execute(int btn);
static void mainmenu_ui_charedit_create(void);
static void mainmenu_ui_charedit_destroy(void);
static bool mainmenu_ui_charedit_button_released(tig_button_handle_t button_handle);
static void mainmenu_ui_charedit_refresh(TigRect* rect);
static void mainmenu_ui_shop_create(void);
static void mainmenu_ui_shop_destroy(void);
static bool mainmenu_ui_shop_button_released(tig_button_handle_t button_handle);
static void mainmenu_ui_shop_refresh(TigRect* rect);
static bool main_menu_button_create(MainMenuButtonInfo* info, int width, int height);
static bool main_menu_button_create_ex(MainMenuButtonInfo* info, int width, int height, unsigned int flags);
static void mainmenu_ui_apply_compact_options(void);
static void mainmenu_ui_refresh_text(tig_window_handle_t window_handle, const char* str, TigRect* rect, unsigned int flags);
static void mainmenu_ui_windows_destroy(void);
static void mainmenu_ui_create_shared_radio_buttons(void);
static bool mainmenu_ui_message_filter(TigMessage* msg);
static void mainmenu_ui_refresh_button_text(int btn, unsigned int flags);
static void mainmenu_ui_anims_start(void);
static void mainmenu_ui_window_cmd(int a1);
static void mmUIWinRefreshScrollBar(void);
static void mainmenu_ui_top_index_set(int a1);
static void mainmenu_ui_textedit_unfocus(void);
static void mainmenu_ui_textedit_on_enter(TextEdit* textedit);
static void mainmenu_ui_textedit_on_change(TextEdit* textedit);
static void mainmenu_ui_feedback(int num);
static void mainmenu_fonts_init(void);
static void mainmenu_fonts_exit(void);
static void mainmenu_ui_female_chars_probe(void);

// 0x5993D0
static int mainmenu_font_nums[MM_FONT_COUNT] = {
    229,
    26,
    27,
    841,
};

// 0x5993E0
static int mainmenu_font_colors[MM_COLOR_COUNT][3] = {
    { 255, 255, 255 },
    { 255, 0, 0 },
    { 0, 0, 255 },
    { 255, 210, 0 },
    { 32, 15, 0 },
    { 0, 255, 0 },
    { 150, 0, 150 },
    { 60, 160, 255 },
    { 255, 128, 0 },
    { 128, 128, 128 },
};

// The multiplayer character list of the original's window types 17 and 22 -
// `multiplayer_load_chars(&mainmenu_ui_mp_chars, &mainmenu_ui_mp_chars_count)`
// fills the pair from `Players\\G_*.mpc`, and the index selects the row the
// arrow keys move and Delete removes. CE keeps the storage (and frees it in
// `mainmenu_ui_exit`) but has neither window, so nothing populates it.
//
// 0x5C3618
int mainmenu_ui_mp_char_idx = -1;

// 0x5C361C
static int mainmenu_ui_pregen_char_cnt = 9;

// Whether the last four pregenerated characters - `BP_VICTORIA_WARRINGTON`
// through `BP_KEREE_MELANGE`, the female ones - are absent from the loaded
// data. `mainmenu_ui_female_chars_probe` probes for Victoria's prototype and her art during init
// and, when both are there, raises `mainmenu_ui_pregen_char_cnt` from 9 to 13
// and the new character screen from 9 buttons to 10. When they are not, this
// stays set and the gender arrows on the new character screen do nothing.
//
// It starts out set, so the probe is a no-op until `mainmenu_ui_female_chars_missing_clear` clears it -
// which `main` does unconditionally at startup, before the UI exists.
//
// 0x5C3620
static bool mainmenu_ui_female_chars_missing = true;

// 0x5C3624
static tig_window_handle_t mainmenu_ui_window_handle = TIG_WINDOW_HANDLE_INVALID;

// 0x5C3628
static TigRect mainmenu_ui_window_rect = { 0, 0, 800, 600 };

// 0x5C3638
static TigRect mainmenu_ui_window_fullscreen_rect = { 0, 0, 800, 600 };

// 0x5C3648
static TigRect mainmenu_ui_window_partial_rect = { 0, 41, 800, 400 };

// 0x5C3658
static tig_window_handle_t mainmenu_ui_top_bar_cover_window_handle = TIG_WINDOW_HANDLE_INVALID;

// 0x5C3660
static TigRect mainmenu_ui_top_bar_cover_rect = { 0, 0, 800, 41 };

// 0x5C3670
static tig_window_handle_t mainmenu_ui_bottom_bar_cover_window_handles[3] = {
    TIG_WINDOW_HANDLE_INVALID,
    TIG_WINDOW_HANDLE_INVALID,
    TIG_WINDOW_HANDLE_INVALID,
};

// 0x5C3680
static TigRect mainmenu_ui_bottom_bar_cover_rects[3] = {
    { 200, 441, 400, 47 },
    { 0, 441, 200, 159 },
    { 600, 441, 200, 159 },
};

// 0x5C36B0
static bool mainmenu_ui_type_flags[6][2] = {
    /*        MM_TYPE_DEFAULT */ { false, true },
    /*              MM_TYPE_1 */ { false, true },
    /*        MM_TYPE_IN_PLAY */ { true, false },
    /* MM_TYPE_IN_PLAY_LOCKED */ { true, false },
    /*        MM_TYPE_OPTIONS */ { true, false },
    /*              MM_TYPE_5 */ { false, false },
};

// 0x5C36E0
static MainMenuButtonInfo mainmenu_ui_mainmenu_in_play_buttons[] = {
    { 410, 143, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_SAVE_GAME, 0, 0, { 0 }, -1 },
    { 410, 193, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_LOAD_GAME, 0, 0, { 0 }, -1 },
    { 410, 243, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_OPTIONS, 0, 0, { 0 }, -1 },
    { 410, 293, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 410, 343, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
};

// 0x5C37D0
static MainMenuButtonInfo mainmenu_ui_mainmenu_in_play_locked_buttons[] = {
    { 410, 243, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_OPTIONS, 0, 0, { 0 }, -1 },
    { 410, 293, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 410, 343, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
};

// NOTE: These two and stru_5C4338 below are MM_WINDOW_26's button list and
// window info, and MM_WINDOW_26 is reached from exactly one place -
// mainmenu_ui_start's MM_TYPE_5 case. Both the type and the window are still
// unidentified, so there is no vocabulary to name the tables from; naming them
// after their contents would mean guessing what MM_TYPE_5 is. The buttons are
// mainmenu_ui_mainmenu_in_play_buttons minus the Load Game row, which suggests
// an in-play menu where loading is not allowed, but that is inference.
//
// Identifying MM_TYPE_5 pays for four names at once: the type, MM_WINDOW_26,
// and these two.
//
// 0x5C3860
static MainMenuButtonInfo stru_5C3860[] = {
    { 410, 143, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_SAVE_GAME, 0, 0, { 0 }, -1 },
    { 410, 243, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_OPTIONS, 0, 0, { 0 }, -1 },
    { 410, 293, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 410, 343, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
};

// 0x5C3AB0
static MainMenuWindowInfo stru_5C3AB0 = {
    2,
    NULL,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    -1,
    0,
    NULL,
    0,
    0,
    0,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    1,
};

// 0x5C3B48
static MainMenuWindowInfo stru_5C3B48 = {
    2,
    NULL,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    -1,
    0,
    NULL,
    0,
    0,
    0,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    2,
};

// 0x5C3BE0
static MainMenuWindowInfo mainmenu_ui_intro_info = {
    2,
    mainmenu_ui_intro_create,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    -1,
    0,
    NULL,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    5,
};

// 0x5C3C78
static MainMenuWindowInfo mainmenu_ui_credits_window_info = {
    2,
    mainmenu_ui_credits_create,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    -1,
    0,
    NULL,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    5,
};

// 0x5C3FB4
static mes_file_handle_t mainmenu_ui_mainmenu_mes_file = MES_FILE_HANDLE_INVALID;

// NOTE: Write-only. It's impossible to understand its meaning.
//
// 0x5C3FB8
static int dword_5C3FB8 = -1;

// NOTE: Unused, and unused in the original too - `search_instructions` finds no
// instruction anywhere in the exe with either address as an operand. Troika's
// own dead data, not something the port dropped.
//
// 0x5C3FC0
static TigRect stru_5C3FC0 = { 260, 293, 60, 20 };

// 0x5C3FD0
static TigRect stru_5C3FD0 = { 160, 263, 150, 20 };

// NOTE: These two are the click rects of the two text fields on the original's
// window type 17, and CE has no such window: its `MainMenuWindowType` is a
// compressed renumbering that drops it (see the enum's comment). 17 is the
// multiplayer host/join screen - its create function at 0x585D50 calls three
// routines in TIG's networking range, which CE stubs out entirely in
// `net_compat.h`.
//
// The original hit-tests them in `mainmenu_ui_message_filter` at 0x54710B and
// 0x54717C and again in the click handler at 0x545FD0, which focuses the shared
// `mainmenu_ui_textedit_buffer` for the first and `byte_64C394` for the second.
// So the data here is faithful; only the window that reads it is unported.
// Multiplayer is back-burner, so they stay unnamed rather than guess which
// field is which.
//
// 0x5C3FE0
static TigRect stru_5C3FE0 = { 160, 263, 150, 20 };

// 0x5C3FF0
static TigRect stru_5C3FF0 = { 560, 203, 150, 20 };

// NOTE: Always true, in the original as well as here. It starts true and the
// only instruction that writes it sets it to 1 again (`mainmenu_ui_default_type_flag_set`, 0x541170,
// which nothing in CE calls); the two reads are the ones below. So the
// `MM_TYPE_1` branches it guards are unreachable, and unnamed for want of any
// evidence of what it was meant to distinguish.
//
// 0x5C4000
static bool dword_5C4000 = true;

// Whether the menu was opened in its pre-game form (`MM_TYPE_DEFAULT` or
// `MM_TYPE_1`) rather than over a running game. Set by `mainmenu_ui_start` on
// every path, read once, by `mainmenu_ui_dismiss_ex` on the way back out.
//
// 0x5C4004
static bool mainmenu_ui_outside_game = true;

// 0x64C2F8
static char mainmenu_ui_textedit_buffer[128];

// 0x5C4008
static TextEdit mainmenu_ui_textedit = {
    0,
    mainmenu_ui_textedit_buffer,
    23,
    mainmenu_ui_textedit_on_enter,
    mainmenu_ui_textedit_on_change,
    NULL,
};

// 0x5C4030
static int mainmenu_ui_font_art_nums[2] = {
    229,
    27,
};

// 0x5C4038
static int mainmenu_ui_body_font_art = 171;

/**
 * UAP (0x5C4038) swaps this art, 226 rather than 171; UAP's archives ship it.
 *
 * That reaches `mainmenu_ui_body_fonts`, `mainmenu_ui_version_fonts` and
 * `mainmenu_ui_version_fonts_red` - so in practice the version banner, and the
 * new-char, pregen-char, charedit and shop screens.
 *
 * NOTE: An earlier version of this comment called those "the menu-entry fonts",
 * which is wrong and worth correcting explicitly, because it invites the
 * opposite conclusion. The main menu's own entries are drawn with
 * `mainmenu_ui_menu_fonts`, built from art 327 below, which UAP does not touch.
 * The split is by `refresh_text_flags & 0x8` - see the comment on
 * `mainmenu_ui_body_fonts`.
 */
#define MAINMENU_UI_UAP_FONT_ART 226

// 0x5C403C
static int mainmenu_ui_menu_font_art = 327;

// 0x5C4040
static int mainmenu_ui_font_colors[3][3] = {
    { 0, 0, 0 },
    { 97, 61, 42 },
    { 100, 0, 0 },
};

// 0x5C4064
static int mainmenu_ui_menu_font_greys[3] = {
    200,
    200,
    200,
};

// 0x5C4070
static int mainmenu_ui_text_pass_offsets[3] = {
    -1,
    2,
    -1,
};

// 0x5C407C
static char mainmenu_ui_ellipsis[] = "...";

// 0x5C5818
static MainMenuButtonInfo mainmenu_ui_mainmenu_no_multiplayer_buttons[] = {
    { 410, 143, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_SINGLE_PLAYER, 0, 0, { 0 }, -1 },
    { 410, 193, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_OPTIONS, 0, 0, { 0 }, -1 },
    { 410, 243, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_CREDITS, 0, 0, { 0 }, -1 },
    { 410, 293, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_0, 0, 0, { 0 }, -1 },
};

// 0x5C4170
// NOTE: The three fields `460`, the button count and the button table below
// are the values the original writes into this struct at RUNTIME, not the
// ones it initialises it with. 0x5C4170+0x24 statically holds `10, 5,
// 0x5C4080` - a FIVE-entry menu with MultiPlayer at y=343 - and 0x549B10
// overwrites the same three slots with `460, 4, 0x5C5818`, the
// no-multiplayer table. CE bakes in the post-call state, consistent with
// multiplayer being out of scope.
//
// The original reaches 0x549B10 from exactly one place, 0x56FF80:
//
//     if (!mes_load("mes\\MultiPlayer.mes", ...)) mainmenu_no_multiplayer();
//
// so a healthy install NEVER takes it - and the path cannot be forced open
// either, because `broadcast_init` loads the same file and calls
// `exit(EXIT_FAILURE)` when it is missing (`src/game/broadcast.c:81`). The
// two menus therefore differ by one entry in any install where both engines
// run, which is measurable: the rendered-frame differential reports ~9,575
// differing px in [319,147,501,376] at 800x600 after the version banner is
// masked, and that region is this table. See `harness/README.md`, *What
// stops a zero-difference frame*.
static MainMenuWindowInfo mainmenu_ui_mainmenu_window_info = {
    329,
    mainmenu_ui_create_mainmenu,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    460,
    SDL_arraysize(mainmenu_ui_mainmenu_no_multiplayer_buttons),
    mainmenu_ui_mainmenu_no_multiplayer_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    0,
};

// 0x5C4208
static MainMenuWindowInfo mainmenu_ui_mainmenu_in_play_window_info = {
    329,
    NULL,
    NULL,
    0,
    NULL,
    mainmenu_ui_press_mainmenu_in_play,
    NULL,
    NULL,
    NULL,
    30,
    SDL_arraysize(mainmenu_ui_mainmenu_in_play_buttons),
    mainmenu_ui_mainmenu_in_play_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    0,
};

// 0x5C42A0
static MainMenuWindowInfo mainmenu_ui_mainmenu_in_play_locked_window_info = {
    329,
    NULL,
    NULL,
    0,
    NULL,
    mainmenu_ui_press_mainmenu_in_play_locked,
    NULL,
    NULL,
    NULL,
    70,
    SDL_arraysize(mainmenu_ui_mainmenu_in_play_locked_buttons),
    mainmenu_ui_mainmenu_in_play_locked_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    0,
};

// 0x5C4338
static MainMenuWindowInfo stru_5C4338 = {
    329,
    NULL,
    NULL,
    0,
    NULL,
    mainmenu_ui_press_mainmenu_in_play,
    NULL,
    NULL,
    NULL,
    30,
    SDL_arraysize(stru_5C3860),
    stru_5C3860,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    0,
};

/**
 * The patch's compact options layout: a shorter background art, and the lens,
 * buttons and controls all lifted by the same offset. Art 669 ships in the
 * patch's own ArcanumZHighRes.dat, not in stock data.
 */
#define MAINMENU_UI_OPTIONS_COMPACT_ART 669
#define MAINMENU_UI_OPTIONS_COMPACT_SHIFT 41

// 0x5C43D0
static MainMenuButtonInfo mainmenu_ui_options_buttons[] = {
    { 130, 221, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 130, 261, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 130, 301, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 130, 341, -1, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
};

// 0x5C4490
static TigRect mainmenu_ui_options_pc_lens_rect = { 84, 67, 89, 89 };

// 0x5C44A0
static MainMenuWindowInfo mainmenu_ui_options_window_info = {
    556,
    mainmenu_ui_create_options,
    mainmenu_ui_destroy_options,
    0,
    NULL,
    mainmenu_ui_press_options,
    NULL,
    NULL,
    NULL,
    4000,
    SDL_arraysize(mainmenu_ui_options_buttons),
    mainmenu_ui_options_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    2,
};

// NOTE: The region `mainmenu_ui_progressbar_init`/`_update` invalidate, but
// nothing ever draws a bar into it - and that is faithful, not a porting slip.
// `mainmenu_ui_progressbar_max_value` (0x64C448) and `..._value` (0x64C44C) are
// written by exactly one instruction each in the original (0x5495F4, 0x549624)
// and read nowhere, so Troika cut the drawing and left the invalidation behind.
//
// 0x5C4538
static TigRect mainmenu_ui_progressbar_rect = { 150, 273, 240, 40 };

// 0x5C4548
static MainMenuButtonInfo mainmenu_ui_load_game_buttons[] = {
    { 675, 55, 321, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7020 },
    { 69, 55, 323, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7002 },
    { 115, 396, 32, TIG_BUTTON_HANDLE_INVALID, -1, -1, 0, { 0 }, 7022 },
};

// 0x5C45D8
static MainMenuButtonInfo mainmenu_ui_load_game_texttoggle_button = {
    499,
    82,
    747, // texttoggle.art
    TIG_BUTTON_HANDLE_INVALID,
    -1,
    -1,
    0,
    { 0 },
    7025, // "Toggle Info Display"
};

// 0x5C4608
static MainMenuWindowInfo mainmenu_ui_load_game_window_info = {
    745,
    mainmenu_ui_load_game_create,
    mainmenu_ui_load_game_destroy,
    0,
    mainmenu_ui_load_game_button_pressed,
    mainmenu_ui_load_game_button_released,
    NULL,
    NULL,
    NULL,
    -1,
    SDL_arraysize(mainmenu_ui_load_game_buttons),
    mainmenu_ui_load_game_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_load_game_refresh,
    mainmenu_ui_load_game_execute,
    { 42, 120, 145, 213 },
    mainmenu_ui_load_game_mouse_up,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    5,
};

// The save info panel, shared by the load game and save game screens: it
// describes the selected entry of `mainmenu_ui_gsl` from the `mainmenu_ui_gsi`
// loaded for it. Everything below `mainmenu_ui_saveinfo_text_shown` is drawn
// only while that flag is set.
//
// The two thumbnail rects are a blit pair, and both are resized to the
// thumbnail's real dimensions on every refresh, so the initializers only give
// the origin (0, 0 in the video buffer, 281, 55 in the window).
//
// 0x5C46B0
static TigRect mainmenu_ui_saveinfo_thumbnail_src_rect = { 0, 0, 468, 300 };

// The thumbnail's window rect, which doubles as the panel's dirty region: the
// refresh functions test it for intersection and fill it black when there is
// no save info to show.
//
// 0x5C46C0
static TigRect mainmenu_ui_saveinfo_thumbnail_dst_rect = { 281, 55, 468, 300 };

// Also carries the "Version Mismatch!" (5004) and "Empty" (5005) notices, which
// replace the panel's text when the save cannot be described.
//
// 0x5C46D0
static TigRect mainmenu_ui_saveinfo_pc_name_rect = { 289, 192, 451, 18 };

// Only the origin is used - `portrait_draw_native` sizes the blit itself.
//
// 0x5C46E0
static TigRect mainmenu_ui_saveinfo_portrait_rect = { 484, 98, 451, 18 };

// 0x5C46F0
static TigRect mainmenu_ui_saveinfo_area_rect = { 281, 212, 465, 18 };

// 0x5C4700
static TigRect mainmenu_ui_saveinfo_time_rect = { 448, 138, 139, 18 };

// 0x5C4710
static TigRect mainmenu_ui_saveinfo_date_rect = { 448, 126, 139, 18 };

// 0x5C4720
static TigRect mainmenu_ui_saveinfo_level_rect = { 547, 126, 109, 18 };

// 0x5C4730
static TigRect mainmenu_ui_saveinfo_description_rect = { 294, 173, 441, 18 };

// 0x5C4740
static TigRect mainmenu_ui_story_state_rects[4] = {
    { 288, 232, 456, 18 },
    { 300, 252, 432, 18 },
    { 329, 272, 386, 18 },
    { 348, 292, 338, 18 },
};

// The live PC's lens on the load game and save game screens (art 746), as
// opposed to `mainmenu_ui_options_pc_lens_rect` on the options screen.
//
// 0x5C4780
static TigRect mainmenu_ui_save_load_pc_lens_rect = { 84, 10, 89, 89 };

// Hides the save info panel's text while the texttoggle button is held down,
// uncovering the thumbnail underneath it. Cleared on button press and set again
// on release, on both screens.
//
// 0x5C4790
static bool mainmenu_ui_saveinfo_text_shown = true;

// The save list's scrollbar. Assigned into `mainmenu_ui_scrollbar_info` by both
// create functions, and re-blitted from the background art at the top of both
// refresh functions to erase the previous thumb.
//
// 0x5C4798
static TigRect mainmenu_ui_scrollbar_rect = { 213, 111, 12, 232 };

// 0x5C47A8
static MainMenuButtonInfo mainmenu_ui_save_game_buttons[] = {
    { 675, 55, 321, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7021 },
    { 69, 55, 323, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7002 },
    { 115, 396, 32, TIG_BUTTON_HANDLE_INVALID, -1, -1, 0, { 0 }, 7022 },
};

// 0x5C4838
static MainMenuButtonInfo mainmenu_ui_save_game_texttoggle_button = {
    499,
    82,
    747,
    TIG_BUTTON_HANDLE_INVALID,
    -1,
    -1,
    0,
    { 0 },
    7025,
};

// 0x5C4868
static MainMenuWindowInfo mainmenu_ui_save_game_window_info = {
    745, // saveloadbackground.art
    mainmenu_ui_save_game_create,
    mainmenu_ui_save_game_destroy,
    0,
    mainmenu_ui_save_game_button_pressed,
    mainmenu_ui_save_game_button_released,
    NULL,
    NULL,
    NULL,
    -1,
    SDL_arraysize(mainmenu_ui_save_game_buttons),
    mainmenu_ui_save_game_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_save_game_refresh,
    mainmenu_ui_save_game_execute,
    { 42, 120, 145, 213 },
    mainmenu_ui_save_game_mouse_up,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    5,
};

// 0x5C4900
static MainMenuWindowInfo mainmenu_ui_last_save_window_info = {
    2, // "black.art"
    mainmenu_ui_last_save_create,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    -1,
    0,
    NULL,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_last_save_refresh,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    5,
};

// 0x5C4998
static MainMenuButtonInfo mainmenu_ui_single_player_buttons[] = {
    { 410, 143, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_PICK_NEW_OR_PREGEN, 0, 0, { 0 }, -1 },
    { 410, 193, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_LOAD_GAME, 0, 0, { 0 }, -1 },
    { 410, 243, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_LAST_SAVE_GAME, 0, 0, { 0 }, -1 },
    { 410, 293, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_INTRO, 0, 0, { 0 }, -1 },
    { 410, 343, -1, TIG_BUTTON_HANDLE_INVALID, -2, 0, 0, { 0 }, -1 },
};

// 0x5C4A88
static MainMenuWindowInfo mainmenu_ui_single_player_window_info = {
    331,
    mainmenu_ui_create_single_player,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    50,
    SDL_arraysize(mainmenu_ui_single_player_buttons),
    mainmenu_ui_single_player_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    2,
};

// 0x5C4B20
static MainMenuButtonInfo mainmenu_ui_pick_new_or_pregen_buttons[] = {
    { 410, 143, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_PREGEN_CHAR, 0, 0, { 0 }, -1 },
    { 410, 193, -1, TIG_BUTTON_HANDLE_INVALID, MM_WINDOW_NEW_CHAR, 0, 0, { 0 }, -1 },
    { 410, 243, -1, TIG_BUTTON_HANDLE_INVALID, -2, 0, 0, { 0 }, -1 },
};

// 0x5C4BB0
static MainMenuWindowInfo mainmenu_ui_pick_new_or_pregen_window_info = {
    329,
    mainmenu_ui_pick_new_or_pregen_create,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    420,
    SDL_arraysize(mainmenu_ui_pick_new_or_pregen_buttons),
    mainmenu_ui_pick_new_or_pregen_buttons,
    0,
    0,
    0xD,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    NULL,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    5,
};

// 0x5C4C48
static MainMenuButtonInfo mainmenu_ui_new_char_buttons[] = {
    { 675, 55, 321, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7003 },
    { 69, 55, 323, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7002 },
    { 43, 118, 283, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 244, 118, 284, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 42, 331, 757, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 255, 332, 758, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 42, 381, 757, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 255, 382, 758, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 42, 281, 757, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 255, 282, 758, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
};

// 0x5C4E28
static MainMenuWindowInfo mainmenu_ui_new_char_window_info = {
    765, // createcharacterbase.art
    mainmenu_ui_new_char_create,
    NULL,
    1,
    NULL,
    mainmenu_ui_new_char_button_released,
    mainmenu_ui_new_char_button_hover,
    mainmenu_ui_new_char_button_leave,
    mainmenu_ui_new_char_mouse_idle,
    -1,
    8, // TODO: `mainmenu_ui_new_char_buttons` has 10 buttons, figure out why only 8 are used.
    mainmenu_ui_new_char_buttons,
    0,
    0,
    0x5,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_new_char_refresh,
    mainmenu_ui_new_char_execute,
    { 184, 35, 145, 196 },
    NULL,
    { 347, 42, 10, 184 },
    NULL,
    0,
    0,
    0,
    0,
    0xB,
};

// 0x5C4EC0
static TigRect mainmenu_ui_shared_char_stats_title_rect = { 339, 23, 409, 19 };

// 0x5C4ED0
static TigRect mainmenu_ui_shared_char_name_rect = { 46, 183, 228, 19 };

// 0x5C4EE0
static TigRect mainmenu_ui_new_char_name_rect = { 46, 183, 228, 18 };

// 0x5C4EF0
static TigRect mainmenu_ui_shared_char_gender_rect = { 66, 243, 186, 19 };

// 0x5C4F00
static TigRect mainmenu_ui_new_char_gender_hover_rect = { 22, 231, 276, 39 };

// 0x5C4F10
static TigRect mainmenu_ui_shared_char_race_rect = { 66, 293, 186, 19 };

// 0x5C4F20
static TigRect mainmenu_ui_new_char_race_hover_rect = { 22, 281, 276, 40 };

// 0x5C4F30
static TigRect mainmenu_ui_shared_char_background_rect = { 66, 343, 186, 19 };

// NOTE: Unused, and unused in the original too - no instruction anywhere in
// the exe takes this address as an operand.
//
// 0x5C4F40
static TigRect stru_5C4F40 = { 22, 331, 276, 40 };

// 0x5C4F50
static TigRect mainmenu_ui_stat_rects[15] = {
    { 346, 46, 50, 15 },
    { 346, 66, 50, 15 },
    { 346, 86, 50, 15 },
    { 346, 106, 50, 15 },
    { 406, 46, 50, 15 },
    { 406, 66, 50, 15 },
    { 406, 86, 50, 15 },
    { 406, 106, 50, 15 },
    { 472, 46, 139, 15 },
    { 472, 66, 139, 15 },
    { 472, 86, 139, 15 },
    { 472, 106, 139, 15 },
    { 618, 46, 142, 15 },
    { 618, 65, 147, 15 },
    { 618, 86, 147, 15 },
};

// NOTE: Unused, and unused in the original too - no instruction anywhere in
// the exe takes this address as an operand.
//
// 0x5C5040
static TigRect stru_5C5040 = { 618, 105, 145, 15 };

// 0x5C5050
static TigRect mainmenu_ui_shared_char_stats_view_rect = { 346, 46, 729, 74 };

// 0x5C5060
static TigRect mainmenu_ui_shared_char_portrait_rect = { 95, 28, 64, 64 };

// 0x5C5070
static TigRect mainmenu_ui_shared_char_desc_view_rect = { 350, 155, 395, 212 };

// 0x5C5080
static TigRect mainmenu_ui_shared_char_desc_title_rect = { 350, 159, 395, 19 };

// 0x5C5090
static TigRect mainmenu_ui_shared_char_desc_body_rect = { 350, 185, 395, 182 };

// 0x5C50A0
static MainMenuButtonInfo mainmenu_ui_new_char_name_button = {
    45,
    224,
    -1,
    TIG_BUTTON_HANDLE_INVALID,
    -1,
    -1,
    0,
    { 0 },
    0,
};

// NOTE: These two keep their placeholder names because nothing uses them.
// Unreferenced in CE and in the original - 0x5C50D0 and 0x5C5100 have no
// references at all - so no window ever built a button from either. They are a
// pair at x=22, y=272 and y=322, on the 50px pitch this file's menu rows use
// and with every other field neutral (-1 art, -1 window, no handler), which
// reads like two menu entries that were laid out and then cut. Nothing says
// what they would have been.
//
// 0x5C50D0
static MainMenuButtonInfo stru_5C50D0 = {
    22,
    272,
    -1,
    TIG_BUTTON_HANDLE_INVALID,
    -1,
    -1,
    0,
    { 0 },
    0,
};

// 0x5C5100
static MainMenuButtonInfo stru_5C5100 = {
    22,
    322,
    -1,
    TIG_BUTTON_HANDLE_INVALID,
    -1,
    -1,
    0,
    { 0 },
    0,
};

// 0x5C5130
static int mainmenu_ui_stat_list[] = {
    STAT_STRENGTH,
    STAT_CONSTITUTION,
    STAT_DEXTERITY,
    STAT_BEAUTY,
    STAT_INTELLIGENCE,
    STAT_WILLPOWER,
    STAT_PERCEPTION,
    STAT_CHARISMA,
    STAT_CARRY_WEIGHT,
    STAT_DAMAGE_BONUS,
    STAT_AC_ADJUSTMENT,
    STAT_SPEED,
    STAT_HEAL_RATE,
    STAT_POISON_RECOVERY,
    STAT_REACTION_MODIFIER,
};

// 0x5C5170
static struct {
    int body_type;
    bool available_for_female;
} mainmenu_ui_race_info[] = {
    /*     RACE_HUMAN */ { TIG_ART_CRITTER_BODY_TYPE_HUMAN, true },
    /*     RACE_DWARF */ { TIG_ART_CRITTER_BODY_TYPE_DWARF, false },
    /*       RACE_ELF */ { TIG_ART_CRITTER_BODY_TYPE_ELF, true },
    // UAP (0x5C5188): "Half Elf PCs now use elven art instead of human's, just
    // like all Half Elf NPCs." Substituted at the two
    // `object_set_gender_and_race` call sites rather than here, so this table
    // keeps matching the binary.
    /*  RACE_HALF_ELF */ { TIG_ART_CRITTER_BODY_TYPE_HUMAN, true },
    /*     RACE_GNOME */ { TIG_ART_CRITTER_BODY_TYPE_HALFLING, false },
    /*  RACE_HALFLING */ { TIG_ART_CRITTER_BODY_TYPE_HALFLING, false },
    /*  RACE_HALF_ORC */ { TIG_ART_CRITTER_BODY_TYPE_HUMAN, true },
    /* RACE_HALF_OGRE */ { TIG_ART_CRITTER_BODY_TYPE_HALF_OGRE, false },
};

/**
 * The body type a newly created character of `race` is given.
 *
 * UAP (0x5C5188) changes only the Half Elf entry of `mainmenu_ui_race_info`, from the
 * human body type to the elven one.
 */
static int mainmenu_ui_race_body_type(int race)
{
    if (uap_fixes_enabled() && race == RACE_HALF_ELF) {
        return TIG_ART_CRITTER_BODY_TYPE_ELF;
    }

    return mainmenu_ui_race_info[race].body_type;
}

// 0x5C51B0
static MainMenuButtonInfo mainmenu_ui_pregen_char_buttons[] = {
    { 675, 55, 321, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7003 },
    { 69, 55, 323, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7002 },
    { 43, 118, 283, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
    { 244, 118, 284, TIG_BUTTON_HANDLE_INVALID, -1, 0, 0, { 0 }, -1 },
};

// 0x5C5270
static MainMenuWindowInfo mainmenu_ui_pregen_char_window_info = {
    766,
    mainmenu_ui_pregen_char_create,
    NULL,
    1,
    NULL,
    mainmenu_ui_pregen_char_button_released,
    NULL,
    NULL,
    NULL,
    -1,
    SDL_arraysize(mainmenu_ui_pregen_char_buttons),
    mainmenu_ui_pregen_char_buttons,
    0,
    0,
    0x5,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_pregen_char_refresh,
    mainmenu_ui_pregen_char_execute,
    { 184, 35, 145, 196 },
    NULL,
    { 347, 42, 10, 184 },
    NULL,
    0,
    0,
    0,
    0,
    0xB,
};

// 0x5C5308
static int mainmenu_ui_pregen_char_idx = 1;

// 0x5C5310
static MainMenuButtonInfo mainmenu_ui_charedit_buttons[] = {
    { 675, 55, 321, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7003 },
    { 69, 55, 323, TIG_BUTTON_HANDLE_INVALID, -2, -1, 1, { 0 }, 7002 },
};

// 0x5C5370
static MainMenuWindowInfo mainmenu_ui_charedit_info = {
    -1,
    mainmenu_ui_charedit_create,
    mainmenu_ui_charedit_destroy,
    0,
    NULL,
    mainmenu_ui_charedit_button_released,
    NULL,
    NULL,
    NULL,
    -1,
    SDL_arraysize(mainmenu_ui_charedit_buttons),
    mainmenu_ui_charedit_buttons,
    0,
    0,
    0x5,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_charedit_refresh,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    0xC,
};

// 0x5C5408
static MainMenuButtonInfo mainmenu_ui_shop_buttons[] = {
    { 675, 55, 321, TIG_BUTTON_HANDLE_INVALID, -1, -1, 1, { 0 }, 7003 },
    { 69, 55, 323, TIG_BUTTON_HANDLE_INVALID, -2, -1, 1, { 0 }, 7002 },
};

// 0x5C5468
static MainMenuWindowInfo mainmenu_ui_shop_info = {
    -1,
    mainmenu_ui_shop_create,
    mainmenu_ui_shop_destroy,
    0,
    NULL,
    mainmenu_ui_shop_button_released,
    NULL,
    NULL,
    NULL,
    -1,
    SDL_arraysize(mainmenu_ui_shop_buttons),
    mainmenu_ui_shop_buttons,
    0,
    0,
    0x5,
    {
        { -1, 0, 0 },
        { -1, 0, 0 },
    },
    mainmenu_ui_shop_refresh,
    NULL,
    { 0 },
    NULL,
    { 0 },
    NULL,
    0,
    0,
    0,
    -1,
    0xE,
};

// 0x5C3A40
static MainMenuWindowInfo* main_menu_window_info[MM_WINDOW_COUNT] = {
    /*                       MM_WINDOW_0 */ &stru_5C3AB0,
    /*                       MM_WINDOW_1 */ &stru_5C3B48,
    /*                MM_WINDOW_MAINMENU */ &mainmenu_ui_mainmenu_window_info,
    /*        MM_WINDOW_MAINMENU_IN_PLAY */ &mainmenu_ui_mainmenu_in_play_window_info,
    /* MM_WINDOW_MAINMENU_IN_PLAY_LOCKED */ &mainmenu_ui_mainmenu_in_play_locked_window_info,
    /*           MM_WINDOW_SINGLE_PLAYER */ &mainmenu_ui_single_player_window_info,
    /*                 MM_WINDOW_OPTIONS */ &mainmenu_ui_options_window_info,
    /*               MM_WINDOW_LOAD_GAME */ &mainmenu_ui_load_game_window_info,
    /*               MM_WINDOW_SAVE_GAME */ &mainmenu_ui_save_game_window_info,
    /*          MM_WINDOW_LAST_SAVE_GAME */ &mainmenu_ui_last_save_window_info,
    /*                   MM_WINDOW_INTRO */ &mainmenu_ui_intro_info,
    /*      MM_WINDOW_PICK_NEW_OR_PREGEN */ &mainmenu_ui_pick_new_or_pregen_window_info,
    /*                MM_WINDOW_NEW_CHAR */ &mainmenu_ui_new_char_window_info,
    /*             MM_WINDOW_PREGEN_CHAR */ &mainmenu_ui_pregen_char_window_info,
    /*                MM_WINDOW_CHAREDIT */ &mainmenu_ui_charedit_info,
    /*                    MM_WINDOW_SHOP */ &mainmenu_ui_shop_info,
    /*                 MM_WINDOW_CREDITS */ &mainmenu_ui_credits_window_info,
    /*                      MM_WINDOW_26 */ &stru_5C4338,
};

// 0x64B870
static MainMenuAnim mainmenu_ui_anims[2];

// 0x64B898
static GameSaveInfo mainmenu_ui_gsi;

// 0x64BBF8
static GameSaveList mainmenu_ui_gsl;

// 0x64BC04
static tig_font_handle_t mainmenu_ui_version_fonts[3];

// NOTE: Built and destroyed alongside mainmenu_ui_version_fonts and never
// pushed - nothing draws with it, in CE or in the original, where 0x64BC10
// appears only in mainmenu_ui_init and mainmenu_ui_exit. Named from its
// construction (identical to the version fonts, but the third entry is red)
// rather than left blank, since the build states what it is; what is missing is
// a user.
//
// 0x64BC10
static tig_font_handle_t mainmenu_ui_version_fonts_red[3];

// 0x64BC1C
static char mainmenu_ui_draw_text_buffer[1000];

// 0x64C004
static MainMenuWindowType mainmenu_ui_window_stack[50];

// The two font sets `mainmenu_ui_refresh_text` chooses between, each [variant]
// [state]. Which one a window gets is decided by its `refresh_text_flags`:
//
//   flags & 0x8 set   -> mainmenu_ui_menu_fonts, built from art 327 and given
//                        the real colours in mainmenu_ui_font_colors (black,
//                        brown, dark red). Every menu screen sets it: the main
//                        menu, options, load, save, credits and intro.
//   flags & 0x8 clear -> mainmenu_ui_body_fonts, built from art 171 and all
//                        black, because mainmenu_ui_body_font_greys is never
//                        written. Only four windows: new-char, pregen-char,
//                        charedit and shop.
//
// mainmenu_ui_body_fonts is additionally the emphasis font for *plain* text
// (flags & 0x1 clear, flags & 0x10 set), so it is not exclusively the
// character-creation face.
//
// 0x64C0CC
static tig_font_handle_t mainmenu_ui_body_fonts[2][3];

// 0x64C0F0
static char mainmenu_ui_textedit_saved_buffer[128];

// 0x64C170
static tig_font_handle_t mainmenu_ui_fonts_tbl[MM_FONT_COUNT][MM_COLOR_COUNT];

// 0x64C210
static tig_font_handle_t mainmenu_ui_text_fonts[2];

// 0x64C218
static tig_font_handle_t mainmenu_ui_text_fonts_red[2];

// 0x64C220
static ScrollbarId mainmenu_ui_scrollbar;

// 0x64C228
static tig_font_handle_t mainmenu_ui_menu_fonts[2][3];

// 0x64C240
static tig_font_handle_t mainmenu_ui_selected_font;

// 0x64C244
static MainMenuType mainmenu_ui_type;

// 0x64C260
static ScrollbarUiControlInfo mainmenu_ui_scrollbar_info;

// 0x64C2F4
static mes_file_handle_t mainmenu_ui_rules_mainmenu_mes_file;

// NOTE: Read but never written, in the original as well as here - the one
// instruction that touches it is a load at 0x548C6B, inside the redraw helper
// for the multiplayer character list. So it is always zero, and CE is faithful.
//
// 0x64C378
static int dword_64C378;

// 0x64C37C
static char* mainmenu_ui_savelist_module_name;

// 0x64C380
static bool mainmenu_ui_initialized;

// 0x64C384
static bool mainmenu_ui_active;

// Forces `mainmenu_ui_create_window_func` to skip the display, so the window
// `mainmenu_ui_init` opens to seed state never reaches the screen.
//
// 0x64C388
static bool mainmenu_ui_suppress_display;

// Set by the transitions that have already fixed up the window stack
// themselves - intro, credits, and loading a save - so the next
// `mainmenu_ui_open` does not push the window it opens. Cleared by that open.
//
// 0x64C38C
static bool mainmenu_ui_skip_window_stack_push;

// 0x64C390
static int mainmenu_ui_num_windows;

// NOTE: The second text entry buffer of the original's window type 17, the
// multiplayer host/join screen CE does not have - see the note on
// `stru_5C3FE0`. Cleared and focused by the click handler at 0x545FD0;
// nothing in CE reaches it.
//
// 0x64C394
static char byte_64C394[128];

// 0x64C414
static MainMenuWindowType mainmenu_ui_window_type;

// 0x64C418
static bool mainmenu_ui_start_new_game;

// 2026-08-21: headless savegame round-trip driver, fed by
// `-roundtrip:SLOT[=OUTSLOT]` (see arcanum-work SAVE-ROUNDTRIP.md). With
// OUTSLOT the run loads SLOT then immediately saves OUTSLOT; without it the
// run stops after loading so the harness can snapshot the restored
// Save\Current\data.sav. A verification seam, not a game feature - it is
// deliberately unreachable from any menu. No original address: these
// statics are new, they exist only in CE.
static bool mainmenu_ui_roundtrip_requested;

// CE: ping frames to run between the round-trip load and save (`@N`).
static int mainmenu_ui_roundtrip_frames;
static char mainmenu_ui_roundtrip_slot[TIG_MAX_PATH];
static char mainmenu_ui_roundtrip_out[TIG_MAX_PATH];

// 0x64C41C
int64_t* mainmenu_ui_mp_chars;

// 0x64C420
int mainmenu_ui_mp_chars_count;

// 0x64C424
static bool mainmenu_ui_auto_equip_items_on_start;

// 0x64C428
static bool mainmenu_ui_textedit_focused;

// NOTE: Never written, in CE or in the original - 0x64C42C has two reads in
// mainmenu_ui_init and no writes anywhere. So all three entries are 0 and every
// mainmenu_ui_body_fonts colour resolves to black. Kept as the original has it.
//
// 0x64C42C
static int mainmenu_ui_body_font_greys[3];

// 0x64C438
static bool mainmenu_ui_was_compact_interface;

// Counts `mainmenu_ui_create_mainmenu` calls. Escape quits the game only once
// this is above one, which is what keeps it from quitting from the main menu
// the game starts on.
//
// 0x64C43C
static int mainmenu_ui_mainmenu_create_count;

// 0x64C440
static int mainmenu_ui_options_tab;

// 0x64C444
static bool mainmenu_ui_gsi_loaded;

// 0x64C448
static int mainmenu_ui_progressbar_max_value;

// 0x64C44C
static int mainmenu_ui_progressbar_value;

// Whether the load game screen's PC lens shows the live map rather than a
// blackout - false only on the shopping map. Clicking the lens returns to the
// game when it is set and backs out one window when it is not.
//
// 0x64C450
static bool mainmenu_ui_load_game_lens_passthrough;

// 0x64C454
static ChareditMode mainmenu_ui_charedit_mode;

// 0x64C458
static MainMenuUiNewCharHoverMode mainmenu_ui_new_char_hover_mode;

// 0x64C460
static int64_t mainmenu_ui_pregen_char_obj;

// 0x64C468
static int mainmenu_ui_key_shortcuts_disable_count;

/**
 * Switch the options screen to the patch's compact layout.
 *
 * `Options = 1` in config.ini. The patch does this by rewriting the same three
 * places in the exe that this rewrites here: the window's background art number
 * at 0x5C44A0, the lens rect's y at 0x5C4494, and the four button y values from
 * 0x5C43D4 on a 48-byte stride. The shift is uniform, so it is applied rather
 * than duplicated into a second set of tables.
 *
 * Guarded, because it mutates statics and must not compound if init runs twice.
 */
static void mainmenu_ui_apply_compact_options(void)
{
    static bool applied;
    int index;

    if (applied || !highres_config_get()->options_compact) {
        return;
    }
    applied = true;

    mainmenu_ui_options_window_info.background_art_num = MAINMENU_UI_OPTIONS_COMPACT_ART;
    mainmenu_ui_options_pc_lens_rect.y -= MAINMENU_UI_OPTIONS_COMPACT_SHIFT;

    for (index = 0; index < (int)SDL_arraysize(mainmenu_ui_options_buttons); index++) {
        mainmenu_ui_options_buttons[index].y -= MAINMENU_UI_OPTIONS_COMPACT_SHIFT;
    }
}

// 2026-08-21: see the statics above. `spec` is `SLOT[=OUTSLOT]`. Never
// returns: exits 0 on success; 20 bad spec or unreadable saveinfo;
// 21 unsupported save version; 22 load failure; 23 save failure. No original
// address: new function.
void mainmenu_ui_roundtrip_set(const char* spec)
{
    const char* eq;
    const char* at;
    size_t n;

    // `@N` is a CE-only suffix: N ping frames between load and save.
    at = strchr(spec, '@');
    if (at != NULL) {
        mainmenu_ui_roundtrip_frames = (int)strtol(at + 1, NULL, 10);
    }

    eq = strchr(spec, '=');
    if (eq != NULL && at != NULL && at < eq) {
        eq = NULL;
    }
    n = eq != NULL ? (size_t)(eq - spec)
                   : (at != NULL ? (size_t)(at - spec) : strlen(spec));
    if (n == 0 || n >= sizeof(mainmenu_ui_roundtrip_slot)) {
        tig_debug_printf("ROUNDTRIP FAIL: bad spec '%s'\n", spec);
        exit(20);
    }

    memcpy(mainmenu_ui_roundtrip_slot, spec, n);
    mainmenu_ui_roundtrip_slot[n] = '\0';

    if (eq != NULL) {
        size_t out_n = at != NULL && at > eq
            ? (size_t)(at - eq - 1)
            : strlen(eq + 1);
        if (out_n >= sizeof(mainmenu_ui_roundtrip_out)) {
            out_n = sizeof(mainmenu_ui_roundtrip_out) - 1;
        }
        memcpy(mainmenu_ui_roundtrip_out, eq + 1, out_n);
        mainmenu_ui_roundtrip_out[out_n] = '\0';
    }

    mainmenu_ui_roundtrip_requested = true;
}

// 2026-08-22: `@N` on the spec runs N ping frames between the load and the
// save. It exists to answer one question - the round-trip gains a stale
// TIMEEVENT_TYPE_CLOCK node because `timeevent_load` appends onto a
// non-empty list, and that node is in the past, so a single
// `timeevent_ping` should fire it and `intgame_clock_process_callback`
// should collapse the pair back to one. Loading and saving back to back
// never gives it that frame, so the harness may be measuring a moment the
// game never sits at. See SAVE-ROUNDTRIP.md section 4c.
void mainmenu_ui_roundtrip_frames_set(int frames)
{
    mainmenu_ui_roundtrip_frames = frames;
}

// 2026-08-21: runs the round-trip when (and only when) `-roundtrip:` was
// given; main() calls this right before entering the loop, so every system a
// real Continue would use is up. The load sequence mirrors the Continue
// button's mainmenu_ui_load_game_by_name (0x5432B0) minus the parts that
// only exist to talk to the player.
// No original address: new function.
void mainmenu_ui_roundtrip_run(void)
{
    // The savelist names slots by their .gsi fname, which embeds the
    // description ("Slot0002test3"), but the archive pair on disk is named
    // from the first eight characters only - the same truncation
    // mainmenu_ui_load_game_by_name does with strncpy before calling
    // gamelib_load. Mirror it, or the loader looks for a tfaf that does
    // not exist.
    char slot_short[9];
    char out_short[9];

    if (!mainmenu_ui_roundtrip_requested) {
        return;
    }
    mainmenu_ui_roundtrip_requested = false;

    snprintf(slot_short, sizeof(slot_short), "%.8s", mainmenu_ui_roundtrip_slot);
    snprintf(out_short, sizeof(out_short), "%.8s", mainmenu_ui_roundtrip_out);

    mainmenu_ui_reset();
    player_destroy();

    if (!gamelib_load(slot_short)) {
        tig_debug_printf("ROUNDTRIP FAIL: gamelib_load failed\n");
        exit(22);
    }

    if (mainmenu_ui_roundtrip_out[0] == '\0') {
        tig_debug_printf("ROUNDTRIP EXTRACT OK slot=%s\n", mainmenu_ui_roundtrip_slot);
        exit(0);
    }

    // Give the restored state N frames before saving, if asked. tig_ping()
    // and gamelib_ping() are the two calls main_loop() makes that advance
    // state; the drawing calls beside them are deliberately skipped.
    if (mainmenu_ui_roundtrip_frames > 0) {
        int frame;

        tig_debug_printf("ROUNDTRIP: running %d ping frame(s) before save\n",
            mainmenu_ui_roundtrip_frames);
        for (frame = 0; frame < mainmenu_ui_roundtrip_frames; frame++) {
            tig_ping();
            gamelib_ping();
        }
    }

    if (!gamelib_save(out_short, "roundtrip")) {
        tig_debug_printf("ROUNDTRIP FAIL: gamelib_save failed\n");
        exit(23);
    }

    tig_debug_printf("ROUNDTRIP OK slot=%s out=%s\n",
        mainmenu_ui_roundtrip_slot,
        mainmenu_ui_roundtrip_out);
    exit(0);
}

// 0x540930
bool mainmenu_ui_init(GameInitInfo* init_info)
{
    int index;
    TigFont font_desc;
    MesFileEntry mes_file_entry;

    (void)init_info;

    if (mainmenu_ui_mainmenu_mes_file == MES_FILE_HANDLE_INVALID) {
        if (!mes_load("mes\\mainmenu.mes", &mainmenu_ui_mainmenu_mes_file)) {
            return false;
        }
    }

    if (!mes_load("rules\\mainmenu.mes", &mainmenu_ui_rules_mainmenu_mes_file)) {
        return false;
    }

    settings_register(&settings, "show version", "0", NULL);

    mainmenu_ui_apply_compact_options();

    mainmenu_fonts_init();

    // UAP (0x5C4038): see `MAINMENU_UI_UAP_FONT_ART`.
    int body_font_art = uap_fixes_enabled()
        ? MAINMENU_UI_UAP_FONT_ART
        : mainmenu_ui_body_font_art;

    for (index = 0; index < 3; index++) {
        font_desc.flags = 0;
        if (index == 1) {
            font_desc.flags = TIG_FONT_BLEND_ADD;
        }
        tig_art_interface_id_create(mainmenu_ui_menu_font_art, 0, 0, 0, &(font_desc.art_id));
        font_desc.str = NULL;
        font_desc.color = tig_color_make(mainmenu_ui_font_colors[index][0], mainmenu_ui_font_colors[index][1], mainmenu_ui_font_colors[index][2]);
        tig_font_create(&font_desc, &(mainmenu_ui_menu_fonts[0][index]));

        tig_art_interface_id_create(mainmenu_ui_menu_font_art, 0, 0, 0, &(font_desc.art_id));
        font_desc.color = index < 2
            ? tig_color_make(mainmenu_ui_font_colors[index][0], mainmenu_ui_font_colors[index][1], mainmenu_ui_font_colors[index][2])
            : tig_color_make(240, 15, 15);
        tig_font_create(&font_desc, &(mainmenu_ui_menu_fonts[1][index]));
    }

    for (index = 0; index < 3; index++) {
        font_desc.flags = 0;
        tig_art_interface_id_create(body_font_art, 0, 0, 0, &(font_desc.art_id));
        font_desc.str = NULL;
        font_desc.color = tig_color_make(mainmenu_ui_body_font_greys[index], mainmenu_ui_body_font_greys[index], mainmenu_ui_body_font_greys[index]);
        tig_font_create(&font_desc, &(mainmenu_ui_body_fonts[0][index]));

        tig_art_interface_id_create(body_font_art, 0, 0, 0, &(font_desc.art_id));
        font_desc.color = index < 2
            ? tig_color_make(mainmenu_ui_body_font_greys[index], mainmenu_ui_body_font_greys[index], mainmenu_ui_body_font_greys[index])
            : tig_color_make(240, 15, 15);
        tig_font_create(&font_desc, &(mainmenu_ui_body_fonts[1][index]));
    }

    for (index = 0; index < 3; index++) {
        font_desc.flags = 0;
        tig_art_interface_id_create(body_font_art, 0, 0, 0, &(font_desc.art_id));
        font_desc.str = NULL;
        font_desc.color = tig_color_make(mainmenu_ui_menu_font_greys[index], mainmenu_ui_menu_font_greys[index], mainmenu_ui_menu_font_greys[index]);
        tig_font_create(&font_desc, &(mainmenu_ui_version_fonts[index]));

        tig_art_interface_id_create(body_font_art, 0, 0, 0, &(font_desc.art_id));
        font_desc.color = index < 2
            ? tig_color_make(mainmenu_ui_menu_font_greys[index], mainmenu_ui_menu_font_greys[index], mainmenu_ui_menu_font_greys[index])
            : tig_color_make(240, 15, 15);
        tig_font_create(&font_desc, &(mainmenu_ui_version_fonts_red[index]));
    }

    for (index = 0; index < 2; index++) {
        font_desc.flags = 0;
        tig_art_interface_id_create(mainmenu_ui_font_art_nums[index], 0, 0, 0, &(font_desc.art_id));
        font_desc.str = NULL;
        font_desc.color = tig_color_make(250, 250, 250);
        tig_font_create(&font_desc, &(mainmenu_ui_text_fonts[index]));

        font_desc.flags = 0;
        tig_art_interface_id_create(mainmenu_ui_font_art_nums[index], 0, 0, 0, &(font_desc.art_id));
        font_desc.str = NULL;
        font_desc.color = tig_color_make(255, 50, 50);
        tig_font_create(&font_desc, &(mainmenu_ui_text_fonts_red[index]));
    }

    font_desc.flags = 0;
    tig_art_interface_id_create(mainmenu_ui_font_art_nums[0], 0, 0, 0, &(font_desc.art_id));
    font_desc.str = NULL;
    font_desc.color = tig_color_make(255, 210, 0);
    tig_font_create(&font_desc, &mainmenu_ui_selected_font);

    mes_file_entry.num = 500;
    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
    strncpy(mainmenu_ui_textedit_buffer, mes_file_entry.str, 23);

    mainmenu_ui_initialized = true;
    mainmenu_ui_window_type = MM_WINDOW_0;

    // NOTE: the round-trip driver does NOT run here. At this point
    // gamelib_mod_load() has not executed yet - main() calls it after
    // gameuilib_init() returns - so the module repository (and with it every
    // save\... path) does not exist. main() invokes
    // mainmenu_ui_roundtrip_run() between gameuilib_mod_load() and
    // mainmenu_ui_handle(), which is the true Continue-click state.

    mainmenu_ui_suppress_display = true;
    mainmenu_ui_start(MM_TYPE_DEFAULT);
    mainmenu_ui_suppress_display = false;

    gamelib_thumbnail_size_set(468, 300);
    mainmenu_ui_start_new_game = false;
    mainmenu_ui_female_chars_probe();
    mainmenu_ui_savelist_module_name = NULL;

    return true;
}

// 0x541050
void mainmenu_ui_exit(void)
{
    int index;

    mainmenu_ui_dismiss_ex(true);
    mainmenu_fonts_exit();

    for (index = 0; index < 3; index++) {
        tig_font_destroy(mainmenu_ui_menu_fonts[0][index]);
        tig_font_destroy(mainmenu_ui_menu_fonts[1][index]);
        tig_font_destroy(mainmenu_ui_body_fonts[0][index]);
        tig_font_destroy(mainmenu_ui_body_fonts[1][index]);
        tig_font_destroy(mainmenu_ui_version_fonts[index]);
        tig_font_destroy(mainmenu_ui_version_fonts_red[index]);
    }

    for (index = 0; index < 2; index++) {
        tig_font_destroy(mainmenu_ui_text_fonts[index]);
        tig_font_destroy(mainmenu_ui_text_fonts_red[index]);
    }

    tig_font_destroy(mainmenu_ui_selected_font);
    mes_unload(mainmenu_ui_rules_mainmenu_mes_file);
    mes_unload(mainmenu_ui_mainmenu_mes_file);

    if (mainmenu_ui_mp_chars != NULL) {
        FREE(mainmenu_ui_mp_chars);
    }

    mainmenu_ui_initialized = false;
}

// NOTE: Unreferenced in CE, and in the original too - no instruction in
// Arcanum.exe calls 0x541150. A dead exported entry that resets the menu to
// the main-menu window and clears the start flags.
//
// 0x541150
void mainmenu_ui_mainmenu_defaults_set(void)
{
    mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
    mainmenu_ui_start_new_game = false;
    mainmenu_ui_auto_equip_items_on_start = false;
}

// NOTE: The sole writer of `dword_5C4000` - the flag that keeps a default
// open from turning into MM_TYPE_1 - and, like 0x541150, unreferenced in both
// binaries: no instruction in Arcanum.exe calls 0x541170.
//
// 0x541170
void mainmenu_ui_default_type_flag_set(void)
{
    dword_5C4000 = true;
}

// 0x541180
void mainmenu_ui_start(MainMenuType type)
{
    tig_art_id_t art_id;

    if (!mainmenu_ui_active) {
        mainmenu_ui_num_windows = 0;

        // CE: Hide main interface to prevent world view and top/bottom bars
        // to be visible while mainmenu is being presented (visible on custom
        // resolutions).
        intgame_hide();

        if (type != MM_TYPE_OPTIONS) {
            timeevent_pause();
        }

        tig_art_interface_id_create(0, 0, 0, 0, &art_id);
        tig_mouse_cursor_set_art_id(art_id);
        inven_ui_destroy();

        if (type == MM_TYPE_DEFAULT && !dword_5C4000) {
            type = MM_TYPE_1;
        }

        mainmenu_ui_type = type;

        switch (type) {
        case MM_TYPE_IN_PLAY:
            mainmenu_ui_window_type = MM_WINDOW_MAINMENU_IN_PLAY;
            mainmenu_ui_outside_game = false;
            mainmenu_ui_open();
            object_hover_obj_set(OBJ_HANDLE_NULL);
            break;
        case MM_TYPE_IN_PLAY_LOCKED:
            mainmenu_ui_window_type = MM_WINDOW_MAINMENU_IN_PLAY_LOCKED;
            mainmenu_ui_outside_game = false;
            mainmenu_ui_open();
            object_hover_obj_set(OBJ_HANDLE_NULL);
            break;
        case MM_TYPE_OPTIONS:
            mainmenu_ui_window_type = MM_WINDOW_OPTIONS;
            mainmenu_ui_outside_game = false;
            mainmenu_ui_open();
            object_hover_obj_set(OBJ_HANDLE_NULL);
            break;
        case MM_TYPE_5:
            mainmenu_ui_window_type = MM_WINDOW_26;
            mainmenu_ui_outside_game = false;
            mainmenu_ui_open();
            object_hover_obj_set(OBJ_HANDLE_NULL);
            break;
        default:
            if (tig_mouse_hide() != TIG_OK) {
                tig_debug_printf("mainmenu_ui_start: ERROR: tig_mouse_hide failed!\n");
                exit(EXIT_FAILURE);
            }
            mainmenu_ui_window_type = MM_WINDOW_0;
            mainmenu_ui_outside_game = true;
            mainmenu_ui_open();
            object_hover_obj_set(OBJ_HANDLE_NULL);
            break;
        }
    }
}

// 0x5412D0
void mainmenu_ui_dismiss(void)
{
    mainmenu_ui_dismiss_ex(false);
}

/**
 * Dismisses the whole main-menu layer and returns to gameplay. While the menu
 * is up (`mainmenu_ui_active`) this auto-equips a fresh character and then
 * either starts the new game - fade, both intro movies, teleport to the start
 * map, first newspaper event, random encounter schedule - when
 * `mainmenu_ui_start_new_game` is set, or simply resumes play. Callers that
 * only mean "close the menu" must clear `mainmenu_ui_start_new_game` first.
 *
 * `exiting` is set only by `mainmenu_ui_exit`, i.e. when the game is shutting
 * down. It suppresses `intgame_draw_bars` and the compact-interface toggle,
 * but not `intgame_refresh_cursor`, `intgame_message_window_set_display_func` or `intgame_show`.
 *
 * NOTE: On the shutdown path those three still reach into Intgame, which
 * `gameuilib_exit` tears down first - it walks `gameuilib_modules` in reverse
 * and "Intgame" sits after "MainMenu-UI".
 *
 * 0x5412E0
 */
void mainmenu_ui_dismiss_ex(bool exiting)
{
    int64_t pc_obj;
    int map;
    int64_t x;
    int64_t y;
    FadeData fade_data;
    TeleportData teleport_data;
    MesFileEntry mes_file_entry;
    DateTime datetime;
    TimeEvent timeevent;

    if (mainmenu_ui_active) {
        gameuilib_wants_mainmenu_unset();

        pc_obj = player_get_local_pc_obj();

        if (mainmenu_ui_auto_equip_items_on_start) {
            if (item_wield_get(pc_obj, ITEM_INV_LOC_WEAPON) == OBJ_HANDLE_NULL) {
                item_wield_best(pc_obj, ITEM_INV_LOC_WEAPON, OBJ_HANDLE_NULL);
            }
            if (item_wield_get(pc_obj, ITEM_INV_LOC_ARMOR) == OBJ_HANDLE_NULL) {
                item_wield_best(pc_obj, ITEM_INV_LOC_ARMOR, OBJ_HANDLE_NULL);
            }
            mainmenu_ui_auto_equip_items_on_start = false;
        }

        if (mainmenu_ui_window_type != MM_WINDOW_0 || !mainmenu_ui_type_flags[mainmenu_ui_type][1]) {
            if (mainmenu_ui_start_new_game) {
                mainmenu_ui_start_new_game = false;

                map = map_by_type(MAP_TYPE_START_MAP);
                if (map == 0) {
                    tig_debug_printf("MMUI: ERROR: Teleport/World Loc Failure!\n");
                    exit(EXIT_FAILURE);
                }
                if (!map_get_starting_location(map, &x, &y)) {
                    tig_debug_printf("MMUI: ERROR: Teleport/World Loc Failure!\n");
                    exit(EXIT_FAILURE);
                }

                fade_data.flags = 0;
                fade_data.color = 0;
                fade_data.steps = 64;
                fade_data.duration = 3.0f;
                gfade_run(&fade_data);

                teleport_data.flags = TELEPORT_MOVIE1 | TELEPORT_MOVIE2 | TELEPORT_FADE_IN;
                teleport_data.obj = pc_obj;
                teleport_data.loc = LOCATION_MAKE(x, y);
                teleport_data.map = map;
                teleport_data.movie1 = 1;
                teleport_data.movie_flags1 = 0;
                teleport_data.movie2 = 7;
                teleport_data.movie_flags2 = 0;
                teleport_data.fade_in.flags = FADE_IN;
                teleport_data.fade_in.steps = 64;
                teleport_data.fade_in.duration = 3.0f;
                teleport_data.fade_in.color = tig_color_make(0, 0, 0);
                teleport_do(&teleport_data);

                gsound_stop_all(0);

                mes_file_entry.num = 6000; // "Please Wait"
                mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                iso_load_screen_draw(mes_file_entry.str);

                timeevent.type = TIMEEVENT_TYPE_NEWSPAPERS;
                datetime_from_milliseconds(&datetime, 86400000 - datetime_start_time_in_milliseconds_get());
                timeevent_add_delay(&timeevent, &datetime);

                wmap_rnd_schedule();
            } else {
                if (mainmenu_ui_outside_game) {
                    map_draw_hook();
                }
                gamelib_draw();
            }
        }

        mainmenu_ui_close(false);
    }

    intgame_refresh_cursor();
    intgame_message_window_set_display_func(0);
    if (!exiting) {
        intgame_draw_bars();

        if (mainmenu_ui_was_compact_interface) {
            intgame_toggle_interface();
            mainmenu_ui_was_compact_interface = false;
        }
    }
    timeevent_unpause();

    // CE: Restore main interface to its normal state.
    intgame_show();
}

// CE: reproducible-run harness flag. Not in the original.
static bool mainmenu_ui_autoload;
static bool mainmenu_ui_autoload_exact;
static char mainmenu_ui_autoload_slot[TIG_MAX_PATH];

// CE: not in the original - see mainmenu_ui.h.
void mainmenu_ui_autoload_set(bool enabled)
{
    mainmenu_ui_autoload = enabled;
}

// CE: an exact slot supplied by `-autoload:SLOT`. Empty preserves the older
// newest-save harness contract. `main` has already bounded the argument, and
// gamelib_save_name_resolve validates the slot syntax and its unique .gsi.
void mainmenu_ui_autoload_slot_set(const char* slot)
{
    mainmenu_ui_autoload_exact = true;
    snprintf(mainmenu_ui_autoload_slot, sizeof(mainmenu_ui_autoload_slot),
        "%s", slot != NULL ? slot : "");
}

// 0x541590
bool mainmenu_ui_handle(void)
{
    tig_timestamp_t timestamp;
    TigMessage msg;

    mainmenu_ui_was_compact_interface = intgame_is_compact_interface();
    if (mainmenu_ui_was_compact_interface) {
        intgame_toggle_interface();
    }

    if (mainmenu_ui_window_type < MM_WINDOW_MAINMENU) {
        mainmenu_ui_close(false);
        mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
        mainmenu_ui_open();

        if (tig_mouse_show() != TIG_OK) {
            tig_debug_printf("mainmenu_ui_handle: ERROR: tig_mouse_show failed!\n");
        }
    }

    broadcast_ui_close();

    // CE: the reproducible-run harness. Loads the newest save as if the
    // player had picked it from the load screen, so a scripted run reaches
    // in-game state with no input at all. See mainmenu_ui.h.
    if (mainmenu_ui_autoload) {
        char exact_path[TIG_MAX_PATH];
        const char* autoload_path;
        bool exact = mainmenu_ui_autoload_exact;

        if (exact) {
            if (!gamelib_save_name_resolve(mainmenu_ui_autoload_slot,
                    exact_path, sizeof(exact_path))) {
                tig_debug_printf("Harness: exact autoload FAILED to resolve '%s'\n",
                    mainmenu_ui_autoload_slot);
                exit(24);
            }
            autoload_path = exact_path;
        } else {
            autoload_path = gamelib_last_save_name();
        }

        mainmenu_ui_autoload = false;
        mainmenu_ui_autoload_exact = false;
        if (exact) {
            tig_debug_printf("Harness: autoload exact slot '%s' as '%s'\n",
                mainmenu_ui_autoload_slot, autoload_path);
        } else {
            tig_debug_printf("Harness: autoload newest save '%s'\n", autoload_path);
        }

        if (autoload_path[0] != '\0'
            && gamelib_saveinfo_load(autoload_path, &mainmenu_ui_gsi)) {
            char autoload_name[256];

            mainmenu_ui_gsi_loaded = true;
            strncpy(autoload_name, autoload_path, 8);
            autoload_name[8] = '\0';

            if (mainmenu_ui_load_game_by_name(autoload_name)) {
                tig_debug_printf("Harness: autoload ok\n");
            } else {
                tig_debug_printf("Harness: autoload FAILED to load '%s'\n",
                    autoload_name);
                if (exact) {
                    exit(24);
                }
            }

            if (mainmenu_ui_gsi_loaded) {
                gamelib_saveinfo_exit(&mainmenu_ui_gsi);
                mainmenu_ui_gsi_loaded = false;
            }
        } else {
            tig_debug_printf("Harness: autoload FAILED to read save info\n");
            if (exact) {
                exit(24);
            }
        }
    }

    while (mainmenu_ui_active) {
        tig_ping();
        tig_timer_now(&timestamp);
        timeevent_ping(timestamp);
        gamelib_ping();
        iso_redraw();
        tig_window_display();

        while (tig_message_dequeue(&msg) == TIG_OK) {
            switch (msg.type) {
            case TIG_MESSAGE_QUIT:
                if (mainmenu_ui_confirm_quit() == TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
                    return false;
                }
                break;
            case TIG_MESSAGE_REDRAW:
                gamelib_redraw();
                break;
            case TIG_MESSAGE_KEYBOARD:
                if (!msg.data.keyboard.pressed
                    && msg.data.keyboard.scancode == SDL_SCANCODE_ESCAPE
                    && mainmenu_ui_window_type != MM_WINDOW_MAINMENU) {
                    mainmenu_ui_close(true);
                }
                break;
            default:
                break;
            }
        }

        tig_window_display();
    }

    return true;
}

// 0x541680
bool mainmenu_ui_is_active(void)
{
    return mainmenu_ui_active;
}

// 0x541690
TigWindowModalDialogChoice mainmenu_ui_confirm_quit(void)
{
    return mainmenu_ui_confirm(5100); // "Are you sure you want to quit?"
}

// 0x5416A0
TigWindowModalDialogChoice mainmenu_ui_confirm(int num)
{
    MesFileEntry mes_file_entry;
    TigWindowModalDialogInfo modal_info;
    TigWindowModalDialogChoice choice;

    mes_file_entry.num = num;
    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

    modal_info.type = TIG_WINDOW_MODAL_DIALOG_TYPE_OK_CANCEL;
    modal_info.x = 237;
    modal_info.y = 232;
    modal_info.text = mes_file_entry.str;
    modal_info.keys[TIG_WINDOW_MODAL_DIALOG_CHOICE_OK] = 'y';
    modal_info.keys[TIG_WINDOW_MODAL_DIALOG_CHOICE_CANCEL] = 'n';
    modal_info.process = NULL;
    modal_info.redraw = gamelib_redraw;
    hrp_center(&(modal_info.x), &(modal_info.y));
    tig_window_modal_dialog(&modal_info, &choice);

    return choice;
}

// 0x541710
void mainmenu_ui_reset(void)
{
    inven_ui_destroy();
    charedit_close();
    sleep_ui_close();
    wmap_ui_close();
    logbook_ui_close();
    fate_ui_close();
    schematic_ui_close();
    gamelib_reset();
    gameuilib_reset();
}

// 0x541740
void mainmenu_ui_open(void)
{
    if (!mainmenu_ui_active) {
        dword_5C3FB8 = -1;
        if (main_menu_window_info[mainmenu_ui_window_type]->init_func != NULL) {
            main_menu_window_info[mainmenu_ui_window_type]->init_func();
        } else {
            mainmenu_ui_create_window();
        }

        if (mainmenu_ui_active) {
            if (!mainmenu_ui_skip_window_stack_push) {
                mainmenu_ui_push_window_stack(mainmenu_ui_window_type);
            }
        }

        mainmenu_ui_skip_window_stack_push = false;
    }
}

// 0x5417A0
void mainmenu_ui_close(bool back)
{
    if (main_menu_window_info[mainmenu_ui_window_type]->exit_func != NULL) {
        main_menu_window_info[mainmenu_ui_window_type]->exit_func();
    }

    mainmenu_ui_windows_destroy();

    if (back) {
        mainmenu_ui_window_type = mainmenu_ui_pop_window_stack();
        if (mainmenu_ui_window_type != MM_WINDOW_0) {
            mainmenu_ui_open();
        }
    }
}

// 0x5417E0
MainMenuWindowType mainmenu_ui_pop_window_stack(void)
{
    if (mainmenu_ui_num_windows > 0) {
        if (--mainmenu_ui_num_windows > 0) {
            return mainmenu_ui_window_stack[--mainmenu_ui_num_windows];
        }
    } else {
        mainmenu_ui_num_windows = 0;
    }
    return MM_WINDOW_0;
}

// 0x541810
void mainmenu_ui_push_window_stack(MainMenuWindowType window_type)
{
    mainmenu_ui_window_stack[mainmenu_ui_num_windows++] = window_type;
}

// Expands the two-character sequence `\t` as it appears in .mes strings into
// five spaces.
//
// 0x541830
void mainmenu_ui_expand_tabs(char* dst, const char* src)
{
    while (*src != '\0') {
        if (src[0] == '\\' && src[1] == 't') {
            // FIX (porting bug #8): Was `strcpy(dst, "    ")` - four spaces
            // plus the terminator strcpy appends - followed by `dst += 5`.
            // That left a NUL at `dst[4]` that nothing overwrote, so every
            // string containing a tab
            // was truncated at the tab. The original writes five spaces and no
            // terminator: `mov edx, 0x20202020` / `mov [esi], edx` /
            // `mov [esi + 4], dl` at 0x54184D, with `dl` holding 0x20.
            memcpy(dst, "     ", 5);
            dst += 5;
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

// 0x5418A0
void mainmenu_ui_draw_text(char* str, TigRect* rect, tig_font_handle_t font, unsigned int flags)
{
    TigVideoBuffer* vb;
    TigRect text_rect;
    TigRect dirty_rect;
    char* newline;
    char* chunk;
    TigFont font_desc;
    size_t pos;

    if (tig_window_vbid_get(mainmenu_ui_window_handle, &vb) != TIG_OK) {
        return;
    }

    text_rect = *rect;

    while (str != NULL) {
        newline = strstr(str, "\\n");
        if (newline != NULL) {
            *newline = '\0';
        }

        mainmenu_ui_expand_tabs(mainmenu_ui_draw_text_buffer, str);

        chunk = mainmenu_ui_draw_text_buffer;
        if ((flags & 0x1) != 0) {
            font_desc.width = 0;
            font_desc.height = 0;
            font_desc.str = chunk;
            font_desc.flags = 0;
            tig_font_measure(&font_desc);

            if (font_desc.width > rect->width) {
                pos = strlen(str);
                while (pos > 0 && font_desc.width > rect->width) {
                    chunk[pos--] = '\0';
                    font_desc.width = 0;
                    font_desc.height = 0;
                    font_desc.str = chunk;
                    font_desc.flags = 0;
                    tig_font_measure(&font_desc);
                }
            }
        } else if ((flags & 0x02) != 0) {
            font_desc.width = 0;
            font_desc.height = 0;
            font_desc.str = chunk;
            font_desc.flags = 0;
            tig_font_measure(&font_desc);

            if (font_desc.width > rect->width) {
                pos = strlen(str);
                while (pos > 0 && font_desc.width > rect->width) {
                    chunk++;
                    pos--;
                    font_desc.width = 0;
                    font_desc.height = 0;
                    font_desc.str = chunk;
                    font_desc.flags = 0;
                    tig_font_measure(&font_desc);
                }
            }
        }

        tig_font_push(font);
        if (tig_font_write(vb, chunk, &text_rect, &dirty_rect) != TIG_OK) {
            tig_debug_printf("MainMenu-UI: mmUITextWrite_func: ERROR: Couldn't write text: '%s'!\n", mainmenu_ui_draw_text_buffer);
        }
        tig_font_pop();

        text_rect.y += dirty_rect.height;
        text_rect.height -= dirty_rect.height;

        if (newline != NULL) {
            *newline = '\\';
            newline += 2;
        }

        str = newline;
    }
}

// 0x541AA0
void mainmenu_ui_create_mainmenu(void)
{
    mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
    mainmenu_ui_create_window();
    ++mainmenu_ui_mainmenu_create_count;
    mainmenu_ui_draw_version();
}

// 0x541AC0
void mainmenu_ui_draw_version(void)
{
    TigRect rect;
    char version[40];

    // UAP (0x541AF7): the `show version` test is defeated rather than removed -
    // the patch retargets its branch to the instruction that follows it - so the
    // banner is always drawn.
    if (!uap_fixes_enabled()
        && settings_get_value(&settings, SHOW_VERSION_KEY) == 0) {
        return;
    }

    if (!gamelib_copy_version(version, NULL, NULL)) {
        return;
    }

    if (uap_fixes_enabled()) {
        // UAP (0x541ACD): the banner moves from the bottom-left corner to the
        // top-right, and narrows to fit "Unofficial Patch 2.0.2".
        rect.x = 680;
        rect.y = 5;
        rect.width = 120;
        rect.height = 20;
    } else {
        rect.x = 10;
        rect.y = 575;
        rect.width = 400;
        rect.height = 20;
    }

    tig_font_push(mainmenu_ui_version_fonts[0]);
    if (tig_window_text_write(mainmenu_ui_window_handle, version, &rect) != TIG_OK) {
        tig_debug_printf("MainMenuUI: ERROR: GameLib_Version_String Failed to draw!\n");
    }
    tig_font_pop();
}

// 0x541B50
bool mainmenu_ui_press_mainmenu_in_play(tig_button_handle_t button_handle)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (button_handle == window->buttons[3].button_handle) {
        if (mainmenu_ui_confirm_quit() != TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
            return false;
        }

        tig_button_hide(button_handle);

        return mainmenu_ui_return_to_mainmenu(button_handle);
    }

    if (button_handle == window->buttons[4].button_handle) {
        if (mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
            mainmenu_ui_dismiss();
            return false;
        }

        mainmenu_ui_close(true);

        if (mainmenu_ui_window_type == MM_WINDOW_0) {
            mainmenu_ui_dismiss();
            return false;
        }

        return false;
    }

    return false;
}

// 0x541BE0
bool mainmenu_ui_press_mainmenu_in_play_locked(tig_button_handle_t button_handle)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (button_handle == window->buttons[1].button_handle) {
        if (mainmenu_ui_confirm_quit() != TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
            return false;
        }

        tig_button_hide(button_handle);

        return mainmenu_ui_return_to_mainmenu(button_handle);
    }

    if (button_handle == window->buttons[2].button_handle) {
        if (mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
            mainmenu_ui_dismiss();
            return false;
        }

        mainmenu_ui_close(true);

        if (mainmenu_ui_window_type == MM_WINDOW_0) {
            mainmenu_ui_dismiss();
            return false;
        }

        return false;
    }

    return false;
}

// 0x541C70
void mainmenu_ui_create_options(void)
{
    PcLens pc_lens;
    int64_t pc_obj;
    int64_t loc;

    mainmenu_ui_options_close_game_screens();
    mainmenu_ui_window_type = MM_WINDOW_OPTIONS;
    mainmenu_ui_create_window_func(false);
    mainmenu_ui_options_tab = 0;
    options_ui_start(OPTIONS_UI_TAB_GAME, mainmenu_ui_window_handle, mainmenu_ui_type_flags[mainmenu_ui_type][1] == 0);
    mainmenu_ui_draw_version();

    pc_lens.window_handle = mainmenu_ui_window_handle;
    pc_lens.rect = &mainmenu_ui_options_pc_lens_rect;
    tig_art_interface_id_create(670, 0, 0, 0, &pc_lens.art_id);
    if (mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
        pc_obj = player_get_local_pc_obj();
        loc = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
        location_origin_set(loc);
        intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &pc_lens);
    } else {
        intgame_pc_lens_do(PC_LENS_MODE_BLACKOUT, &pc_lens);
    }

    tig_window_display();
}

// NOTE: Body identical to 0x542200 (`mainmenu_ui_close_game_screens`) in the
// original as well - Troika compiled the same close-everything sequence twice.
// Kept as two entries to preserve the original's call topology (this one is
// only `mainmenu_ui_create_options`; 0x542200 serves the load/save paths).
//
// 0x541D40
void mainmenu_ui_options_close_game_screens(void)
{
    inven_ui_destroy();
    charedit_close();
    sleep_ui_close();
    wmap_ui_close();
    logbook_ui_close();
    fate_ui_close();
    schematic_ui_close();
}

// 0x541D70
void mainmenu_ui_destroy_options(void)
{
    options_ui_close();
    intgame_pc_lens_do(PC_LENS_MODE_NONE, NULL);
}

// 0x541D90
bool mainmenu_ui_press_options(tig_button_handle_t button_handle)
{
    int index;

    for (index = 0; index < 4; index++) {
        if (mainmenu_ui_options_buttons[index].button_handle == button_handle) {
            break;
        }
    }

    if (index >= 4) {
        return options_ui_handle_button_pressed(button_handle);
    }

    if (index != 3) {
        mainmenu_ui_options_tab_switch(index);
        return true;
    }

    if (mainmenu_ui_window_type == MM_WINDOW_OPTIONS) {
        if (!options_ui_load_module()) {
            return true;
        }
    }

    if (mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
        mainmenu_ui_dismiss();
    } else {
        mainmenu_ui_close(true);
        if (mainmenu_ui_window_type == MM_WINDOW_0) {
            mainmenu_ui_dismiss();
        }
    }

    return false;
}

// 0x541E20
void mainmenu_ui_options_tab_switch(int a1)
{
    if (mainmenu_ui_options_tab != a1 && options_ui_load_module()) {
        options_ui_close();

        mainmenu_ui_options_tab = a1;
        switch (mainmenu_ui_options_tab) {
        case 0:
            options_ui_start(OPTIONS_UI_TAB_GAME, mainmenu_ui_window_handle, mainmenu_ui_type_flags[mainmenu_ui_type][1] == 0);
            break;
        case 1:
            options_ui_start(OPTIONS_UI_TAB_VIDEO, mainmenu_ui_window_handle, mainmenu_ui_type_flags[mainmenu_ui_type][1] == 0);
            break;
        case 2:
            options_ui_start(OPTIONS_UI_TAB_AUDIO, mainmenu_ui_window_handle, mainmenu_ui_type_flags[mainmenu_ui_type][1] == 0);
            break;
        }
    }
}

// 0x541F20
void mainmenu_ui_load_game_create(void)
{
    MainMenuWindowInfo* window;
    int64_t pc_obj;
    PcLens pc_lens;

    mainmenu_ui_window_type = MM_WINDOW_LOAD_GAME;
    window = main_menu_window_info[mainmenu_ui_window_type];

    mainmenu_ui_close_game_screens();

    if (mainmenu_ui_savelist_module_name) {
        gamelib_savelist_create_module(mainmenu_ui_savelist_module_name, &mainmenu_ui_gsl);
    } else {
        gamelib_savelist_create(&mainmenu_ui_gsl);
    }

    gamelib_savelist_sort(&mainmenu_ui_gsl, GAME_SAVE_LIST_ORDER_DATE, false);

    window->cnt = mainmenu_ui_gsl.count;
    if (window->selected_index == -1) {
        if (window->cnt > 0) {
            const char* path = gamelib_last_save_name();
            unsigned int index;

            window->selected_index = 0;

            if (path != NULL && *path != '\0') {
                for (index = 0; index < mainmenu_ui_gsl.count; index++) {
                    if (strcmp(mainmenu_ui_gsl.names[index], path) == 0) {
                        window->selected_index = index;
                        break;
                    }
                }
            }
        }
    } else if (window->selected_index >= window->cnt) {
        window->selected_index = window->cnt > 0 ? 0 : -1;
    }

    window->max_top_index = window->cnt - window->content_rect.height / 20 - 1;
    if (window->max_top_index < 0) {
        window->max_top_index = 0;
    }
    window->top_index = 0;

    mainmenu_ui_create_window_func(false);

    if (!main_menu_button_create_ex(&mainmenu_ui_load_game_texttoggle_button, 0, 0, 0x2)) {
        tig_debug_printf("MainMenu-UI: mainmenu_ui_create_load_game: ERROR: Failed to create button.\n");
        exit(EXIT_FAILURE);
    }

    mainmenu_ui_scrollbar_info.scrollbar_rect = mainmenu_ui_scrollbar_rect;
    mainmenu_ui_scrollbar_info.flags = SB_INFO_VALID
        | SB_INFO_CONTENT_RECT
        | SB_INFO_MAX_VALUE
        | SB_INFO_MIN_VALUE
        | SB_INFO_LINE_STEP
        | SB_INFO_VALUE
        | SB_INFO_ON_VALUE_CHANGED
        | SB_INFO_ON_REFRESH;
    mainmenu_ui_scrollbar_info.min_value = 0;
    mainmenu_ui_scrollbar_info.max_value = window->max_top_index + 1;
    if (mainmenu_ui_scrollbar_info.max_value > 0) {
        mainmenu_ui_scrollbar_info.max_value--;
    }
    mainmenu_ui_scrollbar_info.value = window->selected_index < 7 ? 0 : window->selected_index;
    mainmenu_ui_scrollbar_info.line_step = 1;
    mainmenu_ui_scrollbar_info.on_value_changed = mainmenu_ui_scrollbar_value_changed;
    mainmenu_ui_scrollbar_info.on_refresh = mainmenu_ui_scrollbar_refresh_rect;
    mainmenu_ui_scrollbar_info.content_rect.x = 34;
    mainmenu_ui_scrollbar_info.content_rect.y = 110;
    mainmenu_ui_scrollbar_info.content_rect.width = 195;
    mainmenu_ui_scrollbar_info.content_rect.height = 232;
    scrollbar_ui_control_create(&mainmenu_ui_scrollbar, &mainmenu_ui_scrollbar_info, mainmenu_ui_window_handle);

    mainmenu_ui_load_game_lens_passthrough = false;

    pc_obj = player_get_local_pc_obj();

    pc_lens.window_handle = mainmenu_ui_window_handle;
    pc_lens.rect = &mainmenu_ui_save_load_pc_lens_rect;
    tig_art_interface_id_create(746, 0, 0, 0, &(pc_lens.art_id));

    if (pc_obj != OBJ_HANDLE_NULL) {
        location_origin_set(obj_field_int64_get(pc_obj, OBJ_F_LOCATION));
    }

    if (map_by_type(MAP_TYPE_SHOPPING_MAP) == map_current_map()) {
        intgame_pc_lens_do(PC_LENS_MODE_BLACKOUT, &pc_lens);
    } else {
        intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &pc_lens);
        mainmenu_ui_load_game_lens_passthrough = true;
    }

    scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);
    tig_window_display();
}

// NOTE: Body identical to 0x541D40 (`mainmenu_ui_options_close_game_screens`)
// in the original as well; see the note there. This is the copy the load/save
// and quick-load paths use.
//
// 0x542200
void mainmenu_ui_close_game_screens(void)
{
    inven_ui_destroy();
    charedit_close();
    sleep_ui_close();
    wmap_ui_close();
    logbook_ui_close();
    fate_ui_close();
    schematic_ui_close();
}

// 0x542230
void mainmenu_ui_load_game_destroy(void)
{
    scrollbar_ui_control_destroy(mainmenu_ui_scrollbar);

    if (mainmenu_ui_gsi_loaded) {
        gamelib_saveinfo_exit(&mainmenu_ui_gsi);
        mainmenu_ui_gsi_loaded = false;
    }

    gamelib_savelist_destroy(&mainmenu_ui_gsl);

    intgame_pc_lens_do(PC_LENS_MODE_NONE, NULL);
    if (mainmenu_ui_savelist_module_name != NULL)
        FREE(mainmenu_ui_savelist_module_name);
    mainmenu_ui_savelist_module_name = NULL;
}

// 0x542280
void mainmenu_ui_scrollbar_value_changed(int a1)
{
    main_menu_window_info[mainmenu_ui_window_type]->top_index = a1;
}

// 0x5422A0
void mainmenu_ui_scrollbar_refresh_rect(TigRect* rect)
{
    (void)rect;

    main_menu_window_info[mainmenu_ui_window_type]->refresh_func(NULL);
}

// 0x5422C0
bool mainmenu_ui_load_game_execute(int btn)
{
    int index;
    char name[256];

    (void)btn;

    index = main_menu_window_info[mainmenu_ui_window_type]->selected_index;
    if (index == -1) {
        return false;
    }

    strncpy(name, mainmenu_ui_gsl.names[index], 8);
    name[8] = '\0';

    return mainmenu_ui_load_game_by_name(name);
}

// 0x542420
bool mainmenu_ui_load_game_button_pressed(tig_button_handle_t button_handle)
{
    if (button_handle != mainmenu_ui_load_game_texttoggle_button.button_handle) {
        return false;
    }

    mainmenu_ui_saveinfo_text_shown = false;
    main_menu_window_info[mainmenu_ui_window_type]->refresh_func(&mainmenu_ui_saveinfo_thumbnail_dst_rect);

    return true;
}

// 0x542460
bool mainmenu_ui_load_game_button_released(tig_button_handle_t button_handle)
{
    MainMenuWindowInfo* window;
    int index;

    window = main_menu_window_info[mainmenu_ui_window_type];

    for (index = 0; index < 2; index++) {
        if (button_handle == window->buttons[index].button_handle) {
            mainmenu_ui_window_cmd(index + 2);
            return true;
        }
    }

    if (button_handle == window->buttons[2].button_handle) {
        gsound_play_sfx(0, 1);
        mainmenu_ui_load_game_handle_delete();
        return true;
    }

    if (button_handle == mainmenu_ui_load_game_texttoggle_button.button_handle) {
        mainmenu_ui_saveinfo_text_shown = true;
        window->refresh_func(&mainmenu_ui_saveinfo_thumbnail_dst_rect);
        return true;
    }

    return false;
}

// 0x5424F0
void mainmenu_ui_load_game_mouse_up(int x, int y)
{
    MainMenuWindowInfo* window;

    (void)x;

    window = main_menu_window_info[mainmenu_ui_window_type];
    window->selected_index = window->top_index + y / 20;
    if (window->selected_index >= window->cnt) {
        window->selected_index = -1;
    }
    mainmenu_ui_load_saveinfo_reload();
    window->refresh_func(NULL);
    scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);
}

// 0x542560
void mainmenu_ui_load_saveinfo_reload(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (mainmenu_ui_gsi_loaded) {
        gamelib_saveinfo_exit(&mainmenu_ui_gsi);
        mainmenu_ui_gsi_loaded = false;

        if (window->selected_index > -1
            && gamelib_saveinfo_load(mainmenu_ui_gsl.names[window->selected_index], &mainmenu_ui_gsi)) {
            mainmenu_ui_gsi_loaded = true;
        }
    }
}

// 0x5425C0
void mainmenu_ui_load_game_refresh(TigRect* rect)
{
    MainMenuWindowInfo* window;
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigVideoBufferData video_buffer_data;
    TigWindowBlitInfo win_blit_info;
    MesFileEntry mes_file_entry;
    char str[20];
    tig_font_handle_t font;
    int area;
    char* area_name;
    char* story_state_desc;

    window = main_menu_window_info[mainmenu_ui_window_type];
    tig_art_interface_id_create(window->background_art_num, 0, 0, 0, &art_id);
    tig_art_frame_data(art_id, &art_frame_data);
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        src_rect.x = mainmenu_ui_scrollbar_rect.x;
        src_rect.y = mainmenu_ui_scrollbar_rect.y;
        src_rect.width = mainmenu_ui_scrollbar_rect.width + 1;
        src_rect.height = mainmenu_ui_scrollbar_rect.height + 1;

        dst_rect.x = mainmenu_ui_scrollbar_rect.x;
        dst_rect.y = mainmenu_ui_scrollbar_rect.y;
        dst_rect.width = mainmenu_ui_scrollbar_rect.width + 1;
        dst_rect.height = mainmenu_ui_scrollbar_rect.height + 1;

        art_blit_info.art_id = art_id;
        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info);
    }

    if (rect == NULL
        || (window->content_rect.x < rect->x + rect->width
            && window->content_rect.y < rect->y + rect->height
            && rect->x < window->content_rect.x + window->content_rect.width
            && rect->y < window->content_rect.y + window->content_rect.height)) {
        dst_rect.x = window->content_rect.x;
        dst_rect.y = window->content_rect.y;
        dst_rect.width = window->content_rect.width;
        dst_rect.height = window->content_rect.height + 5;

        if (tig_window_fill(mainmenu_ui_window_handle, &dst_rect, tig_color_make(0, 0, 0)) != TIG_OK) {
            tig_debug_printf("mmUIMPLoadGameRefreshFunc: ERROR: tig_window_fill2 failed!\n");
            exit(EXIT_FAILURE);
        }

        dst_rect.height -= 5;

        if (mainmenu_ui_gsl.count != 0) {
            int index;
            int max_y;
            char* name;

            max_y = dst_rect.y + dst_rect.height - 1;

            dst_rect.width -= 4;
            dst_rect.height = 20;

            for (index = window->top_index; index < window->cnt; index++) {
                if (dst_rect.y >= max_y) {
                    break;
                }

                font = window->selected_index == index ? mainmenu_ui_selected_font : mainmenu_ui_text_fonts[0];
                tig_font_push(font);
                name = mainmenu_ui_savelist_display_name(index);
                if (*name != '\0') {
                    mainmenu_ui_draw_text_ellipsized(name, &dst_rect, font);
                }
                dst_rect.y += 20;
                tig_font_pop();
            }
        }
    }

    if (rect == NULL
        || (mainmenu_ui_saveinfo_thumbnail_dst_rect.x < rect->x + rect->width
            && mainmenu_ui_saveinfo_thumbnail_dst_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_saveinfo_thumbnail_dst_rect.x + mainmenu_ui_saveinfo_thumbnail_dst_rect.width
            && rect->y < mainmenu_ui_saveinfo_thumbnail_dst_rect.y + mainmenu_ui_saveinfo_thumbnail_dst_rect.height)) {
        if (window->selected_index > -1) {
            if (!mainmenu_ui_gsi_loaded
                && mainmenu_ui_gsl.count > 0
                && gamelib_saveinfo_load(mainmenu_ui_gsl.names[window->selected_index], &mainmenu_ui_gsi)) {
                mainmenu_ui_gsi_loaded = true;
            }

            if (mainmenu_ui_gsi_loaded) {
                if (mainmenu_ui_gsi.thumbnail_video_buffer != NULL) {
                    if (tig_video_buffer_data(mainmenu_ui_gsi.thumbnail_video_buffer, &video_buffer_data) == TIG_OK) {
                        mainmenu_ui_saveinfo_thumbnail_src_rect.width = video_buffer_data.width;
                        mainmenu_ui_saveinfo_thumbnail_src_rect.height = video_buffer_data.height;

                        mainmenu_ui_saveinfo_thumbnail_dst_rect.width = video_buffer_data.width;
                        mainmenu_ui_saveinfo_thumbnail_dst_rect.height = video_buffer_data.height;

                        win_blit_info.type = TIG_WINDOW_BLIT_VIDEO_BUFFER_TO_WINDOW;
                        win_blit_info.vb_blit_flags = 0;
                        win_blit_info.src_video_buffer = mainmenu_ui_gsi.thumbnail_video_buffer;
                        win_blit_info.src_rect = &mainmenu_ui_saveinfo_thumbnail_src_rect;
                        win_blit_info.dst_window_handle = mainmenu_ui_window_handle;
                        win_blit_info.dst_rect = &mainmenu_ui_saveinfo_thumbnail_dst_rect;

                        if (tig_window_blit(&win_blit_info) != TIG_OK) {
                            tig_debug_printf("MMUI: ERROR: mmUIMPLoadGameRefreshFunc FAILED to refresh!\n");
                        }
                    }
                }

                font = mainmenu_ui_text_fonts[0];
                tig_font_push(font);

                if (mainmenu_ui_gsi.version == 25) {
                    if (mainmenu_ui_saveinfo_text_shown) {
                        mainmenu_ui_draw_centered_text(mainmenu_ui_gsi.pc_name, &mainmenu_ui_saveinfo_pc_name_rect, font);
                        if (mainmenu_ui_gsi.pc_portrait != 0) {
                            portrait_draw_native(OBJ_HANDLE_NULL,
                                mainmenu_ui_gsi.pc_portrait,
                                mainmenu_ui_window_handle,
                                mainmenu_ui_saveinfo_portrait_rect.x,
                                mainmenu_ui_saveinfo_portrait_rect.y);
                        }

                        mes_file_entry.num = 5051; // "Level %d"
                        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                        snprintf(str, sizeof(str), mes_file_entry.str, mainmenu_ui_gsi.pc_level);
                        mainmenu_ui_draw_centered_text(str, &mainmenu_ui_saveinfo_level_rect, font);

                        mainmenu_ui_draw_centered_text(mainmenu_ui_gsi.description, &mainmenu_ui_saveinfo_description_rect, font);

                        if (map_by_type(MAP_TYPE_START_MAP) == mainmenu_ui_gsi.map) {
                            area = area_get_nearest_area_in_range(mainmenu_ui_gsi.pc_location, true);
                        } else if (!map_get_area(mainmenu_ui_gsi.map, &area)) {
                            area = 0;
                        }

                        if (area > 0) {
                            area_name = area_get_name(area);
                        } else {
                            mes_file_entry.num = 5050; // "Unknown location."
                            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                            area_name = mes_file_entry.str;
                        }

                        mainmenu_ui_draw_centered_text(area_name, &mainmenu_ui_saveinfo_area_rect, font);

                        datetime_format_date(&(mainmenu_ui_gsi.datetime), str, sizeof(str));
                        mainmenu_ui_draw_right_aligned_text(str, &mainmenu_ui_saveinfo_date_rect, font);

                        datetime_format_time(&(mainmenu_ui_gsi.datetime), str, sizeof(str));
                        mainmenu_ui_draw_right_aligned_text(str, &mainmenu_ui_saveinfo_time_rect, font);

                        story_state_desc = script_story_state_info(mainmenu_ui_gsi.story_state);
                        if (story_state_desc != NULL && *story_state_desc != '\0') {
                            mmUITextWriteCenteredToArray(story_state_desc, mainmenu_ui_story_state_rects, 4, font);
                        }
                    }
                } else {
                    mes_file_entry.num = 5004; // "Version Mismatch!"
                    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                    mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_saveinfo_pc_name_rect, font);
                }
                tig_font_pop();
            } else {
                tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_saveinfo_thumbnail_dst_rect, tig_color_make(0, 0, 0));

                font = mainmenu_ui_text_fonts_red[0];
                tig_font_push(font);
                mes_file_entry.num = 5005; // "Empty"
                mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_saveinfo_pc_name_rect, font);
                tig_font_pop();
            }
        } else {
            tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_saveinfo_thumbnail_dst_rect, tig_color_make(0, 0, 0));
        }

        tig_art_interface_id_create(748, 0, 0, 0, &art_id);
        if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK
            && tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = 281;
            dst_rect.y = 55;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.height;

            art_blit_info.art_id = art_id;
            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            if (tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info) != TIG_OK) {
                tig_debug_printf("MMUI: mmUIMPLoadGameRefreshFunc: ERROR: FAILED to refresh!\n");
            }
        }
    }

    mmUIWinRefreshScrollBar();
}

// 0x542D00
void mainmenu_ui_draw_text_ellipsized(char* str, TigRect* rect, tig_font_handle_t font)
{
    TigFont font_desc;
    TigRect text_rect;

    font_desc.width = 0;
    font_desc.height = 0;
    font_desc.str = str;
    font_desc.flags = 0;
    tig_font_measure(&font_desc);

    text_rect = *rect;

    if (font_desc.width < rect->width) {
        mainmenu_ui_draw_text(str, &text_rect, font, 0);
        return;
    }

    font_desc.width = 0;
    font_desc.height = 0;
    font_desc.str = mainmenu_ui_ellipsis;
    font_desc.flags = 0;
    tig_font_measure(&font_desc);

    text_rect.width -= font_desc.width;
    mainmenu_ui_draw_text(str, &text_rect, font, 1);

    text_rect.x += font_desc.width;
    text_rect.width = font_desc.width;
    mainmenu_ui_draw_text(mainmenu_ui_ellipsis, &text_rect, font, 0);
}

// 0x542DF0
void mainmenu_ui_draw_centered_text(char* str, TigRect* rect, tig_font_handle_t font)
{
    TigFont font_desc;
    TigRect text_rect;

    font_desc.width = 0;
    font_desc.height = 0;
    font_desc.str = str;
    font_desc.flags = 0;
    tig_font_measure(&font_desc);

    text_rect = *rect;
    if (font_desc.width < rect->width) {
        text_rect.x += (text_rect.width - font_desc.width) / 2;
        text_rect.width = font_desc.width + 20;
    }

    mainmenu_ui_draw_text(str, &text_rect, font, 0);
}

// 0x542EA0
void mainmenu_ui_draw_right_aligned_text(char* str, TigRect* rect, tig_font_handle_t font)
{
    TigFont font_desc;
    TigRect text_rect;

    font_desc.width = 0;
    font_desc.height = 0;
    font_desc.str = str;
    font_desc.flags = 0;
    tig_font_measure(&font_desc);

    text_rect = *rect;
    if (font_desc.width < rect->width) {
        text_rect.x -= font_desc.width;
        text_rect.width = font_desc.width + 10;
    }

    mainmenu_ui_draw_text(str, &text_rect, font, 0);
}

// 0x542F50
void mmUITextWriteCenteredToArray(char* str, TigRect* rects, int cnt, tig_font_handle_t font)
{
    TigFont font_desc;
    TigRect text_rect;
    char* curr = str;
    char* space = NULL;
    char* pch;

    // NOTE: Original code is slightly different but does the same thing.
    while (*curr != '\0' && cnt > 0) {
        font_desc.width = 0;
        font_desc.height = 0;
        font_desc.flags = 0;
        font_desc.str = curr;
        tig_font_measure(&font_desc);

        text_rect = *rects;
        if (font_desc.width < text_rect.width) {
            text_rect.x += (text_rect.width - font_desc.width) / 2;
            text_rect.width = font_desc.width;
            mainmenu_ui_draw_text(curr, &text_rect, font, 0);
            rects++;
            cnt--;

            if (space == NULL) {
                break;
            }

            curr = space + 1;

            *space = ' ';
            space = NULL;
        } else {
            pch = strrchr(str, ' ');
            if (pch == NULL) {
                tig_debug_printf("MainMenuUI: mmUITextWriteCenteredToArray: ERROR: Coudn't fit entire to string to print: '%s'!\n", str);
                break;
            }

            if (space != NULL) {
                *space = ' ';
            }

            space = pch;
            *space = '\0';
        }
    }

    if (space != NULL) {
        *space = ' ';
        space = NULL;
    }
}

// 0x543040
char* mainmenu_ui_savelist_display_name(int index)
{
    if (mainmenu_ui_gsl.names != NULL) {
        return mainmenu_ui_gsl.names[index] + 8;
    } else {
        return "";
    }
}

// 0x543060
void mainmenu_ui_load_select_prev(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (window->selected_index > 0) {
        window->selected_index--;
        if (window->selected_index < window->top_index) {
            scrollbar_ui_control_set(mainmenu_ui_scrollbar, SCROLLBAR_CURRENT_VALUE, window->selected_index);
        }
        gsound_play_sfx(0, 1);
        mainmenu_ui_load_saveinfo_reload();
        window->refresh_func(NULL);
        scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);
    }
}

// 0x5430D0
void mainmenu_ui_load_select_next(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (window->selected_index < window->cnt - 1) {
        window->selected_index++;
        if (window->selected_index > window->top_index + window->content_rect.height / 20) {
            scrollbar_ui_control_set(mainmenu_ui_scrollbar, SCROLLBAR_CURRENT_VALUE, window->selected_index - window->content_rect.height / 20);
        }
        gsound_play_sfx(0, 1);
        mainmenu_ui_load_saveinfo_reload();
        window->refresh_func(NULL);
        scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);
    }
}

// 0x543160
bool mainmenu_ui_load_game_handle_delete(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (window->selected_index == -1) {
        return false;
    }

    // "Are you sure you want to delete the save game?"
    if (mainmenu_ui_confirm(5150) != TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
        return false;
    }

    if (!gamelib_delete(mainmenu_ui_gsl.names[window->selected_index])) {
        return false;
    }

    if (mainmenu_ui_gsi_loaded) {
        gamelib_saveinfo_exit(&mainmenu_ui_gsi);
        mainmenu_ui_gsi_loaded = false;
    }

    gamelib_savelist_destroy(&mainmenu_ui_gsl);
    gamelib_savelist_create(&mainmenu_ui_gsl);

    gamelib_savelist_sort(&mainmenu_ui_gsl, GAME_SAVE_LIST_ORDER_DATE, false);
    window->selected_index = -1;
    window->cnt--;
    window->refresh_func(NULL);

    return true;
}

// 0x543220
bool mainmenu_ui_quick_load(void)
{
    const char* path;
    char name[256];
    bool rc;

    if (mainmenu_ui_active) {
        return false;
    }

    path = gamelib_last_save_name();
    if (path[0] == '\0' || !gamelib_saveinfo_load(path, &mainmenu_ui_gsi)) {
        return false;
    }
    mainmenu_ui_gsi_loaded = true;

    strncpy(name, path, 8);
    name[8] = '\0';

    rc = mainmenu_ui_load_game_by_name(name);
    if (mainmenu_ui_gsi_loaded) {
        gamelib_saveinfo_exit(&mainmenu_ui_gsi);
        mainmenu_ui_gsi_loaded = false;
    }

    return rc;
}

// 0x5432B0
bool mainmenu_ui_load_game_by_name(const char* name)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mainmenu_ui_close_game_screens();

    if (mainmenu_ui_gsi.version == 25) {
        mainmenu_ui_reset();
        player_destroy();

        if (gamelib_load(name)) {
            gamelib_current_mode_name_set(mainmenu_ui_gsi.module_name);

            mes_file_entry.num = 5000; // "Game Loaded Successfully."
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

            ui_message.type = UI_MSG_TYPE_EXCLAMATION;
            ui_message.str = mes_file_entry.str;
            ui_display_msg(&ui_message);

            mainmenu_ui_start_new_game = false;
            mainmenu_ui_dismiss();

            return true;
        }

        mainmenu_ui_reset();
    }

    mes_file_entry.num = 5001; // "Save Game Corrupt!  Load Failed!"
    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_EXCLAMATION;
    ui_message.str = mes_file_entry.str;
    ui_display_msg(&ui_message);

    return false;
}

// 0x543380
void mainmenu_ui_save_game_create(void)
{
    MainMenuWindowInfo* window;
    int64_t pc_obj;
    PcLens pc_lens;

    mainmenu_ui_window_type = MM_WINDOW_SAVE_GAME;
    window = main_menu_window_info[mainmenu_ui_window_type];

    mainmenu_ui_close_game_screens();
    gamelib_savelist_create(&mainmenu_ui_gsl);
    gamelib_savelist_sort(&mainmenu_ui_gsl, GAME_SAVE_LIST_ORDER_DATE, false);
    mainmenu_ui_textedit_buffer[0] = '\0';
    window->cnt = mainmenu_ui_gsl.count + 1;
    if (mainmenu_ui_gsl.count != 0) {
        window->selected_index = 1;
    } else {
        window->selected_index = -1;
    }

    window->max_top_index = window->cnt - window->content_rect.height / 20 - 1;
    if (window->max_top_index < 0) {
        window->max_top_index = 0;
    }

    window->top_index = 0;
    mainmenu_ui_create_window_func(false);

    if (!main_menu_button_create_ex(&mainmenu_ui_save_game_texttoggle_button, 0, 0, 2)) {
        tig_debug_printf("MainMenu-UI: mainmenu_ui_create_save_game: ERROR: Failed to create button.\n");
        exit(EXIT_FAILURE);
    }

    mainmenu_ui_scrollbar_info.scrollbar_rect = mainmenu_ui_scrollbar_rect;
    mainmenu_ui_scrollbar_info.flags = SB_INFO_VALID
        | SB_INFO_CONTENT_RECT
        | SB_INFO_MAX_VALUE
        | SB_INFO_MIN_VALUE
        | SB_INFO_LINE_STEP
        | SB_INFO_VALUE
        | SB_INFO_ON_VALUE_CHANGED
        | SB_INFO_ON_REFRESH;
    mainmenu_ui_scrollbar_info.min_value = 0;
    mainmenu_ui_scrollbar_info.max_value = window->max_top_index + 1;
    if (mainmenu_ui_scrollbar_info.max_value > 0) {
        mainmenu_ui_scrollbar_info.max_value--;
    }
    mainmenu_ui_scrollbar_info.value = 0;
    mainmenu_ui_scrollbar_info.line_step = 1;
    mainmenu_ui_scrollbar_info.on_value_changed = mainmenu_ui_scrollbar_value_changed;
    mainmenu_ui_scrollbar_info.on_refresh = mainmenu_ui_scrollbar_refresh_rect;
    mainmenu_ui_scrollbar_info.content_rect.x = 34;
    mainmenu_ui_scrollbar_info.content_rect.y = 110;
    mainmenu_ui_scrollbar_info.content_rect.width = 195;
    mainmenu_ui_scrollbar_info.content_rect.height = 232;
    scrollbar_ui_control_create(&mainmenu_ui_scrollbar, &mainmenu_ui_scrollbar_info, mainmenu_ui_window_handle);

    pc_obj = player_get_local_pc_obj();

    pc_lens.window_handle = mainmenu_ui_window_handle;
    pc_lens.rect = &mainmenu_ui_save_load_pc_lens_rect;
    tig_art_interface_id_create(746, 0, 0, 0, &(pc_lens.art_id));

    if (pc_obj != OBJ_HANDLE_NULL) {
        location_origin_set(obj_field_int64_get(pc_obj, OBJ_F_LOCATION));
    }

    intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &pc_lens);
    scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);

    tig_window_display();
}

// 0x543580
void mainmenu_ui_save_game_destroy(void)
{
    scrollbar_ui_control_destroy(mainmenu_ui_scrollbar);

    if (mainmenu_ui_gsi_loaded) {
        gamelib_saveinfo_exit(&mainmenu_ui_gsi);
        mainmenu_ui_gsi_loaded = false;
    }

    gamelib_savelist_destroy(&mainmenu_ui_gsl);
    intgame_pc_lens_do(PC_LENS_MODE_NONE, NULL);
}

// 0x5435D0
bool mainmenu_ui_save_game_execute(int btn)
{
    int v1;
    char fname[COMPAT_MAX_FNAME];
    const char* name;
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int num;

    (void)btn;

    v1 = main_menu_window_info[mainmenu_ui_window_type]->selected_index;
    if (v1 == -1) {
        return false;
    }

    if (v1 > 0) {
        name = strcpy(fname, mainmenu_ui_gsl.names[v1 - 1]);
        strcpy(mainmenu_ui_textedit_buffer, mainmenu_ui_gsl.names[v1 - 1] + 8);
        fname[8] = '\0';

        if (mainmenu_ui_confirm(5064)) {
            return false;
        }
    } else {
        gamelib_savelist_sort(&mainmenu_ui_gsl, GAME_SAVE_LIST_ORDER_NAME, false);

        if (mainmenu_ui_gsl.count > 0) {
            if (SDL_toupper(mainmenu_ui_gsl.names[0][4]) == 'A') {
                if (mainmenu_ui_gsl.count > 1
                    && mainmenu_ui_gsl.names[1] != NULL) {
                    strncpy(fname, mainmenu_ui_gsl.names[1], 8);
                    fname[8] = '\0';
                    num = atoi(&(fname[4])) + 1;
                    if (num >= 9999) {
                        return false;
                    }

                    strcpy(fname, mainmenu_ui_gsl.names[0]);
                    snprintf(&(fname[4]), sizeof(fname) - 4, "%04d", num);
                    name = fname;
                } else {
                    name = "Slot0000";
                }
            } else {
                if (mainmenu_ui_gsl.names[0] != NULL) {
                    strncpy(fname, mainmenu_ui_gsl.names[0], 8);
                    fname[8] = '\0';
                    num = atoi(&(fname[4])) + 1;
                    if (num >= 9999) {
                        return false;
                    }

                    strcpy(fname, mainmenu_ui_gsl.names[0]);
                    snprintf(&(fname[4]), sizeof(fname) - 4, "%04d", num);
                    name = fname;
                } else {
                    name = "Slot0000";
                }
            }
        } else {
            name = "Slot0000";
        }
    }

    mainmenu_ui_close_game_screens();

    if (mainmenu_ui_textedit_buffer[0] != '\0') {
        char* pch = mainmenu_ui_textedit_buffer;
        while (*pch == ' ') {
            pch++;
        }

        if (*pch == '\0') {
            mainmenu_ui_textedit_buffer[0] = '\0';
        }
    }

    if (mainmenu_ui_textedit_buffer[0] == '\0') {
        strcpy(mainmenu_ui_textedit_buffer, name);
    }

    if (!gamelib_save(name, mainmenu_ui_textedit_buffer)) {
        mes_file_entry.num = 5003; // "Save Game Corrupt!  Save Failed!"
        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_EXCLAMATION;
        ui_message.str = mes_file_entry.str;
        ui_display_msg(&ui_message);

        return false;
    }

    mes_file_entry.num = 5002; // "Game Saved Successfully."
    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_EXCLAMATION;
    ui_message.str = mes_file_entry.str;
    ui_display_msg(&ui_message);
    mainmenu_ui_dismiss();
    mainmenu_ui_textedit_buffer[0] = '\0';

    return true;
}

// 0x543850
bool mainmenu_ui_save_game_button_pressed(tig_button_handle_t button_handle)
{
    if (button_handle != mainmenu_ui_save_game_texttoggle_button.button_handle) {
        return false;
    }

    mainmenu_ui_saveinfo_text_shown = false;
    main_menu_window_info[mainmenu_ui_window_type]->refresh_func(&mainmenu_ui_saveinfo_thumbnail_dst_rect);

    return true;
}

// 0x543890
bool mainmenu_ui_save_game_button_released(tig_button_handle_t button_handle)
{
    MainMenuWindowInfo* window;
    int index;

    window = main_menu_window_info[mainmenu_ui_window_type];
    for (index = 0; index < 2; index++) {
        if (button_handle == window->buttons[index].button_handle) {
            mainmenu_ui_window_cmd(index + 2);
            return true;
        }
    }

    if (button_handle == window->buttons[2].button_handle) {
        gsound_play_sfx(0, 1);
        mainmenu_ui_save_game_handle_delete();
        return true;
    }

    if (button_handle == mainmenu_ui_save_game_texttoggle_button.button_handle) {
        mainmenu_ui_saveinfo_text_shown = true;
        window->refresh_func(&mainmenu_ui_saveinfo_thumbnail_dst_rect);
        return true;
    }

    return false;
}

// 0x543920
void mainmenu_ui_save_game_mouse_up(int x, int y)
{
    MainMenuWindowInfo* window;

    (void)x;

    window = main_menu_window_info[mainmenu_ui_window_type];
    window->selected_index = window->top_index + y / 20;
    if (window->selected_index >= window->cnt) {
        window->selected_index = window->cnt - 1;
    }
    mainmenu_ui_save_saveinfo_reload();
    window->refresh_func(NULL);
    scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);
}

// 0x543990
void mainmenu_ui_save_game_refresh(TigRect* rect)
{
    MainMenuWindowInfo* window;
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigVideoBufferData video_buffer_data;
    TigWindowBlitInfo win_blit_info;
    MesFileEntry mes_file_entry;
    char str[20];
    tig_font_handle_t font;
    int area;
    char* area_name;
    char* story_state_desc;

    window = main_menu_window_info[mainmenu_ui_window_type];
    tig_art_interface_id_create(window->background_art_num, 0, 0, 0, &art_id);
    tig_art_frame_data(art_id, &art_frame_data);
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        src_rect.x = mainmenu_ui_scrollbar_rect.x;
        src_rect.y = mainmenu_ui_scrollbar_rect.y;
        src_rect.width = mainmenu_ui_scrollbar_rect.width + 1;
        src_rect.height = mainmenu_ui_scrollbar_rect.height + 1;

        dst_rect.x = mainmenu_ui_scrollbar_rect.x;
        dst_rect.y = mainmenu_ui_scrollbar_rect.y;
        dst_rect.width = mainmenu_ui_scrollbar_rect.width + 1;
        dst_rect.height = mainmenu_ui_scrollbar_rect.height + 1;

        art_blit_info.art_id = art_id;
        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info);
    }

    if (rect == NULL
        || (window->content_rect.x < rect->x + rect->width
            && window->content_rect.y < rect->y + rect->height
            && rect->x < window->content_rect.x + window->content_rect.width
            && rect->y < window->content_rect.y + window->content_rect.height)) {
        dst_rect = window->content_rect;

        if (tig_window_fill(mainmenu_ui_window_handle, &dst_rect, tig_color_make(0, 0, 0)) != TIG_OK) {
            tig_debug_printf("mmUIMPSaveGameRefreshFunc: ERROR: tig_window_fill2 failed!\n");
            exit(EXIT_FAILURE);
        }

        int max_y = dst_rect.y + dst_rect.height - 1;

        dst_rect.height = 20;
        dst_rect.width -= 4;

        for (int idx = window->top_index; idx < window->cnt; idx++) {
            if (dst_rect.y >= max_y) {
                break;
            }

            font = window->selected_index == idx ? mainmenu_ui_selected_font : mainmenu_ui_text_fonts[0];
            tig_font_push(font);

            char* name;
            if (idx > 0) {
                name = mainmenu_ui_savelist_display_name(idx - 1);
            } else if (idx == 0) {
                if (textedit_ui_is_focused()) {
                    name = NULL;
                    mainmenu_ui_textedit_draw(mainmenu_ui_textedit_buffer, &dst_rect, font);
                } else {
                    mes_file_entry.num = 5055;
                    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                    name = mes_file_entry.str;
                }
            }

            if (name != NULL && *name != '\0') {
                mainmenu_ui_draw_text_ellipsized(name, &dst_rect, font);
            }

            dst_rect.y += 20;
            tig_font_pop();
        }
    }

    if (rect == NULL
        || (mainmenu_ui_saveinfo_thumbnail_dst_rect.x < rect->x + rect->width
            && mainmenu_ui_saveinfo_thumbnail_dst_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_saveinfo_thumbnail_dst_rect.x + mainmenu_ui_saveinfo_thumbnail_dst_rect.width
            && rect->y < mainmenu_ui_saveinfo_thumbnail_dst_rect.y + mainmenu_ui_saveinfo_thumbnail_dst_rect.height)) {
        if (window->selected_index > 0) {
            if (!mainmenu_ui_gsi_loaded
                && mainmenu_ui_gsl.count > 0
                && gamelib_saveinfo_load(mainmenu_ui_gsl.names[window->selected_index - 1], &mainmenu_ui_gsi)) {
                mainmenu_ui_gsi_loaded = true;
            }

            if (mainmenu_ui_gsi_loaded) {
                if (mainmenu_ui_gsi.thumbnail_video_buffer != NULL) {
                    if (tig_video_buffer_data(mainmenu_ui_gsi.thumbnail_video_buffer, &video_buffer_data) == TIG_OK) {
                        mainmenu_ui_saveinfo_thumbnail_src_rect.width = video_buffer_data.width;
                        mainmenu_ui_saveinfo_thumbnail_src_rect.height = video_buffer_data.height;

                        mainmenu_ui_saveinfo_thumbnail_dst_rect.width = video_buffer_data.width;
                        mainmenu_ui_saveinfo_thumbnail_dst_rect.height = video_buffer_data.height;

                        win_blit_info.type = TIG_WINDOW_BLIT_VIDEO_BUFFER_TO_WINDOW;
                        win_blit_info.vb_blit_flags = 0;
                        win_blit_info.src_video_buffer = mainmenu_ui_gsi.thumbnail_video_buffer;
                        win_blit_info.src_rect = &mainmenu_ui_saveinfo_thumbnail_src_rect;
                        win_blit_info.dst_window_handle = mainmenu_ui_window_handle;
                        win_blit_info.dst_rect = &mainmenu_ui_saveinfo_thumbnail_dst_rect;

                        if (tig_window_blit(&win_blit_info) != TIG_OK) {
                            tig_debug_printf("MMUI: ERROR: mmUIMPSaveGameRefreshFunc FAILED to refresh!\n");
                        }
                    }
                }

                font = mainmenu_ui_text_fonts[0];
                tig_font_push(font);

                if (mainmenu_ui_saveinfo_text_shown) {
                    mainmenu_ui_draw_centered_text(mainmenu_ui_gsi.pc_name, &mainmenu_ui_saveinfo_pc_name_rect, font);
                    if (mainmenu_ui_gsi.pc_portrait != 0) {
                        portrait_draw_native(OBJ_HANDLE_NULL,
                            mainmenu_ui_gsi.pc_portrait,
                            mainmenu_ui_window_handle,
                            mainmenu_ui_saveinfo_portrait_rect.x,
                            mainmenu_ui_saveinfo_portrait_rect.y);
                    }

                    mes_file_entry.num = 5051; // "Level %d"
                    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                    snprintf(str, sizeof(str), mes_file_entry.str, mainmenu_ui_gsi.pc_level);
                    mainmenu_ui_draw_centered_text(str, &mainmenu_ui_saveinfo_level_rect, font);

                    mainmenu_ui_draw_centered_text(mainmenu_ui_gsi.description, &mainmenu_ui_saveinfo_description_rect, font);

                    if (map_by_type(MAP_TYPE_START_MAP) == mainmenu_ui_gsi.map) {
                        area = area_get_nearest_area_in_range(mainmenu_ui_gsi.pc_location, true);
                    } else if (!map_get_area(mainmenu_ui_gsi.map, &area)) {
                        area = 0;
                    }

                    if (area > 0) {
                        area_name = area_get_name(area);
                    } else {
                        mes_file_entry.num = 5050; // "Unknown location."
                        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                        area_name = mes_file_entry.str;
                    }

                    mainmenu_ui_draw_centered_text(area_name, &mainmenu_ui_saveinfo_area_rect, font);

                    datetime_format_date(&(mainmenu_ui_gsi.datetime), str, sizeof(str));
                    mainmenu_ui_draw_right_aligned_text(str, &mainmenu_ui_saveinfo_date_rect, font);

                    datetime_format_time(&(mainmenu_ui_gsi.datetime), str, sizeof(str));
                    mainmenu_ui_draw_right_aligned_text(str, &mainmenu_ui_saveinfo_time_rect, font);

                    story_state_desc = script_story_state_info(mainmenu_ui_gsi.story_state);
                    if (story_state_desc != NULL && *story_state_desc != '\0') {
                        mmUITextWriteCenteredToArray(story_state_desc, mainmenu_ui_story_state_rects, 4, font);
                    }
                }

                tig_font_pop();
            } else {
                tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_saveinfo_thumbnail_dst_rect, tig_color_make(0, 0, 0));

                font = mainmenu_ui_text_fonts_red[0];
                tig_font_push(font);
                mes_file_entry.num = 5005; // "Empty"
                mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_saveinfo_pc_name_rect, font);
                tig_font_pop();
            }
        } else {
            tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_saveinfo_thumbnail_dst_rect, tig_color_make(0, 0, 0));
        }

        tig_art_interface_id_create(748, 0, 0, 0, &art_id);
        if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK
            && tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = 281;
            dst_rect.y = 55;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.height;

            art_blit_info.art_id = art_id;
            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            if (tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info) != TIG_OK) {
                // NOTE: verified against 0x5440D9 - the binary really contains the "mmUIMPLoadGameRefreshFunc" string (0x5C5AC0) here.
                tig_debug_printf("MMUI: mmUIMPLoadGameRefreshFunc: ERROR: FAILED to refresh!\n");
            }
        }
    }

    mmUIWinRefreshScrollBar();
}

// 0x544100
void mainmenu_ui_textedit_draw(const char* str, TigRect* rect, tig_font_handle_t font)
{
    char mutable_str[200];
    TigFont font_desc;
    TigRect text_rect;
    int pos;

    strcpy(mutable_str, str);

    tig_font_push(font);
    font_desc.width = 0;
    font_desc.height = 0;
    font_desc.str = mutable_str;
    font_desc.flags = 0;
    tig_font_measure(&font_desc);

    text_rect = *rect;

    if (font_desc.width < rect->width) {
        text_rect.x += (rect->width - font_desc.width) / 2;
        text_rect.width = font_desc.width;
        mainmenu_ui_draw_text(mutable_str, &text_rect, font, 0);
    }

    // CE: There's a bug in the original game regarding cursor positioning. It
    // simply places the cursor at the end of the editable string, but the
    // text-edit UI actually keeps track of the cursor position with the
    // keyboard arrow keys. This isn't reflected in the UI, though backspace and
    // delete operate at the correct indexes. The fix is to render cursor
    // separately in a second pass over the already rendered text.
    pos = textedit_ui_pos_get();

    // Clamp editable string at cursor position, and re-measure part of the
    // string before I-beam.
    mutable_str[pos] = '\0';
    font_desc.width = 0;
    font_desc.height = 0;
    tig_font_measure(&font_desc);
    text_rect.x += font_desc.width;

    // Measure the width of I-beam.
    mutable_str[0] = '|';
    mutable_str[1] = '\0';
    font_desc.width = 0;
    font_desc.height = 0;
    tig_font_measure(&font_desc);
    text_rect.width = font_desc.width;

    // Put I-beam into the right place.
    mainmenu_ui_draw_text(mutable_str, &text_rect, font, 0);

    tig_font_pop();
}

// 0x544210
void mainmenu_ui_save_select_prev(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (window->selected_index > 0) {
        window->selected_index--;
        gsound_play_sfx(0, 1);
        mainmenu_ui_save_saveinfo_reload();
        window->refresh_func(NULL);
    }
}

// 0x544250
void mainmenu_ui_save_select_next(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (window->selected_index < window->cnt - 1) {
        window->selected_index++;
        gsound_play_sfx(0, 1);
        mainmenu_ui_save_saveinfo_reload();
        window->refresh_func(NULL);
    }
}

// 0x544290
void mainmenu_ui_save_saveinfo_reload(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    mainmenu_ui_textedit_unfocus();

    if (!mainmenu_ui_gsi_loaded) {
        if (window->selected_index == 0) {
            mainmenu_ui_textedit_buffer[0] = 0;
            mainmenu_ui_textedit_focus(mainmenu_ui_textedit_buffer, 23);
        }
        return;
    }

    gamelib_saveinfo_exit(&mainmenu_ui_gsi);

    mainmenu_ui_gsi_loaded = false;
    if (window->selected_index > 0) {
        if (gamelib_saveinfo_load(mainmenu_ui_gsl.names[window->selected_index - 1], &mainmenu_ui_gsi)) {
            mainmenu_ui_gsi_loaded = true;
        }
    } else {
        mainmenu_ui_textedit_buffer[0] = 0;
        mainmenu_ui_textedit_focus(mainmenu_ui_textedit_buffer, 23);
    }
}

// 0x544320
bool mainmenu_ui_save_game_handle_delete(void)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];

    if (window->selected_index <= 0) {
        return false;
    }

    // "Are you sure you want to delete the save game?"
    if (mainmenu_ui_confirm(5150) != TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
        return false;
    }

    if (!gamelib_delete(mainmenu_ui_gsl.names[window->selected_index - 1])) {
        return false;
    }

    if (mainmenu_ui_gsi_loaded) {
        gamelib_saveinfo_exit(&mainmenu_ui_gsi);
        mainmenu_ui_gsi_loaded = false;
    }

    gamelib_savelist_destroy(&mainmenu_ui_gsl);
    gamelib_savelist_create(&mainmenu_ui_gsl);
    gamelib_savelist_sort(&mainmenu_ui_gsl, GAME_SAVE_LIST_ORDER_DATE, false);
    window->selected_index = -1;
    window->cnt--;
    window->refresh_func(NULL);

    return true;
}

// 0x544440
void mainmenu_ui_last_save_create(void)
{
    const char* path;
    TigRect rect;
    MesFileEntry mes_file_entry;
    char name[256];

    mainmenu_ui_window_type = MM_WINDOW_LAST_SAVE_GAME;
    mainmenu_ui_create_window();
    mainmenu_ui_active = false;

    path = gamelib_last_save_name();
    if (path[0] != '\0') {
        if (mainmenu_ui_gsi_loaded) {
            gamelib_saveinfo_exit(&mainmenu_ui_gsi);
            mainmenu_ui_gsi_loaded = false;
        }

        if (gamelib_saveinfo_load(path, &mainmenu_ui_gsi)) {
            mainmenu_ui_gsi_loaded = true;
            mainmenu_ui_active = true;

            rect.x = 0;
            rect.y = 0;
            rect.width = 800;
            rect.height = 600;
            tig_window_fill(mainmenu_ui_window_handle, &rect, tig_color_make(0, 0, 0));

            rect.x = 340;
            rect.y = 210;
            rect.width = 200;
            rect.height = 20;

            mes_file_entry.num = 5; // "Loading Save Game..."
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
            tig_window_text_write(mainmenu_ui_window_handle, mes_file_entry.str, &rect);

            tig_mouse_hide();
            tig_window_display();
            strncpy(name, path, 8);
            name[8] = '\0';
            mainmenu_ui_load_game_by_name(name);

            if (mainmenu_ui_gsi_loaded) {
                gamelib_saveinfo_exit(&mainmenu_ui_gsi);
                mainmenu_ui_gsi_loaded = false;
            }
        }
    } else {
        mainmenu_ui_active = true;
        mainmenu_ui_close(true);
        mainmenu_ui_skip_window_stack_push = true;
    }

    tig_mouse_show();
}

// 0x5445F0
void mainmenu_ui_intro_create(void)
{
    mainmenu_ui_window_type = MM_WINDOW_INTRO;
    gmovie_play(1, 0, 0);
    gmovie_play(7, 0, 0);
    mainmenu_ui_num_windows++;
    mainmenu_ui_pop_window_stack();
    mainmenu_ui_window_type = MM_WINDOW_SINGLE_PLAYER;
    mainmenu_ui_open();
    mainmenu_ui_skip_window_stack_push = true;
}

// 0x544640
void mainmenu_ui_credits_create(void)
{
    mainmenu_ui_window_type = MM_WINDOW_CREDITS;
    mainmenu_ui_num_windows++;
    mainmenu_ui_pop_window_stack();
    mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
    mainmenu_ui_open();
    mainmenu_ui_skip_window_stack_push = true;
    slide_ui_start(SLIDE_UI_TYPE_CREDITS);

    if (mainmenu_ui_active) {
        if (main_menu_window_info[mainmenu_ui_window_type]->refresh_func != NULL) {
            main_menu_window_info[mainmenu_ui_window_type]->refresh_func(NULL);
        }
    }
}

// 0x544690
void mainmenu_ui_last_save_refresh(TigRect* rect)
{
    (void)rect;
}

// 0x5446A0
void mainmenu_ui_create_single_player(void)
{
    mainmenu_ui_window_type = MM_WINDOW_SINGLE_PLAYER;
    mainmenu_ui_create_window();
    mainmenu_ui_draw_version();
    intgame_interface_background_refresh();
}

// 0x5446D0
void mainmenu_ui_pick_new_or_pregen_create(void)
{
    mainmenu_ui_charedit_mode = CHAREDIT_MODE_CREATE;
    mainmenu_ui_window_type = MM_WINDOW_PICK_NEW_OR_PREGEN;
    mainmenu_ui_create_window();
    mainmenu_ui_draw_version();
}

// 0x5446F0
void mainmenu_ui_new_char_create(void)
{
    PlayerCreateInfo player_create_info;
    MesFileEntry mes_file_entry;

    mainmenu_ui_new_char_hover_mode = MMUI_NEW_CHAR_HOVER_MODE_BACKGROUND;
    mainmenu_ui_window_type = MM_WINDOW_NEW_CHAR;

    player_create_info_init(&player_create_info);
    player_create_info.loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    player_create_info.basic_prototype = 16066;
    if (!player_obj_create_player(&player_create_info)) {
        tig_debug_printf("MainMenu-UI: mainmenu_ui_create_pick_new_or_pregen: ERROR: Player Creation Failed!\n");
        exit(EXIT_FAILURE);
    }

    // CE: There's a bug in the original game — the name persists when
    // navigating back and forth to the "New Char" screen. Everything else
    // (gender/race/background, just a moment above) resets, but not the name.
    mes_file_entry.num = 500; // "Choose Name"
    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
    strncpy(mainmenu_ui_textedit_buffer, mes_file_entry.str, 23);

    mainmenu_ui_create_window();
    if (!main_menu_button_create(&mainmenu_ui_new_char_name_button, mainmenu_ui_new_char_name_rect.width + 2, mainmenu_ui_new_char_name_rect.height + 2)) {
        tig_debug_printf("MainMenu-UI: mainmenu_ui_create_new_char: ERROR: Failed to create button.\n");
    }
}

// 0x5447B0
void mainmenu_ui_new_char_refresh(TigRect* rect)
{
    int64_t pc_obj;
    char* str;
    MesFileEntry mes_file_entry;

    pc_obj = player_get_local_pc_obj();
    mmUISharedCharRefreshFunc(pc_obj, rect);

    if (rect == NULL
        || (mainmenu_ui_shared_char_name_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_name_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_name_rect.x + mainmenu_ui_shared_char_name_rect.width
            && rect->y < mainmenu_ui_shared_char_name_rect.y + mainmenu_ui_shared_char_name_rect.height)) {
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_name_rect, tig_color_make(0, 0, 0)) != TIG_OK) {
            tig_debug_printf("MainMenu-UI: mmUINewCharRefreshFunc: ERROR: Window Fill Failed.\n");
        }

        str = mainmenu_ui_textedit_buffer[0] != '\0' ? mainmenu_ui_textedit_buffer : " ";

        // NOTE: verified against 0x54489F - original calls mes_get_msg(500) and discards the result.
        mes_file_entry.num = 500; // "Choose Name"
        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

        if (textedit_ui_is_focused()) {
            mainmenu_ui_textedit_draw(str, &mainmenu_ui_shared_char_name_rect, mainmenu_ui_text_fonts_red[1]);
        } else {
            mainmenu_ui_draw_centered_text(str, &mainmenu_ui_shared_char_name_rect, mainmenu_ui_text_fonts_red[1]);
        }
    }
}

// 0x5448E0
void mmUISharedCharRefreshFunc(int64_t obj, TigRect* rect)
{
    int race;
    int gender;
    int portrait;
    int background;
    MesFileEntry mes_file_entry;
    tig_font_handle_t font;
    int index;
    char str[32];

    race = stat_level_get(obj, STAT_RACE);
    gender = stat_level_get(obj, STAT_GENDER);

    if (rect == NULL
        || (mainmenu_ui_shared_char_portrait_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_portrait_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_portrait_rect.x + mainmenu_ui_shared_char_portrait_rect.width
            && rect->y < mainmenu_ui_shared_char_portrait_rect.y + mainmenu_ui_shared_char_portrait_rect.height)) {
        portrait = portrait_get(obj);
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_portrait_rect, tig_color_make(0, 0, 0)) != TIG_OK) {
            tig_debug_printf("MainMenu-UI: mmUINewCharRefreshFunc: ERROR: Window Fill #0 Failed.\n");
        }
        portrait_draw_128x128(obj,
            portrait,
            mainmenu_ui_window_handle,
            mainmenu_ui_shared_char_portrait_rect.x,
            mainmenu_ui_shared_char_portrait_rect.y);
    }

    font = mainmenu_ui_text_fonts_red[0];
    tig_font_push(font);
    if (rect == NULL
        || (mainmenu_ui_shared_char_gender_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_gender_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_gender_rect.x + mainmenu_ui_shared_char_gender_rect.width
            && rect->y < mainmenu_ui_shared_char_gender_rect.y + mainmenu_ui_shared_char_gender_rect.height)) {
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_gender_rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            mes_file_entry.num = 740 + gender; // "Female", "Male"
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
            mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_shared_char_gender_rect, font);
        }
    }

    if (rect == NULL
        || (mainmenu_ui_shared_char_race_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_race_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_race_rect.x + mainmenu_ui_shared_char_race_rect.width
            && rect->y < mainmenu_ui_shared_char_race_rect.y + mainmenu_ui_shared_char_race_rect.height)) {
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_race_rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            mes_file_entry.num = 720 + race; // "Human", "Dwarf", "Elf", etc.
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
            mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_shared_char_race_rect, font);
        }
    }

    if (rect == NULL
        || (mainmenu_ui_shared_char_background_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_background_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_background_rect.x + mainmenu_ui_shared_char_background_rect.width
            && rect->y < mainmenu_ui_shared_char_background_rect.y + mainmenu_ui_shared_char_background_rect.height)) {
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_background_rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            background = background_text_get(obj);
            mainmenu_ui_draw_centered_text(background_description_get_name(background),
                &mainmenu_ui_shared_char_background_rect,
                font);
        }
    }
    tig_font_pop();

    if (rect == NULL
        || (mainmenu_ui_shared_char_desc_view_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_desc_view_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_desc_view_rect.x + mainmenu_ui_shared_char_desc_view_rect.width
            && rect->y < mainmenu_ui_shared_char_desc_view_rect.y + mainmenu_ui_shared_char_desc_view_rect.height)) {
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_desc_view_rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            if (mainmenu_ui_new_char_hover_mode == MMUI_NEW_CHAR_HOVER_MODE_GENDER) {
                font = mainmenu_ui_text_fonts[0];
                tig_font_push(font);
                mes_file_entry.num = 742 + gender;
                mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                if (mes_file_entry.str[0] != '\0') {
                    mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_shared_char_desc_body_rect, font);
                } else {
                    tig_debug_printf("MainMenu-UI: mmUISharedCharRefreshFunc: ERROR: Failed to find Racial Description!\n");
                }
                tig_font_pop();
            } else {
                background = background_text_get(obj);
                if (background > 1000 && mainmenu_ui_new_char_hover_mode == MMUI_NEW_CHAR_HOVER_MODE_BACKGROUND) {
                    font = mainmenu_ui_text_fonts[0];
                    tig_font_push(font);
                    mainmenu_ui_draw_centered_text(background_description_get_body(background),
                        &mainmenu_ui_shared_char_desc_view_rect,
                        font);
                    tig_font_pop();
                } else {
                    font = mainmenu_ui_text_fonts[1];
                    tig_font_push(font);
                    mes_file_entry.num = 745; // "Race"
                    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                    if (mes_file_entry.str[0] != '\0') {
                        mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_shared_char_desc_title_rect, font);
                    } else {
                        tig_debug_printf("MainMenu-UI: mmUISharedCharRefreshFunc: ERROR: Failed to find Racial Description!\n");
                    }
                    tig_font_pop();

                    font = mainmenu_ui_text_fonts[0];
                    tig_font_push(font);
                    mes_file_entry.num = 770 + 2 * race + gender;
                    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                    if (mes_file_entry.str[0] != '\0') {
                        mainmenu_ui_draw_centered_text(mes_file_entry.str, &mainmenu_ui_shared_char_desc_body_rect, font);
                    } else {
                        tig_debug_printf("MainMenu-UI: mmUISharedCharRefreshFunc: ERROR: Failed to find Racial Description!\n");
                    }
                    tig_font_pop();
                }
            }
        }
    }

    if (rect == NULL
        || (mainmenu_ui_shared_char_stats_view_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_stats_view_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_stats_view_rect.x + mainmenu_ui_shared_char_stats_view_rect.width
            && rect->y < mainmenu_ui_shared_char_stats_view_rect.y + mainmenu_ui_shared_char_stats_view_rect.height)) {
        font = mainmenu_ui_text_fonts[1];
        tig_font_push(font);
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_stats_title_rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            mes_file_entry.num = 739; // "Initial Stats"
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
            mainmenu_ui_draw_centered_text(mes_file_entry.str,
                &mainmenu_ui_shared_char_stats_title_rect,
                font);
        }
        tig_font_pop();

        font = mainmenu_ui_text_fonts[0];
        tig_font_push(font);
        for (index = 0; index < 15; index++) {
            if (tig_window_fill(mainmenu_ui_window_handle, &(mainmenu_ui_stat_rects[index]), tig_color_make(0, 0, 0)) == TIG_OK) {
                snprintf(str, sizeof(str),
                    "%s: %d",
                    stat_short_name(mainmenu_ui_stat_list[index]),
                    stat_level_get(obj, mainmenu_ui_stat_list[index]));
                mainmenu_ui_draw_text(str, &(mainmenu_ui_stat_rects[index]), font, 0);
            }
        }
        tig_font_pop();
    }
}

// 0x544FF0
bool mainmenu_ui_new_char_button_released(tig_button_handle_t button_handle)
{
    int index;
    int64_t pc_obj;
    MainMenuWindowInfo* window;
    int portrait;
    int background;

    for (index = 0; index < 10; index++) {
        if (button_handle == mainmenu_ui_new_char_buttons[index].button_handle) {
            break;
        }
    }

    if (index >= 10) {
        if (button_handle == mainmenu_ui_new_char_name_button.button_handle
            && mainmenu_ui_textedit_focused_buffer() == NULL) {
            strcpy(mainmenu_ui_textedit_saved_buffer, mainmenu_ui_textedit_buffer);
            mainmenu_ui_textedit_buffer[0] = '\0';
            mainmenu_ui_textedit_focus(mainmenu_ui_textedit_buffer, 23);
        }

        return true;
    }

    pc_obj = player_get_local_pc_obj();
    window = main_menu_window_info[mainmenu_ui_window_type];

    switch (index) {
    case 0:
        if (window->execute_func != NULL && !window->execute_func(0)) {
            return true;
        }

        mainmenu_ui_close(false);
        mainmenu_ui_window_type = MM_WINDOW_CHAREDIT;
        mainmenu_ui_open();
        return true;
    case 1:
        mainmenu_ui_close(true);
        return true;
    case 2:
        portrait = portrait_get(pc_obj);
        if (!portrait_find_prev(pc_obj, &portrait)) {
            portrait_find_last(pc_obj, &portrait);
        }
        obj_field_int32_set(pc_obj, OBJ_F_CRITTER_PORTRAIT, portrait);
        window->refresh_func(&mainmenu_ui_shared_char_portrait_rect);
        return true;
    case 3:
        portrait = portrait_get(pc_obj);
        if (!portrait_find_next(pc_obj, &portrait)) {
            portrait_find_first(pc_obj, &portrait);
        }
        obj_field_int32_set(pc_obj, OBJ_F_CRITTER_PORTRAIT, portrait);
        window->refresh_func(&mainmenu_ui_shared_char_portrait_rect);
        return true;
    case 4:
        if (mainmenu_ui_new_char_prev_race(pc_obj)) {
            window->refresh_func(NULL);
        }
        return true;
    case 5:
        if (mainmenu_ui_new_char_next_race(pc_obj)) {
            window->refresh_func(NULL);
        }
        return true;
    case 6:
        background = background_get(pc_obj);
        if (mainmenu_ui_new_char_prev_background(pc_obj, &background)) {
            background_clear(pc_obj);
            background_set(pc_obj, background, background_get_description(background));
            window->refresh_func(NULL);
        }
        return true;
    case 7:
        background = background_get(pc_obj);
        if (mainmenu_ui_new_char_next_background(pc_obj, &background)) {
            background_clear(pc_obj);
            background_set(pc_obj, background, background_get_description(background));
            window->refresh_func(NULL);
        }
        return true;
    case 8:
        if (!mainmenu_ui_female_chars_missing && mainmenu_ui_new_char_prev_gender(pc_obj)) {
            window->refresh_func(NULL);
        }
        return true;
    case 9:
        if (!mainmenu_ui_female_chars_missing && mainmenu_ui_new_char_next_gender(pc_obj)) {
            window->refresh_func(NULL);
        }
        return true;
    default:
        return true;
    }
}

// 0x5452C0
bool mainmenu_ui_new_char_next_background(int64_t obj, int* background_ptr)
{
    if (background_find_next(obj, background_ptr)) {
        return true;
    }

    if (background_find_first(obj, background_ptr)) {
        return true;
    }

    return false;
}

// 0x545300
bool mainmenu_ui_new_char_prev_background(int64_t obj, int* background_ptr)
{
    if (background_find_prev(obj, background_ptr)) {
        return true;
    }

    while (background_find_next(obj, background_ptr)) {
    }

    return true;
}

// 0x545350
bool mainmenu_ui_new_char_prev_gender(int64_t obj)
{
    if (stat_level_get(obj, STAT_GENDER) == GENDER_FEMALE) {
        background_clear(obj);
        return mainmenu_ui_new_char_set_gender(obj, GENDER_MALE);
    } else {
        background_clear(obj);
        return mainmenu_ui_new_char_set_gender(obj, GENDER_FEMALE);
    }
}

// 0x5453A0
bool mainmenu_ui_new_char_set_gender(int64_t obj, int gender)
{
    int race;
    int portrait;

    if (stat_level_get(obj, STAT_GENDER) == gender) {
        return false;
    }

    race = stat_level_get(obj, STAT_RACE);
    if (gender == GENDER_FEMALE
        && !mainmenu_ui_race_info[race].available_for_female) {
        return false;
    }

    object_set_gender_and_race(obj, mainmenu_ui_race_body_type(race), gender, race);
    object_set_current_aid(obj, obj_field_int32_get(obj, OBJ_F_CURRENT_AID));

    if (portrait_find_first(obj, &portrait)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_PORTRAIT, portrait);
    }

    return true;
}

// 0x545440
bool mainmenu_ui_new_char_next_gender(int64_t obj)
{
    if (stat_level_get(obj, STAT_GENDER) == GENDER_MALE) {
        background_clear(obj);
        return mainmenu_ui_new_char_set_gender(obj, GENDER_FEMALE);
    } else {
        background_clear(obj);
        return mainmenu_ui_new_char_set_gender(obj, GENDER_MALE);
    }
}

// 0x545490
bool mainmenu_ui_new_char_prev_race(int64_t obj)
{
    int race;

    race = stat_level_get(obj, STAT_RACE);
    if (race > 0) {
        if (stat_level_get(obj, STAT_GENDER) == GENDER_FEMALE) {
            do {
                race--;
            } while (race >= 0 && !mainmenu_ui_race_info[race].available_for_female);

            if (race < 0) {
                return false;
            }

            if (!mainmenu_ui_race_info[race].available_for_female) {
                return false;
            }
        } else {
            race--;
        }

        if (race < 0) {
            return false;
        }
    } else {
        race = 7;
        if (stat_level_get(obj, STAT_GENDER) == GENDER_FEMALE) {
            while (race >= 0 && !mainmenu_ui_race_info[race].available_for_female) {
                race--;
            }

            if (race < 0) {
                return false;
            }

            if (!mainmenu_ui_race_info[race].available_for_female) {
                return false;
            }
        }
    }

    background_clear(obj);
    mainmenu_ui_new_char_set_race(obj, race);

    return true;
}

// 0x545550
void mainmenu_ui_new_char_set_race(int64_t obj, int race)
{
    int gender;
    int portrait;

    gender = stat_level_get(obj, STAT_GENDER);
    if (gender == GENDER_FEMALE) {
        gender = !mainmenu_ui_race_info[race].available_for_female
            ? GENDER_MALE
            : GENDER_FEMALE;
    }

    object_set_gender_and_race(obj, mainmenu_ui_race_body_type(race), gender, race);
    object_set_current_aid(obj, obj_field_int32_get(obj, OBJ_F_CURRENT_AID));

    if (portrait_find_first(obj, &portrait)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_PORTRAIT, portrait);
    }
}

// 0x5455D0
bool mainmenu_ui_new_char_next_race(int64_t obj)
{
    int race;

    race = stat_level_get(obj, STAT_RACE);
    if (race < 7) {
        if (stat_level_get(obj, STAT_GENDER) == GENDER_FEMALE) {
            do {
                race++;
            } while (race < 7 && !mainmenu_ui_race_info[race].available_for_female);

            if (race >= 8) {
                // NOTE: verified against 0x545615-0x545618 - the binary contains this dead race>=8 check.
                return false;
            }

            if (!mainmenu_ui_race_info[race].available_for_female) {
                race = RACE_HUMAN;
            }
        } else {
            race++;
        }
    } else {
        race = RACE_HUMAN;
        if (stat_level_get(obj, STAT_GENDER) == GENDER_FEMALE) {
            while (!mainmenu_ui_race_info[race].available_for_female) {
                race++;
            }

            if (race >= 8) {
                // NOTE: verified against 0x545658-0x54565B - the binary contains this dead race>=8 check.
                return false;
            }

            if (!mainmenu_ui_race_info[race].available_for_female) {
                return false;
            }
        }
    }

    if (race < 8) {
        background_clear(obj);
        mainmenu_ui_new_char_set_race(obj, race);
        return true;
    }

    return false;
}

// 0x545690
bool mainmenu_ui_new_char_button_hover(tig_button_handle_t button_handle)
{
    (void)button_handle;

    return true;
}

// 0x5456A0
bool mainmenu_ui_new_char_button_leave(tig_button_handle_t button_handle)
{
    (void)button_handle;

    return true;
}

// 0x5456B0
void mainmenu_ui_new_char_mouse_idle(int x, int y)
{
    MainMenuUiNewCharHoverMode mode;

    y -= 43;
    if (x >= mainmenu_ui_new_char_gender_hover_rect.x
        && y >= mainmenu_ui_new_char_gender_hover_rect.y
        && x < mainmenu_ui_new_char_gender_hover_rect.x + mainmenu_ui_new_char_gender_hover_rect.width
        && y < mainmenu_ui_new_char_gender_hover_rect.y + mainmenu_ui_new_char_gender_hover_rect.height) {
        mode = MMUI_NEW_CHAR_HOVER_MODE_GENDER;
    } else if (x >= mainmenu_ui_new_char_race_hover_rect.x
        && y >= mainmenu_ui_new_char_race_hover_rect.y
        && x < mainmenu_ui_new_char_race_hover_rect.x + mainmenu_ui_new_char_race_hover_rect.width
        && y < mainmenu_ui_new_char_race_hover_rect.y + mainmenu_ui_new_char_race_hover_rect.height) {
        mode = MMUI_NEW_CHAR_HOVER_MODE_RACE;
    } else {
        mode = MMUI_NEW_CHAR_HOVER_MODE_BACKGROUND;
    }

    if (mainmenu_ui_new_char_hover_mode != mode) {
        mainmenu_ui_new_char_hover_mode = mode;
        main_menu_window_info[mainmenu_ui_window_type]->refresh_func(&mainmenu_ui_shared_char_desc_view_rect);
    }
}

// 0x545780
bool mainmenu_ui_new_char_execute(int btn)
{
    int64_t pc_obj;
    MesFileEntry mes_file_entry;

    (void)btn;

    pc_obj = player_get_local_pc_obj();

    mes_file_entry.num = 500; // "Choose Name"
    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
    if (mainmenu_ui_textedit_buffer[0] == '\0') {
        strcpy(mainmenu_ui_textedit_buffer, mes_file_entry.str);
    }

    if (strcmp(mainmenu_ui_textedit_buffer, mes_file_entry.str) == 0) {
        mes_file_entry.num = 506; // "You must choose a name."
        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
        intgame_message_window_display_str(-1, mes_file_entry.str);
        return false;
    }

    obj_field_string_set(pc_obj, OBJ_F_PC_PLAYER_NAME, mainmenu_ui_textedit_buffer);
    ui_spell_maintain_refresh();
    mainmenu_ui_auto_equip_items_on_start = true;

    return true;
}

// 0x545870
void mainmenu_ui_pregen_char_create(void)
{
    mainmenu_ui_window_type = MM_WINDOW_PREGEN_CHAR;
    mainmenu_ui_pregen_char_idx = 1;
    mainmenu_ui_pregen_char_obj = obj_pool_perm_lookup(obj_get_id(proto_get_by_description(BP_MERWIN_TUMBLEBROOK)));
    mainmenu_ui_create_window();
}

// 0x5458D0
void mainmenu_ui_pregen_char_refresh(TigRect* rect)
{
    char* name;

    mmUISharedCharRefreshFunc(mainmenu_ui_pregen_char_obj, rect);
    if (rect == NULL
        || (mainmenu_ui_shared_char_name_rect.x < rect->x + rect->width
            && mainmenu_ui_shared_char_name_rect.y < rect->y + rect->height
            && rect->x < mainmenu_ui_shared_char_name_rect.x + mainmenu_ui_shared_char_name_rect.width
            && rect->y < mainmenu_ui_shared_char_name_rect.y + mainmenu_ui_shared_char_name_rect.height)) {
        tig_font_push(mainmenu_ui_text_fonts_red[1]);
        if (tig_window_fill(mainmenu_ui_window_handle, &mainmenu_ui_shared_char_name_rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            obj_field_string_get(mainmenu_ui_pregen_char_obj, OBJ_F_PC_PLAYER_NAME, &name);
            if (name != NULL) {
                mainmenu_ui_draw_centered_text(name, &mainmenu_ui_shared_char_name_rect, mainmenu_ui_text_fonts_red[1]);
                FREE(name);
            }
        }
        tig_font_pop();
    }
}

// 0x5459F0
bool mainmenu_ui_pregen_char_button_released(tig_button_handle_t button_handle)
{
    int index;
    int64_t pc_obj;
    MainMenuWindowInfo* window;

    for (index = 0; index < 4; index++) {
        if (button_handle == mainmenu_ui_pregen_char_buttons[index].button_handle) {
            break;
        }
    }

    if (index >= 4) {
        return true;
    }

    pc_obj = player_get_local_pc_obj();
    window = main_menu_window_info[mainmenu_ui_window_type];

    switch (index) {
    case 0:
        if (window->execute_func != NULL && !window->execute_func(0)) {
            return true;
        }
        mainmenu_ui_dismiss();
        return true;
    case 1:
        mainmenu_ui_close(true);
        return true;
    case 2:
        if (mainmenu_ui_pregen_char_idx > 1) {
            mainmenu_ui_pregen_char_idx--;
        } else {
            mainmenu_ui_pregen_char_idx = mainmenu_ui_pregen_char_cnt - 1;
        }
        mainmenu_ui_pregen_char_obj = obj_pool_perm_lookup(obj_get_id(proto_get_by_description(mainmenu_ui_pregen_char_idx + BP_GENERIC_PC)));
        window->refresh_func(NULL);
        return true;
    case 3:
        if (mainmenu_ui_pregen_char_idx < mainmenu_ui_pregen_char_cnt - 1) {
            mainmenu_ui_pregen_char_idx++;
        } else {
            mainmenu_ui_pregen_char_idx = 1;
        }
        mainmenu_ui_pregen_char_obj = obj_pool_perm_lookup(obj_get_id(proto_get_by_description(mainmenu_ui_pregen_char_idx + BP_GENERIC_PC)));
        window->refresh_func(NULL);
        return true;
    default:
        return true;
    }
}

// 0x545B60
bool mainmenu_ui_pregen_char_execute(int btn)
{
    PlayerCreateInfo player_create_info;
    MesFileEntry mes_file_entry;
    int index;
    int flag;

    (void)btn;

    mainmenu_ui_start_new_game = true;

    player_create_info_init(&player_create_info);
    player_create_info.loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    player_create_info.basic_prototype = mainmenu_ui_pregen_char_idx + BP_GENERIC_PC;
    if (!player_obj_create_player(&player_create_info)) {
        tig_debug_printf("MainMenu-UI: mmUIPregenCharExecuteFunc: ERROR: Player Creation Failed!\n");
        exit(EXIT_FAILURE);
    }

    for (index = 0; index < mainmenu_ui_pregen_char_cnt - 1; index++) {
        mes_file_entry.num = 551 + index;
        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
        flag = atoi(mes_file_entry.str);
        if (flag > 0) {
            script_global_flag_set(flag, mainmenu_ui_pregen_char_idx - 1 == index);
        }
    }

    item_wield_best_all(player_create_info.obj, OBJ_HANDLE_NULL);
    mainmenu_ui_dismiss();

    return true;
}

// 0x545C50
void mainmenu_ui_charedit_create(void)
{
    mainmenu_ui_window_type = MM_WINDOW_CHAREDIT;
    mainmenu_ui_create_window();
}

// 0x545C60
void mainmenu_ui_charedit_destroy(void)
{
    if (charedit_is_created()) {
        charedit_close();
    }
}

// 0x545C70
bool mainmenu_ui_charedit_button_released(tig_button_handle_t button_handle)
{
    if (button_handle == mainmenu_ui_charedit_buttons[0].button_handle) {
        mainmenu_ui_window_cmd(2);
        return true;
    }

    if (button_handle == mainmenu_ui_charedit_buttons[1].button_handle) {
        mainmenu_ui_close(true);
        return true;
    }

    return false;
}

// 0x545DF0
void mainmenu_ui_charedit_refresh(TigRect* rect)
{
    int64_t pc_obj;

    (void)rect;

    pc_obj = player_get_local_pc_obj();
    if (!charedit_open(pc_obj, mainmenu_ui_charedit_mode)) {
        mainmenu_ui_close(true);
    }
    mainmenu_ui_charedit_mode = CHAREDIT_MODE_3;
}

// 0x545E20
void mainmenu_ui_shop_create(void)
{
    mainmenu_ui_window_type = MM_WINDOW_SHOP;
    mainmenu_ui_create_window();
}

// 0x545E30
void mainmenu_ui_shop_destroy(void)
{
    if (inven_ui_is_created()) {
        inven_ui_destroy();
    }
}

// 0x545E40
bool mainmenu_ui_shop_button_released(tig_button_handle_t button_handle)
{
    if (button_handle == mainmenu_ui_shop_buttons[0].button_handle) {
        mainmenu_ui_dismiss();
        return true;
    }

    if (button_handle == mainmenu_ui_shop_buttons[1].button_handle) {
        mainmenu_ui_close(true);
        return true;
    }

    return false;
}

// 0x545E80
void mainmenu_ui_shop_refresh(TigRect* rect)
{
    int64_t pc_obj;
    LocRect loc_rect;
    ObjectList objects;
    int64_t npc_obj;
    int64_t substitute_inventory_obj;

    (void)rect;

    pc_obj = player_get_local_pc_obj();
    item_generate_inventory(pc_obj);
    if (map_by_type(MAP_TYPE_SHOPPING_MAP) == 0) {
        mainmenu_ui_dismiss();
        return;
    }

    loc_rect.x1 = 0;
    loc_rect.y1 = 0;
    location_limits_get(&(loc_rect.x2), &(loc_rect.y2));
    object_list_rect(&loc_rect, OBJ_TM_NPC, &objects);

    if (objects.head != NULL) {
        npc_obj = objects.head->obj;
    } else {
        npc_obj = OBJ_HANDLE_NULL;
    }
    object_list_destroy(&objects);

    mainmenu_ui_start_new_game = true;

    if (npc_obj == OBJ_HANDLE_NULL) {
        mainmenu_ui_dismiss();
        return;
    }

    reaction_forget(npc_obj, pc_obj);
    item_inventory_restock_contents(npc_obj);

    substitute_inventory_obj = critter_substitute_inventory_get(npc_obj);
    if (substitute_inventory_obj != OBJ_HANDLE_NULL) {
        item_inventory_restock_contents(substitute_inventory_obj);
    }

    if (!inven_ui_open(pc_obj, npc_obj, INVEN_UI_MODE_BARTER)) {
        mainmenu_ui_dismiss();
        return;
    }
}

// 0x5461A0
bool main_menu_button_create(MainMenuButtonInfo* info, int width, int height)
{
    return main_menu_button_create_ex(info, width, height, TIG_BUTTON_MOMENTARY);
}

// 0x5461C0
bool main_menu_button_create_ex(MainMenuButtonInfo* info, int width, int height, unsigned int flags)
{
    TigButtonData button_data;
    int index;

    button_data.flags = flags;
    button_data.x = info->x;
    button_data.y = info->y;

    if (info->art_num != -1) {
        tig_art_interface_id_create(info->art_num, 0, 0, 0, &(button_data.art_id));
    } else {
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.width = width;
        button_data.height = height;
    }

    if ((info->flags & 0x1) != 0) {
        for (index = 0; index < 3; index++) {
            if (info->x >= mainmenu_ui_bottom_bar_cover_rects[index].x
                && info->x < mainmenu_ui_bottom_bar_cover_rects[index].x + mainmenu_ui_bottom_bar_cover_rects[index].width
                && info->y + 441 >= mainmenu_ui_bottom_bar_cover_rects[index].y
                && info->y + 441 < mainmenu_ui_bottom_bar_cover_rects[index].y + mainmenu_ui_bottom_bar_cover_rects[index].height) {
                break;
            }
        }

        if (index >= 3) {
            return false;
        }

        button_data.window_handle = mainmenu_ui_bottom_bar_cover_window_handles[index];
        button_data.x -= mainmenu_ui_bottom_bar_cover_rects[index].x;
    } else {
        button_data.window_handle = mainmenu_ui_window_handle;
        button_data.y -= mainmenu_ui_window_rect.y;
    }

    if ((info->flags & 0x2) != 0) {
        button_data.flags |= TIG_BUTTON_TOGGLE;
    }

    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;

    if (info->art_num != -1) {
        button_data.mouse_down_snd_id = SND_INTERFACE_BUTTON_MEDIUM;
        button_data.mouse_up_snd_id = SND_INTERFACE_BUTTON_MEDIUM_RELEASE;
    } else {
        button_data.mouse_up_snd_id = -1;
        button_data.mouse_down_snd_id = SND_INTERFACE_MORPHTEXT_CLICK;
        button_data.mouse_enter_snd_id = SND_INTERFACE_MORPHTEXT_HOVER;
    }

    return tig_button_create(&button_data, &(info->button_handle)) == TIG_OK;
}

// 0x546330
void mainmenu_ui_create_window(void)
{
    mainmenu_ui_create_window_func(true);
}

// 0x546340
void mainmenu_ui_create_window_func(bool should_display)
{
    MainMenuWindowInfo* window;
    MainMenuButtonInfo* button;
    MesFileEntry mes_file_entry;
    TigArtBlitInfo art_blit_info;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigFont font_desc;
    TigRect src_rect;
    TigRect dst_rect;
    TigRect text_rect;
    TigWindowData window_data;
    tig_art_id_t art_id;
    tig_font_handle_t font;
    tig_window_handle_t window_handle;
    bool v1 = false;
    int idx;
    int rc;

    if (mainmenu_ui_suppress_display) {
        should_display = false;
    }

    if (mainmenu_ui_active) {
        return;
    }

    window = main_menu_window_info[mainmenu_ui_window_type];
    if (window->background_art_num != -1) {
        tig_art_interface_id_create(window->background_art_num, 0, 0, 0, &art_id);
        if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
            if (art_frame_data.height == 600) {
                mainmenu_ui_window_rect = mainmenu_ui_window_fullscreen_rect;
            } else {
                mainmenu_ui_window_rect = mainmenu_ui_window_partial_rect;
                v1 = true;
            }
        }

        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            window_data.flags = TIG_WINDOW_ALWAYS_ON_TOP | TIG_WINDOW_MESSAGE_FILTER;
            window_data.rect = mainmenu_ui_window_rect;
            window_data.background_color = art_anim_data.color_key;
            window_data.color_key = art_anim_data.color_key;
            window_data.message_filter = mainmenu_ui_message_filter;
            hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

            src_rect.x = mainmenu_ui_window_rect.x;
            src_rect.y = 0;
            src_rect.width = mainmenu_ui_window_rect.width;
            src_rect.height = mainmenu_ui_window_rect.height;

            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.width = mainmenu_ui_window_rect.width;
            dst_rect.height = mainmenu_ui_window_rect.height;

            art_blit_info.flags = 0;
            art_blit_info.art_id = art_id;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;

            if (tig_window_create(&window_data, &mainmenu_ui_window_handle) != TIG_OK) {
                tig_debug_printf("mainmenu_ui_create_window_func: ERROR: tig_art_anim_data failed!\n");
                exit(EXIT_SUCCESS); // NOTE: verified against 0x546645 - original exits 0 (EXIT_SUCCESS).
            }

            tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info);
        }
    } else {
        v1 = true;
    }

    if (v1) {
        v1 = false;

        tig_art_interface_id_create(335, 0, 0, 0, &art_id);
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            window_data.flags = TIG_WINDOW_ALWAYS_ON_TOP | TIG_WINDOW_MESSAGE_FILTER;
            window_data.background_color = art_anim_data.color_key;
            window_data.color_key = art_anim_data.color_key;
            window_data.message_filter = mainmenu_ui_message_filter;

            for (idx = 0; idx < 3; idx++) {
                window_data.rect.x = mainmenu_ui_bottom_bar_cover_rects[idx].x;
                window_data.rect.y = mainmenu_ui_bottom_bar_cover_rects[idx].y;
                window_data.rect.width = mainmenu_ui_bottom_bar_cover_rects[idx].width;
                window_data.rect.height = mainmenu_ui_bottom_bar_cover_rects[idx].height;
                hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

                src_rect.x = mainmenu_ui_bottom_bar_cover_rects[idx].x;
                src_rect.y = 0;
                src_rect.width = mainmenu_ui_bottom_bar_cover_rects[idx].width;
                src_rect.height = mainmenu_ui_bottom_bar_cover_rects[idx].height;

                dst_rect.x = 0;
                dst_rect.y = 0;
                dst_rect.width = src_rect.width;
                dst_rect.height = src_rect.height;

                art_blit_info.flags = 0;
                art_blit_info.art_id = art_id;
                art_blit_info.src_rect = &src_rect;
                art_blit_info.dst_rect = &dst_rect;

                rc = tig_window_create(&window_data, &(mainmenu_ui_bottom_bar_cover_window_handles[idx]));
                if (rc != TIG_OK) {
                    tig_debug_printf("MainMenu-UI: mainmenu_ui_create_window_func: ERROR: tig_window_create failed: Result: %d!\n", rc);
                    exit(EXIT_FAILURE);
                }

                tig_window_blit_art(mainmenu_ui_bottom_bar_cover_window_handles[idx], &art_blit_info);

                v1 = true;
            }
        }

        tig_art_interface_id_create(336, 0, 0, 0, &art_id);
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            window_data.flags = TIG_WINDOW_ALWAYS_ON_TOP | TIG_WINDOW_MESSAGE_FILTER;
            window_data.rect = mainmenu_ui_top_bar_cover_rect;
            window_data.background_color = art_anim_data.color_key;
            window_data.color_key = art_anim_data.color_key;
            window_data.message_filter = mainmenu_ui_message_filter;
            hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

            src_rect.x = mainmenu_ui_top_bar_cover_rect.x;
            src_rect.y = 0;
            src_rect.width = mainmenu_ui_top_bar_cover_rect.width;
            src_rect.height = mainmenu_ui_top_bar_cover_rect.height;

            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.width = mainmenu_ui_top_bar_cover_rect.width;
            dst_rect.height = mainmenu_ui_top_bar_cover_rect.height;

            art_blit_info.flags = 0;
            art_blit_info.art_id = art_id;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;

            if (tig_window_create(&window_data, &mainmenu_ui_top_bar_cover_window_handle) != TIG_OK) {
                if (!v1) {
                    tig_debug_printf("mainmenu_ui_create_window_func: ERROR: tig_art_anim_data2 failed!\n");
                    exit(EXIT_SUCCESS); // NOTE: verified against 0x546825 - original exits 0 (EXIT_SUCCESS).
                }
            }

            tig_window_blit_art(mainmenu_ui_top_bar_cover_window_handle, &art_blit_info);
        } else {
            if (!v1) {
                tig_debug_printf("mainmenu_ui_create_window_func: ERROR: tig_art_anim_data2 failed!\n");
                exit(EXIT_SUCCESS); // NOTE: verified against 0x546825 - original exits 0 (EXIT_SUCCESS).
            }
        }
    }

    if (window->background_art_num != -1) {
        src_rect.x = 0;
        src_rect.y = 0;
        art_blit_info.flags = 0;

        for (idx = 0; idx < 2; idx++) {
            if (window->anims[idx].art_num != -1) {
                tig_art_interface_id_create(window->anims[idx].art_num, 0, 0, 0, &art_id);
                mainmenu_ui_anims[idx].art_id = art_id;
                if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
                    tig_debug_printf("mainmenu_ui_create_window_func: ERROR: tig_art_frame_data failed!\n");
                    exit(EXIT_FAILURE);
                }

                src_rect.width = art_frame_data.width;
                src_rect.height = art_frame_data.height;

                dst_rect.x = window->anims[idx].x;
                dst_rect.y = window->anims[idx].y;
                dst_rect.width = art_frame_data.width;
                dst_rect.height = art_frame_data.height;

                art_blit_info.art_id = art_id;
                tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info);
            }
        }

        mainmenu_ui_anims_start();
    }

    mes_file_entry.num = window->num;
    font_desc.width = 0;
    font_desc.height = 0;

    if ((window->refresh_text_flags & 0x1) != 0) {
        if ((window->refresh_text_flags & 0x8) != 0) {
            font = mainmenu_ui_menu_fonts[0][0];
        } else {
            font = mainmenu_ui_body_fonts[0][0];
        }
    } else {
        if ((window->refresh_text_flags & 0x10) != 0) {
            font = mainmenu_ui_text_fonts_red[0];
        } else {
            font = mainmenu_ui_text_fonts[0];
        }
    }

    for (idx = 0; idx < window->num_buttons; idx++) {
        button = &(window->buttons[idx]);

        if ((button->flags & 0x1) != 0) {
            int j;

            for (j = 0; j < 3; j++) {
                if (button->x >= mainmenu_ui_bottom_bar_cover_rects[j].x
                    && button->y + 441 >= mainmenu_ui_bottom_bar_cover_rects[j].y
                    && button->x < mainmenu_ui_bottom_bar_cover_rects[j].x + mainmenu_ui_bottom_bar_cover_rects[j].width
                    && button->y + 441 < mainmenu_ui_bottom_bar_cover_rects[j].y + mainmenu_ui_bottom_bar_cover_rects[j].height) {
                    break;
                }
            }

            if (j >= 3) {
                tig_debug_printf("mainmenu_ui_create_window_func: ERROR: j >= MM_UI_NUM_ROTWIN_COVERS!\n");
                exit(EXIT_FAILURE);
            }
            window_handle = mainmenu_ui_bottom_bar_cover_window_handles[j];
        } else {
            window_handle = mainmenu_ui_window_handle;
        }

        if (mes_file_entry.num != -1
            && (button->flags & 0x4) == 0) {
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
            tig_font_push(font);
            font_desc.str = mes_file_entry.str;
            font_desc.width = 0;
            font_desc.height = 0;
            font_desc.flags = 0;
            tig_font_measure(&font_desc);
            tig_font_pop();

            if ((window->refresh_text_flags & 0x20) == 0) {
                button->rect.width = font_desc.width;
                button->rect.height = font_desc.height;

                if ((window->refresh_text_flags & 0x04) != 0) {
                    button->x -= font_desc.width / 2;
                }
            }

            text_rect = button->rect;
            text_rect.x = button->x - window->field_30;
            text_rect.y = button->y - window->field_34;
            mainmenu_ui_refresh_text(window_handle,
                mes_file_entry.str,
                &text_rect,
                window->refresh_text_flags | 0x20);

            mes_file_entry.num += 10;

            if (button->field_14 == 0 && (button->flags & 0x08) == 0) {
                if (mes_search(mainmenu_ui_mainmenu_mes_file, &mes_file_entry)) {
                    mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
                    button->field_14 = SDL_toupper(mes_file_entry.str[0]);
                } else {
                    tig_debug_printf("MainMenu: Error: Can't Find Hotkey!");
                    button->field_14 = -1;
                }
            }

            mes_file_entry.num -= 9;
        }

        if (!main_menu_button_create(button, font_desc.width, font_desc.height)) {
            tig_debug_printf("mainmenu_ui_create_window_func: ERROR: main_menu_button_create failed!\n");
            exit(EXIT_FAILURE);
        }
    }

    window->refresh_text_flags |= 0x20;
    mainmenu_ui_active = true;

    if (window->refresh_func != NULL) {
        window->refresh_func(NULL);
    }

    if (should_display) {
        tig_window_display();
    }
}

// 0x546B40
void mainmenu_ui_refresh_text(tig_window_handle_t window_handle, const char* str, TigRect* rect, unsigned int flags)
{
    TigRect text_rect;
    TigFont font_desc;
    TigArtAnimData art_anim_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    tig_font_handle_t* fonts;
    tig_art_id_t art_id;
    int pass;

    text_rect = *rect;

    if ((flags & 0x1) == 0) {
        fonts = (flags & 0x10) != 0 ? mainmenu_ui_body_fonts[0] : mainmenu_ui_text_fonts;

        if ((flags & 0x20) == 0) {
            tig_font_push(fonts[0]);
            font_desc.width = 0;
            font_desc.height = 0;
            font_desc.str = str;
            font_desc.flags = 0;
            tig_font_measure(&font_desc);
            tig_font_pop();

            text_rect.width = font_desc.width;
            text_rect.height = font_desc.height;
            if ((flags & 0x4) != 0) {
                text_rect.x -= font_desc.width / 2;
            }
        }

        tig_font_push(fonts[0]);
        if (tig_window_text_write(window_handle, str, &text_rect) != TIG_OK) {
            tig_debug_printf("mainmenu_ui_refresh_text: ERROR: tig_window_text_write failed!\n");
        }
        tig_font_pop();
    } else {
        if ((flags & 0x08) != 0) {
            fonts = mainmenu_ui_menu_fonts[(flags & 0x2) != 0 ? 1 : 0];
        } else {
            fonts = mainmenu_ui_body_fonts[(flags & 0x2) != 0 ? 1 : 0];
        }

        if ((flags & 0x20) == 0) {
            tig_font_push(fonts[0]);
            font_desc.width = 0;
            font_desc.height = 0;
            font_desc.str = str;
            font_desc.flags = 0;
            tig_font_measure(&font_desc);
            tig_font_pop();

            text_rect.width = font_desc.width;
            text_rect.height = font_desc.height;
            if ((flags & 0x4) != 0) {
                text_rect.x -= font_desc.width / 2;
            }
        }

        if (main_menu_window_info[mainmenu_ui_window_type]->background_art_num != -1) {
            tig_art_interface_id_create(main_menu_window_info[mainmenu_ui_window_type]->background_art_num, 0, 0, 0, &art_id);
            if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
                src_rect.x = rect->x;
                src_rect.y = rect->y;
                src_rect.width = rect->width + 1;
                src_rect.height = rect->height + 1;

                dst_rect.x = rect->x;
                dst_rect.y = rect->y;
                dst_rect.width = rect->width + 1;
                dst_rect.height = rect->height + 1;

                art_blit_info.flags = 0;
                art_blit_info.art_id = art_id;
                art_blit_info.src_rect = &src_rect;
                art_blit_info.dst_rect = &dst_rect;
                tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info);
            }
        }

        for (pass = 0; pass < 3; pass++) {
            tig_font_push(fonts[pass]);
            text_rect.x += mainmenu_ui_text_pass_offsets[pass];
            text_rect.y += mainmenu_ui_text_pass_offsets[pass];
            if (tig_window_text_write(window_handle, str, &text_rect) != TIG_OK) {
                tig_debug_printf("mainmenu_ui_refresh_text: ERROR: tig_window_text_write2 failed!\n");
            }
            tig_font_pop();
        }
    }
}

// 0x546DD0
void mainmenu_ui_windows_destroy(void)
{
    int index;

    if (mainmenu_ui_active) {
        mainmenu_ui_textedit_unfocus();
        timeevent_clear_all_typed(TIMEEVENT_TYPE_MAINMENU);

        for (index = 0; index < 2; index++) {
            mainmenu_ui_anims[index].art_id = TIG_ART_ID_INVALID;
        }

        if (mainmenu_ui_window_handle != TIG_WINDOW_HANDLE_INVALID
            && tig_window_destroy(mainmenu_ui_window_handle) == TIG_OK) {
            mainmenu_ui_window_handle = TIG_WINDOW_HANDLE_INVALID;
        }

        for (index = 0; index < 3; index++) {
            if (mainmenu_ui_bottom_bar_cover_window_handles[index] != TIG_WINDOW_HANDLE_INVALID
                && tig_window_destroy(mainmenu_ui_bottom_bar_cover_window_handles[index]) == TIG_OK) {
                mainmenu_ui_bottom_bar_cover_window_handles[index] = TIG_WINDOW_HANDLE_INVALID;
            }
        }

        if (mainmenu_ui_top_bar_cover_window_handle != TIG_WINDOW_HANDLE_INVALID
            && tig_window_destroy(mainmenu_ui_top_bar_cover_window_handle) == TIG_OK) {
            mainmenu_ui_top_bar_cover_window_handle = TIG_WINDOW_HANDLE_INVALID;
        }

        mainmenu_ui_active = false;
    }
}

// 0x546E80
void mainmenu_ui_create_shared_radio_buttons(void)
{
    MainMenuWindowInfo* info;
    tig_button_handle_t group[2];

    info = main_menu_window_info[mainmenu_ui_window_type];
    group[0] = info->buttons[4].button_handle;
    group[1] = info->buttons[5].button_handle;
    if (tig_button_radio_group_create(2, group, info->flags & 1) != TIG_OK) {
        tig_debug_printf("mainmenu_ui_create_shared_radio_buttons: ERROR: tig_button_radio_group failed!\n");
    }
}

// 0x546EE0
bool mainmenu_ui_message_filter(TigMessage* msg)
{
    MainMenuWindowInfo* window;
    int idx;
    MesFileEntry mes_file_entry;
    MesFileEntry description_mes_file_entry;
    UiMessage ui_message;
    char str[MAX_STRING];
    int v2;
    TigMessage tmp_msg = *msg;
    msg = &tmp_msg;

    // Convert mouse position from screen coordinate system to centered 800x600
    // area.
    if (msg->type == TIG_MESSAGE_MOUSE) {
        TigRect rect = { 0, 0, 800, 600 };
        hrp_apply(&rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);
        msg->data.mouse.x -= rect.x;
        msg->data.mouse.y -= rect.y;
    }

    window = main_menu_window_info[mainmenu_ui_window_type];

    if (slide_ui_is_active()) {
        return false;
    }

    if (msg->type == TIG_MESSAGE_MOUSE) {
        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN) {
            if (window->scrollbar_rect.width > 0
                && msg->data.mouse.x >= window->content_rect.x
                && msg->data.mouse.y >= window->content_rect.y
                && msg->data.mouse.x < window->content_rect.x + window->content_rect.width
                && msg->data.mouse.y < window->content_rect.y + window->content_rect.height) {
                if (window->mouse_down_func != NULL) {
                    window->mouse_down_func(msg->data.mouse.x - window->scrollbar_rect.x, msg->data.mouse.y - window->scrollbar_rect.y - mainmenu_ui_window_rect.y);
                    return true;
                }
            }

            if (mainmenu_ui_window_type != MM_WINDOW_SAVE_GAME
                || (msg->data.mouse.x < mainmenu_ui_window_partial_rect.x
                    || msg->data.mouse.y < mainmenu_ui_window_partial_rect.y
                    || msg->data.mouse.x >= mainmenu_ui_window_partial_rect.x + mainmenu_ui_window_partial_rect.width
                    || msg->data.mouse.y >= mainmenu_ui_window_partial_rect.y + mainmenu_ui_window_partial_rect.height)) {
                mainmenu_ui_textedit_unfocus();
            }
        }

        switch (msg->data.mouse.event) {
        case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
            switch (mainmenu_ui_window_type) {
            case MM_WINDOW_0:
            case MM_WINDOW_1:
                mainmenu_ui_close(false);
                mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
                mainmenu_ui_open();
                return true;
            case MM_WINDOW_OPTIONS:
                if (intgame_pc_lens_check_pt(msg->data.mouse.x, msg->data.mouse.y)) {
                    if (mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
                        if (!options_ui_load_module()) {
                            mainmenu_ui_dismiss();
                        }
                    } else {
                        gsound_play_sfx(0, 1);
                        mainmenu_ui_close(true);
                    }
                    return true;
                }
                break;
            case MM_WINDOW_LOAD_GAME:
                if (intgame_pc_lens_check_pt(msg->data.mouse.x, msg->data.mouse.y)) {
                    if (mainmenu_ui_load_game_lens_passthrough) {
                        mainmenu_ui_dismiss();
                    } else {
                        mainmenu_ui_close(true);
                    }
                    return true;
                }
                break;
            case MM_WINDOW_SAVE_GAME:
                if (intgame_pc_lens_check_pt(msg->data.mouse.x, msg->data.mouse.y)) {
                    mainmenu_ui_dismiss();
                    return true;
                }
                break;
            default:
                break;
            }

            if (window->mouse_up_func != NULL
                && window->content_rect.width > 0
                && msg->data.mouse.x >= window->content_rect.x
                && msg->data.mouse.y - mainmenu_ui_window_rect.y >= window->content_rect.y
                && msg->data.mouse.x < window->content_rect.x + window->content_rect.width
                && msg->data.mouse.y - mainmenu_ui_window_rect.y < window->content_rect.y + window->content_rect.height) {
                gsound_play_sfx(0, 1);
                window->mouse_up_func(msg->data.mouse.x - window->content_rect.x, msg->data.mouse.y - window->content_rect.y - mainmenu_ui_window_rect.y);
                return true;
            }

            return false;
        case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
        case TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_UP:
            switch (mainmenu_ui_window_type) {
            case MM_WINDOW_0:
            case MM_WINDOW_1:
                mainmenu_ui_close(false);
                mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
                mainmenu_ui_open();
                return true;
            default:
                break;
            }
            return false;
        case TIG_MESSAGE_MOUSE_IDLE:
            if (window->mouse_idle_func != NULL) {
                window->mouse_idle_func(msg->data.mouse.x, msg->data.mouse.y);
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    if (msg->type == TIG_MESSAGE_BUTTON) {
        switch (msg->data.button.state) {
        case TIG_BUTTON_STATE_PRESSED:
            if (window->button_press_func != NULL && window->button_press_func(msg->data.button.button_handle)) {
                return true;
            }
            return false;
        case TIG_BUTTON_STATE_RELEASED:
            if (window->button_release_func != NULL && window->button_release_func(msg->data.button.button_handle)) {
                return true;
            }

            for (idx = 0; idx < window->num_buttons; idx++) {
                if (msg->data.button.button_handle == window->buttons[idx].button_handle) {
                    if (window->buttons[idx].field_10 > 0) {
                        if (window->execute_func != NULL) {
                            dword_5C3FB8 = window->execute_func(idx);
                            if (dword_5C3FB8 == 0) {
                                return true;
                            }
                        }

                        mainmenu_ui_close(false);
                        mainmenu_ui_window_type = window->buttons[idx].field_10;
                        mainmenu_ui_open();
                        return true;
                    }

                    if (window->buttons[idx].field_10 == 0) {
                        mainmenu_ui_dismiss();
                        if (mainmenu_ui_type_flags[mainmenu_ui_type][1]
                            || mainmenu_ui_window_type == MM_WINDOW_MAINMENU
                            || mainmenu_ui_window_type == MM_WINDOW_MAINMENU_IN_PLAY) {
                            mainmenu_ui_exit_game();
                        }
                        mainmenu_ui_window_type = MM_WINDOW_0;
                        return true;
                    }

                    if (window->buttons[idx].field_10 == -2) {
                        mainmenu_ui_close(true);
                        if (mainmenu_ui_window_type == MM_WINDOW_0) {
                            mainmenu_ui_dismiss();
                        }
                    }
                    return true;
                }
            }

            return false;
        case TIG_BUTTON_STATE_MOUSE_INSIDE:
            if (window->button_hover_func != NULL) {
                window->button_hover_func(msg->data.button.button_handle);
            }

            for (idx = 0; idx < window->num_buttons; idx++) {
                if (msg->data.button.button_handle == window->buttons[idx].button_handle) {
                    break;
                }
            }

            if (idx < window->num_buttons) {
                if (window->num != -1) {
                    mainmenu_ui_refresh_button_text(idx, 0x2);
                }

                v2 = window->buttons[idx].field_2C;
            } else {
                v2 = -1;
            }

            if (v2 == 0) {
                if (msg->data.button.button_handle == mainmenu_ui_load_game_texttoggle_button.button_handle) {
                    v2 = mainmenu_ui_load_game_texttoggle_button.field_2C;
                } else if (msg->data.button.button_handle == mainmenu_ui_save_game_texttoggle_button.button_handle) {
                    v2 = mainmenu_ui_save_game_texttoggle_button.field_2C;
                } else {
                    return false;
                }
            }

            if (v2 > 0) {
                mes_file_entry.num = 6999;
                if (!mes_search(mainmenu_ui_mainmenu_mes_file, &mes_file_entry)) {
                    tig_debug_printf("MMUI: ERROR: Hover Text for button is Unreachable!\n");
                    return false;
                }

                mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

                description_mes_file_entry.num = v2;
                mes_get_msg(mainmenu_ui_mainmenu_mes_file, &description_mes_file_entry);

                snprintf(str, sizeof(str), "%s\n%s", mes_file_entry.str, description_mes_file_entry.str);

                ui_message.type = UI_MSG_TYPE_FEEDBACK;
                ui_message.str = str;
                intgame_message_window_display_msg(&ui_message);
                return true;
            }

            return false;
        case TIG_BUTTON_STATE_MOUSE_OUTSIDE:
            if (window->button_leave_func != NULL) {
                window->button_leave_func(msg->data.button.button_handle);
            }

            for (idx = 0; idx < window->num_buttons; idx++) {
                if (msg->data.button.button_handle == window->buttons[idx].button_handle) {
                    break;
                }
            }

            if (idx < window->num_buttons) {
                if (window->num != -1) {
                    mainmenu_ui_refresh_button_text(idx, 0);
                }

                v2 = window->buttons[idx].field_2C;
            } else {
                v2 = -1;
            }

            if (v2 == 0) {
                if (msg->data.button.button_handle == mainmenu_ui_load_game_texttoggle_button.button_handle) {
                    v2 = mainmenu_ui_load_game_texttoggle_button.field_2C;
                } else if (msg->data.button.button_handle == mainmenu_ui_save_game_texttoggle_button.button_handle) {
                    v2 = mainmenu_ui_save_game_texttoggle_button.field_2C;
                } else {
                    return false;
                }
            }

            if (v2 > 0) {
                intgame_message_window_clear();
                return true;
            }

            return false;
        default:
            return false;
        }
    }

    if (msg->type == TIG_MESSAGE_KEYBOARD) {
        // CE: With intgame hidden we have to manually route keyboard events to
        // textedit ui.
        if (textedit_ui_is_focused()) {
            return textedit_ui_process_message(msg);
        }

        if (!msg->data.keyboard.pressed) {
            switch (mainmenu_ui_window_type) {
            case MM_WINDOW_0:
                return false;
            case MM_WINDOW_1:
                mainmenu_ui_close(false);
                mainmenu_ui_window_type = 2;
                mainmenu_ui_open();
                return true;
            case MM_WINDOW_MAINMENU:
                if (msg->data.keyboard.scancode == SDL_SCANCODE_ESCAPE
                    && mainmenu_ui_mainmenu_create_count > 1) {
                    gsound_play_sfx(0, 1);
                    mainmenu_ui_dismiss();
                    mainmenu_ui_window_type = 0;
                    mainmenu_ui_exit_game();
                }
                return false;
            case MM_WINDOW_OPTIONS:
                if (msg->data.keyboard.scancode == SDL_SCANCODE_O) {
                    if (!mainmenu_ui_type_flags[mainmenu_ui_type][1]) {
                        mainmenu_ui_dismiss();
                    }
                    return true;
                }
                return false;
            case MM_WINDOW_LOAD_GAME:
                switch (msg->data.keyboard.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    gsound_play_sfx(0, 1);
                    mainmenu_ui_close(true);
                    if (mainmenu_ui_window_type == MM_WINDOW_0) {
                        mainmenu_ui_dismiss();
                    }
                    return true;
                case SDL_SCANCODE_UP:
                    mainmenu_ui_load_select_prev();
                    return true;
                case SDL_SCANCODE_DOWN:
                    mainmenu_ui_load_select_next();
                    return true;
                case SDL_SCANCODE_BACKSPACE:
                case SDL_SCANCODE_DELETE:
                    gsound_play_sfx(0, 1);
                    mainmenu_ui_load_game_handle_delete();
                    return true;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    mainmenu_ui_window_cmd(2);
                    return true;
                default:
                    break;
                }
                return false;
            case MM_WINDOW_SAVE_GAME:
                switch (msg->data.keyboard.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    gsound_play_sfx(0, 1);
                    mainmenu_ui_close(true);
                    if (mainmenu_ui_window_type == MM_WINDOW_0) {
                        mainmenu_ui_dismiss();
                    }
                    return true;
                case SDL_SCANCODE_UP:
                    mainmenu_ui_save_select_prev();
                    return true;
                case SDL_SCANCODE_DOWN:
                    mainmenu_ui_save_select_next();
                    return true;
                case SDL_SCANCODE_BACKSPACE:
                case SDL_SCANCODE_DELETE:
                    gsound_play_sfx(0, 1);
                    mainmenu_ui_save_game_handle_delete();
                    return true;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    mainmenu_ui_window_cmd(2);
                    return true;
                default:
                    break;
                }
                return false;
            default:
                switch (msg->data.keyboard.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    gsound_play_sfx(0, 1);
                    mainmenu_ui_close(true);
                    if (mainmenu_ui_window_type == MM_WINDOW_0) {
                        mainmenu_ui_dismiss();
                    }
                    return true;
                default:
                    break;
                }
                return false;
            }
        } else {
            if (mainmenu_ui_key_shortcuts_disable_count_get()) {
                return false;
            }

            if ((mainmenu_ui_window_type == MM_WINDOW_SHOP || mainmenu_ui_window_type == MM_WINDOW_CHAREDIT)
                && msg->data.keyboard.key == SDLK_RETURN
                && iso_interface_window_get() != ROTWIN_TYPE_QUANTITY) {
                gsound_play_sfx(0, 1);
                mainmenu_ui_window_cmd(2);
                return true;
            }

            for (idx = 0; idx < window->num_buttons; idx++) {
                bool hidden;
                tig_button_is_hidden(window->buttons[idx].button_handle, &hidden);
                if (!hidden && SDL_toupper(msg->data.keyboard.key) == window->buttons[idx].field_14) {
                    break;
                }
            }

            if (idx >= window->num_buttons) {
                return false;
            }

            if (window->buttons[idx].art_num == -1) {
                gsound_play_sfx(SND_INTERFACE_MORPHTEXT_CLICK, 1);
            } else {
                gsound_play_sfx(0, 1);
            }

            if (window->buttons[idx].field_10 > 0) {
                if (window->execute_func != NULL) {
                    dword_5C3FB8 = window->execute_func(idx);
                    if (!dword_5C3FB8) {
                        return true;
                    }
                }

                mainmenu_ui_close(false);
                mainmenu_ui_window_type = window->buttons[idx].field_10;
                mainmenu_ui_open();
                return true;
            }

            if (window->buttons[idx].field_10 == 0) {
                mainmenu_ui_dismiss();
                if (mainmenu_ui_type_flags[mainmenu_ui_type][1]
                    || mainmenu_ui_window_type == MM_WINDOW_MAINMENU
                    || mainmenu_ui_window_type == MM_WINDOW_MAINMENU_IN_PLAY) {
                    mainmenu_ui_exit_game();
                }
                mainmenu_ui_window_type = MM_WINDOW_0;
                return true;
            }

            if (window->buttons[idx].field_10 == -2) {
                mainmenu_ui_close(true);
                if (mainmenu_ui_window_type == MM_WINDOW_0) {
                    mainmenu_ui_dismiss();
                }
                return true;
            }

            if (window->buttons[idx].field_10 == -1) {
                switch (mainmenu_ui_window_type) {
                case MM_WINDOW_MAINMENU_IN_PLAY:
                    switch (idx) {
                    case 3:
                        if (mainmenu_ui_confirm_quit() == TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
                            // NOTE: Faithful. The original pushes the literal 3
                            // too, at 0x547C83:
                            //
                            //     call 0x541690   ; mainmenu_ui_confirm_quit
                            //     test eax, eax
                            //     jne  0x547D7D   ; not OK -> return true
                            //     push 0x3
                            //     call 0x538840   ; tig_button_hide
                            //     ...
                            //     push 0x3
                            //     call 0x549310   ; mainmenu_ui_return_to_mainmenu
                            //
                            // So Troika passes the *switch index* where
                            // `tig_button_hide` wants a `tig_button_handle_t`.
                            // It reads wrong and it is theirs; whichever button
                            // happens to hold handle 3 is hidden. Not repaired
                            // because the intended handle cannot be recovered.
                            //
                            // The original also skips the
                            // `mainmenu_ui_return_to_mainmenu` call when
                            // `tig_net` is active (`ds:0x62AC6C & 1` at
                            // 0x547C8A, deferring via 0x4A38B0 instead). CE
                            // defines `tig_net_is_active()` as 0
                            // (`src/net_compat.h`), so that path is unreachable
                            // here by design, not dropped by accident.
                            tig_button_hide(3);

                            return mainmenu_ui_return_to_mainmenu(3);
                        }
                        return true;
                    case 4:
                        if (!mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
                            mainmenu_ui_close(true);

                            if (mainmenu_ui_window_type == MM_WINDOW_0) {
                                mainmenu_ui_dismiss();
                            }
                        }
                        return true;
                    }

                    return true;
                case MM_WINDOW_MAINMENU_IN_PLAY_LOCKED:
                    switch (idx) {
                    case 1:
                        if (mainmenu_ui_confirm_quit() == TIG_WINDOW_MODAL_DIALOG_CHOICE_OK) {
                            // NOTE: Faithful, same shape as the MM_WINDOW_
                            // MAINMENU_IN_PLAY case above - see the note there.
                            // Here the original pushes `esi` rather than a
                            // literal (0x547D25), but it has just tested
                            // `cmp si, 1` to reach this arm, so the value is 1.
                            tig_button_hide(1);

                            return mainmenu_ui_return_to_mainmenu(1);
                        }
                        return true;
                    case 2:
                        if (!mainmenu_ui_type_flags[mainmenu_ui_type][0]) {
                            mainmenu_ui_close(true);

                            if (mainmenu_ui_window_type == MM_WINDOW_0) {
                                mainmenu_ui_dismiss();
                            }
                        }
                        return true;
                    }

                    return true;
                default:
                    break;
                }

                return true;
            }

            return true;
        }
    }

    // CE: With intgame hidden we have to manually route keyboard events to
    // textedit ui.
    if (msg->type == TIG_MESSAGE_TEXT_INPUT) {
        return textedit_ui_process_message(msg);
    }

    return false;
}

// 0x547E00
void mainmenu_ui_refresh_button_text(int btn, unsigned int flags)
{
    MainMenuWindowInfo* window;
    MainMenuButtonInfo* button;
    bool hidden;
    MesFileEntry mes_file_entry;
    TigRect rect;

    window = main_menu_window_info[mainmenu_ui_window_type];
    button = &(window->buttons[btn]);

    // NOTE: verified against 0x547E2B - original ignores the return value and uses the out-param only.
    tig_button_is_hidden(button->button_handle, &hidden);
    if (!hidden) {
        mes_file_entry.num = window->num;
        if ((button->flags & 0x1) != 0) {
            tig_debug_printf("mainmenu_ui_refresh_button_text: ERROR: flags wrong!\n");
            exit(EXIT_FAILURE);
        }

        if (window->num != -1 && (button->flags & 0x4) == 0) {
            mes_file_entry.num = window->num + btn;
            mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);

            rect = button->rect;
            rect.x = button->x - window->field_30;
            rect.y = button->y - window->field_34;
            mainmenu_ui_refresh_text(mainmenu_ui_window_handle,
                mes_file_entry.str,
                &rect,
                window->refresh_text_flags | flags);
        }
    }
}

// 0x547EF0
void mainmenu_ui_anims_start(void)
{
    TigArtAnimData art_anim_data;
    DateTime datetime;
    TimeEvent timeevent;
    int index;

    for (index = 0; index < 2; index++) {
        if (tig_art_anim_data(mainmenu_ui_anims[index].art_id, &art_anim_data) == TIG_OK
            && art_anim_data.num_frames > 1) {
            mainmenu_ui_anims[index].max_frame = art_anim_data.num_frames - 1;
            mainmenu_ui_anims[index].fps = 1000 / art_anim_data.fps;
            mainmenu_ui_anims[index].x = main_menu_window_info[mainmenu_ui_window_type]->anims[index].x;
            mainmenu_ui_anims[index].y = main_menu_window_info[mainmenu_ui_window_type]->anims[index].y;

            timeevent.type = TIMEEVENT_TYPE_MAINMENU;
            timeevent.params[0].integer_value = index;
            datetime_from_milliseconds(&datetime, mainmenu_ui_anims[index].fps);
            timeevent_add_delay(&timeevent, &datetime);
        }
    }
}

// 0x547F90
bool mainmenu_ui_process_callback(TimeEvent* timeevent)
{
    int index;
    tig_art_id_t next_art_id;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    DateTime datetime;
    TimeEvent next_timeevent;

    index = timeevent->params[0].integer_value;
    if (mainmenu_ui_anims[index].art_id == TIG_ART_ID_INVALID) {
        return true;
    }

    next_art_id = tig_art_id_frame_inc(mainmenu_ui_anims[index].art_id);
    if (tig_art_id_frame_get(next_art_id) >= mainmenu_ui_anims[index].max_frame) {
        next_art_id = tig_art_id_frame_set(next_art_id, 0);
    }

    if (tig_art_frame_data(next_art_id, &art_frame_data) == TIG_OK) {
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.height;

        dst_rect.x = mainmenu_ui_anims[index].x;
        dst_rect.y = mainmenu_ui_anims[index].y;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.height;

        art_blit_info.flags = 0;
        art_blit_info.art_id = next_art_id;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(mainmenu_ui_window_handle, &art_blit_info);
    } else {
        tig_debug_printf("main_menu_ui_process_callback: ERROR: tig_art_frame_data failed!\n");
    }

    next_timeevent.type = TIMEEVENT_TYPE_MAINMENU;
    next_timeevent.params[0].integer_value = index;
    datetime_from_milliseconds(&datetime, mainmenu_ui_anims[index].fps);
    timeevent_add_delay(&next_timeevent, &datetime);

    return true;
}

// Command dispatch for the generic MM window machinery:
//   0 scroll up, 1 scroll down (top_index -1/+1, clamped),
//   2 execute: run the window's execute_func(-1) and advance to the next
//     window (load/save OK, charedit/shop continue, RETURN keys),
//   3 close the current window (load/save cancel),
//   4/5 toggle window->flags bit 0x1 off/on and adjust selected_index for
//     flags 0x2/0x4 - NOTE: no caller in CE, and none in the original
//     either: the two 0x5480C0 call sites in the exe's message filter
//     (0x5477C3, 0x547B08) both push 2.
//
// 0x5480C0
void mainmenu_ui_window_cmd(int a1)
{
    MainMenuWindowInfo* window;

    window = main_menu_window_info[mainmenu_ui_window_type];
    switch (a1) {
    case 0:
        mainmenu_ui_top_index_set(window->top_index - 1);
        return;
    case 1:
        mainmenu_ui_top_index_set(window->top_index + 1);
        return;
    case 2:
        if (window->execute_func != NULL) {
            dword_5C3FB8 = window->execute_func(-1);
            if (!dword_5C3FB8) {
                return;
            }
        }
        if (mainmenu_ui_active) {
            if (mainmenu_ui_window_type == MM_WINDOW_SHOP) {
                mainmenu_ui_dismiss();
            } else {
                mainmenu_ui_close(false);
                mainmenu_ui_window_type++;
                mainmenu_ui_open();
            }
        }
        return;
    case 3:
        if (mainmenu_ui_window_type != MM_WINDOW_OPTIONS || options_ui_load_module()) {
            mainmenu_ui_close(true);
        }
        return;
    case 4:
        window->flags &= ~0x1;
        if ((window->flags & 0x2) != 0) {
            window->selected_index = 0;
        }
        if ((window->flags & 0x4) != 0) {
            if (window->selected_index > 0) {
                window->selected_index--;
            }
        }
        if (window->refresh_func != NULL) {
            window->refresh_func(NULL);
        }
        return;
    case 5:
        window->flags |= 0x1;
        if ((window->flags & 0x2) != 0) {
            window->selected_index = -1;
        }
        if ((window->flags & 0x4) != 0) {
            if (window->selected_index < window->cnt - 1) {
                window->selected_index++;
            }
        }
        if (window->refresh_func != NULL) {
            window->refresh_func(NULL);
        }
        return;
    }
}

// 0x548F10
void mmUIWinRefreshScrollBar(void)
{
    TigRect src_rect;
    TigRect dst_rect;
    TigArtBlitInfo blit_info;
    TigArtFrameData art_frame_data;
    tig_art_id_t art_id;
    MainMenuWindowInfo* curr_window_info;

    curr_window_info = main_menu_window_info[mainmenu_ui_window_type];
    if (curr_window_info->scrollbar_rect.width > 0) {
        tig_art_interface_id_create(316, 0, 0, 0, &art_id);
        if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
            tig_debug_printf("mmUIWinRefreshScrollBar: ERROR: tig_art_frame_data failed!\n");
            exit(EXIT_FAILURE);
        }

        src_rect.x = 0;
        src_rect.y = curr_window_info->top_index + art_frame_data.height / 2 - curr_window_info->scrollbar_rect.height / 2;
        src_rect.width = art_frame_data.width;
        src_rect.height = curr_window_info->scrollbar_rect.height;

        dst_rect.x = curr_window_info->scrollbar_rect.x;
        dst_rect.y = curr_window_info->scrollbar_rect.y;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = curr_window_info->scrollbar_rect.height;

        blit_info.flags = 0;
        blit_info.art_id = art_id;
        blit_info.src_rect = &src_rect;
        blit_info.dst_rect = &dst_rect;

        tig_window_blit_art(mainmenu_ui_window_handle, &blit_info);
    }
}

// 0x548FF0
void mainmenu_ui_top_index_set(int a1)
{
    MainMenuWindowInfo* curr_window_info;

    curr_window_info = main_menu_window_info[mainmenu_ui_window_type];
    if (a1 < 0) {
        a1 = 0;
    } else if (a1 > curr_window_info->max_top_index) {
        a1 = curr_window_info->max_top_index;
    }

    if (curr_window_info->top_index != a1) {
        curr_window_info->top_index = a1;
        mmUIWinRefreshScrollBar();
        if (curr_window_info->refresh_func != NULL) {
            curr_window_info->refresh_func(NULL);
        }
    }
}

// 0x549310
bool mainmenu_ui_return_to_mainmenu(tig_button_handle_t button_handle)
{
    if (button_handle != TIG_BUTTON_HANDLE_INVALID) {
        tig_button_show(button_handle);
    }

    if (mainmenu_ui_active) {
        mainmenu_ui_close(false);
        mainmenu_ui_window_type = MM_WINDOW_MAINMENU;
        mainmenu_ui_num_windows = 0;
        mainmenu_ui_type = !dword_5C4000 ? MM_TYPE_1 : MM_TYPE_DEFAULT;
        mainmenu_ui_open();
    } else {
        gameuilib_wants_mainmenu_set();
    }

    if (!gamelib_mod_load(gamelib_default_module_name_get())
        || !gameuilib_mod_load()) {
        tig_debug_printf("MainMenu: Unable to load default module %s.\n",
            gamelib_default_module_name_get());
        exit(EXIT_SUCCESS); // NOTE: verified against 0x5493B5 - original exits 0 (EXIT_SUCCESS).
    }

    mainmenu_ui_reset();

    return true;
}

// 0x5493C0
void mainmenu_ui_textedit_focus(char* buffer, int size)
{
    MainMenuWindowInfo* curr_window_info;

    if (!mainmenu_ui_textedit_focused) {
        if (mainmenu_ui_textedit_buffer[0] != '\0') {
            strcpy(mainmenu_ui_textedit_saved_buffer, buffer);
        }

        mainmenu_ui_textedit.size = size;
        mainmenu_ui_textedit.flags = mainmenu_ui_window_type == MM_WINDOW_SAVE_GAME
            ? TEXTEDIT_PATH_SAFE
            : 0;
        mainmenu_ui_textedit.buffer = buffer;
        textedit_ui_focus(&mainmenu_ui_textedit);
        mainmenu_ui_textedit_focused = true;

        curr_window_info = main_menu_window_info[mainmenu_ui_window_type];
        curr_window_info->refresh_func(&(curr_window_info->content_rect));

        mainmenu_ui_key_shortcuts_disable();
    }
}

// 0x549450
void mainmenu_ui_textedit_unfocus(void)
{
    MainMenuWindowInfo* curr_window_info;

    if (mainmenu_ui_textedit_focused) {
        if (mainmenu_ui_textedit.buffer[0] == '\0') {
            strcpy(mainmenu_ui_textedit.buffer, mainmenu_ui_textedit_saved_buffer);
        }
        textedit_ui_unfocus(&mainmenu_ui_textedit);
        mainmenu_ui_textedit_focused = false;

        curr_window_info = main_menu_window_info[mainmenu_ui_window_type];
        curr_window_info->refresh_func(&(curr_window_info->content_rect));

        mainmenu_ui_key_shortcuts_enable();
    }
}

// 0x5494C0
void mainmenu_ui_textedit_on_enter(TextEdit* textedit)
{
    MesFileEntry mes_file_entry;

    if (textedit->buffer[0] == '\0' && mainmenu_ui_window_type != MM_WINDOW_SAVE_GAME) {
        mes_file_entry.num = 500; // "Choose Name"
        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
        strncpy(mainmenu_ui_textedit_buffer, mes_file_entry.str, 23);
    }

    mainmenu_ui_textedit_unfocus();

    if (mainmenu_ui_window_type == MM_WINDOW_SAVE_GAME) {
        mainmenu_ui_save_game_execute(-1);
    }
}

// 0x549520
char* mainmenu_ui_textedit_focused_buffer(void)
{
    return mainmenu_ui_textedit_focused != 0 ? mainmenu_ui_textedit.buffer : NULL;
}

// 0x549540
void mainmenu_ui_textedit_on_change(TextEdit* textedit)
{
    MainMenuWindowInfo* curr_window_info;

    (void)textedit;

    curr_window_info = main_menu_window_info[mainmenu_ui_window_type];
    curr_window_info->refresh_func(&(curr_window_info->content_rect));

    if (mainmenu_ui_window_type == MM_WINDOW_SAVE_GAME) {
        scrollbar_ui_control_redraw(mainmenu_ui_scrollbar);
    }
}

// 0x549580
void mainmenu_ui_exit_game(void)
{
    tig_debug_printf("mainmenu_ui_exit_game: Exiting Game!\n");
    gameuilib_mod_unload();
    gamelib_mod_unload();
    gameuilib_exit();
    gamelib_exit();
    tig_exit();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_GROUPED_BLOCKS);
    exit(EXIT_SUCCESS);
}

// 0x5495F0
void mainmenu_ui_progressbar_init(int max_value)
{
    mainmenu_ui_progressbar_max_value = max_value;

    if (mainmenu_ui_active) {
        if (main_menu_window_info[mainmenu_ui_window_type]->refresh_func != NULL) {
            main_menu_window_info[mainmenu_ui_window_type]->refresh_func(&mainmenu_ui_progressbar_rect);
        }
    }
}

// 0x549620
void mainmenu_ui_progressbar_update(int value)
{
    mainmenu_ui_progressbar_value = value;

    if (mainmenu_ui_active) {
        if (main_menu_window_info[mainmenu_ui_window_type]->refresh_func != NULL) {
            main_menu_window_info[mainmenu_ui_window_type]->refresh_func(&mainmenu_ui_progressbar_rect);
        }
    }
}

// NOTE: Unreferenced in CE; the original's charedit/shop screens call it
// (0x581958, 0x581E69, 0x583B29, 0x583C88) where CE reaches the table
// directly. Kept as the exported accessor it is in the original.
//
// 0x5496C0
MainMenuWindowInfo* mainmenu_ui_window_info_get(int index)
{
    return main_menu_window_info[index];
}

// 0x5496D0
MainMenuWindowType mainmenu_ui_window_type_get(void)
{
    return mainmenu_ui_window_type;
}

// 0x5496E0
void mainmenu_ui_feedback_saving(void)
{
    mainmenu_ui_feedback(5060); // "Saving..."
}

// 0x5496F0
void mainmenu_ui_feedback(int num)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mes_file_entry.num = num;
    if (mes_search(mainmenu_ui_mainmenu_mes_file, &mes_file_entry)) {
        mes_get_msg(mainmenu_ui_mainmenu_mes_file, &mes_file_entry);
        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);
        tig_window_display();
    }
}

// 0x549750
void mainmenu_ui_feedback_saving_completed(void)
{
    mainmenu_ui_feedback(5062); // "Save completed."
}

// 0x549760
void mainmenu_ui_feedback_cannot_save_in_tb(void)
{
    mainmenu_ui_feedback(5065); // "Cannot save during turn-based combat when it isn't your turn."
}

// 0x549770
void mainmenu_ui_feedback_loading(void)
{
    mainmenu_ui_feedback(5061); // "Loading..."
}

// 0x549780
void mainmenu_ui_feedback_loading_completed(void)
{
    mainmenu_ui_feedback(5063); // "Load completed."
}

// 0x549820
tig_window_handle_t mainmenu_ui_window_handle_get(void)
{
    return mainmenu_ui_window_handle;
}

// 0x549830
void mainmenu_ui_window_type_set(MainMenuWindowType window_type)
{
    mainmenu_ui_window_type = window_type;
}

// 0x549840
mes_file_handle_t mainmenu_ui_mes_file(void)
{
    return mainmenu_ui_mainmenu_mes_file;
}

// 0x549850
void mainmenu_fonts_init(void)
{
    int fnt;
    int clr;
    TigFont font_info;

    font_info.flags = 0;
    font_info.str = NULL;

    for (fnt = 0; fnt < MM_FONT_COUNT; fnt++) {
        tig_art_interface_id_create(mainmenu_font_nums[fnt], 0, 0, 0, &(font_info.art_id));

        for (clr = 0; clr < MM_COLOR_COUNT; clr++) {
            font_info.color = tig_color_make(mainmenu_font_colors[clr][0],
                mainmenu_font_colors[clr][1],
                mainmenu_font_colors[clr][2]);
            tig_font_create(&font_info, &(mainmenu_ui_fonts_tbl[fnt][clr]));
        }
    }
}

// 0x549910
void mainmenu_fonts_exit(void)
{
    int fnt;
    int clr;

    for (fnt = 0; fnt < MM_FONT_COUNT; fnt++) {
        for (clr = 0; clr < MM_COLOR_COUNT; clr++) {
            tig_font_destroy(mainmenu_ui_fonts_tbl[fnt][clr]);
        }
    }
}

// 0x549940
tig_font_handle_t mainmenu_ui_font(MainMenuFont font, MainMenuColor color)
{
    return mainmenu_ui_fonts_tbl[font][color];
}

// NOTE: Unreferenced in CE. The original's shop/charedit screens call it on
// open (0x548BD5, 0x582845, 0x583819, 0x5850AB, 0x5883C5); CE refreshes its
// button texts inline instead.
//
// 0x549960
void mainmenu_ui_button_texts_refresh(void)
{
    int index;

    for (index = 0; index < main_menu_window_info[mainmenu_ui_window_type]->num_buttons; index++) {
        mainmenu_ui_refresh_button_text(index, 0);
    }
}

// Installs a return path over the current window: the caller's list of
// window types goes on the stack above the entry already pushed for the
// active window.
//
// NOTE: Unreferenced in CE. The original's charedit/shop/pregen screens build
// their multi-window return paths with it (0x5818A4, 0x5834ED, 0x583B02,
// 0x584BE6, 0x585E07); CE pushes and pops its stack explicitly instead.
//
// 0x549990
void mainmenu_ui_window_stack_set(int* a1, int num)
{
    memcpy(&(mainmenu_ui_window_stack[1]), a1, sizeof(*a1) * num);
    mainmenu_ui_num_windows = num + 1;
}

// 0x549A40
void mainmenu_ui_key_shortcuts_disable(void)
{
    mainmenu_ui_key_shortcuts_disable_count++;
}

// 0x549A50
void mainmenu_ui_key_shortcuts_enable(void)
{
    mainmenu_ui_key_shortcuts_disable_count--;
}

// 0x549A60
int mainmenu_ui_key_shortcuts_disable_count_get(void)
{
    return mainmenu_ui_key_shortcuts_disable_count;
}

// 0x549A70
void mainmenu_ui_female_chars_missing_clear(void)
{
    mainmenu_ui_female_chars_missing = false;
}

// 0x549A80
void mainmenu_ui_female_chars_probe(void)
{
    int64_t obj;

    if (!mainmenu_ui_female_chars_missing) {
        obj = obj_pool_perm_lookup(obj_get_id(proto_get_by_description(BP_VICTORIA_WARRINGTON)));
        if (obj != OBJ_HANDLE_NULL
            && tig_art_exists(obj_field_int32_get(obj, OBJ_F_CURRENT_AID)) == TIG_OK) {
            mainmenu_ui_female_chars_missing = false;
            mainmenu_ui_pregen_char_cnt = 13;
            mainmenu_ui_new_char_window_info.num_buttons = 10;
        } else {
            mainmenu_ui_female_chars_missing = true;
        }
    }
}
