#include "ui/spell_ui.h"

#include "game/critter.h"
#include "game/item.h"
#include "game/magictech.h"
#include "game/mt_item.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/ui.h"
#include "ui/charedit_ui.h"
#include "ui/hotkey_ui.h"
#include "ui/intgame.h"
#include "ui/inven_ui.h"

#define FIVE 5

typedef struct S5CB3A8 {
    int mt_id;
    int field_4;
} S5CB3A8;

static void spell_ui_maintain_reset(void);

// 0x5CB3A0
static int dword_5CB3A0 = 10000;

// 0x5CB3A8
static S5CB3A8 spell_ui_maintain_slots[FIVE] = {
    { -1, 0 },
    { -1, 0 },
    { -1, 0 },
    { -1, 0 },
    { -1, 0 },
};

// 0x5CB3D0
static int spell_ui_maintain_bar_num_slots = -1;

// 0x6834F8
static int64_t qword_6834F8;

// 0x683500
static int64_t qword_683500;

// 0x683508
static bool dword_683508;

// 0x57BB30
bool spell_ui_init(GameInitInfo* init_info)
{
    (void)init_info;

    return true;
}

// 0x57BB40
void spell_ui_reset(void)
{
    spell_ui_maintain_reset();
}

// 0x57BB50
void spell_ui_exit(void)
{
}

// 0x57BB60
bool spell_ui_save(TigFile* stream)
{
    int index;

    for (index = 0; index < FIVE; index++) {
        if (tig_file_fwrite(&(spell_ui_maintain_slots[index].mt_id), sizeof(spell_ui_maintain_slots[index].mt_id), 1, stream) != 1) return false;
        if (tig_file_fwrite(&(spell_ui_maintain_slots[index].field_4), sizeof(spell_ui_maintain_slots[index].field_4), 1, stream) != 1) return false;
    }

    return true;
}

// 0x57BBB0
bool spell_ui_load(GameLoadInfo* load_info)
{
    int index;

    for (index = 0; index < FIVE; index++) {
        if (tig_file_fread(&(spell_ui_maintain_slots[index].mt_id), sizeof(spell_ui_maintain_slots[index].mt_id), 1, load_info->stream) != 1) return false;
        if (tig_file_fread(&(spell_ui_maintain_slots[index].field_4), sizeof(spell_ui_maintain_slots[index].field_4), 1, load_info->stream) != 1) return false;

        if (spell_ui_maintain_slots[index].field_4) {
            intgame_spell_maintain_art_set(index, magictech_spell_icon_art_id_get(spell_ui_maintain_slots[index].mt_id));
        }
    }

    return true;
}

// 0x57BC30
void spell_ui_error_target_not_damaged(void)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mes_file_entry.num = 606; // "The target is already fully healed."
    magictech_get_msg(&mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    ui_display_msg(&ui_message);
}

// 0x57BC70
SpellUiActivate spell_ui_activate(int64_t obj, int spl)
{
    uint64_t* tgt_ptr;
    uint64_t tgt;
    bool v1 = false;
    TargetDescriptor td;

    if (obj == OBJ_HANDLE_NULL) {
        return SPELL_UI_ACTIVATE_ERR;
    }

    dword_683508 = true;

    if (obj_type_is_item(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        if (!item_parent(obj, &qword_683500)) {
            return SPELL_UI_ACTIVATE_ERR;
        }
    } else {
        qword_683500 = obj;
    }

    if (qword_683500 == OBJ_HANDLE_NULL) {
        return SPELL_UI_ACTIVATE_ERR;
    }

    if (!player_is_pc_obj(qword_683500)) {
        return SPELL_UI_ACTIVATE_ERR;
    }

    if ((obj_field_int32_get(qword_683500, OBJ_F_FLAGS) & OF_OFF) != 0) {
        return SPELL_UI_ACTIVATE_ERR;
    }

    if (critter_is_dead(qword_683500)) {
        return SPELL_UI_ACTIVATE_ERR;
    }

    if (critter_is_unconscious(qword_683500)) {
        return SPELL_UI_ACTIVATE_ERR;
    }

    if (intgame_mode_get() == INTGAME_MODE_SPELL) {
        spell_ui_cancel();
    }

    if (!magictech_is_enabled(spl)) {
        dword_683508 = false;
        return SPELL_UI_ACTIVATE_ERR;
    }

    dword_5CB3A0 = spl;
    qword_6834F8 = obj;

    magictech_target_flags_get(spl, &tgt_ptr);
    tgt = *tgt_ptr;

    if (magictech_is_aggressive(dword_5CB3A0)
        && !tig_kb_get_modifier(SDL_KMOD_ALT)) {
        tgt |= TGT_NON_PARTY_CRITTERS;
    }

    if ((tgt & TGT_OBJ_INVEN) != 0) {
        if (intgame_mode_get() == INTGAME_MODE_BARTER
            || intgame_mode_get() == INTGAME_MODE_STEAL) {
            inven_ui_destroy();
        }

        if (inven_ui_is_created()) {
            v1 = true;
        } else {
            inven_ui_open(qword_6834F8, OBJ_HANDLE_NULL, INVEN_UI_MODE_INVENTORY);
        }
    } else {
        if (!intgame_mode_set(INTGAME_MODE_MAIN)) {
            dword_683508 = false;
            return SPELL_UI_ACTIVATE_ERR;
        }
    }

    // NOTE: Verified against 0x57BE68-0x57BFA2 - the self/no-tile and TGT_OBJ_RADIUS immediate-apply path is original; transcription faithful.
    if (*tgt_ptr == TGT_NONE
        || ((*tgt_ptr & TGT_SELF) != 0
            && (*tgt_ptr & TGT_TILE) == 0)
        || *tgt_ptr == TGT_OBJ_RADIUS) {
        target_descriptor_set_loc(&td, obj_field_int64_get(qword_683500, OBJ_F_LOCATION));
        spell_ui_apply(&td);
        dword_683508 = false;
        return SPELL_UI_ACTIVATE_ERR;
    }

    if (!combat_check_cast_spell(qword_683500)) {
        dword_683508 = false;
        return false;
    }

    target_flags_set(tgt);
    if (intgame_mode_set(INTGAME_MODE_SPELL)) {
        target_flags_set(tgt);

        if ((*tgt_ptr & TGT_OBJECT) != 0
            && tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
            target_descriptor_set_obj(&td, player_get_local_pc_obj());
            if ((*tgt_ptr & TGT_OBJ_NO_SELF) == 0) {
                spell_ui_apply(&td);
                dword_683508 = false;
                return SPELL_UI_ACTIVATE_ERR;
            }
        }
    }

    if (v1) {
        if (!inven_ui_is_created()) {
            inven_ui_open(qword_6834F8, OBJ_HANDLE_NULL, INVEN_UI_MODE_INVENTORY);
        }
    }

    dword_683508 = false;
    return SPELL_UI_ACTIVATE_OK;
}

// 0x57BFF0
void spell_ui_cancel(void)
{
    intgame_mode_set(INTGAME_MODE_MAIN);

    if (!dword_683508) {
        dword_5CB3A0 = 10000;
        qword_6834F8 = OBJ_HANDLE_NULL;
        qword_683500 = OBJ_HANDLE_NULL;
    }
}

// 0x57C040
void spell_ui_item_spell_activate(int64_t obj, int index)
{
    int spl;

    spl = mt_item_spell(obj, index);
    if (!magictech_has_item_triggers(spl) && spl != -1) {
        spell_ui_activate(obj, spl);
    }
}

// 0x57C080
void spell_ui_item_spell_activate_by_spl(int64_t obj, int spl)
{
    if (!magictech_has_item_triggers(spl) && spl != -1) {
        spell_ui_activate(obj, spl);
    }
}

// 0x57C0B0
void spell_ui_aggressive_mode_off(void)
{
    uint64_t tgt;

    if (magictech_is_aggressive(dword_5CB3A0)) {
        tgt = target_flags_get();
        tgt &= ~TGT_NON_PARTY_CRITTERS;
        target_flags_set(tgt);
    }
}

// 0x57C0E0
void spell_ui_aggressive_mode_on(void)
{
    uint64_t tgt;

    if (magictech_is_aggressive(dword_5CB3A0)) {
        tgt = target_flags_get();
        tgt |= TGT_NON_PARTY_CRITTERS;
        target_flags_set(tgt);
    }
}

// 0x57C110
void spell_ui_apply(TargetDescriptor* td)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    MagicTechInvocation mt_invocation;

    if (critter_is_dead(qword_683500)
        || critter_is_unconscious(qword_683500)) {
        return;
    }

    if (obj_type_is_critter(obj_field_int32_get(qword_6834F8, OBJ_F_TYPE))
        && !magictech_can_charge_spell_fatigue(qword_683500, dword_5CB3A0)) {
        mes_file_entry.num = 600; // "Not enough Energy."
        magictech_get_msg(&mes_file_entry);

        ui_display_warning(mes_file_entry.str);
        return;
    }

    magictech_invocation_init(&mt_invocation, qword_6834F8, dword_5CB3A0);
    if (td->is_loc) {
        mt_invocation.target_loc = td->loc;
    } else {
        object_ref_init(td->obj, &(mt_invocation.target_obj));
    }

    if (!magictech_check_los(&mt_invocation)) {
        mes_file_entry.num = 604; // "You cannot see the target."
        magictech_get_msg(&mes_file_entry);

        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        ui_display_msg(&ui_message);
        return;
    }

    magictech_invocation_run(&mt_invocation);
}

// 0x57C290
bool spell_ui_maintain_add(int mt_id)
{
    int candidate = -1;
    int64_t pc_obj;
    int cnt;
    int index;

    if (!magictech_run_spell_valid(mt_id)) {
        return false;
    }

    pc_obj = player_get_local_pc_obj();
    cnt = stat_level_get(pc_obj, STAT_INTELLIGENCE) / 4;

    for (index = 0; index < cnt; index++) {
        if (spell_ui_maintain_slots[index].field_4 == 1 && spell_ui_maintain_slots[index].mt_id == mt_id) {
            return true;
        }

        if (spell_ui_maintain_slots[index].mt_id == -1 && candidate == -1) {
            candidate = index;
        }
    }

    if (candidate >= cnt) {
        return false;
    }

    spell_ui_maintain_slots[candidate].mt_id = mt_id;
    spell_ui_maintain_slots[candidate].field_4 = 1;
    intgame_spell_maintain_art_set(candidate, magictech_spell_icon_art_id_get(mt_id));

    return true;
}

// 0x57C320
void spell_ui_maintain_end(int mt_id)
{
    int index;

    if (mt_id == -1) {
        return;
    }

    for (index = 0; index < FIVE; index++) {
        if (spell_ui_maintain_slots[index].mt_id == mt_id
            && spell_ui_maintain_slots[index].field_4 >= 1) {
            break;
        }
    }

    if (index < FIVE) {
        spell_ui_maintain_slots[index].mt_id = -1;
        spell_ui_maintain_slots[index].field_4 = 0;
        intgame_spell_maintain_art_set(index, TIG_ART_ID_INVALID);
    }
}

// 0x57C370
void spell_ui_maintain_click(int slot)
{

    if (spell_ui_maintain_slots[slot].mt_id == -1) {
        return;
    }

    if (spell_ui_maintain_slots[slot].field_4 != 1) {
        return;
    }

    spell_ui_maintain_slots[slot].field_4 = 2;

    magictech_interrupt(spell_ui_maintain_slots[slot].mt_id);
}

// 0x57C3F0
void spell_ui_maintain_hover(int slot)
{
    MagicTechRunInfo* run_info;
    int64_t obj;

    // NOTE: Verified against 0x57C3F3 - original calls player_get_local_pc_obj and discards the result; transcription faithful.
    player_get_local_pc_obj();

    if (spell_ui_maintain_slots[slot].field_4 != 1) {
        return;
    }

    if (!magictech_id_to_run_info(spell_ui_maintain_slots[slot].mt_id, &run_info)) {
        return;
    }

    obj = run_info->target_obj.obj;
    if (obj == OBJ_HANDLE_NULL) {
        if (run_info->summoned_obj != NULL) {
            obj = run_info->summoned_obj->obj;
        }
        if (obj == OBJ_HANDLE_NULL) {
            obj = run_info->parent_obj.obj;
        }
    }

    intgame_message_window_display_spell(run_info->spell);

    if (obj != OBJ_HANDLE_NULL) {
        object_hover_obj_set(obj);
    }
}

// 0x57C470
void spell_ui_maintain_reset(void)
{
    int index;

    for (index = 0; index < FIVE; index++) {
        spell_ui_maintain_slots[index].mt_id = -1;
        spell_ui_maintain_slots[index].field_4 = 0;
        intgame_spell_maintain_art_set(index, TIG_ART_ID_INVALID);
    }

    spell_ui_maintain_bar_num_slots = -1;
}

// 0x57C4B0
void spell_ui_maintain_refresh(void)
{
    int64_t obj;
    int num_slots;
    int slot;

    obj = player_get_local_pc_obj();
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    num_slots = stat_level_get(obj, STAT_INTELLIGENCE) / 4;
    if (num_slots == spell_ui_maintain_bar_num_slots) {
        return;
    }

    slot = 0;
    while (slot < num_slots) {
        intgame_spell_maintain_refresh(slot++, true);
    }
    while (slot < FIVE) {
        intgame_spell_maintain_refresh(slot++, false);
    }

    spell_ui_maintain_bar_num_slots = num_slots;
}

// 0x57C520
bool spell_ui_maintain_has(int slot)
{
    return spell_ui_maintain_slots[slot].field_4 != 0;
}

// 0x57C540
void spell_ui_add(int64_t obj, int spell)
{
    int cost;
    int unspent_points;

    cost = spell_cost(spell);
    unspent_points = stat_level_get(obj, STAT_UNSPENT_POINTS);

    if (cost > unspent_points) {
        charedit_error_not_enough_character_points();
        return;
    }

    if (!spell_add(obj, spell, false)) {
        if (spell_min_level(spell) > stat_level_get(obj, STAT_LEVEL)) {
            charedit_error_not_enough_level();
            return;
        }

        if (spell_min_intelligence(spell) > stat_level_get(obj, STAT_INTELLIGENCE)) {
            charedit_error_not_enough_intelligence();
        } else {
            charedit_error_not_enough_stat(6);
        }

        return;
    }

    stat_base_set(obj, STAT_UNSPENT_POINTS, unspent_points - cost);
    if (charedit_is_created()) {
        charedit_refresh();
        intgame_message_window_clear();
    }
}

// 0x57C670
void spell_ui_remove(int64_t obj, int spell)
{
    int cost;
    int unspent_points;

    cost = spell_cost(spell);
    unspent_points = stat_level_get(obj, STAT_UNSPENT_POINTS);
    if (spell_remove(obj, spell)) {
        hotkey_ui_notify_spell_removed(spell);
        stat_base_set(obj, STAT_UNSPENT_POINTS, unspent_points + cost);

        if (charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    }
}
