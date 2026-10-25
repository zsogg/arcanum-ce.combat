#include "ui/item_ui.h"

#include "game/anim.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/item.h"
#include "game/magictech.h"
#include "game/mt_item.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/skill.h"
#include "game/target.h"
#include "game/trap.h"
#include "game/uap.h"
#include "ui/dialog_ui.h"
#include "ui/intgame.h"
#include "ui/inven_ui.h"
#include "ui/spell_ui.h"

static void item_ui_target_flags_get(int64_t item_obj, uint64_t** tgt_ptr);

// 0x5CAC40
static uint64_t item_ui_default_tgt = TGT_OBJECT;

// Unreferenced, in the original too: no instruction names 0x5CAC48.
//
// 0x5CAC48
static uint64_t qword_5CAC48 = TGT_OBJECT | TGT_OBJ_INVEN;

// 0x5CAC50
static uint64_t item_ui_trap_device_tgt = TGT_OBJECT | TGT_TILE;

// 0x6810D8
static int64_t item_ui_item_obj;

// 0x5719D0
bool item_ui_init(GameInitInfo* init_info)
{
    (void)init_info;

    return true;
}

// 0x5719E0
void item_ui_exit(void)
{
}

// 0x5719F0
void item_ui_target_flags_get(int64_t item_obj, uint64_t** tgt_ptr)
{
    int mana_store;
    unsigned int flags;
    int spl;

    *tgt_ptr = 0;

    if (item_obj != OBJ_HANDLE_NULL) {
        mana_store = obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_MANA_STORE);
        flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
        spl = mt_item_spell(item_obj, 0);
        if (mana_store != 0 || (flags & OIF_IS_MAGICAL) != 0) {
            magictech_target_flags_get(spl, tgt_ptr);
        } else if (trap_is_trap_device(item_obj)) {
            *tgt_ptr = &item_ui_trap_device_tgt;
        } else if (spl == -1) {
            *tgt_ptr = &item_ui_default_tgt;
        } else {
            magictech_target_flags_get(spl, tgt_ptr);
        }
    }
}

// 0x571AA0
void item_ui_activate(int64_t owner_obj, int64_t item_obj)
{
    uint64_t* tgt_ptr;
    TargetDescriptor td;

    if (item_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // FIX (porting bug #215): an owner that is off, dead or unconscious
    // activates nothing (0x571ACD-0x571AF3, three `jne` to the exit); CE
    // returned only for an owner both off and alive and conscious.
    if ((obj_field_int32_get(owner_obj, OBJ_F_FLAGS) & OF_OFF) != 0
        || critter_is_dead(owner_obj)
        || critter_is_unconscious(owner_obj)) {
        return;
    }

    if (intgame_mode_get() == INTGAME_MODE_ITEM) {
        item_ui_deactivate();
    }

    item_ui_item_obj = item_obj;

    item_ui_target_flags_get(item_obj, &tgt_ptr);

    if ((*tgt_ptr & TGT_OBJ_INVEN) == 0) {
        inven_ui_destroy();

        if (!intgame_mode_set(INTGAME_MODE_MAIN)) {
            return;
        }
    }

    // NOTE: Verified against 0x571C4C-0x571C66: the condition matches the
    // original, and the loc path calls 0x571CB0 (item_ui_apply).
    //
    // FIX (porting bug #19): an earlier transcription called spell_ui_apply
    // here, so self-only items were applied through the spell path.
    if (*tgt_ptr == TGT_NONE
        || ((*tgt_ptr & TGT_SELF) != 0
            && (*tgt_ptr & TGT_TILE) == 0)
        || *tgt_ptr == TGT_OBJ_RADIUS) {
        target_descriptor_set_loc(&td, obj_field_int64_get(owner_obj, OBJ_F_LOCATION));
        item_ui_apply(&td);
        return;
    }

    // CE (real-time session C): the shared cost - the original's 4.
    if (combat_check_action_points(owner_obj, combat_use_item_cost(owner_obj, item_ui_item_obj))) {
        target_flags_set(*tgt_ptr);
        if (intgame_mode_set(INTGAME_MODE_ITEM)) {
            target_flags_set(*tgt_ptr);
            // UAP (0x571BE7): `jge` becomes `jg`, widening the test from
            // "< 0" to "<= 0", so an item of neutral complexity gets the
            // magickal targeting cursor too rather than keeping the default.
            int complexity = obj_field_int32_get(item_ui_item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
            if (uap_fixes_enabled() ? complexity <= 0 : complexity < 0) {
                intgame_item_mode_cursor_set(620);
                intgame_refresh_cursor();
            }

            if ((*tgt_ptr & TGT_OBJECT) != 0
                && tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                target_descriptor_set_obj(&td, player_get_local_pc_obj());
                item_ui_apply(&td);
            }
        }
    }
}

// 0x571C80
void item_ui_deactivate(void)
{
    intgame_mode_set(INTGAME_MODE_MAIN);
    item_ui_item_obj = OBJ_HANDLE_NULL;
    intgame_item_mode_cursor_set(21);
}

// 0x571CB0
void item_ui_apply(TargetDescriptor* td)
{
    int64_t pc_obj;
    SkillInvocation skill_invocation;
    int spell_mana_store = 0;
    unsigned int item_flags = 0;
    int range = -1;
    int64_t item_obj;
    char str[1000];

    pc_obj = player_get_local_pc_obj();

    if (critter_is_dead(pc_obj)) {
        return;
    }

    if (critter_is_unconscious(pc_obj)) {
        return;
    }

    if (obj_field_int32_get(item_ui_item_obj, OBJ_F_TYPE) == OBJ_TYPE_GENERIC
        && !tig_kb_get_modifier(SDL_KMOD_LCTRL)
        && (obj_field_int32_get(item_ui_item_obj, OBJ_F_GENERIC_FLAGS) & OGF_IS_LOCKPICK) != 0) {
        skill_invocation_init(&skill_invocation);
        object_ref_init(pc_obj, &(skill_invocation.source));
        if (!td->is_loc) {
            object_ref_init(td->obj, &(skill_invocation.target));
        }
        skill_invocation.skill = SKILL_PICK_LOCKS;
        skill_invocation.item.obj = item_find_first_generic(skill_invocation.source.obj, OGF_IS_LOCKPICK);
        skill_pick_best_follower(&skill_invocation);

        if (pc_obj == skill_invocation.source.obj) {
            item_obj = item_ui_item_obj;
        } else {
            item_obj = item_find_first_generic(skill_invocation.source.obj, OGF_IS_LOCKPICK);
            dialog_copy_npc_let_me_handle_msg(skill_invocation.source.obj, pc_obj, str);
            dialog_ui_float_line(skill_invocation.source.obj, pc_obj, str, 0);
            // FIX (porting bug #216): from here the follower who said "let me
            // handle it" is the one who uses the lockpick (0x571CB0 sets the
            // actor to the invocation's source after the float line, and every
            // call below takes it); CE kept the player as the actor.
            pc_obj = skill_invocation.source.obj;
            if (item_obj == OBJ_HANDLE_NULL) {
                anim_goal_use_item_on_obj_with_skill(skill_invocation.source.obj,
                    OBJ_HANDLE_NULL,
                    skill_invocation.target.obj,
                    SKILL_PICK_LOCKS,
                    0);
                item_ui_deactivate();
                return;
            }
        }
    } else {
        item_obj = item_ui_item_obj;
    }

    if (item_obj != OBJ_HANDLE_NULL) {
        spell_mana_store = obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_MANA_STORE);
        item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
    }

    if (td->is_loc) {
        if (spell_mana_store != 0 || (item_flags & OIF_IS_MAGICAL) != 0) {
            item_use_on_loc(pc_obj, item_obj, td->loc);
        } else if (trap_is_trap_device(item_obj)) {
            anim_goal_use_item_on_loc(pc_obj, td->loc, item_obj, 0);
        } else if (!combat_throw_refused(pc_obj)) {
            // CE (`exertion`): refused before it starts when it cannot be
            // paid for (`combat_throw_refused`).
            anim_goal_throw_item(pc_obj, item_obj, td->loc);
        }
    } else {
        if (spell_mana_store != 0 || (item_flags & OIF_IS_MAGICAL) != 0) {
            range = magictech_get_range(obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_1));
            if (range < object_dist(pc_obj, td->obj)) {
                range = -1;
            }
        }

        // TODO: Too many conditions merged with previous block, probably wrong.
        if ((spell_mana_store != 0
                && range != -1
                && (item_flags & OIF_NO_RANGED_USE) == 0)
            || ((item_flags & OIF_IS_MAGICAL) == 0
                && range == -1
                && (item_flags & OIF_NO_RANGED_USE) == 0)
            || ((item_flags & OIF_IS_MAGICAL) != 0
                && range != -1
                && (item_flags & OIF_NO_RANGED_USE) == 0)) {
            item_use_on_obj(pc_obj, item_obj, td->obj);
            item_ui_deactivate();
            return;
        }

        anim_goal_use_item_on_obj(pc_obj, td->obj, item_obj, 0);
        if (tig_kb_get_modifier(SDL_KMOD_LSHIFT)) {
            anim_scratch_goal_turn_on_stand_in_place();
            item_ui_deactivate();
            return;
        }

        if (tig_kb_get_modifier(SDL_KMOD_LCTRL)
            || !tig_kb_get_modifier(SDL_KMOD_NUM)
            || (get_always_run(pc_obj) && !tig_kb_get_modifier(SDL_KMOD_LCTRL))) {
            anim_scratch_goal_turn_on_running();
            item_ui_deactivate();
            return;
        }
    }

    item_ui_deactivate();
}
