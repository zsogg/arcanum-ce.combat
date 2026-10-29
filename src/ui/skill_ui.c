#include "ui/skill_ui.h"

#include "game/anim.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/feature.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/script.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/tf.h"
#include "game/uap.h"
#include "ui/charedit_ui.h"
#include "ui/dialog_ui.h"
#include "ui/intgame.h"
#include "ui/inven_ui.h"
#include "ui/types.h"

#define FOUR 4

static bool skill_ui_is_pickpocket_immune(int64_t obj);
static void skill_ui_delegate_to_follower(SkillInvocation* skill_invocation);
static void skill_ui_run_invocation(SkillInvocation* skill_invocation);
static bool skill_ui_open_steal_loot(int64_t a1, int64_t a2);
static bool skill_ui_success_fail_msg_field_0(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_steal_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success);
static bool skill_ui_plant_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success);
static bool skill_ui_success_fail_msg_field_C(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_disarm_trap(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_repair(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_no_repair(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_lock(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_no_lock(int64_t source_obj);

// 0x5CB220
static int skill_ui_button_art_nums[FOUR] = {
    279,
    280,
    282,
    278,
};

// 0x5CB230
static int skill_ui_icon_art_nums[FOUR] = {
    279,
    280,
    282,
    278,
};

// 0x5CB240
static int skill_ui_skills[FOUR] = {
    SKILL_PROWLING,
    SKILL_PICK_POCKET,
    SKILL_DISARM_TRAPS,
    SKILL_REPAIR,
};

// 0x5CB250
static uint64_t skill_ui_target_flags[FOUR] = {
    TGT_SELF,
    TGT_OBJ_NO_ST_CRITTER_ANIMAL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_TILE | TGT_OBJECT,
    TGT_OBJ_INVEN | TGT_OBJECT,
};

// 0x5CB270
static int skill_ui_current_type = -1;

// 0x683478
static char* skill_ui_names[FOUR];

// 0x683488
static mes_file_handle_t skill_ui_mes_file;

// 0x683490
static int64_t skill_ui_current_obj;

// 0x579E80
bool skill_ui_init(GameInitInfo* init_info)
{
    int index;
    SkillCallbacks callbacks;
    MesFileEntry mes_file_entry;

    (void)init_info;

    callbacks.field_0 = skill_ui_success_fail_msg_field_0;
    callbacks.steal_item_func = skill_ui_steal_item;
    callbacks.plant_item_output = skill_ui_plant_item;
    callbacks.field_C = skill_ui_success_fail_msg_field_C;
    callbacks.disarm_trap_func = skill_ui_disarm_trap;
    callbacks.repair_func = skill_ui_repair;
    callbacks.no_repair_func = skill_ui_no_repair;
    callbacks.lock_func = skill_ui_lock;
    callbacks.no_lock_func = skill_ui_no_lock;
    skill_set_callbacks(&callbacks);

    if (!mes_load("mes\\skill_ui.mes", &skill_ui_mes_file)) {
        return false;
    }

    for (index = 0; index < FOUR; index++) {
        mes_file_entry.num = 100 + index;
        mes_get_msg(skill_ui_mes_file, &mes_file_entry);
        skill_ui_names[index] = mes_file_entry.str;
    }

    return true;
}

// 0x579F30
void skill_ui_reset(void)
{
    skill_ui_current_obj = OBJ_HANDLE_NULL;
}

// 0x579F40
void skill_ui_exit(void)
{
    mes_unload(skill_ui_mes_file);
}

// 0x579F50
int skill_ui_button_art_num(int index)
{
    return index >= 0 && index < FOUR ? skill_ui_button_art_nums[index] : skill_ui_button_art_nums[0];
}

// 0x579F70
int skill_ui_icon_art_num(int index)
{
    return index >= 0 && index < FOUR ? skill_ui_icon_art_nums[index] : skill_ui_icon_art_nums[0];
}

// 0x579F90
int skill_ui_current_type_get(void)
{
    return skill_ui_current_type;
}

// Entry point from the skill buttons and hotkeyed skills: remembers obj/type,
// cancels an active skill mode, and starts the skill UI or preprocessing.
//
// 0x579FA0
void skill_ui_begin(int64_t obj, int type)
{
    int64_t pc_obj;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj != obj) {
        if (critter_is_dead(pc_obj)
            || critter_is_unconscious(pc_obj)) {
            return;
        }
    }

    // FIX (porting bug #98): the actor itself is tested whoever it is. The
    // original checks `obj` after the PC block, unconditionally
    // (0x579FE8-0x57A006); CE nested it inside `pc_obj != obj`, so for the
    // player - every skill button - an unconscious PC could still start a
    // skill.
    if (critter_is_dead(obj)
        || critter_is_unconscious(obj)) {
        return;
    }

    if (intgame_mode_get() == INTGAME_MODE_SKILL) {
        skill_ui_cancel();
    }

    skill_ui_current_obj = obj;
    skill_ui_current_type = type;

    // The original also broadcasts a skill-mode Packet1E here when
    // multiplayer_is_locked() is false. CE never reaches that branch:
    // tig_net_is_active() is always 0, so multiplayer_is_locked() is always
    // true. The branch is deliberately not transcribed.
    if ((skill_ui_target_flags[type] & 0x2000000) != 0
        || intgame_mode_set(INTGAME_MODE_MAIN)) {
        skill_ui_preprocess(obj, type);
    }
}

// 0x57A0A0
void skill_ui_preprocess(int64_t obj, int type)
{
    bool is_pc;
    uint64_t tgt;
    SkillInvocation skill_invocation;

    is_pc = player_is_local_pc_obj(obj);
    skill_ui_current_obj = obj;
    skill_ui_current_type = type;

    tgt = skill_ui_target_flags[type];
    skill_invocation_init(&skill_invocation);

    switch (type) {
    case 0:
        // CE (`prowl in combat master only`; zsogg, 2026-10-12): the manual's
        // Master perk - only a master prowler starts prowling in combat mode.
        // One already prowling when the fight starts keeps prowling.
        if (feature_enabled(FEATURE_PROWL_IN_COMBAT_MASTER_ONLY)
            && !critter_is_concealed(obj)
            && combat_critter_is_combat_mode_active(obj)
            && basic_skill_training_get(obj, BASIC_SKILL_PROWLING) < TRAINING_MASTER) {
            tf_add(obj, TF_TYPE_WHITE, "Only a master prowler hides in a fight");
            // CE (card MI1, IN-3): a refused press leaves no command a replay
            // could record, so the log says it happened.
            tig_debug_printf("Prowl: refused: not a master prowler, in combat mode\n");
            break;
        }
        // CE (`prowl in combat costs ap`; zsogg, 2026-10-28): starting to prowl
        // in combat mode costs AP, so a backstabber cannot hide again between
        // strikes for nothing. Turn-based only: the call charges nothing
        // outside it.
        if (feature_enabled(FEATURE_PROWL_IN_COMBAT_COSTS_AP)
            && !critter_is_concealed(obj)
            && combat_critter_is_combat_mode_active(obj)
            && !combat_consume_action_points(obj, COMBAT_PROWL_COST)) {
            tig_debug_printf("Prowl: refused: %d AP to prowl in combat mode\n", COMBAT_PROWL_COST);
            break;
        }
        critter_set_concealed(obj, !critter_is_concealed(obj));
        break;
    case 1:
    case 2:
        break;
    case 3:
        if (!is_pc) {
            return;
        }

        if (!inven_ui_is_created()) {
            inven_ui_open(obj, OBJ_HANDLE_NULL, INVEN_UI_MODE_INVENTORY);
        }
        break;
    default:
        tig_debug_printf("skill_ui_preprocess: ERROR: type out of range!\n");
        return;
    }

    if (is_pc && tgt != TGT_NONE && tgt != TGT_SELF) {
        if (intgame_mode_set(INTGAME_MODE_SKILL)) {
            target_flags_set(tgt);
        }
    }
}

// 0x57A1A0
void skill_ui_cancel(void)
{
    intgame_mode_set(INTGAME_MODE_MAIN);
    skill_ui_current_obj = OBJ_HANDLE_NULL;
}

// 0x57A1F0
void skill_ui_apply(TargetDescriptor* td)
{
    int64_t pc_obj;

    pc_obj = player_get_local_pc_obj();

    if ((pc_obj == skill_ui_current_obj || !critter_is_unconscious(pc_obj))
        && !critter_is_unconscious(skill_ui_current_obj)) {
        skill_ui_activate(td, skill_ui_current_obj, skill_ui_current_type);
    }
}

// 0x57A320
void skill_ui_activate(TargetDescriptor* td, int64_t obj, int a3)
{
    bool is_pc;
    int skill;
    unsigned int spell_flags;
    MesFileEntry mes_file_entry;
    SkillInvocation skill_invocation;

    if (critter_is_dead(obj)) {
        return;
    }

    is_pc = player_is_local_pc_obj(obj);
    skill = skill_ui_skill_for_action(a3, td);

    switch (skill) {
    case SKILL_PICK_POCKET:
        if (!td->is_loc
            && is_pc
            && obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
            skill_ui_cancel();

            spell_flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);

            if ((obj_field_int32_get(td->obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) == 0) {
                if (skill_ui_is_pickpocket_immune(td->obj)) {
                    if (player_is_local_pc_obj(obj)) {
                        mes_file_entry.num = 525; // "You cannot pickpocket this creature."
                        mes_get_msg(skill_ui_mes_file, &mes_file_entry);
                        tf_add(obj, TF_TYPE_WHITE, mes_file_entry.str);
                    }
                } else if (critter_is_dead(td->obj)) {
                    if (object_script_execute(obj, td->obj, obj, SAP_USE, 0)
                        && (spell_flags & OSF_POLYMORPHED) == 0
                        && !anim_is_busy(td->obj, 0)) {
                        ui_show_inven_loot(obj, td->obj);
                    }
                } else {
                    skill_ui_open_steal_loot(obj, td->obj);
                }
            }
        }
        return;
    case SKILL_REPAIR:
        if (!td->is_loc) {
            skill_invocation_init(&skill_invocation);
            object_ref_init(obj, &(skill_invocation.source));
            object_ref_init(td->obj, &(skill_invocation.target));
            object_ref_init(OBJ_HANDLE_NULL, &(skill_invocation.item));
            skill_invocation.flags = 0;
            skill_invocation.skill = SKILL_REPAIR;
            skill_ui_run_invocation(&skill_invocation);
        }
        return;
    case SKILL_DISARM_TRAPS:
        // CE (real-time session C): the shared cost - the original's 4.
        if (!td->is_loc && combat_consume_action_points(obj, combat_skill_cost(obj, SKILL_DISARM_TRAPS))) {
            skill_invocation_init(&skill_invocation);
            object_ref_init(obj, &(skill_invocation.source));
            object_ref_init(td->obj, &(skill_invocation.target));
            object_ref_init(OBJ_HANDLE_NULL, &(skill_invocation.item));
            skill_invocation.flags = SKILL_INVOCATION_0x04;
            skill_invocation.skill = SKILL_DISARM_TRAPS;
            skill_ui_delegate_to_follower(&skill_invocation);
            skill_ui_run_invocation(&skill_invocation);
        }
        return;
    default:
        if (!td->is_loc) {
            skill_use(obj, skill, td->obj, 0);
        }
        return;
    }
}

// Pickpocketing is refused for mechanical critters and for critters flagged
// OCF2_NO_PICKPOCKET.
//
// 0x57A5E0
bool skill_ui_is_pickpocket_immune(int64_t obj)
{
    return (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NO_PICKPOCKET) != 0
        || (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_MECHANICAL) != 0;
}

// Unless CTRL is held, let the best-qualified follower attempt the skill and
// float the "let me handle it" line when ownership changes.
//
// 0x57A620
void skill_ui_delegate_to_follower(SkillInvocation* skill_invocation)
{
    int64_t obj;
    char str[1000];

    if (!tig_kb_get_modifier(SDL_KMOD_CTRL)) {
        obj = skill_invocation->source.obj;
        skill_pick_best_follower(skill_invocation);
        if (obj != skill_invocation->source.obj) {
            dialog_copy_npc_let_me_handle_msg(skill_invocation->source.obj, obj, str);
            dialog_ui_float_line(skill_invocation->source.obj, obj, str, 0);
        }
    }
}

// 0x57A6A0
int skill_ui_skill(int index)
{
    return skill_ui_skills[index];
}

// 0x57A6B0
void skill_ui_run_invocation(SkillInvocation* skill_invocation)
{
    skill_invocation_run(skill_invocation);
}

// 0x57A6C0
int skill_ui_skill_for_action(int a1, TargetDescriptor* td)
{
    int v1;
    int complexity;

    v1 = skill_ui_skills[a1];
    if (v1 == -1) {
        complexity = obj_field_int32_get(td->obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
        // FIX: the original returns SKILL_REPAIR for complexity *below* 20,
        // not at or above it. `cmp $0x14,%eax` then `mov $0xc,%eax` (which
        // does not touch the flags) then `jl 0x57A6F8` at 0x57A6EC-0x57A6F4:
        // the jump SKIPS the `mov %edi,%eax` that restores `v1`, so taking it
        // - complexity < 20 - is what returns 12. CE had the comparison the
        // other way round.
        //
        // NOT a numbered porting bug: the branch is dead as shipped. It runs
        // only when `skill_ui_skills[a1]` is -1, and the original's table at
        // 0x5CB240 holds {6, 5, 15, 12} - byte-identical to CE's - with the
        // only two references to it in the whole image being the reads here
        // and in `skill_ui_skill_get` (0x57A6A0). Corrected anyway, on the
        // same reasoning as the blit stepping: unreachable as shipped,
        // correct for whoever changes the table next, which in a modding
        // toolkit is a live prospect.
        if (a1 == 3 && complexity < 20) {
            v1 = SKILL_REPAIR;
        }
    }

    return v1;
}

// 0x57A700
char* skill_ui_name(int index)
{
    return skill_ui_names[index];
}

// Opens the target inventory in steal mode for critters and loot mode for
// containers/corpses.
//
// 0x57A710
bool skill_ui_open_steal_loot(int64_t pc_obj, int64_t target_obj)
{
    int mode;

    if (pc_obj != target_obj) {
        mode = obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))
            ? INVEN_UI_MODE_STEAL
            : INVEN_UI_MODE_LOOT;
        inven_ui_open(pc_obj, target_obj, mode);
    }

    return true;
}

// Generic success/fail feedback for SkillCallbacks.field_0. The original also
// never reads field_0 after skill_ui_init assigns it (xrefs to 0x6876A0 have
// only skill_set_callbacks/skill_init writes); kept as the named callback slot.
//
// 0x57A770
bool skill_ui_success_fail_msg_field_0(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57A7F0
bool skill_ui_steal_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;
    (void)item_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;

    if (player_is_local_pc_obj(source_obj)) {
        intgame_message_window_display_msg(&ui_message);
    }

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// NOTE: Exactly the same implementation as above.
//
// 0x57A8C0
bool skill_ui_plant_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;
    (void)item_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;

    if (player_is_local_pc_obj(source_obj)) {
        intgame_message_window_display_msg(&ui_message);
    }

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// Byte-identical twin of skill_ui_success_fail_msg_field_0, wired to
// SkillCallbacks.field_C and invoked by the non-trap-handling disarm path in
// skill_invocation_run.
//
// 0x57A990
bool skill_ui_success_fail_msg_field_C(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57AA10
bool skill_ui_disarm_trap(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 560; // "Trap successfully disarmed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 561; // "Trap disarm failed."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57AA90
bool skill_ui_repair(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57AB10
bool skill_ui_no_repair(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    (void)target_obj;
    (void)success;

    mes_file_entry.num = 505; // "That is not in need of repair."
    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, TF_TYPE_RED, mes_file_entry.str);

    return true;
}

// 0x57AB70
bool skill_ui_lock(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 550; // "Lock successfully picked."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 551; // "Lock picking failed."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57ABF0
bool skill_ui_no_lock(int64_t source_obj)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mes_file_entry.num = 512; // "That has no lock."
    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, TF_TYPE_RED, mes_file_entry.str);

    return true;
}

// 0x57AC50
void skill_ui_adjust_skill(int64_t obj, int skill, int action)
{
    switch (action) {
    case 1:
        skill_ui_inc_skill(obj, skill);
        break;
    case -1:
        skill_ui_dec_skill(obj, skill);
        break;
    }
}

// 0x57AC90
void skill_ui_set_training(int64_t obj, int skill, int training)
{
    // FIX (porting bug #99): tech skills to the tech setter. The original
    // sends `skill > 12` to tech_skill_training_set(skill - 12) and the rest
    // to basic_skill_training_set (0x57AC98 `cmp $0xc`/`jle`); CE had the
    // setters swapped. The original's own test leaves 12 (Repair) to the
    // basic setter, which refuses it - kept, since only multiplayer calls
    // this.
    if (skill > 12) {
        tech_skill_training_set(obj, skill - 12, training);
    } else {
        basic_skill_training_set(obj, skill, training);
    }
}

// 0x57ACD0
void skill_ui_inc_skill(int64_t obj, int skill)
{
    bool is_pc;
    int base;
    int level;
    int cost;
    int points;

    // UAP (0x57ACE1): "You can now disable your party members auto-level
    // scheme and distribute their character points yourself if you so
    // desire." The patch redirects this call to 0x40DA0E - the
    // `mov eax, 1; ret` tail inside `player_is_local_pc_obj` - so the test is
    // unconditionally true. `is_pc` gates only the character editor's
    // feedback (error floaters, `charedit_refresh`, the message-window
    // clear), so without it a follower's +/- buttons apply the change but
    // never redraw it.
    is_pc = uap_fixes_enabled() || player_is_local_pc_obj(obj);
    if (IS_TECH_SKILL(skill)) {
        skill = GET_TECH_SKILL(skill);
        base = tech_skill_points_get(obj, skill);
        level = tech_skill_level(obj, skill);
        cost = tech_skill_cost_inc(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);
        if (cost > points) {
            if (is_pc && charedit_is_created()) {
                charedit_error_not_enough_character_points();
            }
            return;
        }

        if (tech_skill_points_set(obj, skill, base + cost) != base + cost) {
            if (level == 20) {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_at_max();
                }
            } else {
                if (is_pc && charedit_is_created()) {
                    charedit_error_not_enough_stat(tech_skill_stat(skill));
                }
            }
            return;
        }

        stat_base_set(obj, STAT_UNSPENT_POINTS, points - cost);
        if (is_pc && charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    } else {
        skill = GET_BASIC_SKILL(skill);
        base = basic_skill_points_get(obj, skill);
        level = basic_skill_level(obj, skill);
        cost = basic_skill_cost_inc(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);
        if (cost > points) {
            if (is_pc && charedit_is_created()) {
                charedit_error_not_enough_character_points();
            }
            return;
        }

        if (basic_skill_points_set(obj, skill, base + cost) != base + cost) {
            if (level == 20) {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_at_max();
                }
            } else {
                if (is_pc && charedit_is_created()) {
                    charedit_error_not_enough_stat(basic_skill_stat(skill));
                }
            }
            return;
        }

        stat_base_set(obj, STAT_UNSPENT_POINTS, points - cost);
        if (is_pc && charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    }
}

// 0x57AEB0
void skill_ui_dec_skill(int64_t obj, int skill)
{
    bool is_pc;
    int base;
    int level;
    int cost;
    int points;

    // UAP (0x57AEC1): "You can now disable your party members auto-level
    // scheme and distribute their character points yourself if you so
    // desire." The patch redirects this call to 0x40DA0E - the
    // `mov eax, 1; ret` tail inside `player_is_local_pc_obj` - so the test is
    // unconditionally true. `is_pc` gates only the character editor's
    // feedback (error floaters, `charedit_refresh`, the message-window
    // clear), so without it a follower's +/- buttons apply the change but
    // never redraw it.
    is_pc = uap_fixes_enabled() || player_is_local_pc_obj(obj);
    if (IS_TECH_SKILL(skill)) {
        skill = GET_TECH_SKILL(skill);
        base = tech_skill_points_get(obj, skill);
        // NOTE: Verified against 0x57AFBF-0x57AFC7 - original calls tech_skill_level and discards the result.
        tech_skill_level(obj, skill);
        cost = tech_skill_cost_dec(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);

        if (tech_skill_points_set(obj, skill, base - cost) != base - cost) {
            if (tech_skill_points_get(obj, skill) != 0) {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_at_min();
                }
            } else {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_is_zero();
                }
            }
            return;
        }

        stat_base_set(obj, STAT_UNSPENT_POINTS, points + cost);
        if (is_pc && charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    } else {
        skill = GET_BASIC_SKILL(skill);
        base = basic_skill_points_get(obj, skill);
        level = basic_skill_level(obj, skill);
        cost = basic_skill_cost_dec(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);

        // NOTE: This code is different from tech skills code path above.
        while (level == basic_skill_level(obj, skill)) {
            if (basic_skill_points_set(obj, skill, base - cost) != base - cost) {
                if (basic_skill_points_get(obj, skill) != 0) {
                    // NOTE: Verified against 0x57AF98-0x57AFA8 - original calls charedit_error_skill_at_min/zero unconditionally on the basic-skill failure path; transcription faithful.
                    charedit_error_skill_at_min();
                } else {
                    // NOTE: Same original path at 0x57AF9A/0x57B069 and 0x57AFA0; the is_pc/charedit checks are intentionally absent.
                    charedit_error_skill_is_zero();
                }
                return;
            }

            points = stat_base_set(obj, STAT_UNSPENT_POINTS, points + cost);
            if (is_pc && charedit_is_created()) {
                charedit_refresh();
                intgame_message_window_clear();
            }
        }
    }
}
