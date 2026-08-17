#include "game/ui.h"

// 0x5E8658
static UiCallbacks ui_callbacks;

// 0x5E87C8
static TigIdxTable stru_5E87C8;

// 0x5E87D8
static int dword_5E87D8;

// 0x45FD40
void ui_init(UiCallbacks* callbacks)
{
    if (callbacks != NULL) {
        ui_callbacks = *callbacks;
    }

    tig_idxtable_init(&stru_5E87C8, sizeof(UiMessage));
}

// 0x460150
void ui_exit(void)
{
    tig_idxtable_exit(&stru_5E87C8);
}

// 0x460160
bool ui_timeevent_process(TimeEvent* timeevent)
{
    UiMessage ui_message;

    if (timeevent->params[0].integer_value != 0) {
        tig_debug_printf("UI: Unknown type of timeevent passed to ui_timeevent_process (%d).\n",
            timeevent->params[0].integer_value);
    } else {
        tig_idxtable_get(&stru_5E87C8, timeevent->params[1].integer_value, &ui_message);
        ui_display_msg(&ui_message);
        FREE(ui_message.str);
    }

    return true;
}

/**
 * Notifies the UI that the local PC's stats or abilities changed.
 *
 * Called after stat, spell, magick and fatigue changes; the in-game
 * implementation redraws the interface bars and the character sheet.
 *
 * 0x4601C0
 */
void ui_refresh_interface(void)
{
    if (ui_callbacks.refresh_interface != NULL) {
        ui_callbacks.refresh_interface();
    }
}

// 0x4601D0
void ui_notify_object_destroyed(int64_t obj)
{
    if (ui_callbacks.notify_object_destroyed != NULL) {
        ui_callbacks.notify_object_destroyed(obj);
    }
}

// 0x4601F0
void ui_update_inven(int64_t obj)
{
    if (ui_callbacks.update_inven != NULL) {
        ui_callbacks.update_inven(obj);
    }
}

// 0x460240
void ui_refresh_health_bar(int64_t obj)
{
    if (ui_callbacks.refresh_health_bar != NULL) {
        ui_callbacks.refresh_health_bar(obj);
    }
}

// 0x460260
void ui_refresh_fatigue_bar(int64_t obj)
{
    if (ui_callbacks.refresh_fatigue_bar != NULL) {
        ui_callbacks.refresh_fatigue_bar(obj);
    }
}

// 0x460280
void ui_set_examine_obj(int64_t obj)
{
    if (ui_callbacks.set_examine_obj != NULL) {
        ui_callbacks.set_examine_obj(obj);
    }
}

// 0x4602A0
void ui_notify_item_inserted_or_removed(int64_t item_obj, bool removed, int inventory_location)
{
    if (ui_callbacks.notify_item_inserted_or_removed != NULL) {
        ui_callbacks.notify_item_inserted_or_removed(item_obj, removed, inventory_location);
    }
}

// 0x4602D0
void ui_show_inven_loot(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.show_inven_loot != NULL) {
        ui_callbacks.show_inven_loot(pc_obj, target_obj);
    }
}

// 0x460300
void ui_show_inven_identify(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.show_inven_identify != NULL) {
        ui_callbacks.show_inven_identify(pc_obj, target_obj);
    }
}

// 0x460330
void ui_tech_adjust_degree(int64_t obj, int tech, int action)
{
    if (ui_callbacks.tech_adjust_degree != NULL) {
        ui_callbacks.tech_adjust_degree(obj, tech, action);
    }
}

// 0x460360
void ui_skill_preprocess(int64_t obj, int type)
{
    if (ui_callbacks.skill_preprocess != NULL) {
        ui_callbacks.skill_preprocess(obj, type);
    }
}

// 0x460380
void ui_skill_activate(TargetDescriptor* td, int64_t obj, int a3)
{
    if (ui_callbacks.skill_activate != NULL) {
        ui_callbacks.skill_activate(td, obj, a3);
    }
}

// 0x4603B0
void ui_adjust_skill(int64_t obj, int skill, int action)
{
    if (ui_callbacks.adjust_skill != NULL) {
        ui_callbacks.adjust_skill(obj, skill, action);
    }
}

// 0x4603E0
void ui_set_skill_training(int64_t obj, int skill, int training)
{
    if (ui_callbacks.set_skill_training != NULL) {
        ui_callbacks.set_skill_training(obj, skill, training);
    }
}

// 0x460410
void ui_talk_to(int64_t a1, int64_t a2)
{
    if (ui_callbacks.talk_to != NULL) {
        ui_callbacks.talk_to(a1, a2);
    }
}

// 0x460460
int ui_is_in_dialog(int64_t obj)
{
    if (ui_callbacks.is_in_dialog != NULL) {
        return ui_callbacks.is_in_dialog(obj);
    } else {
        return 0;
    }
}

// 0x460480
void ui_spell_add(int64_t obj, int spell)
{
    if (ui_callbacks.spell_add != NULL) {
        ui_callbacks.spell_add(obj, spell);
    }
}

// 0x4604A0
bool ui_spell_maintain_add(int mt_id)
{
    if (ui_callbacks.spell_maintain_add != NULL) {
        return ui_callbacks.spell_maintain_add(mt_id);
    } else {
        return false;
    }
}

// 0x4604C0
void ui_spell_maintain_end(int mt_id)
{
    if (ui_callbacks.spell_maintain_end != NULL) {
        ui_callbacks.spell_maintain_end(mt_id);
    }
}

// 0x4604E0
void ui_spell_maintain_refresh(void)
{
    if (ui_callbacks.spell_maintain_refresh != NULL) {
        ui_callbacks.spell_maintain_refresh();
    }
}

// 0x4604F0
void ui_examine_object(int64_t a1, int64_t a2)
{
    if (ui_callbacks.examine_object != NULL) {
        ui_callbacks.examine_object(a1, a2);
    }
}

// 0x460520
void ui_end_death(void)
{
    if (ui_callbacks.end_death != NULL) {
        ui_callbacks.end_death();
    }
}

// 0x460530
void ui_end_game(void)
{
    if (ui_callbacks.end_game != NULL) {
        ui_callbacks.end_game();
    }
}

// 0x460540
void ui_clear_hotkey(int a1)
{
    if (ui_callbacks.clear_hotkey != NULL) {
        ui_callbacks.clear_hotkey(a1);
    }
}

// 0x460560
void ui_set_item_location(int64_t a1, int64_t a2, int a3)
{
    if (ui_callbacks.set_item_location != NULL) {
        ui_callbacks.set_item_location(a1, a2, a3);
    }
}

// 0x460590
int ui_assign_item_hotkey(int64_t a1, int a2)
{
    if (ui_callbacks.assign_item_hotkey != NULL) {
        return ui_callbacks.assign_item_hotkey(a1, a2);
    } else {
        return 0;
    }
}

/**
 * Reset the interface after the world under it changed.
 *
 * Recovers the hotkeys, forces INTGAME_MODE_MAIN and drops the hover object.
 * Called on a map change and from multiplayer.
 *
 * 0x4605C0
 */
void ui_reset_interface(void)
{
    if (ui_callbacks.reset_interface != NULL) {
        ui_callbacks.reset_interface();
    }
}

/**
 * Redraw the interface counters - money, mana, ammo.
 *
 * Distinct from `ui_refresh_interface`, which redraws the bars and the
 * character sheet after a stat change; this one fires after gold transfers,
 * item moves and magictech changes.
 *
 * 0x4605D0
 */
void ui_refresh_counters(void)
{
    if (ui_callbacks.refresh_counters != NULL) {
        ui_callbacks.refresh_counters();
    }
}

// 0x4605E0
void ui_item_spell_activate(int64_t a1, void* a2, int a3)
{
    if (ui_callbacks.item_spell_activate != NULL) {
        ui_callbacks.item_spell_activate(a1, a2, a3);
    }
}

// 0x460610
void ui_display_warning(char* str)
{
    if (ui_callbacks.display_warning != NULL) {
        ui_callbacks.display_warning(str);
    }
}

// 0x460630
void ui_display_msg(UiMessage* ui_message)
{
    if (ui_callbacks.display_msg != NULL) {
        ui_callbacks.display_msg(ui_message);
    }
}

// 0x460650
// NOTE: The original calls this from broadcast_msg_client's network branch
// (the local player gets the message queued for display after 1000 ms); that
// branch is untranscribed in CE, consistent with multiplayer being out of
// scope.
void ui_display_msg_delayed(UiMessage* ui_message, unsigned int milliseconds)
{
    int id;

    DateTime datetime;
    TimeEvent timeevent;

    id = dword_5E87D8++;
    ui_message->str = STRDUP(ui_message->str);
    timeevent.type = TIMEEVENT_TYPE_UI;
    timeevent.params[0].integer_value = 0;
    timeevent.params[1].integer_value = id;
    datetime_from_milliseconds(&datetime, milliseconds);
    tig_idxtable_set(&stru_5E87C8, id, ui_message);
    timeevent_add_delay(&timeevent, &datetime);
}

// 0x4606C0
void ui_feedback_msg(int a1)
{
    if (ui_callbacks.feedback_msg != NULL) {
        ui_callbacks.feedback_msg(a1);
    }
}

// 0x4606E0
void ui_wants_mainmenu_set(void)
{
    if (ui_callbacks.wants_mainmenu_set != NULL) {
        ui_callbacks.wants_mainmenu_set();
    }
}

// 0x4606F0
void ui_written_start_obj(int64_t written_obj, int64_t pc_obj)
{
    if (ui_callbacks.written_start_obj != NULL) {
        ui_callbacks.written_start_obj(written_obj, pc_obj);
    }
}

// 0x460720
void ui_written_start_type(WrittenType written_type, int num)
{
    if (ui_callbacks.written_start_type != NULL) {
        ui_callbacks.written_start_type(written_type, num);
    }
}

// 0x460740
void ui_item_activate(int64_t owner_obj, int64_t item_obj)
{
    if (ui_callbacks.item_activate != NULL) {
        ui_callbacks.item_activate(owner_obj, item_obj);
    }
}

// 0x460770
void ui_follower_refresh(void)
{
    if (ui_callbacks.follower_refresh != NULL) {
        ui_callbacks.follower_refresh();
    }
}

// 0x460780
void ui_follower_update(void)
{
    if (ui_callbacks.follower_update != NULL) {
        ui_callbacks.follower_update();
    }
}

// 0x460790
void ui_toggle_primary_button(UiPrimaryButton btn, bool on)
{
    if (ui_callbacks.toggle_primary_button != NULL) {
        ui_callbacks.toggle_primary_button(btn, on);
    }
}

// 0x4607B0
void ui_set_map_button(UiPrimaryButton btn)
{
    if (ui_callbacks.set_map_button != NULL) {
        ui_callbacks.set_map_button(btn);
    }
}

// 0x4607D0
void ui_notify_sector_changed(int64_t sec, int64_t obj)
{
    if (ui_callbacks.notify_sector_changed != NULL) {
        ui_callbacks.notify_sector_changed(sec, obj);
    }
}

// 0x460800
void ui_written_newspaper_headline(int newspaper, char* str)
{
    if (ui_callbacks.written_newspaper_headline != NULL) {
        ui_callbacks.written_newspaper_headline(newspaper, str);
    }
}

// 0x460820
void ui_sleep_toggle(int64_t bed_obj)
{
    if (ui_callbacks.sleep_toggle != NULL) {
        ui_callbacks.sleep_toggle(bed_obj);
    }
}

// 0x460840
void ui_charedit_error_msg(int type, int param)
{
    if (ui_callbacks.charedit_error_msg != NULL) {
        ui_callbacks.charedit_error_msg(type, param);
    }
}

// 0x460860
void ui_charedit_refresh(void)
{
    if (ui_callbacks.charedit_refresh != NULL) {
        ui_callbacks.charedit_refresh();
    }
}

// 0x460870
void ui_progressbar_init(int max_value)
{
    if (ui_callbacks.progressbar_init != NULL) {
        ui_callbacks.progressbar_init(max_value);
    }
}

// 0x460890
void ui_progressbar_update(int value)
{
    if (ui_callbacks.progressbar_update != NULL) {
        ui_callbacks.progressbar_update(value);
    }
}

// 0x4608B0
void ui_wmap_select(int64_t obj, int spell)
{
    if (ui_callbacks.wmap_select != NULL) {
        ui_callbacks.wmap_select(obj, spell);
    }
}

// 0x4608D0
void ui_schematic_process(int schematic, int64_t primary_obj, int64_t secondary_obj)
{
    if (ui_callbacks.schematic_process != NULL) {
        ui_callbacks.schematic_process(schematic, primary_obj, secondary_obj);
    }
}

// 0x460900
void ui_schematic_feedback(bool success, int64_t primary_obj, int64_t secondary_obj)
{
    if (ui_callbacks.schematic_feedback != NULL) {
        ui_callbacks.schematic_feedback(success, primary_obj, secondary_obj);
    }
}

// 0x460930
void ui_open_charedit_passive(int64_t a1, int64_t a2)
{
    if (ui_callbacks.open_charedit_passive != NULL) {
        ui_callbacks.open_charedit_passive(a1, a2);
    }
}

// 0x460960
void ui_process_item_transfer(Packet81* pkt)
{
    if (ui_callbacks.process_item_transfer != NULL) {
        ui_callbacks.process_item_transfer(pkt);
    }
}

// 0x460980
void ui_begin_item_drag(int64_t a1)
{
    if (ui_callbacks.begin_item_drag != NULL) {
        ui_callbacks.begin_item_drag(a1);
    }
}

// 0x4609D0
void ui_gameuilib_reset(void)
{
    if (ui_callbacks.gameuilib_reset != NULL) {
        ui_callbacks.gameuilib_reset();
    }
}

// 0x4609E0
void ui_start_dialog(int64_t pc_obj, int64_t npc_obj, int script_num, int script_line, int num)
{
    if (ui_callbacks.start_dialog != NULL) {
        ui_callbacks.start_dialog(pc_obj, npc_obj, script_num, script_line, num);
    }
}

// 0x460A20
void ui_end_dialog(int64_t pc_obj, int a3)
{
    if (ui_callbacks.end_dialog != NULL) {
        ui_callbacks.end_dialog(pc_obj, a3);
    }
}

// 0x460AB0
// Dispatches a UI event to the UiCallbacks slot 0xF8. The original wires
// that slot to the gameuilib event dispatcher at 0x569C70 (untranscribed in
// CE, so tb_ui_init NULLs it). Known events: 115 (party roster changed, from
// party_add/party_remove) and 116 (from the multiplayer event handler).
void ui_event_notify(int event, int a2, void* data)
{
    if (ui_callbacks.field_F8 != NULL) {
        ui_callbacks.field_F8(event, a2, data);
    }
}

// 0x460AD0
void ui_return_to_mainmenu(int a1)
{
    if (ui_callbacks.return_to_mainmenu != NULL) {
        ui_callbacks.return_to_mainmenu(a1);
    }
}

// 0x460AF0
void ui_refresh_cursor(void)
{
    if (ui_callbacks.refresh_cursor != NULL) {
        ui_callbacks.refresh_cursor();
    }
}

// 0x460B00
int ui_skill_for_action(int a1, TargetDescriptor* td)
{
    if (ui_callbacks.skill_for_action != NULL) {
        return ui_callbacks.skill_for_action(a1, td);
    } else {
        return 0;
    }
}

// 0x460B20
void ui_drop_dragged_item(void)
{
    if (ui_callbacks.drop_dragged_item != NULL) {
        ui_callbacks.drop_dragged_item();
    }
}

// 0x460B30
bool ui_wmap_rnd_timeevent_process(TimeEvent* timeevent)
{
    if (ui_callbacks.wmap_rnd_timeevent_process != NULL) {
        return ui_callbacks.wmap_rnd_timeevent_process(timeevent);
    } else {
        return false;
    }
}

// 0x460B50
// Opens the schematic UI (SAT_START_SCHEMATIC_UI): the PC passes itself as
// both objects, an NPC passes itself and its leader. Dispatches to the
// UiCallbacks slot 0x110, which is unwired in BOTH binaries - the original
// never stores it in tb_ui_init either, so the dispatch fires through stack
// garbage. Faithful, deliberately not fixed.
void ui_open_schematic(int64_t primary_obj, int64_t secondary_obj)
{
    if (ui_callbacks.field_110 != NULL) {
        ui_callbacks.field_110(primary_obj, secondary_obj);
    }
}

// 0x460B80
void ui_queue_slide(int slide)
{
    if (ui_callbacks.queue_slide != NULL) {
        ui_callbacks.queue_slide(slide);
    }
}

// 0x460BA0
bool ui_gameuilib_mod_load(void)
{
    if (ui_callbacks.gameuilib_mod_load != NULL) {
        return ui_callbacks.gameuilib_mod_load();
    } else {
        return false;
    }
}

// 0x460BB0
int ui_confirm_save_char(void)
{
    if (ui_callbacks.confirm_save_char != NULL) {
        return ui_callbacks.confirm_save_char();
    } else {
        return 0;
    }
}

// 0x460BC0
int ui_confirm_export_char(void)
{
    // NOTE: Checks the wrong callback for NULL, and Troika's - `ui_callbacks`
    // is at 0x5E8658, so 0x460BC0 tests 0x5E8774 (+0x11C, confirm_save_char)
    // and calls 0x5E8778 (+0x120, confirm_export_char). Harmless in practice
    // because tb_ui installs both, but it would call through NULL if only the
    // export one were ever left unset. Faithful, so left alone.
    if (ui_callbacks.confirm_save_char != NULL) {
        return ui_callbacks.confirm_export_char();
    } else {
        return 0;
    }
}

// 0x460BE0
int ui_confirm_overwrite_char(const char* a1, const char* a2)
{
    if (ui_callbacks.confirm_overwrite_char != NULL) {
        return ui_callbacks.confirm_overwrite_char(a1, a2);
    } else {
        return 0;
    }
}

// 0x460C00
void ui_schematic_info_get(int schematic, SchematicInfo* schematic_info)
{
    if (ui_callbacks.schematic_info_get != NULL) {
        ui_callbacks.schematic_info_get(schematic, schematic_info);
    }
}

// 0x460C20
int64_t ui_inven_target_obj_get(void)
{
    if (ui_callbacks.inven_target_obj_get != NULL) {
        return ui_callbacks.inven_target_obj_get();
    } else {
        return OBJ_HANDLE_NULL;
    }
}

// 0x460C30
void ui_refresh_examine(int64_t a1)
{
    if (ui_callbacks.refresh_examine != NULL) {
        ui_callbacks.refresh_examine(a1);
    }
}

// 0x460C50
int ui_can_open_inven(int64_t a1, int64_t a2, int a3)
{
    if (ui_callbacks.can_open_inven != NULL) {
        return ui_callbacks.can_open_inven(a1, a2, a3);
    } else {
        return 0;
    }
}

// 0x460C80
int ui_enter_inven_mode(int64_t a1, int64_t a2, int a3)
{
    if (ui_callbacks.enter_inven_mode != NULL) {
        return ui_callbacks.enter_inven_mode(a1, a2, a3);
    } else {
        return 0;
    }
}

// 0x460CB0
void ui_prepare_inven_target(int64_t a1, int64_t a2, int a3)
{
    if (ui_callbacks.prepare_inven_target != NULL) {
        ui_callbacks.prepare_inven_target(a1, a2, a3);
    }
}

// 0x460CE0
bool ui_inven_create(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (ui_callbacks.inven_create != NULL) {
        return ui_callbacks.inven_create(pc_obj, target_obj, mode);
    } else {
        return false;
    }
}

// 0x460D10
// NOTE: Slot 0x144. The original wires it to the gameuilib chat function
// 0x582510 (formats mes 10054, "%s has entered the room."); untranscribed in
// CE, so tb_ui_init NULLs the slot. Never called in either binary.
void ui_chat_player_entered(const char* name)
{
    if (ui_callbacks.field_144 != NULL) {
        ui_callbacks.field_144(name);
    }
}

// 0x460D30
// NOTE: Slot 0x148. The original wires it to the gameuilib chat function
// 0x5825B0 (formats mes 10055, "%s has left."); untranscribed in CE, so
// tb_ui_init NULLs the slot. Never called in either binary.
void ui_chat_player_left(const char* name)
{
    if (ui_callbacks.field_148 != NULL) {
        ui_callbacks.field_148(name);
    }
}

// 0x460D50
// NOTE: Slot 0x14C. The original wires it to the gameuilib chat function
// 0x582650 (appends a type-0 chat line to the history ring); untranscribed
// in CE, so tb_ui_init NULLs the slot. Never called in either binary.
void ui_chat_line_type_0(const char* a1, const char* a2)
{
    if (ui_callbacks.field_14C != NULL) {
        ui_callbacks.field_14C(a1, a2);
    }
}

// 0x460D70
// NOTE: Slot 0x150. The original wires it to the gameuilib chat function
// 0x582670 (type-1 chat line); untranscribed in CE, so tb_ui_init NULLs the
// slot. Never called in either binary.
void ui_chat_line_type_1(const char* a1, const char* a2)
{
    if (ui_callbacks.field_150 != NULL) {
        ui_callbacks.field_150(a1, a2);
    }
}

// 0x460D90
// NOTE: Slot 0x154. The original wires it to 0x5826B0, which is a bare
// `ret` even there - a dead chat callback in both binaries. Never called in
// either.
void ui_chat_field_154_hook(int a1, int a2)
{
    if (ui_callbacks.field_154 != NULL) {
        ui_callbacks.field_154(a1, a2);
    }
}

// 0x460DB0
// NOTE: Slot 0x158. The original wires it to the gameuilib chat function
// 0x582690 (type-6 chat line); untranscribed in CE, so tb_ui_init NULLs the
// slot. Never called in either binary.
void ui_chat_line_type_6(const char* a1, const char* a2)
{
    if (ui_callbacks.field_158 != NULL) {
        ui_callbacks.field_158(a1, a2);
    }
}

// 0x460DD0
// NOTE: Slot 0x15C. The original wires it to the gameuilib window reset
// 0x5826C0 (jmp 0x5830F0, which destroys and recreates the chat window);
// untranscribed in CE, so tb_ui_init NULLs the slot. Never called in either
// binary.
void ui_chat_window_reset(void)
{
    if (ui_callbacks.field_15C != NULL) {
        ui_callbacks.field_15C();
    }
}

// 0x460DE0
void ui_show_inven_npc_identify(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.show_inven_npc_identify != NULL) {
        ui_callbacks.show_inven_npc_identify(pc_obj, target_obj);
    }
}
