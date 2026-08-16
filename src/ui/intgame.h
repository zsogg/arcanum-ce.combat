#ifndef ARCANUM_UI_INTGAME_H_
#define ARCANUM_UI_INTGAME_H_

#include "game/context.h"
#include "game/mes.h"
#include "game/target.h"
#include "game/timeevent.h"
#include "game/ui.h"
#include "ui/types.h"

typedef enum PcLensMode {
    PC_LENS_MODE_NONE,
    PC_LENS_MODE_PASSTHROUGH,
    PC_LENS_MODE_BLACKOUT,
} PcLensMode;

typedef struct PcLens {
    tig_window_handle_t window_handle;
    TigRect* rect;
    tig_art_id_t art_id;
} PcLens;

typedef enum IntgameBar {
    INTGAME_BAR_HEALTH,
    INTGAME_BAR_FATIGUE,
    INTGAME_BAR_COUNT,
} IntgameBar;

typedef enum IntgameMode {
    INTGAME_MODE_MAIN,
    INTGAME_MODE_SPELL,
    INTGAME_MODE_SKILL,
    INTGAME_MODE_DIALOG,
    INTGAME_MODE_BARTER,
    INTGAME_MODE_WMAP,
    INTGAME_MODE_SLEEP,
    INTGAME_MODE_LOGBOOK,
    INTGAME_MODE_INVEN,
    INTGAME_MODE_CHAREDIT,
    INTGAME_MODE_LOOT,
    INTGAME_MODE_STEAL,
    INTGAME_MODE_12,
    INTGAME_MODE_QUANTITY,
    INTGAME_MODE_SCHEMATIC,
    INTGAME_MODE_WRITTEN,
    INTGAME_MODE_ITEM,
    INTGAME_MODE_17,
    INTGAME_MODE_FOLLOWER,
    INTGAME_MODE_NPC_IDENTIFY,
    INTGAME_MODE_NPC_REPAIR,
    INTGAME_MODE_COUNT,
} IntgameMode;

typedef enum RotatingWindowType {
    ROTWIN_TYPE_INVALID = -1,
    ROTWIN_TYPE_MSG,
    ROTWIN_TYPE_SPELLS,
    ROTWIN_TYPE_SKILLS,
    ROTWIN_TYPE_CHAT,
    ROTWIN_TYPE_TRAPS,
    ROTWIN_TYPE_DIALOGUE,
    ROTWIN_TYPE_MAP_NOTE,
    ROTWIN_TYPE_BROADCAST,
    ROTWIN_TYPE_MAGICTECH,
    ROTWIN_TYPE_QUANTITY,
    ROTWIN_TYPE_MP_KICKBAN,
    ROTWIN_TYPE_COUNT,
} RotatingWindowType;

extern tig_font_handle_t intgame_morph15_white_font;

bool intgame_init(GameInitInfo* init_info);
void intgame_reset(void);
void intgame_resize(GameResizeInfo* resize_info);
void intgame_exit(void);
bool intgame_save(TigFile* stream);
bool intgame_load(GameLoadInfo* load_info);
void iso_interface_create(tig_window_handle_t window_handle);
void iso_interface_destroy(void);
void intgame_mode_stack_reset(void);
bool intgame_button_create_ex(tig_window_handle_t window_handle, TigRect* rect, UiButtonInfo* button_info, unsigned int flags);
bool intgame_button_create(UiButtonInfo* button_info);
void intgame_button_destroy(UiButtonInfo* button_info);
void intgame_draw_bar(int bar);
void intgame_draw_bars(void);
void intgame_counters_refresh(void);
bool intgame_process_message(TigMessage* msg);
void intgame_process_event(TigMessage* msg);
void intgame_apply_mode_to_target(TargetDescriptor* td);
bool intgame_hotkey_is_dragging(void);
void intgame_hotkey_activate(Hotkey* hotkey);
void intgame_hotkey_highlight(Hotkey* hotkey);
void iso_interface_window_set(RotatingWindowType window_type);
void intgame_message_window_clear(void);
void intgame_message_window_display_msg(UiMessage* ui_message);
void intgame_message_window_display_str(int a1, char* str);
void intgame_message_window_set_display_func(void (*func)(UiMessage* ui_message));
void intgame_message_window_display_spell(int spl);
void intgame_message_window_display_college(int college);
void intgame_message_window_display_skill(int value);
void intgame_message_window_clear_internal(void);
void intgame_pc_lens_do(PcLensMode mode, PcLens* pc_lens);
bool intgame_pc_lens_check_pt(int x, int y);
bool intgame_pc_lens_check_pt_unscale(int x, int y);
void intgame_pc_lens_redraw(void);
void iso_interface_refresh(void);
bool intgame_msg_is_over_iso_window(TigMessage* msg);
bool intgame_get_location_under_cursor(int64_t* loc_ptr);
IntgameMode intgame_mode_get(void);
bool intgame_mode_set(IntgameMode mode);
bool intgame_mode_supports_scrolling(IntgameMode mode);
RotatingWindowType iso_interface_window_get(void);
void iso_interface_window_set_animated(RotatingWindowType window_type);
void intgame_text_edit_refresh(const char* str, tig_font_handle_t font);
void intgame_text_edit_refresh_color(const char* str, tig_font_handle_t font, tig_color_t color, bool a4);
bool intgame_clock_process_callback(TimeEvent* timeevent);
bool intgame_dialog_begin(bool (*func)(TigMessage* msg));
bool intgame_dialog_is_active(void);
void intgame_dialog_end(void);
void intgame_dialog_clear(void);
void intgame_dialog_set_option(int index, const char* str);
int intgame_dialog_get_option(TigMessage* msg);
RotatingWindowType iso_interface_window_get_2(void);
void intgame_spell_maintain_art_set(int slot, tig_art_id_t art_id);
void intgame_spell_maintain_refresh(int slot, bool active);
void intgame_refresh_cursor(void);
void intgame_item_mode_cursor_set(int art_num);
void intgame_examine_object(int64_t pc_obj, int64_t target_obj, char* str);
bool intgame_examine_portrait(int64_t pc_obj, int64_t target_obj, int* portrait_ptr);
tig_art_id_t intgame_item_icon_art_id_get(int64_t obj);
void intgame_message_window_display_attack(int64_t obj);
void intgame_message_window_display_defense(int64_t obj);
void intgame_toggle_primary_button(UiPrimaryButton btn, bool on);
void intgame_set_map_button(UiPrimaryButton btn);
void intgame_magictech_item_validate(void);
void intgame_set_examine_obj(int64_t obj);
void intgame_notify_item_inserted_or_removed(int64_t item_obj, bool removed, int inventory_location);
void intgame_refresh_health_bar(int64_t obj);
bool intgame_big_window_lock(TigWindowMessageFilterFunc func, tig_window_handle_t* window_handle_ptr);
void intgame_big_window_unlock(void);
void intgame_target_interact(int64_t source_obj, int64_t target_obj);
void intgame_there_is_nothing_to_loot(void);
void intgame_interface_background_refresh(void);
void intgame_feedback_msg(int index);
void intgame_refresh_examine(int64_t obj);
unsigned int intgame_get_iso_window_flags(void);
void intgame_set_iso_window_flags(unsigned int flags);
void intgame_set_iso_window_width(int width);
void intgame_set_iso_window_height(int height);
bool intgame_create_iso_window(tig_window_handle_t* window_handle_ptr);
bool intgame_is_compact_interface(void);
void intgame_set_fullscreen(void);
void intgame_toggle_interface(void);
RotatingWindowType iso_interface_window_get_3(void);
int intgame_selected_college_get(void);
void intgame_spell_button_get(int group, int index, UiButtonInfo* button_info);
int64_t intgame_magictech_item_get(void);
mes_file_handle_t intgame_hotkey_mes_file(void);
UiButtonInfo* intgame_recent_action_button_get(int index);
void intgame_recent_action_button_position_set(int index, int x, int y);
int intgame_recent_action_default_skill(int index);
int intgame_skill_button_at_mouse(void);
int intgame_mt_spell_button_hit_test(void);
int intgame_spell_button_hit_test(void);
void intgame_hide(void);
void intgame_show(void);

#endif /* ARCANUM_UI_INTGAME_H_ */
