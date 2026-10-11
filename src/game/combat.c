#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/crit_effects.h"
#include "game/crit_guard.h"
#include "game/rt_budget.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/animfx.h"
#include "game/armor_rules.h"
#include "game/ci.h"
#include "game/criticals.h"
#include "game/critter.h"
#include "game/descriptions.h"
#include "game/effect.h"
#include "game/fate.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/logbook.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/material.h"
#include "game/mes.h"
#include "game/mt_item.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/penetration.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/reload.h"
#include "game/resistance.h"
#include "game/script.h"
#include "game/sfx.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/spell_rules.h"
#include "game/stat.h"
#include "game/tf.h"
#include "game/trap.h"
#include "game/uap.h"
#include "game/ui.h"
#include "game/weapon_rules.h"

static void turn_based_changed(void);
static void fast_turn_based_changed(void);
static void combat_create_projectile(CombatContext* combat, int64_t loc, int a3, int a4, tig_art_id_t proj_aid);
static void combat_projectile_end(int64_t proj_obj, int64_t a2, int64_t a3, CombatContext* combat, bool a5);
static int combat_weapon_loudness(int64_t weapon_obj);
static void combat_process_attack_result(CombatContext* combat);
static void combat_process_melee_attack(CombatContext* combat);
static void combat_process_ranged_attack(CombatContext* combat);
static void combat_critter_toggle_combat_mode(int64_t obj);
static int64_t combat_critter_armor(int64_t critter_obj, int hit_loc);
static void combat_apply_weapon_wear(CombatContext* combat);
static void combat_remove_blood_splotch(int64_t loc);
static void combat_process_crit_hit(CombatContext* combat);
static void combat_process_crit_miss(CombatContext* combat);
static int combat_random_hit_loc(void);
static bool combat_consume_ammo(int64_t weapon_obj, int64_t critter_obj, int cnt, bool destroy);
static void combat_calc_dmg(CombatContext* combat);
static void combat_apply_resistance(CombatContext* combat);
static int combat_armor_resolve(CombatContext* combat);
static int combat_play_hit_fx(CombatContext* combat);
static void combat_process_taunts(CombatContext* combat);
static void combat_turn_based_anims_complete(void);
static bool combat_turn_based_start(void);
static void combat_turn_based_end(void);
static bool combat_turn_based_begin_turn(void);
static void combat_recalc_perception_range(void);
static bool combat_turn_based_critter_skip(ObjectNode* object_node);
static void combat_turn_based_subturn_start(void);
static void combat_turn_based_subturn_end(void);
static void combat_turn_based_end_turn(void);
static int combat_move_cost(int64_t source_obj, int64_t target_loc, bool adjacent);
static bool combat_turn_based_critter_is_dontdraw(int64_t obj);
static void sort_combat_list(void);
static bool combat_critter_list_holds(int64_t obj);
static void combat_order_summons(void);
static void combat_summons_round_began(void);
static void pc_switch_weapon(int64_t pc_obj, int64_t target_obj);

// CE (reload prototype session 2): the weapon that last left the player's
// hand, for `weapon swap cost` - see `combat_weapon_swap_entered`.
static int64_t combat_weapon_swap_last;

// 0x5B5798
static int hit_loc_penalties[HIT_LOC_COUNT] = {
    /* HIT_LOC_TORSO */ 0,
    /*  HIT_LOC_HEAD */ -50,
    /*   HIT_LOC_ARM */ -30,
    /*   HIT_LOC_LEG */ -30,
};

// 0x5B57A8
static int combat_critical_dodge_chance_tbl[TRAINING_COUNT] = {
    0,
    10,
    50,
    100,
};

// 0x5B57B8
static int combat_perception_range = 15;

// 0x5B57BC
static int combat_material_damage_reduction_tbl[MATERIAL_COUNT] = {
    /*  STONE */ 10,
    /*  BRICK */ 10,
    /*   WOOD */ 5,
    /*  PLANT */ 0,
    /*  FLESH */ 0,
    /*  METAL */ 10,
    /*  GLASS */ 0,
    /*  CLOTH */ 0,
    /* LIQUID */ 0,
    /*  PAPER */ 0,
    /*    GAS */ 0,
    /*  FORCE */ 10,
    /*   FIRE */ 0,
    /* POWDER */ 0,
};

// 0x5B57FC
static int combat_multishot_offsets[2] = {
    0,
    10,
};

// 0x5B5804
static int combat_damage_to_resistance_tbl[DAMAGE_TYPE_COUNT] = {
    /*     DAMAGE_TYPE_NORMAL */ RESISTANCE_TYPE_NORMAL,
    /*     DAMAGE_TYPE_POISON */ RESISTANCE_TYPE_POISON,
    /* DAMAGE_TYPE_ELECTRICAL */ RESISTANCE_TYPE_ELECTRICAL,
    /*       DAMAGE_TYPE_FIRE */ RESISTANCE_TYPE_FIRE,
    /*    DAMAGE_TYPE_FATIGUE */ RESISTANCE_TYPE_NORMAL,
};

// 0x5FC178
static mes_file_handle_t combat_mes_file;

// 0x5FC180
static ObjectList combat_critter_list;

// 0x5FC1D8
static bool combat_editor;

// 0x5FC1F8
static AnimFxList combat_eye_candies;

// NOTE: It's `bool`, but needs to be 4 byte integer because of saving/reading
// compatibility.
//
// 0x5FC224
static int combat_turn_based;

// 0x5FC228
static bool combat_fast_turn_based;

// NOTE: It's `bool`, but needs to be 4 byte integer because of saving/reading
// compatibility.
//
// 0x5FC22C
static int combat_turn_based_active;

// 0x5FC230
static int combat_turn_based_turn;

// 0x5FC234
static int combat_action_points;

// 0x5FC238
// NOTE: Savegame ballast, the same class documented in anim.c. `combat_save`
// writes it and `combat_load` reads it back, and nothing else in the file
// touches it - so it is only ever the OBJ_HANDLE_NULL it starts as, written
// out and read in again. Left unnamed: there is no use to infer a meaning
// from, and the save format needs the slot regardless.
static int64_t qword_5FC238;

// 0x5FC240
static ObjectNode* combat_whos_turn_node;

// 0x5FC244
static int combat_required_action_points;

// 0x5FC248
static int64_t combat_whos_turn_obj;

// 0x5FC250
static int combat_turn_index;

// 0x5FC258
static int64_t combat_last_turn_obj;

// 0x5FC260
static int combat_last_action_points;

// 0x5FC264
// The turn-based stall detector, and only meaningful as a trio with
// `combat_last_turn_obj` and `combat_last_action_points`. `combat_turn_based_anims_complete`
// remembers whose turn it saw last and how many action points they had; if
// both are unchanged since the previous pass the turn is making no progress,
// so it sets this flag. `combat_tb_timeevent_process` then clears it and burns
// the remaining points to force the turn to end. Resetting one of the three
// without the others either misses a stall or ends a live turn early.
static bool combat_turn_stalled;

// 0x5FC268
static bool in_combat_reset;

// 0x5FC26C
static bool combat_in_taking_damage_script;

// 0x5FC270
static int64_t combat_debug_obj;

// 0x5FC1E0
static CombatCallbacks combat_callbacks;

// 0x4B1D50
bool combat_init(GameInitInfo* init_info)
{
    combat_editor = init_info->editor;

    if (!mes_load("mes\\combat.mes", &combat_mes_file)) {
        return false;
    }

    if (!combat_editor) {
        if (!animfx_list_init(&combat_eye_candies)) {
            return false;
        }

        combat_eye_candies.path = "Rules\\CombatEyeCandy.mes";
        combat_eye_candies.capacity = 1;
        if (!animfx_list_load(&combat_eye_candies)) {
            return false;
        }
    }

    settings_register(&settings, TURN_BASED_KEY, "0", turn_based_changed);
    combat_turn_based = settings_get_value(&settings, TURN_BASED_KEY);

    settings_register(&settings, FAST_TURN_BASED_KEY, "0", fast_turn_based_changed);
    combat_fast_turn_based = settings_get_value(&settings, FAST_TURN_BASED_KEY);

    settings_register(&settings, AUTO_ATTACK_KEY, "0", NULL);

    // CE (real-time session B): the action budget's settings, and an empty table.
    rt_budget_init();
    // CE (turn-based rebalance): the listed weapons' rules, and the armor
    // table (armor milestone 2).
    weapon_rules_init();
    spell_rules_init();
    armor_rules_init();
    settings_register(&settings, COMBAT_TAUNTS_KEY, "0", NULL);

    return true;
}

// 0x4B1E50
void combat_exit(void)
{
    mes_unload(combat_mes_file);
    weapon_rules_exit();
    spell_rules_exit();
    armor_rules_exit();
    combat_turn_based_end();
    if (!combat_editor) {
        animfx_list_exit(&combat_eye_candies);
    }
}

// 0x4B1E80
void combat_reset(void)
{
    // CE (reload prototype session 2): a new game or a load has no last weapon.
    combat_weapon_swap_last = OBJ_HANDLE_NULL;

    // CE (real-time session B): recovery is not saved (rule 3) - a new game or a
    // load starts every actor ready.
    rt_budget_reset();
    // CE (criticals, milestone CR-1): protection after critical
    // incapacitation is not saved either.
    crit_guard_reset();
    // CE (criticals, milestone CR-2): nor are temporary critical effects.
    crit_effects_reset();

    in_combat_reset = true;
    combat_turn_based_end();
    in_combat_reset = false;
}

// NOTE: `oid` is deliberately left uninitialized, and the null branch below
// sets only its `type` before all 24 bytes are written. That leaks
// uninitialized stack into every turn-based save - and it is faithful. The
// original does the same at 0x4B1F58:
//
//     4b1f29: mov  eax,ds:0x5fc238          ; qword_5FC238
//     4b1f38: je   0x4b1f58                 ; ... is null
//     4b1f58: mov  WORD PTR [esp+0xc],0x0   ; type = OID_TYPE_NULL, and only
//     4b1f66: push 0x18                     ; ... yet 24 bytes are written
//
// Consequence for the original-vs-CE differential: a save taken while
// turn-based combat is active can never compare byte-identical in the Combat
// module, in either direction, because both engines write whatever their
// stack happened to hold. This is the explanation for the "Combat 17B"
// difference recorded against `Slot0003 turnbased-mid` in SAVE-MATRIX.md,
// which was previously filed as live-combat state diverging.
//
// Not fixed: zeroing it would be a behaviour change to the save format for
// no player-visible gain, and CE's rule is to mirror 1.0.7.4 unless a
// divergence is justified. `combat_load` reads the same 24 bytes and uses
// only `type`, so the padding is never interpreted.
//
// 0x4B1EA0
bool combat_save(TigFile* stream)
{
    ObjectID oid;

    if (stream == NULL) return false;
    if (tig_file_fwrite(&combat_turn_based, sizeof(combat_turn_based), 1, stream) != 1) return false;
    if (tig_file_fwrite(&combat_turn_based_active, sizeof(combat_turn_based_active), 1, stream) != 1) return false;
    if (tig_file_fwrite(&combat_turn_based_turn, sizeof(combat_turn_based_turn), 1, stream) != 1) return false;

    if (!combat_turn_based_active) {
        return true;
    }

    if (tig_file_fwrite(&combat_action_points, sizeof(combat_action_points), 1, stream) != 1) return false;

    if (qword_5FC238 == OBJ_HANDLE_NULL) {
        oid.type = OID_TYPE_NULL;
    } else {
        oid = obj_get_id(qword_5FC238);
    }
    if (tig_file_fwrite(&oid, sizeof(oid), 1, stream) != 1) return false;

    oid = obj_get_id(combat_whos_turn_node->obj);
    if (tig_file_fwrite(&oid, sizeof(oid), 1, stream) != 1) return false;

    if (tig_file_fwrite(&combat_required_action_points, sizeof(combat_required_action_points), 1, stream) != 1) return false;

    oid = obj_get_id(combat_whos_turn_obj);
    if (tig_file_fwrite(&oid, sizeof(oid), 1, stream) != 1) return false;

    return true;
}

// 0x4B2020
bool combat_load(GameLoadInfo* load_info)
{
    int saved_action_points;
    ObjectID oid;
    int64_t obj;
    ObjectNode* node;

    // CE (real-time session B): rule 3, as in combat_reset.
    rt_budget_reset();
    // CE (criticals, milestone CR-1): as in combat_reset.
    crit_guard_reset();
    // CE (criticals, milestone CR-2): as in combat_reset.
    crit_effects_reset();

    if (load_info->stream == NULL) return false;
    if (tig_file_fread(&combat_turn_based, sizeof(combat_turn_based), 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&combat_turn_based_active, sizeof(combat_turn_based_active), 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&combat_turn_based_turn, sizeof(combat_turn_based_turn), 1, load_info->stream) != 1) return false;

    if (!combat_turn_based_active) {
        return true;
    }

    combat_callbacks.field_4();

    if (tig_file_fread(&combat_action_points, sizeof(combat_action_points), 1, load_info->stream) != 1) return false;
    saved_action_points = combat_action_points;
    combat_turn_based_active = false;

    if (!combat_turn_based_start()) {
        return false;
    }

    combat_action_points = saved_action_points;

    if (tig_file_fread(&oid, sizeof(oid), 1, load_info->stream) != 1) return false;
    qword_5FC238 = obj_pool_perm_lookup(oid);

    if (tig_file_fread(&oid, sizeof(oid), 1, load_info->stream) != 1) return false;
    obj = obj_pool_perm_lookup(oid);
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    node = combat_critter_list.head;
    while (node != NULL) {
        if (node->obj == obj) {
            break;
        }
        node = node->next;
    }

    if (node == NULL) {
        return false;
    }

    combat_whos_turn_node = node;

    if (tig_file_fread(&combat_required_action_points, sizeof(combat_required_action_points), 1, load_info->stream) != 1) return false;

    combat_required_action_points = 0;

    if (tig_file_fread(&oid, sizeof(oid), 1, load_info->stream) != 1) return false;
    combat_whos_turn_obj = obj_pool_perm_lookup(oid);
    if (combat_whos_turn_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    combat_callbacks.field_C(combat_action_points);

    return true;
}

/**
 * Initializes `combat` for an attack by `attacker_obj` on `target_obj`:
 * resolves the attacker's wielded weapon (or uses the attacker itself as the
 * weapon for traps and other non-critters) and its skill, zeroes all damage,
 * and pre-sets the backstab and CF_RANGED flags. Either object may be
 * OBJ_HANDLE_NULL - environmental damage passes a null attacker. Callers fill
 * in damage amounts afterwards and hand the context to combat_dmg or
 * combat_heal.
 *
 * 0x4B2210
 */
void combat_context_init(int64_t attacker_obj, int64_t target_obj, CombatContext* combat)
{
    int type;

    combat->flags = 0;
    combat->attacker_obj = attacker_obj;

    if (attacker_obj != OBJ_HANDLE_NULL) {
        type = obj_field_int32_get(attacker_obj, OBJ_F_TYPE);
        if (obj_type_is_critter(type)) {
            combat->weapon_obj = combat_critter_weapon(attacker_obj);
        } else {
            combat->weapon_obj = attacker_obj;
        }
    } else {
        combat->weapon_obj = OBJ_HANDLE_NULL;
    }

    combat->skill = item_weapon_skill(combat->weapon_obj);

    if (attacker_obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(type)
        && target_obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))
        && combat->skill == BASIC_SKILL_MELEE
        && critter_can_backstab(attacker_obj, target_obj)) {
        combat->flags |= 0x4000;
        if (ai_can_see(target_obj, attacker_obj) != 0
            && ai_can_hear(target_obj, attacker_obj, LOUDNESS_SILENT) != 0) {
            combat->flags |= 0x8000;
        }
    }

    combat->target_obj = target_obj;
    combat->field_28 = target_obj;

    if (target_obj != OBJ_HANDLE_NULL) {
        combat->target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    }

    combat->field_30 = attacker_obj;
    combat->hit_loc = 0;
    combat->dam_flags = 0;
    combat->total_dam = 0;
    combat->dam[DAMAGE_TYPE_NORMAL] = 0;
    combat->dam[DAMAGE_TYPE_POISON] = 0;
    combat->dam[DAMAGE_TYPE_ELECTRICAL] = 0;
    combat->dam[DAMAGE_TYPE_FIRE] = 0;
    combat->dam[DAMAGE_TYPE_FATIGUE] = 0;

    if (combat->skill == BASIC_SKILL_THROWING
        || item_weapon_range(combat->weapon_obj, combat->attacker_obj) > 1) {
        combat->flags |= CF_RANGED;
    }
}

// 0x4B23B0
int64_t combat_critter_weapon(int64_t critter_obj)
{
    return item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
}

// 0x4B23D0
void turn_based_changed(void)
{
    int value;

    value = settings_get_value(&settings, TURN_BASED_KEY);

    combat_turn_based_set(value);
}

// 0x4B2410
void fast_turn_based_changed(void)
{
    combat_fast_turn_based = settings_get_value(&settings, FAST_TURN_BASED_KEY);
}

// 0x4B24F0
void combat_create_projectile(CombatContext* combat, int64_t loc, int a3, int a4, tig_art_id_t proj_aid)
{
    int64_t proj_obj;
    int hit_loc;
    unsigned int weapon_flags;
    unsigned int critter_flags2;

    if (!object_create(proto_get_by_description(BP_PROJECTILE), loc, &proj_obj)) {
        return;
    }

    obj_field_int32_set(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT_DAMAGE, combat->dam_flags);

    if ((combat->flags & 0x100) != 0) {
        hit_loc = combat->dam[DAMAGE_TYPE_NORMAL];
    } else {
        hit_loc = combat->hit_loc;
    }
    obj_field_int32_set(proj_obj, OBJ_F_PROJECTILE_HIT_LOC, hit_loc);

    obj_field_handle_set(proj_obj, OBJ_F_PROJECTILE_PARENT_WEAPON, combat->weapon_obj);

    if (combat->weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
        weapon_flags = obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_FLAGS);
        if ((weapon_flags & OWF_TRANS_PROJECTILE) != 0) {
            obj_field_int32_set(proj_obj, OBJ_F_BLIT_FLAGS, TIG_ART_BLT_BLEND_ADD);
        }
        if ((weapon_flags & OWF_BOOMERANGS) != 0) {
            critter_flags2 = obj_field_int32_get(combat->attacker_obj, OBJ_F_CRITTER_FLAGS2);
            critter_flags2 |= OCF2_USING_BOOMERANG;
            obj_field_int32_set(combat->attacker_obj, OBJ_F_CRITTER_FLAGS2, critter_flags2);

            combat->flags |= 0x1000;
        }
    }

    obj_field_int32_set(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT, combat->flags);

    anim_goal_projectile(combat->attacker_obj,
        proj_obj,
        proj_aid,
        a3,
        a4,
        combat->target_obj,
        combat->target_loc,
        combat->weapon_obj);
}

// 0x4B2650
void combat_projectile_goal_end(int64_t a1, int64_t a2, CombatContext* combat)
{
    combat_projectile_end(a1, a2, combat != NULL ? combat->field_28 : OBJ_HANDLE_NULL, combat, false);
}

// 0x4B2690
void combat_projectile_end(int64_t proj_obj, int64_t a2, int64_t a3, CombatContext* combat, bool a5)
{
    unsigned int proj_flags;
    int64_t weapon_obj;
    int64_t loc;
    unsigned int critter_flags2;

    proj_flags = obj_field_int32_get(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT);
    if ((proj_flags & 0x40) != 0) {
        weapon_obj = obj_field_handle_get(proj_obj, OBJ_F_PROJECTILE_PARENT_WEAPON);
        loc = obj_field_int64_get(proj_obj, OBJ_F_LOCATION);
        object_move_to(weapon_obj, loc, 0, 0);
        object_flags_unset(weapon_obj, OF_OFF);
        object_destroy(proj_obj);
    } else if ((proj_flags & 0x1000) != 0) {
        if (a5 && (proj_flags & 0x2000) == 0) {
            proj_flags |= 0x2000;
            obj_field_int32_set(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT, proj_flags);
            if (!anim_goal_projectile_retarget(proj_obj, obj_field_int64_get(a2, OBJ_F_LOCATION), a3)) {
                combat_projectile_end(proj_obj, a2, a3, combat, true);
                return;
            }
        } else {
            critter_flags2 = obj_field_int32_get(a2, OBJ_F_CRITTER_FLAGS2);
            critter_flags2 &= ~OCF2_USING_BOOMERANG;
            obj_field_int32_set(a2, OBJ_F_CRITTER_FLAGS2, critter_flags2);

            object_destroy(proj_obj);
            ai_start_attacking(a2, a3);
        }
    } else {
        object_destroy(proj_obj);
    }

    if (combat != NULL
        && combat->target_obj != OBJ_HANDLE_NULL) {
        int loudness = combat_weapon_loudness(combat->weapon_obj);
        if ((combat->flags & 0x800) != 0) {
            ai_attack(combat->attacker_obj, combat->target_obj, loudness, 0x01);
        } else {
            ai_attack(combat->attacker_obj, combat->target_obj, loudness, 0);
        }
    }
}

// 0x4B2810
int combat_weapon_loudness(int64_t weapon_obj)
{
    if (weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
        unsigned int weapon_flags = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_FLAGS);
        if ((weapon_flags & OWF_LOUD) != 0) {
            return LOUDNESS_LOUD;
        }
        if ((weapon_flags & OWF_SILENT) != 0) {
            return LOUDNESS_SILENT;
        }
    }

    return LOUDNESS_NORMAL;
}

// CE (card RT1): one `projectile.ended` per way a flight can end. Emitted
// before the ending is carried out, so the attack a strike causes follows it in
// the stream rather than preceding it.
void combat_projectile_observe(int64_t attacker_obj, int64_t target_obj,
    int64_t struck_obj, const char* outcome)
{
    char source[COMBAT_OBSERVER_IDENTITY_SIZE];
    char target[COMBAT_OBSERVER_IDENTITY_SIZE];
    char struck[COMBAT_OBSERVER_IDENTITY_SIZE];

    if (!combat_observer_is_enabled()) {
        return;
    }

    combat_observer_object_id(attacker_obj, source);
    combat_observer_object_id(target_obj, target);
    combat_observer_object_id(struck_obj, struck);
    combat_observer_projectile_ended(source, target, struck, outcome);
}

// 0x4B2870
bool combat_projectile_arrive(int64_t attacker_obj, int64_t target_obj, int64_t target_loc, int64_t proj_obj, int range, int64_t cur_loc, int64_t a7)
{
    unsigned int proj_flags;
    unsigned int proj_dam_flags;
    int64_t weapon_obj;
    int64_t proj_loc;
    int hit_loc;
    int weapon_obj_type;
    int64_t attacker_loc;
    int64_t attacker_to_target_dist;
    int64_t proj_to_target_dist;
    int scale;
    int64_t block_obj = OBJ_HANDLE_NULL;
    int block_obj_type;
    int dam = 0;

    proj_flags = obj_field_int32_get(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT);
    proj_dam_flags = obj_field_int32_get(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT_DAMAGE);
    weapon_obj = obj_field_handle_get(proj_obj, OBJ_F_PROJECTILE_PARENT_WEAPON);
    proj_loc = obj_field_int64_get(proj_obj, OBJ_F_LOCATION);
    hit_loc = obj_field_int32_get(proj_obj, OBJ_F_PROJECTILE_HIT_LOC);

    if (weapon_obj != OBJ_HANDLE_NULL) {
        weapon_obj_type = obj_field_int32_get(weapon_obj, OBJ_F_TYPE);
    } else {
        weapon_obj_type = -1;
    }

    if (weapon_obj_type != 5) {
        attacker_loc = obj_field_int64_get(attacker_obj, OBJ_F_LOCATION);
        attacker_to_target_dist = location_dist(attacker_loc, target_loc);
        proj_to_target_dist = location_dist(proj_loc, target_loc);
        if (attacker_to_target_dist > 0
            && proj_to_target_dist > 0
            && proj_to_target_dist < attacker_to_target_dist) {
            if (attacker_to_target_dist - proj_to_target_dist * 2 > 0
                && attacker_to_target_dist - proj_to_target_dist != 0) {
                scale = (int)(200 - 150 * (attacker_to_target_dist - proj_to_target_dist) / attacker_to_target_dist);
            } else {
                scale = (int)(200 + 150 * (attacker_to_target_dist - proj_to_target_dist) / attacker_to_target_dist);
            }
        } else {
            scale = 50;
        }

        tig_debug_printf("Scaling to %d\n", scale);
        object_set_blit_scale(proj_obj, scale);
    }

    if (proj_loc == target_loc) {
        if (target_obj == OBJ_HANDLE_NULL
            || (proj_flags & 0x2000) != 0) {
            combat_projectile_observe(attacker_obj, target_obj, OBJ_HANDLE_NULL,
                (proj_flags & 0x2000) != 0 ? "returned" : "tile");
            combat_projectile_end(proj_obj, attacker_obj, a7, NULL, true);

            if (weapon_obj != OBJ_HANDLE_NULL) {
                object_script_execute(attacker_obj, weapon_obj, target_obj, SAP_HIT, 0);

                if ((proj_flags & 0x04) != 0
                    && weapon_obj_type == OBJ_TYPE_WEAPON) {
                    object_script_execute(attacker_obj, weapon_obj, target_obj, SAP_CRITICAL_HIT, 0);
                }

                mt_item_notify_parent_attacks_loc(attacker_obj, weapon_obj, target_loc);
            }

            return false;
        }

        unsigned int new_proj_flags = proj_flags;
        new_proj_flags &= ~0x02;
        new_proj_flags |= 0x800 | 0x20;
        obj_field_int32_set(proj_obj, OBJ_F_PROJECTILE_FLAGS_COMBAT, new_proj_flags);
    }

    if ((proj_flags & 0x2000) != 0) {
        return false;
    }

    if ((proj_flags & 0x02) != 0) {
        if (target_obj != OBJ_HANDLE_NULL
            && proj_loc == obj_field_int64_get(target_obj, OBJ_F_LOCATION)) {
            block_obj = target_obj;

            if ((proj_flags & 0x100) != 0) {
                dam = hit_loc;
                hit_loc = HIT_LOC_TORSO;
            }
        }
    } else {
        if ((proj_flags & 0x20) != 0) {
            object_calc_traversal_cost(proj_obj,
                proj_obj,
                location_rot(proj_loc, cur_loc),
                OBJ_TRAVERSAL_PORTAL_BLOCKS | OBJ_TRAVERSAL_IGNORE_CRITTERS | OBJ_TRAVERSAL_PROJECTILE,
                &block_obj,
                &block_obj_type,
                NULL);
        }

        if (block_obj == OBJ_HANDLE_NULL) {
            ObjectList objects;
            ObjectNode* node;

            object_list_location(proj_loc, OBJ_TM_CRITTER, &objects);
            node = objects.head;
            while (node != NULL) {
                if (!critter_is_dead(node->obj)
                    && node->obj != target_obj
                    && node->obj != attacker_obj
                    && random_between(1, 3) == 1) {
                    block_obj = node->obj;
                    break;
                }
                node = node->next;
            }
            object_list_destroy(&objects);
        }

        if (block_obj != OBJ_HANDLE_NULL) {
            proj_dam_flags = 0;

            proj_flags &= ~0x04;
            proj_flags |= 0x800 | 0x02;

            dam = (proj_flags & 0x100) != 0 ? hit_loc : 0;
            hit_loc = HIT_LOC_TORSO;
        }
    }

    if (block_obj != OBJ_HANDLE_NULL) {
        CombatContext combat;

        combat_projectile_observe(attacker_obj, target_obj, block_obj,
            "struck");
        combat_context_init(attacker_obj, block_obj, &combat);

        if (target_obj != OBJ_HANDLE_NULL) {
            combat.field_28 = target_obj;
        }

        combat.flags = proj_flags | 0x40000 | CF_RANGED;
        combat.dam_flags = proj_dam_flags;
        combat.dam[DAMAGE_TYPE_NORMAL] = dam;
        combat.hit_loc = hit_loc;
        combat.weapon_obj = weapon_obj;
        combat.skill = item_weapon_skill(weapon_obj);
        combat.flags &= ~0xC000;
        combat_projectile_end(proj_obj, attacker_obj, target_obj, &combat, 1);
        combat_process_attack_result(&combat);
        combat_process_taunts(&combat);
        mt_item_notify_parent_attacks_loc(attacker_obj,
            weapon_obj,
            obj_field_int64_get(block_obj, OBJ_F_LOCATION));
        return combat_play_hit_fx(&combat);
    }

    int weapon_range;
    if ((proj_flags & 0x100) != 0) {
        weapon_range = 20;
    } else if ((proj_flags & 0x40) != 0) {
        weapon_range = item_throwing_distance(weapon_obj, attacker_obj);
    } else {
        weapon_range = item_weapon_range(weapon_obj, attacker_obj);
    }

    if (range > weapon_range) {
        if (proj_obj != OBJ_HANDLE_NULL) {
            proj_loc = obj_field_int64_get(proj_obj, OBJ_F_LOCATION);
        }

        combat_projectile_observe(attacker_obj, target_obj, OBJ_HANDLE_NULL,
            "out_of_range");
        combat_projectile_end(proj_obj, attacker_obj, target_obj, NULL, true);

        if (weapon_obj != OBJ_HANDLE_NULL) {
            object_script_execute(attacker_obj, weapon_obj, target_obj, SAP_MISS, 0);

            if ((proj_flags & 0x04) != 0
                && weapon_obj_type == OBJ_TYPE_WEAPON) {
                object_script_execute(attacker_obj, weapon_obj, target_obj, SAP_CRITICAL_MISS, 0);
            }

            mt_item_notify_parent_attacks_loc(attacker_obj, weapon_obj, proj_loc);
        }
    }

    return false;
}

// 0x4B2F60
void combat_process_attack_result(CombatContext* combat)
{
    int sound_id;

    if ((combat->flags & CF_HIT) != 0) {
        if (combat->weapon_obj != OBJ_HANDLE_NULL) {
            object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_HIT, 0);

            if ((combat->flags & CF_CRITICAL) != 0
                && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
                object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_CRITICAL_HIT, 0);
            }
        }

        if (combat->attacker_obj != OBJ_HANDLE_NULL
            && combat->target_obj != OBJ_HANDLE_NULL) {
            object_script_execute(combat->target_obj, combat->attacker_obj, OBJ_HANDLE_NULL, SAP_CRITTER_HITS, 0);
        }

        combat_calc_dmg(combat);
        combat_dmg(combat);

        if ((combat->flags & CF_CRITICAL) != 0) {
            sound_id = sfx_critter_sound(combat->target_obj, CRITTER_SOUND_CRITICALLY_HIT);
            gsound_play_sfx_on_obj(sound_id, 1, combat->target_obj);

            sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_CRITICAL_HIT);
            gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);
        } else {
            sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_HIT);
            gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);
        }
    } else {
        sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_MISS);
        gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);

        if (combat->target_obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(combat->target_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
            ai_process(combat->target_obj);
        }

        if (combat->weapon_obj != OBJ_HANDLE_NULL) {
            object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_MISS, 0);

            if ((combat->flags & CF_CRITICAL) != 0
                && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
                object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_CRITICAL_MISS, 0);
            }
        }
    }
}

void combat_observer_snapshot_obj(int64_t obj)
{
    char actor[COMBAT_OBSERVER_IDENTITY_SIZE];
    int64_t weapon_obj;
    int64_t armor_obj;
    int weapon_skill;
    int weapon_skill_level;
    int weapon_training;

    if (!combat_observer_is_enabled()
        || obj == OBJ_HANDLE_NULL
        || !obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return;
    }

    combat_observer_object_id(obj, actor);
    weapon_obj = item_wield_get(obj, ITEM_INV_LOC_WEAPON);
    armor_obj = item_wield_get(obj, ITEM_INV_LOC_ARMOR);
    weapon_skill = item_weapon_skill(weapon_obj);
    if (IS_TECH_SKILL(weapon_skill)) {
        weapon_skill_level = tech_skill_level(obj, GET_TECH_SKILL(weapon_skill));
        weapon_training = tech_skill_training_get(obj, GET_TECH_SKILL(weapon_skill));
    } else {
        weapon_skill_level = basic_skill_level(obj, GET_BASIC_SKILL(weapon_skill));
        weapon_training = basic_skill_training_get(obj, GET_BASIC_SKILL(weapon_skill));
    }
    combat_observer_actor_snapshot(actor,
        stat_level_get(obj, STAT_LEVEL),
        stat_level_get(obj, STAT_STRENGTH),
        stat_level_get(obj, STAT_DEXTERITY),
        stat_level_get(obj, STAT_SPEED),
        basic_skill_level(obj, BASIC_SKILL_MELEE),
        basic_skill_level(obj, BASIC_SKILL_DODGE),
        object_hp_current(obj), object_hp_max(obj),
        critter_fatigue_current(obj), critter_fatigue_max(obj),
        weapon_obj != OBJ_HANDLE_NULL
            ? obj_field_int32_get(weapon_obj, OBJ_F_DESCRIPTION)
            : 0,
        armor_obj != OBJ_HANDLE_NULL
            ? obj_field_int32_get(armor_obj, OBJ_F_DESCRIPTION)
            : 0,
        weapon_skill, weapon_skill_level, weapon_training,
        item_weapon_magic_speed(weapon_obj, obj));
}

// 0x4B3170
int combat_attack(CombatContext* combat)
{
    bool was_in_bed = false;
    int sound_id;
    int aptitude_crit_failure_chance;
    int attack_result;
    unsigned long long observer_action_id = 0;
    char observer_attacker[COMBAT_OBSERVER_IDENTITY_SIZE] = "NULL";
    char observer_target[COMBAT_OBSERVER_IDENTITY_SIZE] = "NULL";

    if (combat->target_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->target_obj, OBJ_F_TYPE) == OBJ_TYPE_SCENERY) {
        int64_t critter_obj = obj_field_handle_get(combat->target_obj, OBJ_F_SCENERY_WHOS_IN_ME);
        if (critter_obj != OBJ_HANDLE_NULL) {
            critter_leave_bed(critter_obj, combat->target_obj);

            combat->target_obj = critter_obj;
            combat->field_28 = critter_obj;

            was_in_bed = true;
        }
    }

    // Kept because the substitution below is destructive: afterwards
    // `combat->hit_loc` is where the blow landed, not what was asked for, and
    // telling those apart is what the observer could not do.
    int requested_hit_loc = combat->hit_loc;

    if (combat->hit_loc != HIT_LOC_TORSO) {
        combat->flags |= CF_AIM;
    } else {
        combat->hit_loc = combat_random_hit_loc();
    }

    if (combat_observer_is_enabled()) {
        combat_observer_object_id(combat->attacker_obj, observer_attacker);
        combat_observer_object_id(combat->target_obj, observer_target);
        observer_action_id = combat_observer_attack_started(observer_attacker,
            observer_target, combat->skill, combat->hit_loc,
            requested_hit_loc, (combat->flags & CF_AIM) != 0);
        combat_observer_snapshot_obj(combat->attacker_obj);
        combat_observer_snapshot_obj(combat->target_obj);
    }

    // CE (reload prototype session 2): a gun under `firearm reloading` fires a
    // loaded shot and spends no spare ammunition; the reload paid for that.
    // Only this, the ordinary shot - a critical miss's ammunition loss below
    // still takes spare ammunition, as it always did.
    bool reloading = reload_applies(combat->attacker_obj, combat->weapon_obj);
    if (reloading
            ? !reload_fire(combat->attacker_obj, combat->weapon_obj)
            : !combat_consume_ammo(combat->weapon_obj, combat->attacker_obj, 1, false)) {
        combat->flags |= 0x400;

        sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_OUT_OF_AMMO);
        gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);

        // An empty gun under reloading is reloaded, not swapped for another
        // (zsogg, 2026-09-25); its attack is normally refused before this.
        if (reloading) {
            tf_add(combat->attacker_obj, TF_TYPE_WHITE,
                reload_jammed(combat->weapon_obj) ? "Jammed" : "Reload");
        } else if (obj_field_int32_get(combat->attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
            ai_switch_weapon(combat->attacker_obj);
        } else {
            pc_switch_weapon(combat->attacker_obj, combat->target_obj);
        }

        combat_observer_attack_resolved(observer_action_id,
            observer_attacker, observer_target, combat->skill,
            combat->hit_loc, combat->flags, 0);
        return 0;
    }

    sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_USE);
    gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);

    bool is_melee = true;
    if ((combat->flags & CF_RANGED) != 0
        && (combat->skill == SKILL_THROWING
            || obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_MISSILE_AID) != -1)) {
        is_melee = false;
    }

    if (combat->target_obj != combat->field_28) {
        combat->flags |= 0x800;
    }

    if (is_melee && combat->target_obj != OBJ_HANDLE_NULL) {
        int loudness = combat_weapon_loudness(combat->weapon_obj);
        if ((combat->flags & 0x800) != 0) {
            ai_attack(combat->attacker_obj, combat->target_obj, loudness, 1);
        } else {
            ai_attack(combat->attacker_obj, combat->target_obj, loudness, 0);
        }
    }

    if (combat->weapon_obj != OBJ_HANDLE_NULL && combat->skill != SKILL_THROWING) {
        aptitude_crit_failure_chance = item_aptitude_crit_failure_chance(combat->weapon_obj, combat->attacker_obj);
    } else {
        aptitude_crit_failure_chance = 0;
    }

    if (random_between(1, 100) <= aptitude_crit_failure_chance) {
        combat->flags |= CF_CRITICAL;
    } else if ((combat->flags & 0x100) != 0) {
        combat->flags |= CF_HIT;
    } else {
        if ((combat->flags & CF_RANGED) != 0) {
            int64_t blocking_obj;

            ai_line_of_fire_cost(combat->attacker_obj, combat->target_loc, &blocking_obj);
            if (blocking_obj != OBJ_HANDLE_NULL) {
                combat->target_obj = blocking_obj;
            }
        }

        int v3 = 0;
        if (combat->weapon_obj != OBJ_HANDLE_NULL && combat->skill != SKILL_THROWING) {
            v3 = item_adjust_magic_target(combat->weapon_obj, combat->attacker_obj, combat->target_obj);
        }

        bool cont = true;
        if (random_between(1, 100) <= v3) {
            if (combat->skill == SKILL_MELEE
                && combat->weapon_obj != OBJ_HANDLE_NULL
                && obj_field_int32_get(combat->weapon_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) > 0) {
                combat->flags |= 0x20000;
            } else {
                cont = false;
            }
        }

        if (cont) {
            if (combat->target_obj != OBJ_HANDLE_NULL
                && !obj_type_is_critter(obj_field_int32_get(combat->target_obj, OBJ_F_TYPE))) {
                combat->flags |= CF_HIT;
                cont = false;
            }
        }

        if (cont) {
            SkillInvocation skill_invocation;

            skill_invocation_init(&skill_invocation);
            object_ref_init(combat->attacker_obj, &(skill_invocation.source));
            object_ref_init(combat->target_obj, &(skill_invocation.target));
            skill_invocation.target_loc = combat->target_loc;
            object_ref_init(combat->weapon_obj, &(skill_invocation.item));
            skill_invocation.hit_loc = combat->hit_loc;
            skill_invocation.skill = combat->skill;

            if ((combat->flags & CF_AIM) != 0) {
                skill_invocation.flags |= SKILL_INVOCATION_AIM;
            }

            if ((combat->flags & 0x8000) != 0) {
                skill_invocation.flags |= SKILL_INVOCATION_BACKSTAB;
            }

            if ((combat->flags & 0x20000) != 0) {
                skill_invocation.flags |= SKILL_INVOCATION_NO_MAGIC_ADJ;
            }

            skill_invocation.modifier = 0;

            if (was_in_bed) {
                skill_invocation.modifier -= 30;
            }

            if (!skill_invocation_run(&skill_invocation)) {
                combat_observer_attack_resolved(observer_action_id,
                    observer_attacker, observer_target, combat->skill,
                    combat->hit_loc, combat->flags, 0);
                return 0;
            }

            if ((skill_invocation.flags & SKILL_INVOCATION_SUCCESS) != 0) {
                combat->flags |= CF_HIT;
            }

            if ((skill_invocation.flags & SKILL_INVOCATION_CRITICAL) != 0) {
                combat->flags |= CF_CRITICAL;
            }

            // CE (turn-based rebalance): a weapon whose `rules/weapons.mes`
            // entry says `Dodgeable: 0`, under `weapon rules`, gets no Dodge
            // roll - the Flame Thrower (zsogg, 2026-10-02).
            if (combat->target_obj != OBJ_HANDLE_NULL
                && (combat->flags & CF_HIT) != 0
                && weapon_rules_dodgeable(combat->weapon_obj)) {
                skill_invocation_init(&skill_invocation);
                object_ref_init(combat->target_obj, &(skill_invocation.source));
                skill_invocation.skill = SKILL_DODGE;
                if (!skill_invocation_run(&skill_invocation)) {
                    combat_observer_attack_resolved(observer_action_id,
                        observer_attacker, observer_target, combat->skill,
                        combat->hit_loc, combat->flags, 0);
                    return 0;
                }

                if ((skill_invocation.flags & SKILL_INVOCATION_SUCCESS) != 0) {
                    MesFileEntry mes_file_entry;

                    mes_file_entry.num = 11; // "Dodge!"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, 0, mes_file_entry.str);

                    combat->flags &= ~(CF_HIT | CF_CRITICAL);

                    if ((skill_invocation.flags & SKILL_INVOCATION_CRITICAL) != 0) {
                        int training = basic_skill_training_get(combat->target_obj, BASIC_SKILL_DODGE);
                        if (random_between(1, 100) <= combat_critical_dodge_chance_tbl[training]) {
                            combat->flags |= CF_CRITICAL;
                        }
                    }
                }
            }
        }
    }

    if (combat->attacker_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
        if (fate_resolve(combat->attacker_obj, FATE_CRIT_HIT)) {
            combat->flags |= CF_HIT | CF_CRITICAL;
        }
    } else if (combat->target_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
        if (fate_resolve(combat->target_obj, FATE_CRIT_MISS)) {
            combat->flags &= ~CF_HIT;
            combat->flags |= CF_CRITICAL;
        }
    }

    if ((combat->flags & CF_HIT) == 0
        && (combat->flags & CF_CRITICAL) != 0) {
        combat_process_crit_miss(combat);
    }

    if ((combat->flags & CF_RANGED) != 0) {
        combat_process_ranged_attack(combat);
    } else {
        combat_process_melee_attack(combat);
    }

    anim_play_weapon_fx(combat, combat->attacker_obj, combat->attacker_obj, ANIM_WEAPON_EYE_CANDY_TYPE_FIRE);

    if (is_melee) {
        combat_process_taunts(combat);
        attack_result = combat_play_hit_fx(combat);
        combat_observer_attack_resolved(observer_action_id,
            observer_attacker, observer_target, combat->skill,
            combat->hit_loc, combat->flags, attack_result);
        return attack_result;
    }

    combat_observer_attack_resolved(observer_action_id,
        observer_attacker, observer_target, combat->skill,
        combat->hit_loc, combat->flags, 0);
    return 0;
}

// 0x4B3770
void combat_process_melee_attack(CombatContext* combat)
{
    int sound_id;
    CombatContext body_of_fire;

    if ((combat->flags & CF_HIT) != 0) {
        if (combat->weapon_obj != OBJ_HANDLE_NULL) {
            object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_HIT, 0);

            if ((combat->flags & CF_CRITICAL) != 0) {
                object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_CRITICAL_HIT, 0);
            }
        }

        if (combat->attacker_obj != OBJ_HANDLE_NULL
            && combat->target_obj != OBJ_HANDLE_NULL) {
            object_script_execute(combat->target_obj, combat->attacker_obj, OBJ_HANDLE_NULL, SAP_CRITTER_HITS, 0);
        }

        combat_calc_dmg(combat);
        combat_dmg(combat);

        if ((combat->flags & CF_CRITICAL) != 0) {
            sound_id = sfx_critter_sound(combat->target_obj, CRITTER_SOUND_CRITICALLY_HIT);
            gsound_play_sfx_on_obj(sound_id, 1, combat->target_obj);

            sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_CRITICAL_HIT);
            gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);
        } else {
            sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_HIT);
            gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);
        }

        if ((obj_field_int32_get(combat->attacker_obj, OBJ_F_SPELL_FLAGS) & OSF_BODY_OF_FIRE) == 0
            && (obj_field_int32_get(combat->target_obj, OBJ_F_SPELL_FLAGS) & OSF_BODY_OF_FIRE) != 0) {
            combat_context_init(combat->target_obj, combat->attacker_obj, &body_of_fire);
            body_of_fire.dam[DAMAGE_TYPE_FIRE] = 5;
            combat_dmg(&body_of_fire);
        }
    } else {
        sound_id = sfx_item_sound(combat->weapon_obj, combat->attacker_obj, combat->target_obj, WEAPON_SOUND_MISS);
        gsound_play_sfx_on_obj(sound_id, 1, combat->attacker_obj);

        if (obj_field_int32_get(combat->target_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
            ai_process(combat->target_obj);
        }

        if (combat->weapon_obj != OBJ_HANDLE_NULL) {
            object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_MISS, 0);

            if ((combat->flags & CF_CRITICAL) != 0) {
                object_script_execute(combat->attacker_obj, combat->weapon_obj, combat->target_obj, SAP_CRITICAL_MISS, 0);
            }
        }
    }
}

// 0x4B39B0
void combat_process_ranged_attack(CombatContext* combat)
{
    bool is_boomerangs = false;
    tig_art_id_t missile_art_id = TIG_ART_ID_INVALID;
    int64_t loc;
    int range;
    int64_t max_range;
    int num_arrows;
    int arrow;

    if ((combat->flags & 0x100) == 0
        && combat->weapon_obj != OBJ_HANDLE_NULL) {
        if (obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
            missile_art_id = obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_MISSILE_AID);
            is_boomerangs = (obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_BOOMERANGS) != 0;
        }
    } else {
        tig_art_scenery_id_create(0, 0, 0, 0, 0, &missile_art_id);
    }

    if (missile_art_id == TIG_ART_ID_INVALID) {
        if (combat->skill == BASIC_SKILL_THROWING) {
            missile_art_id = obj_field_int32_get(combat->weapon_obj, OBJ_F_CURRENT_AID);
        }
    }

    if (missile_art_id == TIG_ART_ID_INVALID) {
        combat_process_attack_result(combat);
        return;
    }

    loc = obj_field_int64_get(combat->attacker_obj, OBJ_F_LOCATION);
    if (combat->skill == BASIC_SKILL_THROWING
        && !is_boomerangs
        && (combat->flags & CF_HIT) == 0) {
        range = (20 - basic_skill_level(combat->attacker_obj, BASIC_SKILL_THROWING)) / 5 + 1;
        max_range = location_dist(loc, combat->target_loc);
        if (range > max_range) {
            range = (int)max_range;
        }

        location_in_range(combat->target_loc, random_between(0, 7), range, &(combat->target_loc));
        combat->target_obj = OBJ_HANDLE_NULL;
        combat->flags |= 0x20;
    }

    // NOTE: Verified against 0x4B39B0 - original calls tig_art_id_rotation_get and discards the result.
    tig_art_id_rotation_get(obj_field_int32_get(combat->attacker_obj, OBJ_F_CURRENT_AID));

    num_arrows = 1;
    if (combat->skill == BASIC_SKILL_BOW
        && basic_skill_training_get(combat->attacker_obj, BASIC_SKILL_BOW) >= TRAINING_EXPERT) {
        num_arrows = 2;
    }

    for (arrow = 0; arrow < num_arrows; arrow++) {
        combat_create_projectile(combat, loc, combat_multishot_offsets[arrow], combat_multishot_offsets[arrow], missile_art_id);
    }
}

// 0x4B3BB0
void combat_attack_weapon_wear(int64_t attacker_obj, int64_t target_obj, int hit_loc)
{
    CombatContext combat;

    combat_context_init(attacker_obj, target_obj, &combat);
    combat.hit_loc = hit_loc;
    combat.flags |= CF_WEAPON_WEAR;
    combat.flags |= 0x40000;
    combat_attack(&combat);
}

// 0x4B3C00
void combat_throw(int64_t attacker_obj, int64_t weapon_obj, int64_t target_obj, int64_t target_loc, int hit_loc)
{
    int64_t attacker_loc;
    CombatContext combat;

    attacker_loc = obj_field_int64_get(attacker_obj, OBJ_F_LOCATION);
    if (obj_field_int32_get(attacker_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        item_remove(weapon_obj);
    }

    if (object_script_execute(attacker_obj, weapon_obj, target_obj, SAP_THROW, 0)) {
        combat_context_init(attacker_obj, target_obj, &combat);
        combat.hit_loc = hit_loc;
        combat.weapon_obj = weapon_obj;
        combat.skill = BASIC_SKILL_THROWING;
        if (target_obj == OBJ_HANDLE_NULL) {
            combat.target_loc = target_loc;
        }
        combat.flags &= ~0xC000;
        combat.flags |= 0x40000;
        combat.flags |= CF_RANGED;
        combat.flags |= 0x40;
        object_flags_set(weapon_obj, OF_OFF);
        object_drop(weapon_obj, attacker_loc);
        combat_attack(&combat);
    } else {
        object_drop(weapon_obj, attacker_loc);
        item_transfer(weapon_obj, attacker_obj);
    }
}

// 0x4B3D50
bool combat_critter_is_combat_mode_active(int64_t obj)
{
    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_COMBAT_MODE_ACTIVE) != 0;
    } else {
        return false;
    }
}

// 0x4B3D90
bool combat_can_exit_combat_mode(int64_t obj)
{
    bool ret = true;
    ObjectList pcs;
    ObjectNode* pc_node;
    ObjectList npcs;
    ObjectNode* npc_node;
    int64_t combat_focus_obj;

    if (!combat_critter_is_combat_mode_active(obj)) {
        return true;
    }

    if (!critter_is_active(obj)) {
        return true;
    }

    if (anim_is_current_goal_type(obj, AG_ATTEMPT_ATTACK, NULL)) {
        return false;
    }

    object_list_all_followers(obj, &pcs);
    object_list_vicinity(obj, OBJ_TM_NPC, &npcs);

    npc_node = npcs.head;
    while (npc_node != NULL && ret) {
        combat_focus_obj = obj_field_handle_get(npc_node->obj, OBJ_F_NPC_COMBAT_FOCUS);
        if (combat_focus_obj == obj) {
            if (critter_is_active(npc_node->obj) && ai_is_fighting(npc_node->obj)) {
                ret = false;
                break;
            }
        } else {
            pc_node = pcs.head;
            while (pc_node != NULL) {
                if (combat_focus_obj == pc_node->obj
                    && critter_is_active(npc_node->obj)
                    && ai_is_fighting(npc_node->obj)) {
                    ret = false;
                    break;
                }
                pc_node = pc_node->next;
            }
        }
        npc_node = npc_node->next;
    }

    object_list_destroy(&npcs);
    object_list_destroy(&pcs);

    return ret;
}

// 0x4B3F20
void combat_critter_deactivate_combat_mode(int64_t obj)
{
    if (combat_critter_is_combat_mode_active(obj)) {
        combat_critter_toggle_combat_mode(obj);
    }
}

// 0x4B3F50
void combat_critter_activate_combat_mode(int64_t obj)
{
    if (!combat_critter_is_combat_mode_active(obj)) {
        combat_critter_toggle_combat_mode(obj);
    }
}

// 0x4B3F80
void combat_critter_toggle_combat_mode(int64_t obj)
{
    bool v1 = false;
    bool combat_mode_is_active;
    bool is_pc;
    int obj_type;
    bool is_tb;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    is_pc = player_is_local_pc_obj(obj);
    combat_mode_is_active = combat_critter_is_combat_mode_active(obj);
    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    if (!obj_type_is_critter(obj_type) || !critter_is_dead(obj)) {
        if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
            return;
        }

        if (obj_type_is_critter(obj_type)) {
            unsigned int critter_flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
            if ((critter_flags & OCF_PARALYZED) != 0) {
                return;
            }

            if (combat_mode_is_active) {
                if ((critter_flags & OCF_STUNNED) != 0) {
                    return;
                }
            } else {
                if (!critter_is_active(obj)) {
                    return;
                }
            }
        }
    }

    is_tb = combat_is_turn_based();

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (tig_art_id_anim_get(art_id) == TIG_ART_ANIM_CONCEAL_FIDGET) {
        v1 = true;
        if (!is_pc && !critter_is_dead(obj)) {
            return;
        }
    }

    unsigned int critter_flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
    unsigned int critter_flags2 = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);

    if (combat_mode_is_active) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, critter_flags & ~OCF_COMBAT_MODE_ACTIVE);
        ai_target_unlock(obj);

        int anim = tig_art_id_anim_get(art_id);
        if (anim == TIG_ART_ANIM_ATTACK || anim == TIG_ART_ANIM_ATTACK_LOW) {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        }

        art_id = tig_art_critter_id_weapon_set(art_id, TIG_ART_WEAPON_TYPE_NO_WEAPON);
        art_id = tig_art_critter_id_shield_set(art_id, 0);

        if (is_pc) {
            if (is_tb) {
                combat_callbacks.field_8();
                combat_turn_based_end();
            }
            gsound_stop_combat_music(obj);

            // CE (criticals, a CR-2 repair): the fight is over for the
            // player, so the opening shot's memory clears.
            crit_guard_engaged_clear();
        }

        if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_INVISIBLE) != 0
            && !player_is_local_pc_obj(obj)) {
            object_flags_set(obj, OF_INVISIBLE);
        }
    } else {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, critter_flags | OCF_COMBAT_MODE_ACTIVE);

        art_id = tig_art_critter_id_weapon_set(art_id, TIG_ART_WEAPON_TYPE_UNARMED);

        if (is_pc && is_tb) {
            map_draw_hook();

            if (!combat_turn_based_start()) {
                return;
            }

            combat_callbacks.field_4();

            if (combat_turn_based_active) {
                combat_turn_based_whos_turn_set(obj);
            }
        }

        gsound_start_combat_music(obj);
    }

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        tig_debug_printf("Combat: combat_critter_toggle_combat_mode: Failed to grab art: %u!\n", art_id);
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        return;
    }

    if (tig_art_id_frame_get(art_id) >= art_anim_data.num_frames) {
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    object_set_current_aid(obj, art_id);
    object_set_current_aid(obj, item_wield_art_id(obj));

    if (combat_mode_is_active) {
        if (is_pc) {
            combat_callbacks.field_0(0);
        }

        if (!v1 && anim_is_fidgeting(obj)) {
            anim_interrupt_all_goals_of_obj(obj, 3, 0, 0);
        }
    } else {
        if (is_pc) {
            combat_callbacks.field_0(1);
            ai_allow_attacking(obj);
        }

        if (!v1 && !is_tb) {
            anim_goal_fidget(obj);
        }
    }

    if ((critter_flags2 & OCF2_COMBAT_TOGGLE_FX) != 0) {
        magictech_fx_add(obj, MAGICTECH_FX_COMBAT_TOGGLE);
    }
}

// 0x4B4320
void combat_critter_fidget_restart(int64_t obj)
{
    if (obj != OBJ_HANDLE_NULL
        && (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC
            || obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)
        && ((!combat_turn_based_active && combat_critter_is_combat_mode_active(obj))
            || (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_AUTO_ANIMATES) != 0)) {
        anim_goal_fidget(obj);
    }
}

// 0x4B4390
void combat_dmg(CombatContext* combat)
{
    int obj_type;
    unsigned int dam_flags;
    int dam;
    MesFileEntry mes_file_entry;
    char str[80];
    unsigned int spell_flags = 0;
    bool weapon_dropped = false;
    int observer_hp_before = -1;
    int observer_fatigue_before = -1;
    char observer_attacker[COMBAT_OBSERVER_IDENTITY_SIZE] = "NULL";
    char observer_target[COMBAT_OBSERVER_IDENTITY_SIZE] = "NULL";

    if (combat->target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    obj_type = obj_field_int32_get(combat->target_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(obj_type)) {
        unsigned int critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        spell_flags = obj_field_int32_get(combat->target_obj, OBJ_F_SPELL_FLAGS);

        if (combat_observer_is_enabled()) {
            observer_hp_before = object_hp_damage_get(combat->target_obj);
            observer_fatigue_before = critter_fatigue_damage_get(combat->target_obj);
            combat_observer_object_id(combat->attacker_obj, observer_attacker);
            combat_observer_object_id(combat->target_obj, observer_target);
        }

        if ((critter_flags & (OCF_UNDEAD | OCF_MECHANICAL)) != 0) {
            combat->dam[DAMAGE_TYPE_POISON] = 0;
        } else if ((spell_flags & OSF_STONED) != 0) {
            combat->dam[DAMAGE_TYPE_NORMAL] = 0;
            combat->dam[DAMAGE_TYPE_POISON] = 0;
            combat->dam[DAMAGE_TYPE_ELECTRICAL] = 0;
            combat->dam[DAMAGE_TYPE_FIRE] = 0;
            combat->dam[DAMAGE_TYPE_FATIGUE] = 0;
        }
    }

    if (!combat_in_taking_damage_script) {
        bool def;

        combat_in_taking_damage_script = true;
        def = object_script_execute(combat->attacker_obj, combat->target_obj, OBJ_HANDLE_NULL, SAP_TAKING_DAMAGE, 0);
        combat_in_taking_damage_script = false;

        if (!def) {
            return;
        }
    }

    if ((obj_field_int32_get(combat->target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_INVULNERABLE)) != 0) {
        return;
    }

    if (obj_type_is_critter(obj_type)
        && (obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NIGH_INVULNERABLE) != 0
        && (combat->weapon_obj == OBJ_HANDLE_NULL
            || (obj_field_int32_get(combat->weapon_obj, OBJ_F_ITEM_FLAGS) & OIF_UBER) == 0)) {
        combat->dam[DAMAGE_TYPE_NORMAL] = 0;
        combat->dam[DAMAGE_TYPE_POISON] = 0;
        combat->dam[DAMAGE_TYPE_ELECTRICAL] = 0;
        combat->dam[DAMAGE_TYPE_FIRE] = 0;
        combat->dam[DAMAGE_TYPE_FATIGUE] = 0;
        return;
    }

    if (obj_type == OBJ_TYPE_WALL) {
        return;
    }

    // CE (criticals, milestone CR-2): under `critical effects` a fumble's
    // hit on its wielder (0x8) has already rolled its effects in
    // `combat_process_crit_miss` and does not roll them again (MODDING-NOTES
    // quirk 58).
    if ((combat->flags & CF_CRITICAL) != 0
        && !(feature_enabled(FEATURE_CRITICAL_EFFECTS) && (combat->flags & 0x8) != 0)) {
        combat_process_crit_hit(combat);
    }

    // CE (armor milestone 3): under `armor penetration` a weapon hit's normal
    // damage is resolved against armor instead of normal resistance, with the
    // difficulty scaling and the critical bonus already in it (zsogg's decision
    // 3). It is set aside here and added back after the multipliers below,
    // which then scale only the elemental damage.
    int armor_normal = combat_armor_resolve(combat);
    if (armor_normal >= 0) {
        combat->dam[DAMAGE_TYPE_NORMAL] = 0;
    }

    combat_apply_resistance(combat);

    dam_flags = combat->dam_flags;
    if ((dam_flags & (CDF_FULL | CDF_DEATH)) != 0) {
        dam = object_hp_current(combat->target_obj);
    } else {
        dam = combat->dam[DAMAGE_TYPE_NORMAL]
            + combat->dam[DAMAGE_TYPE_ELECTRICAL]
            + combat->dam[DAMAGE_TYPE_FIRE];
    }

    if (combat->dam[DAMAGE_TYPE_FIRE] > 0) {
        dam_flags |= CDF_DAMAGE_ARMOR;
    }

    if (combat->weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
        && (obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_DAMAGE_ARMOR) != 0) {
        dam_flags |= CDF_DAMAGE_ARMOR;
    }

    if (player_is_pc_obj(combat->attacker_obj)) {
        combat->game_difficulty = gamelib_game_difficulty_get();

        switch (combat->game_difficulty) {
        case GAME_DIFFICULTY_EASY:
            dam += dam / 2;
            break;
        case GAME_DIFFICULTY_HARD:
            dam -= dam / 4;
            break;
        }
    }

    if ((dam_flags & CDF_BONUS_DAM_200) != 0) {
        dam *= 3;
    } else if ((dam_flags & CDF_BONUS_DAM_100) != 0) {
        dam *= 2;
    } else if ((dam_flags & CDF_BONUS_DAM_50) != 0) {
        dam += dam / 2;
    }

    if (armor_normal >= 0) {
        combat->dam[DAMAGE_TYPE_NORMAL] = armor_normal;
        dam += armor_normal;
    }

    if (obj_type_is_critter(obj_type)) {
        // 0x4B475F
        if ((combat->flags & CF_TRAP) != 0) {
            combat->flags |= CF_HIT;
            combat_play_hit_fx(combat);
        }

        if (critter_is_dead(combat->target_obj)) {
            return;
        }

        if ((combat->flags & CF_CRITICAL) != 0) {
            int inventory_location;
            int64_t item_obj;

            for (inventory_location = FIRST_WEAR_INV_LOC; inventory_location <= LAST_WEAR_INV_LOC; inventory_location++) {
                item_obj = item_wield_get(combat->target_obj, inventory_location);
                if (item_obj != OBJ_HANDLE_NULL
                    && obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_ARMOR) {
                    object_script_execute(combat->attacker_obj, item_obj, combat->target_obj, SAP_TAKING_DAMAGE, 0);
                }
            }
        }

        int tf_type;
        if (obj_type == OBJ_TYPE_PC) {
            tf_type = TF_TYPE_RED;
        } else if (obj_type == OBJ_TYPE_NPC
            && critter_pc_leader_get(combat->target_obj) == player_get_local_pc_obj()) {
            tf_type = TF_TYPE_YELLOW;
        } else {
            tf_type = TF_TYPE_WHITE;
        }

        if ((combat->flags & 0x18) != 0) {
            mes_file_entry.num = 2; // "Critical miss"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->attacker_obj, tf_type, mes_file_entry.str);
        } else if ((combat->flags & CF_CRITICAL) != 0) {
            mes_file_entry.num = 12; // "Critical hit"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->target_obj, tf_type, mes_file_entry.str);
        }

        if ((dam_flags & CDF_BONUS_DAM_200) != 0) {
            mes_file_entry.num = 15; // "Damage +200%"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->target_obj, tf_type, mes_file_entry.str);
        } else if ((dam_flags & CDF_BONUS_DAM_100) != 0) {
            mes_file_entry.num = 14; // "Damage +100%"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->target_obj, tf_type, mes_file_entry.str);
        } else if ((dam_flags & CDF_BONUS_DAM_50) != 0) {
            mes_file_entry.num = 13; // "Damage +50%"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->target_obj, tf_type, mes_file_entry.str);
        }

        unsigned int critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        if ((dam_flags & CDF_STUN) != 0
            && (critter_flags & OCF_STUNNED) == 0
            && (dam_flags & (CDF_KNOCKDOWN | CDF_KNOCKOUT)) == 0) {
            mt_item_notify_parent_stunned(combat->attacker_obj, combat->target_obj);
            critter_flags |= OCF_STUNNED;

            if (!anim_goal_animate_stunned(combat->target_obj)) {
                critter_flags &= ~OCF_STUNNED;
            }

            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);

            mes_file_entry.num = 16; // "Stunned"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->target_obj, tf_type, mes_file_entry.str);
        }

        if ((dam_flags & (CDF_KNOCKDOWN | CDF_KNOCKOUT)) != 0) {
            mt_item_notify_parent_going_unconscious(combat->attacker_obj, combat->target_obj);
            mt_item_notify_target_going_unconscious(combat->attacker_obj, combat->target_obj);
            anim_goal_knockdown(combat->target_obj);

            if ((dam_flags & CDF_KNOCKOUT) != 0) {
                if (!critter_is_unconscious(combat->target_obj)) {
                    int max_fatigue = critter_fatigue_max(combat->target_obj);

                    combat->field_64 = random_between(10, 20);

                    critter_fatigue_damage_set(combat->target_obj, combat->field_64 + max_fatigue);

                    mes_file_entry.num = 17; // "Unconscious"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);
                }
            } else {
                mes_file_entry.num = 18; // "Knocked down"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            }
        }

        if ((dam_flags & CDF_SCAR) != 0) {
            effect_add(combat->target_obj, EFFECT_SCARRING, EFFECT_CAUSE_INJURY);
            logbook_add_injury(combat->target_obj, combat->attacker_obj, LBI_SCARRED);

            mes_file_entry.num = 3; // "Scarred"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            tf_add(combat->target_obj, tf_type, mes_file_entry.str);
        }

        if ((dam_flags & CDF_BLIND) != 0) {
            if ((critter_flags & OCF_BLINDED) == 0) {
                critter_flags |= OCF_BLINDED;
                obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
                logbook_add_injury(combat->target_obj, combat->attacker_obj, LBI_BLINDED);

                mes_file_entry.num = 4; // "Blinded"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            }
        }

        if ((dam_flags & CDF_CRIPPLE_ARM) != 0) {
            if ((critter_flags & OCF_CRIPPLED_ARMS_ONE) == 0) {
                critter_flags |= OCF_CRIPPLED_ARMS_ONE;
                obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
                logbook_add_injury(combat->target_obj, combat->attacker_obj, LBI_CRIPPLED_ARM);

                mes_file_entry.num = 5; // "Crippled arm"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            } else if ((critter_flags & OCF_CRIPPLED_ARMS_BOTH) == 0
                && random_between(1, 100) <= 50) {
                critter_flags |= OCF_CRIPPLED_ARMS_BOTH;
                obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
                logbook_add_injury(combat->target_obj, combat->attacker_obj, LBI_CRIPPLED_ARM);

                mes_file_entry.num = 5; // "Crippled arm"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            }

            int64_t weapon_obj = item_wield_get(combat->target_obj, ITEM_INV_LOC_WEAPON);
            if (weapon_obj != OBJ_HANDLE_NULL
                && item_check_wield(weapon_obj, ITEM_INV_LOC_WEAPON, combat->target_obj)) {
                item_drop_nearby(weapon_obj);

                mes_file_entry.num = 19; // "Weapon dropped"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);

                weapon_dropped = true;
            }

            int64_t shield_obj = item_wield_get(combat->target_obj, ITEM_INV_LOC_SHIELD);
            if (shield_obj != OBJ_HANDLE_NULL
                && item_check_wield(shield_obj, ITEM_INV_LOC_SHIELD, combat->target_obj)) {
                item_drop_nearby(weapon_obj);

                mes_file_entry.num = 20; // "Item dropped"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            }
        }

        if ((dam_flags & CDF_CRIPPLE_LEG) != 0) {
            if ((critter_flags & OCF_CRIPPLED_LEGS_BOTH) == 0) {
                critter_flags |= OCF_CRIPPLED_LEGS_BOTH;
                obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
                logbook_add_injury(combat->target_obj, combat->attacker_obj, LBI_CRIPPLED_LEG);

                mes_file_entry.num = 6; // "Crippled leg"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            }
        }

        if ((dam_flags & CDF_DAMAGE_ARMOR) != 0) {
            int64_t armor_obj = combat_critter_armor(combat->target_obj, combat->hit_loc);
            if (armor_obj != OBJ_HANDLE_NULL) {
                // CE (turn-based rebalance): the weapon's `rules/weapons.mes`
                // ArmorDamage, under `weapon rules`, replaces the 10 a hit
                // takes from armor.
                object_hp_damage_set(armor_obj,
                    object_hp_damage_get(armor_obj)
                        + weapon_rules_armor_damage(combat->weapon_obj, 10));

                if (object_hp_current(armor_obj) > 0) {
                    mes_file_entry.num = 7; // "Armor damaged"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);
                } else {
                    object_bust(armor_obj, combat->attacker_obj);

                    mes_file_entry.num = 24; // "Armor broken"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);
                }
            }
        }

        if ((dam_flags & CDF_DROP_HELMET) != 0) {
            int64_t helmet_obj = item_wield_get(combat->target_obj, ITEM_INV_LOC_HELMET);
            if (helmet_obj != OBJ_HANDLE_NULL) {
                item_drop_nearby(helmet_obj);

                mes_file_entry.num = 20; // "Item dropped"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);
            }
        }

        int64_t weapon_obj = combat_critter_weapon(combat->target_obj);
        if (weapon_obj != OBJ_HANDLE_NULL) {
            if ((obj_field_int32_get(weapon_obj, OBJ_F_FLAGS) & OF_INVULNERABLE) != 0) {
                dam_flags &= ~(CDF_EXPLODE_WEAPON | CDF_DESTROY_WEAPON | CDF_DAMAGE_WEAPON);
            }

            if ((dam_flags & CDF_DAMAGE_WEAPON) != 0) {
                object_hp_damage_set(weapon_obj, object_hp_damage_get(weapon_obj) + 10);

                if (object_hp_current(weapon_obj) > 0) {
                    mes_file_entry.num = 8; // "Weapon damaged"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);
                } else {
                    object_bust(weapon_obj, combat->attacker_obj);

                    mes_file_entry.num = 23; // "Weapon broken"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);

                    weapon_dropped = true;
                }
            }

            if ((dam_flags & CDF_EXPLODE_WEAPON) != 0) {
                dam_flags |= CDF_DESTROY_WEAPON;
                script_play_explosion_fx(combat->target_obj);
            }

            if ((dam_flags & CDF_DESTROY_WEAPON) != 0) {
                object_destroy(weapon_obj);

                mes_file_entry.num = 22; // "Weapon destroyed"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                tf_add(combat->target_obj, tf_type, mes_file_entry.str);

                weapon_dropped = true;
            } else {
                if ((dam_flags & CDF_DESTROY_AMMO) != 0) {
                    combat_consume_ammo(weapon_obj, combat->target_obj, 0, true);

                    mes_file_entry.num = 9; // "Ammo lost"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);
                } else if ((dam_flags & CDF_LOST_AMMO) != 0) {
                    combat_consume_ammo(weapon_obj, combat->target_obj, 5, false);

                    mes_file_entry.num = 9; // "Ammo lost"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);
                }

                if ((dam_flags & CDF_DROP_WEAPON) != 0) {
                    item_drop_nearby(weapon_obj);

                    mes_file_entry.num = 19; // "Weapon dropped"
                    mes_get_msg(combat_mes_file, &mes_file_entry);
                    tf_add(combat->target_obj, tf_type, mes_file_entry.str);

                    weapon_dropped = true;
                }
            }
        }

        if (dam > 0
            && (obj_field_int32_get(combat->target_obj, OBJ_F_SPELL_FLAGS) & OSF_MIRRORED) != 0) {
            magictech_mirrored_hit(combat->target_obj);
        }

        int hp_ratio_before;

        if (dam > 0) {
            combat->dam_flags |= CDF_HAVE_DAMAGE;
            hp_ratio_before = ai_object_hp_ratio(combat->target_obj);
        }

        // The original clears the string here (0x4B504D) even though the
        // `snprintf` below always overwrites it - dead in the unpatched flow,
        // but the UAP fix skips that `snprintf` and leaves this the only
        // initialization.
        str[0] = '\0';

        // UAP (0x4B5054): a blow the target shrugged off no longer floats
        // "HT 0" over its head - with nothing written here, the `str[0]`
        // guard below suppresses the message. A hit that only cost fatigue
        // still floats its "FT" part, which the fatigue block appends.
        if (!uap_fixes_enabled() || dam > 0) {
            mes_file_entry.num = 1; // "HT"
            mes_get_msg(combat_mes_file, &mes_file_entry);
            snprintf(str, sizeof(str), "%s %d", mes_file_entry.str, dam);
        }

        combat->total_dam = dam;

        int hp_dam = object_hp_damage_get(combat->target_obj) + dam;
        if (hp_dam < 0) {
            hp_dam = 0;
        }
        object_hp_damage_set(combat->target_obj, hp_dam);

        if (dam > 0) {
            int64_t leader_obj = critter_pc_leader_get(combat->target_obj);
            if (leader_obj != OBJ_HANDLE_NULL
                && hp_ratio_before >= 20
                && ai_object_hp_ratio(combat->target_obj) < 20) {
                ai_npc_near_death(combat->target_obj, leader_obj);
            }
        }

        if ((critter_flags & OCF_FATIGUE_IMMUNE) == 0) {
            if (combat->dam[DAMAGE_TYPE_FATIGUE] > 0) {
                mes_file_entry.num = 10;
                mes_get_msg(combat_mes_file, &mes_file_entry);

                snprintf(&(str[strlen(str)]), sizeof(str) - strlen(str),
                    " %d %s",
                    combat->dam[DAMAGE_TYPE_FATIGUE],
                    mes_file_entry.str);

                critter_fatigue_damage_set(combat->target_obj,
                    critter_fatigue_damage_get(combat->target_obj) + combat->dam[DAMAGE_TYPE_FATIGUE]);
            }
        }

        if (tf_level_get() == TF_LEVEL_VERBOSE && str[0] != '\0') {
            tf_add(combat->target_obj, tf_type, str);
        }

        int poison = stat_base_get(combat->target_obj, STAT_POISON_LEVEL) + combat->dam[DAMAGE_TYPE_POISON];
        if (poison < 0) {
            poison = 0;
        }
        stat_base_set(combat->target_obj, STAT_POISON_LEVEL, poison);

        if (combat->dam[DAMAGE_TYPE_POISON] > 0 && tf_level_get() == TF_LEVEL_VERBOSE) {
            mes_file_entry.num = 0;
            mes_get_msg(combat_mes_file, &mes_file_entry);
            snprintf(str, sizeof(str),
                "%s %+d",
                mes_file_entry.str,
                combat->dam[DAMAGE_TYPE_POISON]);
            tf_add(combat->target_obj, TF_TYPE_GREEN, str);
        }

        if (combat->field_30 != OBJ_HANDLE_NULL
            && obj_type == OBJ_TYPE_NPC
            && (spell_flags & OSF_STONED) == 0) {
            int remaining_experience = obj_field_int32_get(combat->target_obj, OBJ_F_NPC_EXPERIENCE_POOL);
            if (remaining_experience > 0) {
                int dam_ratio = 100 * dam / object_hp_max(combat->target_obj);
                int awarded_experience = obj_field_int32_get(combat->target_obj, OBJ_F_NPC_EXPERIENCE_WORTH) * dam_ratio / 100;
                if (awarded_experience > remaining_experience) {
                    awarded_experience = remaining_experience;
                }
                obj_field_int32_set(combat->target_obj, OBJ_F_NPC_EXPERIENCE_POOL, remaining_experience - awarded_experience);

                if (obj_field_int32_get(combat->field_30, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                    critter_give_xp(combat->field_30, awarded_experience);
                }
            }
        }

        if (critter_is_dead(combat->target_obj)) {
            int anim;

            if ((dam_flags & CDF_DEATH) != 0 || dam <= 20) {
                anim = 7;
            } else if (combat->hit_loc == HIT_LOC_HEAD) {
                anim = 17;
            } else if (combat->hit_loc == HIT_LOC_LEG) {
                anim = 19;
            } else {
                anim = 18;
            }

            critter_notify_killed(combat->target_obj, combat->field_30, anim);

            if (obj_type == OBJ_TYPE_NPC
                && critter_pc_leader_get(combat->target_obj) == player_get_local_pc_obj()) {
                ui_follower_update();
            }
        } else if (obj_type == OBJ_TYPE_NPC) {
            ai_process(combat->target_obj);

            if (critter_pc_leader_get(combat->target_obj) == player_get_local_pc_obj()) {
                ui_follower_update();
            }
        }

        if (combat->attacker_obj != OBJ_HANDLE_NULL) {
            mt_item_notify_parent_hit(combat->attacker_obj, combat, combat->target_obj);

            if ((combat->flags & 0x40000) != 0) {
                mt_item_notify_parent_dmgs_obj(combat->attacker_obj, combat->weapon_obj, combat->target_obj);
            } else {
                mt_item_notify_parent_dmgs_obj(combat->attacker_obj, OBJ_HANDLE_NULL, combat->target_obj);
            }
        }

        combat_apply_weapon_wear(combat);

        if (weapon_dropped) {
            pc_switch_weapon(combat->target_obj, combat->attacker_obj);
        }

        if (combat_observer_is_enabled()) {
            combat_observer_damage_committed(observer_attacker,
                observer_target, combat->dam,
                dam_flags, dam, observer_hp_before,
                object_hp_damage_get(combat->target_obj),
                observer_fatigue_before,
                critter_fatigue_damage_get(combat->target_obj),
                critter_is_dead(combat->target_obj),
                critter_is_unconscious(combat->target_obj));
        }
    } else {
        // 0x4B462E
        if (obj_type == OBJ_TYPE_PORTAL
            || obj_type == OBJ_TYPE_CONTAINER) {
            int material = obj_field_int32_get(combat->target_obj, OBJ_F_MATERIAL);
            dam -= combat_material_damage_reduction_tbl[material];
        }

        if (dam < 0) {
            dam = 0;
        } else if (dam > 0) {
            combat->dam_flags |= CDF_HAVE_DAMAGE;
        }

        int hp_dam = object_hp_damage_get(combat->target_obj) + dam;
        if (hp_dam < 0) {
            hp_dam = 0;
        }
        object_hp_damage_set(combat->target_obj, hp_dam);

        // UAP (0x4B4679): do not float a damage number over an object that
        // took no damage - hitting a door or a container that shrugs the blow
        // off used to announce "HT 0".
        if ((!uap_fixes_enabled() || dam > 0)
            && tf_level_get() == TF_LEVEL_VERBOSE) {
            mes_file_entry.num = 1; // "HT"
            mes_get_msg(combat_mes_file, &mes_file_entry);

            snprintf(str, sizeof(str), "%s %d", mes_file_entry.str, dam);
            tf_add(combat->target_obj, TF_TYPE_WHITE, str);
        }

        if (object_hp_current(combat->target_obj) > 0) {
            if (combat->field_30 != OBJ_HANDLE_NULL) {
                ai_notify_portal_container_guards(combat->field_30, combat->target_obj, true, LOUDNESS_LOUD);
                combat_apply_weapon_wear(combat);
            }
        } else {
            if (trap_type(combat->target_obj) != TRAP_TYPE_INVALID) {
                int64_t triggerer_obj = (combat->flags & CF_RANGED) == 0
                    ? combat->attacker_obj
                    : OBJ_HANDLE_NULL;
                trap_invoke(triggerer_obj, combat->target_obj);
            }

            object_bust(combat->target_obj, combat->attacker_obj);

            if (combat->field_30 != OBJ_HANDLE_NULL) {
                ai_notify_portal_container_guards(combat->field_30, combat->target_obj, false, LOUDNESS_LOUD);
            }
        }
    }
}

// 0x4B54B0
int64_t combat_critter_armor(int64_t critter_obj, int hit_loc)
{
    switch (hit_loc) {
    case HIT_LOC_HEAD:
        return item_wield_get(critter_obj, ITEM_INV_LOC_HELMET);
    case HIT_LOC_ARM:
        return item_wield_get(critter_obj, ITEM_INV_LOC_GAUNTLET);
    case HIT_LOC_LEG:
        return item_wield_get(critter_obj, ITEM_INV_LOC_BOOTS);
    default:
        return item_wield_get(critter_obj, ITEM_INV_LOC_ARMOR);
    }
}

// 0x4B5580
void combat_apply_weapon_wear(CombatContext* combat)
{
    int dam;
    int complexity;
    char str[80];
    MesFileEntry mes_file_entry;

    if (combat->attacker_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (combat->target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if ((combat->flags & CF_WEAPON_WEAR) == 0) {
        return;
    }

    if ((combat->dam_flags & CDF_HAVE_DAMAGE) == 0) {
        return;
    }

    if (combat->skill != SKILL_MELEE) {
        return;
    }

    switch (obj_field_int32_get(combat->target_obj, OBJ_F_MATERIAL)) {
    case MATERIAL_STONE:
        dam = 4;
        break;
    case MATERIAL_BRICK:
        dam = 3;
        break;
    case MATERIAL_WOOD:
        dam = 2;
        break;
    case MATERIAL_PLANT:
        dam = 1;
        break;
    case MATERIAL_METAL:
        dam = 5;
        break;
    case MATERIAL_GLASS:
        dam = combat->weapon_obj != OBJ_HANDLE_NULL ? 0 : 5;
        break;
    case MATERIAL_FIRE:
        dam = 5;
        break;
    default:
        dam = 0;
        break;
    }

    if (combat->weapon_obj != OBJ_HANDLE_NULL) {
        complexity = obj_field_int32_get(combat->weapon_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
        if (complexity < 0) {
            complexity = -complexity;
        }

        if (complexity > 100) {
            complexity = 100;
        }

        dam -= dam * complexity / 100;

        switch (tig_art_item_id_subtype_get(obj_field_int32_get(combat->weapon_obj, OBJ_F_ITEM_USE_AID_FRAGMENT))) {
        case TIG_ART_WEAPON_TYPE_AXE:
            dam -= 2;
            break;
        case TIG_ART_WEAPON_TYPE_MACE:
        case TIG_ART_WEAPON_TYPE_STAFF:
            dam -= 1;
            break;
        }
    } else {
        int hp = object_hp_current(combat->attacker_obj);
        if (dam >= hp) {
            dam = hp - 1;
        }
    }

    str[0] = '\0';

    if (dam > 0) {
        if (combat->weapon_obj != OBJ_HANDLE_NULL) {
            // Apply damage to the weapon.
            object_hp_damage_set(combat->weapon_obj,
                object_hp_damage_get(combat->weapon_obj) + dam);

            mes_file_entry.num = 8; // "Weapon damaged"
            if (object_hp_current(combat->weapon_obj) <= 0) {
                object_bust(combat->weapon_obj, combat->attacker_obj);
                mes_file_entry.num = 23; // "Weapon broken"
                pc_switch_weapon(combat->attacker_obj, combat->target_obj);
            }

            mes_get_msg(combat_mes_file, &mes_file_entry);
            strcpy(str, mes_file_entry.str);
        } else {
            if (tf_level_get() == TF_LEVEL_VERBOSE) {
                mes_file_entry.num = 1; // "HT"
                mes_get_msg(combat_mes_file, &mes_file_entry);
                snprintf(str, sizeof(str), "%d %s", dam, mes_file_entry.str);
            }

            // Apply damage to the target.
            object_hp_damage_set(combat->attacker_obj,
                object_hp_damage_get(combat->attacker_obj) + dam);
        }

        if (str[0] != '\0') {
            tf_add(combat->attacker_obj, TF_TYPE_RED, str);
        }
    }
}

// 0x4B5810
void combat_acid_dmg(CombatContext* combat)
{
    int64_t target_obj;
    int inventory_location;
    int64_t item_obj;

    target_obj = combat->target_obj;

    if (obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))) {
        for (inventory_location = FIRST_WEAR_INV_LOC; inventory_location <= LAST_WEAR_INV_LOC; inventory_location++) {
            item_obj = item_wield_get(target_obj, inventory_location);
            if (item_obj != OBJ_HANDLE_NULL) {
                combat->target_obj = item_obj;
                combat_dmg(combat);
            }
        }
    }

    combat->target_obj = target_obj;
    combat->dam[DAMAGE_TYPE_NORMAL] /= 3;
    combat_dmg(combat);
}

// 0x4B58C0
void combat_heal(CombatContext* combat)
{
    int type;
    int index;
    unsigned int critter_flags;
    bool unressurectable = false;
    bool v2 = false;
    tig_art_id_t art_id;

    type = obj_field_int32_get(combat->target_obj, OBJ_F_TYPE);
    if (!obj_type_is_critter(type)) {
        return;
    }

    if ((combat->dam_flags & CDF_SCAR) != 0
        && (combat->dam_flags & CDF_FULL) == 0) {
        for (index = 0; index < 5; index++) {
            effect_remove_one_caused_by(combat->target_obj, EFFECT_CAUSE_INJURY);
        }
    }

    if (critter_is_dead(combat->target_obj)) {
        if ((combat->dam_flags & CDF_RESURRECT) == 0) {
            return;
        }

        critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        if ((critter_flags & OCF_UNREVIVIFIABLE) != 0
            && (combat->dam_flags & CDF_REANIMATE) != 0) {
            unressurectable = true;
        }
        if ((critter_flags & OCF_UNRESSURECTABLE) != 0
            && (combat->dam_flags & CDF_REANIMATE) == 0) {
            unressurectable = true;
        }

        if (!object_script_execute(combat->attacker_obj, combat->target_obj, combat->weapon_obj, SAP_RESURRECT, 0) || unressurectable) {
            magictech_error_unressurectable(combat->target_obj);
            return;
        }

        art_id = obj_field_int32_get(combat->target_obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(combat->target_obj, art_id);

        v2 = true;

        combat_remove_blood_splotch(combat->target_loc);
        ai_stop_fleeing(combat->target_obj);
        object_flags_unset(combat->target_obj, OF_FLAT | OF_NO_BLOCK);

        critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        critter_flags &= ~OCF_STUNNED;
        obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);

        critter_decay_timeevent_cancel(combat->target_obj);

        if (critter_fatigue_damage_get(combat->target_obj) != 10) {
            critter_fatigue_damage_set(combat->target_obj, 10);
        }

        magictech_interrupt_runs_for_obj(combat->target_obj);

        if (combat->attacker_obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(combat->attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
            int reaction_level = reaction_get(combat->target_obj, combat->attacker_obj);
            if (reaction_level > 0 && reaction_level < 70) {
                reaction_adj(combat->target_obj, combat->attacker_obj, 70 - reaction_level);
            }
        }

        if (type == OBJ_TYPE_NPC) {
            int64_t leader_obj = critter_leader_get(combat->target_obj);
            if (leader_obj != OBJ_HANDLE_NULL) {
                if (!critter_follow(combat->target_obj, leader_obj, false)) {
                    critter_leader_set(combat->target_obj, OBJ_HANDLE_NULL);
                }
            }
        }
    }

    if ((combat->dam_flags & CDF_FULL) != 0) {
        combat->dam[DAMAGE_TYPE_NORMAL] = object_hp_damage_get(combat->target_obj);
        if (combat->dam[DAMAGE_TYPE_FATIGUE] > 0) {
            combat->dam[DAMAGE_TYPE_FATIGUE] = critter_fatigue_damage_get(combat->target_obj);
        }

        for (index = 0; index < 5; index++) {
            effect_remove_one_caused_by(combat->target_obj, EFFECT_CAUSE_INJURY);
        }

        critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        if ((critter_flags & OCF_BLINDED) != 0) {
            critter_flags &= ~OCF_BLINDED;
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
        if ((critter_flags & OCF_CRIPPLED_ARMS_ONE) != 0) {
            critter_flags &= ~OCF_CRIPPLED_ARMS_ONE;
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
        if ((critter_flags & OCF_CRIPPLED_ARMS_BOTH) != 0) {
            critter_flags &= ~OCF_CRIPPLED_ARMS_BOTH;
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
        if ((critter_flags & OCF_CRIPPLED_LEGS_BOTH) != 0) {
            critter_flags &= ~OCF_CRIPPLED_LEGS_BOTH;
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
    } else {
        critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        if ((combat->dam_flags & CDF_BLIND) != 0
            && (critter_flags & OCF_BLINDED) != 0) {
            critter_flags &= ~OCF_BLINDED;
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
        if ((combat->dam_flags & CDF_CRIPPLE_ARM) != 0
            && (critter_flags & (OCF_CRIPPLED_ARMS_ONE | OCF_CRIPPLED_ARMS_BOTH)) != 0) {
            critter_flags &= ~(OCF_CRIPPLED_ARMS_ONE | OCF_CRIPPLED_ARMS_BOTH);
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
        if ((combat->dam_flags & CDF_CRIPPLE_LEG) != 0
            && (critter_flags & OCF_CRIPPLED_LEGS_BOTH) != 0) {
            critter_flags &= ~OCF_BLINDED;
            obj_field_int32_set(combat->target_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
    }

    if (combat->dam[DAMAGE_TYPE_NORMAL] > 0) {
        int new_dam;
        int cur_dam;

        cur_dam = object_hp_damage_get(combat->target_obj);
        if (cur_dam != 0) {
            new_dam = cur_dam - combat->dam[DAMAGE_TYPE_NORMAL];
            if (new_dam < 0) {
                new_dam = 0;
            }
            object_hp_damage_set(combat->target_obj, new_dam);
        }
    }

    if (combat->dam[DAMAGE_TYPE_FATIGUE] > 0) {
        int new_dam;
        int cur_dam;
        int fatigue;

        cur_dam = critter_fatigue_damage_get(combat->target_obj);
        if (cur_dam != 0) {
            fatigue = critter_fatigue_current(combat->target_obj);
            new_dam = cur_dam - combat->dam[DAMAGE_TYPE_FATIGUE];
            if (new_dam < 0) {
                new_dam = 0;
            }
            critter_fatigue_damage_set(combat->target_obj, new_dam);

            if (fatigue <= 0 && critter_fatigue_current(combat->target_obj) > 0) {
                anim_goal_get_up(combat->target_obj);
            }
        }
    }

    if (combat->dam[DAMAGE_TYPE_POISON] > 0) {
        int new_dam;
        int cur_dam;

        cur_dam = stat_base_get(combat->target_obj, STAT_POISON_LEVEL);
        if (cur_dam != 0) {
            new_dam = cur_dam - combat->dam[DAMAGE_TYPE_POISON];
            if (new_dam < 0) {
                new_dam = 0;
            }
            stat_base_set(combat->target_obj, STAT_POISON_LEVEL, new_dam);
        }
    }

    if (v2 && type != OBJ_TYPE_PC) {
        ai_timeevent_schedule(combat->target_obj);
    }

    if (type == OBJ_TYPE_NPC) {
        if (critter_pc_leader_get(combat->target_obj) == player_get_local_pc_obj()) {
            ui_follower_update();
        }
    }

    combat_recalc_reaction(combat->target_obj);
}

// 0x4B5E90
void combat_remove_blood_splotch(int64_t loc)
{
    ObjectList objects;
    ObjectNode* node;
    tig_art_id_t art_id;
    int64_t obj;

    object_list_location(loc, OBJ_TM_SCENERY, &objects);

    node = objects.head;
    while (node != NULL) {
        art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
        if (tig_art_scenery_id_type_get(art_id) == TIG_ART_SCENERY_TYPE_MISC_SMALL
            && tig_art_num_get(art_id) == 0) {
            obj = node->obj;
            object_list_destroy(&objects);
            critter_decay_timeevent_cancel(obj);
            object_destroy(obj);
            return;
        }
        node = node->next;
    }

    object_list_destroy(&objects);
}

// 0x4B5F30
int combat_hit_loc_penalty(int hit_loc)
{
    return hit_loc_penalties[hit_loc];
}

// CE (criticals, milestone CR-3): what the critical being processed added as
// Winded, and the normal damage that was taken from, for `critical.rolled`;
// 0 and 0 for none. Set in `combat_process_crit_hit`.
static int combat_observe_winded_fatigue;
static int combat_observe_winded_from;

// CE (criticals, milestone CR-0): `critical.rolled` for one critical, with the
// effects this roll added. A fumble writes its miss and then the hit it deals
// its own wielder, so a hit reports only what its own pass set. `resisted` is
// what CR-1's protection skipped, `temporary` CR-2's effects, and `outcome`
// what a miss did (CR-4.0; NULL for a hit).
static void combat_observe_critical(CombatContext* combat, bool hit, int effect_bonus,
    bool player_spared, int weapon_chart, int body_chart, unsigned int effects,
    unsigned int resisted, unsigned int temporary, const char* outcome)
{
    char attacker[COMBAT_OBSERVER_IDENTITY_SIZE];
    char target[COMBAT_OBSERVER_IDENTITY_SIZE];

    if (!combat_observer_is_enabled()) {
        return;
    }

    combat_observer_object_id(combat->attacker_obj, attacker);
    combat_observer_object_id(combat->target_obj, target);
    combat_observer_critical_rolled(attacker, target, hit, combat->hit_loc,
        effect_bonus, (combat->flags & CF_AIM) != 0, player_spared,
        weapon_chart, body_chart, effects, resisted,
        obj_type_is_critter(obj_field_int32_get(combat->target_obj, OBJ_F_TYPE))
            && crit_guard_is_down(combat->target_obj),
        temporary, outcome,
        hit ? combat_observe_winded_fatigue : 0,
        hit ? combat_observe_winded_from : 0);
}

// 0x4B5F40
void combat_process_crit_hit(CombatContext* combat)
{
    int target_obj_type;
    int attacker_obj_type;
    bool npc_attacks_pc;
    CritHitType crit_hit_type;
    int chance;
    CritBodyType crit_body_type;
    unsigned int critter_flags;
    int64_t helmet_obj;
    int difficulty;
    unsigned int flags_before;
    unsigned int resisted;
    unsigned int temporary = 0;

    if (combat->target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (combat->attacker_obj == OBJ_HANDLE_NULL) {
        return;
    }

    target_obj_type = obj_field_int32_get(combat->target_obj, OBJ_F_TYPE);
    attacker_obj_type = obj_field_int32_get(combat->attacker_obj, OBJ_F_TYPE);

    // Make sure it's a clash between critters.
    if (!obj_type_is_critter(target_obj_type)
        || !obj_type_is_critter(attacker_obj_type)) {
        return;
    }

    npc_attacks_pc = target_obj_type == OBJ_TYPE_PC
        && attacker_obj_type == OBJ_TYPE_NPC;

    // CE (criticals, milestone CR-0): what was already set, so the event
    // reports only this pass's effects.
    flags_before = combat->dam_flags;
    // CE (criticals, milestone CR-3): and no Winded until this pass adds one.
    combat_observe_winded_fatigue = 0;
    combat_observe_winded_from = 0;

    crit_body_type = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_CRIT_HIT_CHART);

    if (combat->weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
        crit_hit_type = obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_CRIT_HIT_CHART);
        if ((combat->flags & 0x20000) == 0) {
            // CE (turn-based rebalance): a weapon's `rules/weapons.mes`
            // CritEffect replaces its critical-effect bonus, unscaled by
            // the wielder's aptitude (zsogg, 2026-10-05); without one, the
            // stock bonus as stock scales it.
            chance = weapon_rules_crit_effect_for(combat->weapon_obj,
                combat->attacker_obj);
        }
    } else {
        crit_hit_type = CRIT_HIT_TYPE_CRUSHING;
        chance = 0;
    }

    chance = effect_adjust_crit_hit_effect(combat->attacker_obj, chance);

    // CE (criticals, milestone CR-1): the test instrument's other half. Under
    // `debug criticals` a forced critical also rolls its effects 60 points
    // more easily, so incapacitations happen often enough to measure - unless
    // its value adds 4 (CR-3.0), which compares weapons' own bonuses.
    if (crit_guard_debug_boosts(combat->attacker_obj, combat->target_obj)) {
        chance += 60;
    }

    if ((combat->flags & CF_AIM) != 0) {
        chance += 10;
    }

    if (!npc_attacks_pc) {
        if (random_between(1, 100) <= chance + 10) {
            combat->dam_flags |= CDF_BONUS_DAM_200;
        } else if (random_between(1, 100) <= chance + 30) {
            combat->dam_flags |= CDF_BONUS_DAM_100;
        } else if (random_between(1, 100) <= chance + 60) {
            combat->dam_flags |= CDF_BONUS_DAM_50;
        }
    }

    if (crit_body_type != CRIT_BODY_TYPE_AMORPHOUS) {
        critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        if ((critter_flags & OCF_UNDEAD) == 0) {
            helmet_obj = item_wield_get(combat->target_obj, ITEM_INV_LOC_HELMET);

            difficulty = chance + 5;

            if (crit_hit_type == CRIT_HIT_TYPE_CRUSHING
                || crit_hit_type == CRIT_HIT_TYPE_ELECTRICAL) {
                difficulty += 10;
            }

            if (combat->hit_loc == HIT_LOC_HEAD && helmet_obj == OBJ_HANDLE_NULL) {
                difficulty += 10;
            }

            if (random_between(1, 100) <= difficulty
                && !critter_check_stat(combat->target_obj, STAT_CONSTITUTION, -5)) {
                combat->dam_flags |= CDF_STUN;

                if (target_obj_type != OBJ_TYPE_PC
                    && (critter_flags & OCF_FATIGUE_IMMUNE) == 0
                    && random_between(1, 100) < difficulty
                    && !critter_check_stat(combat->target_obj, STAT_CONSTITUTION, -5)) {
                    combat->dam_flags |= CDF_KNOCKOUT;
                }
            }

            // NOTE: Not sure how to reimplement it without goto.
            do {
                if (crit_body_type != CRIT_BODY_TYPE_SNAKE) {
                    if (crit_body_type != CRIT_BODY_TYPE_AVIAN && combat->hit_loc == HIT_LOC_ARM) {
                        if (random_between(1, 100) <= chance + 1
                            && !critter_check_stat(combat->target_obj, STAT_CONSTITUTION, -5)) {
                            combat->dam_flags |= CDF_CRIPPLE_ARM;
                        }
                        break;
                    }

                    if (combat->hit_loc == HIT_LOC_LEG) {
                        if (random_between(1, 100) <= chance + 1
                            && !critter_check_stat(combat->target_obj, STAT_CONSTITUTION, -5)) {
                            combat->dam_flags |= CDF_CRIPPLE_LEG;
                        }
                        break;
                    }
                }

                if (combat->hit_loc == HIT_LOC_HEAD) {
                    if (crit_hit_type == CRIT_HIT_TYPE_CRUSHING) {
                        if (helmet_obj != OBJ_HANDLE_NULL
                            && random_between(1, 100) <= chance + 5) {
                            combat->dam_flags |= CDF_DROP_HELMET;
                            break;
                        }
                    }

                    if (!npc_attacks_pc
                        && random_between(1, 100) <= chance + (helmet_obj == OBJ_HANDLE_NULL ? 1 : 0)
                        && !critter_check_stat(combat->target_obj, STAT_CONSTITUTION, 0)) {
                        combat->dam_flags |= CDF_BLIND;
                        break;
                    }
                }
            } while (0);

            if (target_obj_type == OBJ_TYPE_PC
                && ((combat->flags & 0x8) == 0
                    || effect_count_effects_of_type(combat->target_obj, EFFECT_SCARRING) <= 0)
                && random_between(1, 100) <= chance + 5) {
                combat->dam_flags |= CDF_SCAR;
            }
        }

        if (crit_body_type == CRIT_BODY_TYPE_BIPED) {
            difficulty = chance + 5;

            if (crit_hit_type == CRIT_HIT_TYPE_CRUSHING) {
                difficulty += 5;
            }

            if (combat->hit_loc == HIT_LOC_LEG) {
                difficulty += 10;
            }

            // CE (criticals, milestone CR-3): an Impact weapon's critical
            // knocks down more easily, under `firearm criticals`.
            if (weapon_rules_crit_style(combat->weapon_obj) == WEAPON_CRIT_STYLE_IMPACT) {
                difficulty += 10;
            }

            if (random_between(1, 100) <= difficulty) {
                combat->dam_flags |= CDF_KNOCKDOWN;
            }
        }
    }

    if (combat_critter_weapon(combat->target_obj) != OBJ_HANDLE_NULL) {
        if (random_between(1, 100) <= chance + 10) {
            combat->dam_flags |= CDF_DAMAGE_WEAPON;
        }

        if (!npc_attacks_pc
            && random_between(1, 100) <= chance + 10) {
            combat->dam_flags |= CDF_DROP_WEAPON;
        }
    }

    if (combat_critter_armor(combat->target_obj, combat->hit_loc) != OBJ_HANDLE_NULL
        && random_between(1, 100) <= chance + 10) {
        combat->dam_flags |= CDF_DAMAGE_ARMOR;
    }

    // CE (criticals, milestone CR-2): under `critical effects`, the
    // temporary effect of where the critical struck, and the stock injuries
    // made mostly temporary: a crippling or blinding leaves its temporary
    // version and stays permanent only on a second roll, a scar likewise, and
    // a fumble's hit on its wielder (0x8) never scars. Undead and amorphous
    // creatures are spared, as from the stock bodily effects.
    if (feature_enabled(FEATURE_CRITICAL_EFFECTS)
        && crit_body_type != CRIT_BODY_TYPE_AMORPHOUS
        && (obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS) & OCF_UNDEAD) == 0) {
        unsigned int added = combat->dam_flags & ~flags_before;

        temporary = crit_effects_for_location(combat->hit_loc);
        if ((added & CDF_CRIPPLE_ARM) != 0
            && random_between(1, 100) > CRIT_EFFECT_PERMANENT_CHANCE) {
            combat->dam_flags &= ~CDF_CRIPPLE_ARM;
            temporary |= CRIT_EFFECT_NUMBED_ARM;
        }
        if ((added & CDF_CRIPPLE_LEG) != 0
            && random_between(1, 100) > CRIT_EFFECT_PERMANENT_CHANCE) {
            combat->dam_flags &= ~CDF_CRIPPLE_LEG;
            temporary |= CRIT_EFFECT_HOBBLED;
        }
        if ((added & CDF_BLIND) != 0
            && random_between(1, 100) > CRIT_EFFECT_PERMANENT_CHANCE) {
            combat->dam_flags &= ~CDF_BLIND;
            temporary |= CRIT_EFFECT_DAZZLED;
        }
        if ((added & CDF_SCAR) != 0
            && ((combat->flags & 0x8) != 0
                || random_between(1, 100) > CRIT_EFFECT_PERMANENT_CHANCE)) {
            combat->dam_flags &= ~CDF_SCAR;
        }
        if ((temporary & CRIT_EFFECT_WINDED) != 0
            && (obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS) & OCF_FATIGUE_IMMUNE) == 0) {
            // CE (criticals, milestone CR-3): an Impact weapon's Winded takes
            // the hit's whole damage as fatigue, under `firearm criticals`.
            combat_observe_winded_from = combat->dam[DAMAGE_TYPE_NORMAL];
            combat_observe_winded_fatigue = weapon_rules_crit_style(combat->weapon_obj) == WEAPON_CRIT_STYLE_IMPACT
                ? combat->dam[DAMAGE_TYPE_NORMAL]
                : combat->dam[DAMAGE_TYPE_NORMAL] / 2;
            combat->dam[DAMAGE_TYPE_FATIGUE] += combat_observe_winded_fatigue;
        } else {
            temporary &= ~CRIT_EFFECT_WINDED;
        }
        crit_effects_apply(combat->target_obj, temporary);
    }

    if (combat->dam_flags == 0) {
        combat->dam_flags = CDF_BONUS_DAM_50;
    }

    // CE (criticals, milestone CR-1): under `critical protection`, a target
    // that has recovered from a critical's incapacitation resists another
    // until its protection ends. The critical still lands; only the stun,
    // knockdown or knockout is skipped, after every roll, so no draw changes.
    resisted = crit_guard_filter(combat->target_obj, combat->dam_flags & ~flags_before);
    combat->dam_flags &= ~resisted;

    combat_observe_critical(combat, true, chance, npc_attacks_pc,
        crit_hit_type, crit_body_type, combat->dam_flags & ~flags_before, resisted,
        temporary, NULL);
}

// 0x4B6410
void combat_process_crit_miss(CombatContext* combat)
{
    CritMissType crit_miss_type;
    int chance;

    if (combat->weapon_obj != OBJ_HANDLE_NULL) {
        if (obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
            crit_miss_type = obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_CRIT_MISS_CHART);
            if ((combat->flags & 0x20000) == 0) {
                chance = item_adjust_magic(combat->weapon_obj,
                    combat->attacker_obj,
                    obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_MAGIC_CRIT_MISS_EFFECT));
            } else {
                chance = 0;
            }
        } else {
            crit_miss_type = CRIT_MISS_TYPE_BLUDGEON;
            chance = 0;
        }
    } else {
        crit_miss_type = CRIT_MISS_TYPE_HANDS;
        chance = 0;
    }

    chance = effect_adjust_crit_fail_effect(combat->attacker_obj, chance);

    // CE (criticals, milestones CR-4.1 and CR-4.2): under `firearm fumbles`
    // anyone's fumble with a gun reloading governs jams the gun half the time
    // and does nothing 30% of the time; either way the attack is an ordinary
    // miss. The remaining fifth hits its wielder below, tamed (zsogg,
    // 2026-10-06). Stock turns every fumble on its wielder (MODDING-NOTES
    // quirk 59).
    bool firearm_fumble = reload_jams_apply(combat->attacker_obj, combat->weapon_obj);
    if (firearm_fumble) {
        ReloadFumble fumble = reload_fumble_outcome(random_between(1, 100),
            tech_skill_training_get(combat->attacker_obj, TECH_SKILL_FIREARMS));
        if (fumble != RELOAD_FUMBLE_SELF_HIT) {
            combat->flags &= ~CF_CRITICAL;
            if (fumble == RELOAD_FUMBLE_JAM) {
                reload_jam(combat->attacker_obj, combat->weapon_obj);
            }
            combat_observe_critical(combat, false, chance, false, crit_miss_type, -1, 0, 0, 0,
                fumble == RELOAD_FUMBLE_JAM ? "jam" : "nothing");
            return;
        }
    }

    combat->flags &= ~CF_AIM;
    combat->flags &= ~CF_CRITICAL;
    combat->flags |= 0x8;
    combat->flags |= CF_HIT;

    combat->target_obj = combat->attacker_obj;
    combat->target_loc = obj_field_int64_get(combat->target_obj, OBJ_F_LOCATION);

    if (random_between(1, 100) <= 50) {
        combat->flags |= CF_CRITICAL;
        if (crit_miss_type != CRIT_MISS_TYPE_HANDS
            && random_between(1, 100) <= chance + 1) {
            combat->dam_flags |= CDF_DESTROY_WEAPON;
        }

        if ((crit_miss_type == CRIT_MISS_TYPE_EXPLOSIVE
                || crit_miss_type == CRIT_MISS_TYPE_FIRE
                || crit_miss_type == CRIT_MISS_TYPE_ELECTRICAL)
            && random_between(1, 100) <= chance + 1) {
            combat->dam_flags |= CDF_EXPLODE_WEAPON;
        }

        if (item_weapon_ammo_type(combat->weapon_obj) != 10000) {
            if (random_between(1, 100) <= chance + 1) {
                // CE (criticals, milestone CR-4.1): under `firearm fumbles`
                // such a fumble loses 5 shots, never all the spare
                // ammunition (zsogg, 2026-10-06).
                combat->dam_flags |= firearm_fumble ? CDF_LOST_AMMO : CDF_DESTROY_AMMO;
            } else if (random_between(1, 100) <= chance + 11) {
                combat->dam_flags |= CDF_LOST_AMMO;
            }
        }

        // CE (criticals, milestone CR-0): the miss, then the hit it deals
        // its own wielder.
        combat_observe_critical(combat, false, chance, false, crit_miss_type, -1,
            combat->dam_flags, 0, 0, "critical_self_hit");
        combat_process_crit_hit(combat);
    } else {
        // CE (criticals, milestone CR-0): the other half. The flags above
        // already made the attack a hit on its own wielder, so this is an
        // ordinary, non-critical self-hit, as in the original (verified at
        // 0x4B64AB-0x4B659B, 2026-10-06; MODDING-NOTES quirk 59).
        combat_observe_critical(combat, false, chance, false, crit_miss_type, -1, 0, 0, 0,
            "self_hit");
    }
}

// 0x4B65A0
int combat_random_hit_loc(void)
{
    int value = random_between(1, 100);
    if (value <= 70) return HIT_LOC_TORSO;
    if (value <= 85) return HIT_LOC_LEG;
    if (value <= 95) return HIT_LOC_ARM;
    return HIT_LOC_HEAD;
}

// 0x4B65D0
bool combat_consume_ammo(int64_t weapon_obj, int64_t critter_obj, int cnt, bool destroy)
{
    int ammo_type;
    int qty;
    int consumption;

    if (obj_field_int32_get(critter_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC
        && critter_pc_leader_get(critter_obj) == OBJ_HANDLE_NULL) {
        return true;
    }

    ammo_type = item_weapon_ammo_type(weapon_obj);
    if (ammo_type == 10000) {
        return true;
    }

    qty = item_ammo_quantity_get(critter_obj, ammo_type);

    if (destroy) {
        item_ammo_transfer(critter_obj, OBJ_HANDLE_NULL, qty, ammo_type, OBJ_HANDLE_NULL);
        return true;
    }

    // CE (turn-based rebalance): `rules/weapons.mes` AmmoPerShot.
    consumption = cnt * weapon_rules_ammo_per_shot(weapon_obj);
    if (qty >= consumption) {
        item_ammo_transfer(critter_obj, OBJ_HANDLE_NULL, consumption, ammo_type, OBJ_HANDLE_NULL);
        // CE (real-time combat session 1): charged once per what? The session
        // exists to answer that, and it cannot be answered by counting arrows.
        //
        // Note what is NOT observed: the two early returns above. An NPC with
        // no PC leader leaves before reaching here and spends nothing, which
        // is the barrage the investigation describes - so a trace showing
        // releases with no matching charge is that path, not a lost event.
        if (combat_observer_is_enabled()) {
            char shooter[COMBAT_OBSERVER_IDENTITY_SIZE];
            combat_observer_object_id(critter_obj, shooter);
            combat_observer_resource_charged(shooter, "ammo", -1, consumption,
                qty, item_ammo_quantity_get(critter_obj, ammo_type));
        }
        return true;
    }

    return false;
}

// 0x4B6680
void combat_calc_dmg(CombatContext* combat)
{
    unsigned int critter_flags = 0;
    unsigned int critter_flags2 = 0;
    unsigned int spell_flags = 0;
    DamageType damage_type;
    int min_damage;
    int max_damage;
    int damage;

    if ((combat->flags & 0x100) != 0) {
        return;
    }

    if (obj_type_is_critter(obj_field_int32_get(combat->target_obj, OBJ_F_TYPE))) {
        critter_flags = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS);
        critter_flags2 = obj_field_int32_get(combat->target_obj, OBJ_F_CRITTER_FLAGS2);
        spell_flags = obj_field_int32_get(combat->target_obj, OBJ_F_SPELL_FLAGS);
    }

    for (damage_type = 0; damage_type < DAMAGE_TYPE_COUNT; damage_type++) {
        item_weapon_damage(combat->weapon_obj,
            combat->attacker_obj,
            damage_type,
            combat->skill,
            (combat->flags & 0x20000) != 0,
            &min_damage,
            &max_damage);

        if (damage_type == DAMAGE_TYPE_POISON
            && ((critter_flags & (OCF_UNDEAD | OCF_MECHANICAL)) != 0
                || (spell_flags & OSF_STONED) != 0)) {
            damage = 0;
        } else {
            damage = random_between(min_damage, max_damage);

            switch (damage_type) {
            case DAMAGE_TYPE_NORMAL:
                if ((combat->flags & 0x8000) != 0) {
                    damage += basic_skill_level(combat->attacker_obj, BASIC_SKILL_BACKSTAB) * 5;
                } else if ((combat->flags & 0x4000) != 0) {
                    damage += basic_skill_level(combat->attacker_obj, BASIC_SKILL_BACKSTAB);
                }
                break;
            case DAMAGE_TYPE_FATIGUE:
                if ((critter_flags2 & OCF2_FATIGUE_DRAINING) != 0 && damage > 0) {
                    damage *= 4;
                }
                if ((critter_flags & OCF_FATIGUE_LIMITING) != 0 && damage > 0) {
                    damage /= 4;
                }
                break;
            default:
                break;
            }
        }

        combat->dam[damage_type] = damage;
    }

    spell_flags = obj_field_int32_get(combat->attacker_obj, OBJ_F_SPELL_FLAGS);
    if (combat->weapon_obj == OBJ_HANDLE_NULL) {
        if ((spell_flags & OSF_BODY_OF_EARTH) != 0) {
            combat->dam[DAMAGE_TYPE_NORMAL] += 5;
        } else if ((spell_flags & OSF_BODY_OF_FIRE) != 0) {
            combat->dam[DAMAGE_TYPE_FIRE] += 15;
        } else if ((spell_flags & OSF_BODY_OF_WATER) != 0) {
            combat->dam[DAMAGE_TYPE_FATIGUE] += 15;
        } else if ((spell_flags & OSF_HARDENED_HANDS) != 0) {
            combat->dam[DAMAGE_TYPE_NORMAL] += 2;
        }
    }

    // CE (criticals, milestone CR-4.1): under `firearm fumbles` a firearm
    // fumble that hits its wielder (0x8) deals half its damage,
    // critical or not (zsogg, 2026-10-06).
    if ((combat->flags & 0x8) != 0
        && combat->target_obj == combat->attacker_obj
        && reload_jams_apply(combat->attacker_obj, combat->weapon_obj)) {
        for (damage_type = 0; damage_type < DAMAGE_TYPE_COUNT; damage_type++) {
            combat->dam[damage_type] /= 2;
        }
    }
}

// 0x4B6860
// CE (turn-based rebalance, armor milestone 3): the armor rules
// (`penetration.h`) for one weapon hit's normal damage, under `armor
// penetration`. Returns the normal damage that gets through - with the
// player's difficulty scaling and the critical bonus applied first, as
// `combat_dmg` applies them to the rest - or -1 when the rules do not apply
// and the stock path stands:
//
//   - only a connected weapon hit on a critter: not a spell (0x80), a forced
//     or struck-bystander hit (0x100), a trap, a full or killing blow, or a
//     weapon or hit that ignores resistance;
//   - only when a penetration is known: the weapon table's, else the default
//     for the kind of attack (`armor_rules.h`).
//
// Worn protection comes from the armor table, a piece it does not list
// keeping its stock normal resistance; natural protection is the creature's
// own normal resistance and innate effects, scaled for the undead by the
// table's undead entry; magical protection is the worn pieces' enchantments,
// rings' and the amulet's resistance, and every other effect (zsogg's
// decisions 7 and 8). Fatigue and the elemental damage keep stock resistance
// (decision 4); a monster's natural attack turns the table's share of what
// armor stopped into fatigue.
static int combat_armor_piece(int64_t critter_obj, int inventory_location)
{
    int64_t item_obj;
    int protection;

    item_obj = item_wield_get(critter_obj, inventory_location);
    if (item_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_ARMOR) {
        return 0;
    }
    if (armor_rules_protection(item_obj, &protection)) {
        return protection;
    }
    return obj_arrayfield_int32_get(item_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_NORMAL);
}

static int combat_armor_magic(int64_t critter_obj)
{
    int magic;
    int innate;
    int inventory_location;
    int64_t item_obj;

    effect_resistance_split(critter_obj, RESISTANCE_TYPE_NORMAL, &innate, &magic);

    for (inventory_location = FIRST_WEAR_INV_LOC; inventory_location <= LAST_WEAR_INV_LOC; inventory_location++) {
        item_obj = item_wield_get(critter_obj, inventory_location);
        if (item_obj == OBJ_HANDLE_NULL
            || obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_ARMOR) {
            continue;
        }
        magic += item_adjust_magic(item_obj, critter_obj,
            obj_arrayfield_int32_get(item_obj, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_NORMAL));
        if (inventory_location == ITEM_INV_LOC_RING1
            || inventory_location == ITEM_INV_LOC_RING2
            || inventory_location == ITEM_INV_LOC_MEDALLION) {
            magic += obj_arrayfield_int32_get(item_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_NORMAL);
        }
    }

    return magic;
}

static bool combat_armor_weapon_penetration(int64_t weapon_obj, bool thrown, int* penetration)
{
    int skill;

    if (weapon_rules_penetration(weapon_obj, penetration)) {
        return true;
    }

    if (thrown) {
        *penetration = armor_rules_default_penetration(ARMOR_RULE_KIND_THROWN);
    } else if (weapon_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(weapon_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON) {
        *penetration = armor_rules_default_penetration(ARMOR_RULE_KIND_NATURAL);
    } else {
        skill = item_weapon_skill(weapon_obj);
        if (skill == SKILL_BOW) {
            *penetration = armor_rules_default_penetration(ARMOR_RULE_KIND_BOW);
        } else if (skill == SKILL_FIREARMS) {
            *penetration = armor_rules_default_penetration(ARMOR_RULE_KIND_FIREARM);
        } else {
            *penetration = armor_rules_default_penetration(ARMOR_RULE_KIND_MELEE);
        }
    }

    return *penetration != ARMOR_RULE_UNSET;
}

static bool combat_armor_penetration(CombatContext* combat, int* penetration)
{
    return combat_armor_weapon_penetration(combat->weapon_obj, (combat->flags & 0x40) != 0, penetration);
}

// The critter's natural protection: its own normal resistance and innate
// effects, the undead's share of it under the table's undead entry.
static int combat_armor_natural(int64_t critter_obj)
{
    int innate;
    int unused;
    int natural;
    int undead_scale;

    effect_resistance_split(critter_obj, RESISTANCE_TYPE_NORMAL, &innate, &unused);
    natural = obj_arrayfield_int32_get(critter_obj, OBJ_F_RESISTANCE_IDX, RESISTANCE_TYPE_NORMAL) + innate;
    // The undead keep a share of it (zsogg, 2026-10-03), to the nearest point.
    undead_scale = armor_rules_undead_natural_scale();
    if (undead_scale != ARMOR_RULE_UNSET
        && natural > 0
        && (obj_field_int32_get(critter_obj, OBJ_F_CRITTER_FLAGS) & OCF_UNDEAD) != 0) {
        natural = (natural * undead_scale + 50) / 100;
    }
    return natural;
}

int combat_armor_expected_permille(int64_t weapon_obj, int64_t target_obj)
{
    PenetrationHit hit = { 0 };
    int64_t shield_obj;
    int block_chance = 0;
    int penetration;

    if (!feature_enabled(FEATURE_ARMOR_PENETRATION)
        || target_obj == OBJ_HANDLE_NULL
        || !obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))) {
        return -1;
    }
    if (weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
        && (obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_IGNORE_RESISTANCE) != 0) {
        return 1000;
    }
    if (!combat_armor_weapon_penetration(weapon_obj, false, &penetration)) {
        return -1;
    }

    hit.worn.helmet = combat_armor_piece(target_obj, ITEM_INV_LOC_HELMET);
    hit.worn.body = combat_armor_piece(target_obj, ITEM_INV_LOC_ARMOR);
    hit.worn.gloves = combat_armor_piece(target_obj, ITEM_INV_LOC_GAUNTLET);
    hit.worn.boots = combat_armor_piece(target_obj, ITEM_INV_LOC_BOOTS);
    hit.natural = combat_armor_natural(target_obj);
    hit.magic = combat_armor_magic(target_obj);
    hit.penetration = penetration;

    shield_obj = item_wield_get(target_obj, ITEM_INV_LOC_SHIELD);
    if (shield_obj != OBJ_HANDLE_NULL
        && armor_rules_block_chance(shield_obj, &block_chance)) {
        armor_rules_protection(shield_obj, &hit.shield);
    } else {
        block_chance = 0;
    }

    return penetration_expected_permille(&hit, block_chance);
}

static int combat_armor_resolve(CombatContext* combat)
{
    PenetrationHit hit = { 0 };
    PenetrationResult result;
    int64_t shield_obj;
    int block_chance;
    int stopped_fatigue;
    int fatigue_added = 0;
    int crit_bonus;
    int damage;
    char attacker[COMBAT_OBSERVER_IDENTITY_SIZE] = "NULL";
    char target[COMBAT_OBSERVER_IDENTITY_SIZE] = "NULL";

    if (!feature_enabled(FEATURE_ARMOR_PENETRATION)
        || !obj_type_is_critter(obj_field_int32_get(combat->target_obj, OBJ_F_TYPE))
        || (combat->flags & CF_HIT) == 0
        || (combat->flags & (0x80 | 0x100 | CF_TRAP)) != 0
        || (combat->dam_flags & (CDF_FULL | CDF_DEATH | CDF_IGNORE_RESISTANCE)) != 0
        || combat->dam[DAMAGE_TYPE_NORMAL] <= 0) {
        return -1;
    }
    if (combat->weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
        && (obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_IGNORE_RESISTANCE) != 0) {
        return -1;
    }
    if (!combat_armor_penetration(combat, &hit.penetration)) {
        return -1;
    }

    // The normal damage as `combat_dmg` scales the rest: the player's
    // difficulty, then the critical bonus.
    damage = combat->dam[DAMAGE_TYPE_NORMAL];
    if (player_is_pc_obj(combat->attacker_obj)) {
        switch (gamelib_game_difficulty_get()) {
        case GAME_DIFFICULTY_EASY:
            damage += damage / 2;
            break;
        case GAME_DIFFICULTY_HARD:
            damage -= damage / 4;
            break;
        }
    }
    if ((combat->dam_flags & CDF_BONUS_DAM_200) != 0) {
        damage *= 3;
    } else if ((combat->dam_flags & CDF_BONUS_DAM_100) != 0) {
        damage *= 2;
    } else if ((combat->dam_flags & CDF_BONUS_DAM_50) != 0) {
        damage += damage / 2;
    }

    hit.hit_loc = combat->hit_loc;
    hit.worn.helmet = combat_armor_piece(combat->target_obj, ITEM_INV_LOC_HELMET);
    hit.worn.body = combat_armor_piece(combat->target_obj, ITEM_INV_LOC_ARMOR);
    hit.worn.gloves = combat_armor_piece(combat->target_obj, ITEM_INV_LOC_GAUNTLET);
    hit.worn.boots = combat_armor_piece(combat->target_obj, ITEM_INV_LOC_BOOTS);
    hit.natural = combat_armor_natural(combat->target_obj);
    hit.magic = combat_armor_magic(combat->target_obj);

    // A shield blocks on its own chance, before any gap.
    shield_obj = item_wield_get(combat->target_obj, ITEM_INV_LOC_SHIELD);
    if (shield_obj != OBJ_HANDLE_NULL
        && armor_rules_block_chance(shield_obj, &block_chance)
        && random_between(1, 100) <= block_chance) {
        hit.blocked = true;
        armor_rules_protection(shield_obj, &hit.shield);
    }

    // An unblocked critical finds a gap on 50%, the weapon's critical bonus,
    // and the aiming bonus.
    if (!hit.blocked && (combat->flags & CF_CRITICAL) != 0) {
        crit_bonus = 0;
        if (combat->weapon_obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
            && (combat->flags & 0x20000) == 0) {
            crit_bonus = weapon_rules_crit_effect_for(combat->weapon_obj,
                combat->attacker_obj);
        }
        hit.gap = random_between(1, 100)
            <= penetration_gap_chance(crit_bonus, combat->hit_loc, (combat->flags & CF_AIM) != 0);
        // CE (criticals, milestone CR-3): a Precise weapon's critical always
        // finds the gap, under `firearm criticals`. The roll is still drawn.
        if (weapon_rules_crit_style(combat->weapon_obj) == WEAPON_CRIT_STYLE_PRECISE) {
            hit.gap = true;
        }
    }

    hit.variation_percent = random_between(PENETRATION_VARIATION_MIN, PENETRATION_VARIATION_MAX);
    hit.damage = damage;
    result = penetration_resolve(&hit);

    // A monster's natural attack: part of what armor stopped lands as fatigue
    // (zsogg, 2026-10-04: monsters still threaten a player in plate). Added
    // before resistance, so fatigue's stock resistance applies to it.
    stopped_fatigue = armor_rules_stopped_to_fatigue();
    if (stopped_fatigue != ARMOR_RULE_UNSET
        && damage > result.damage
        && (combat->flags & 0x40) == 0
        && (combat->weapon_obj == OBJ_HANDLE_NULL
            || obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON)
        && critter_is_monstrous(combat->attacker_obj)) {
        fatigue_added = ((damage - result.damage) * stopped_fatigue + 50) / 100;
        combat->dam[DAMAGE_TYPE_FATIGUE] += fatigue_added;
    }

    if (combat_observer_is_enabled()) {
        combat_observer_object_id(combat->attacker_obj, attacker);
        combat_observer_object_id(combat->target_obj, target);
        combat_observer_armor_resolved(attacker, target, hit.hit_loc,
            penetration_location_protection_centi(hit.hit_loc, &(hit.worn), 0),
            hit.natural, hit.magic, hit.blocked, hit.gap, hit.penetration,
            hit.variation_percent, result.protection_centi,
            result.penetration_centi, result.passed_num, result.passed_den,
            damage, result.damage, fatigue_added);
    }

    return result.damage;
}

void combat_apply_resistance(CombatContext* combat)
{
    int damage_type;
    int resistance;

    if (combat->target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if ((combat->dam_flags & CDF_IGNORE_RESISTANCE) != 0) {
        return;
    }

    if (combat->weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
        && (obj_field_int32_get(combat->weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_IGNORE_RESISTANCE) != 0) {
        return;
    }

    for (damage_type = 0; damage_type < DAMAGE_TYPE_COUNT; damage_type++) {
        resistance = object_get_resistance(combat->target_obj, combat_damage_to_resistance_tbl[damage_type], false);
        if (damage_type == DAMAGE_TYPE_FATIGUE) {
            resistance = 3 * resistance / 4;
        }
        if (resistance > 0) {
            combat->dam[damage_type] -= resistance * combat->dam[damage_type] / 100;
        }
    }
}

// 0x4B6930
int combat_play_hit_fx(CombatContext* combat)
{
    int blood_splotch_type = BLOOD_SPLOTCH_TYPE_NONE;
    int max_damage = 0;
    int max_damage_type = 0;
    int damage_type;

    if (combat->target_obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(obj_field_int32_get(combat->target_obj, OBJ_F_TYPE))
        && (combat->flags & CF_HIT) != 0) {
        if ((combat->flags & CF_CRITICAL) != 0) {
            blood_splotch_type = BLOOD_SPLOTCH_TYPE_CRITICAL;
        } else {
            for (damage_type = 0; damage_type < 4; damage_type++) {
                if (combat->dam[damage_type] > max_damage) {
                    max_damage = combat->dam[damage_type];
                    max_damage_type = damage_type;
                }
            }

            blood_splotch_type = max_damage_type + 1;

            // NOTE: Verified against 0x4B6930 - original checks blood_splotch_type == 0 (max_damage_type == -1) after initializing it to 0; unreachable but transcription faithful.
            if (max_damage_type == -1) {
                return blood_splotch_type;
            }
        }

        if (!anim_play_weapon_fx(combat, combat->weapon_obj, combat->target_obj, ANIM_WEAPON_EYE_CANDY_TYPE_HIT)) {
            if (max_damage > 0) {
                if ((combat->flags & 0x80) == 0) {
                    anim_play_blood_splotch_fx(combat->target_obj, blood_splotch_type, DAMAGE_TYPE_NORMAL, combat);
                }
            } else {
                magictech_anim_play_hit_fx(combat->target_obj, combat);
            }
        }
    }

    return blood_splotch_type;
}

// 0x4B6A00
int combat_projectile_rot(int64_t from_loc, int64_t to_loc)
{
    int64_t x1;
    int64_t y1;
    int64_t x2;
    int64_t y2;
    double v1;
    int extended_rotation;

    if (from_loc == to_loc) {
        return 0;
    }

    location_xy(from_loc, &x1, &y1);
    location_xy(to_loc, &x2, &y2);

    v1 = atan2((double)(y2 - y1), (double)(x2 - x1));
    if (v1 < 0.0) {
        v1 += M_PI * 2.0;
    }

    extended_rotation = (int)(v1 * 32.0 / (M_PI * 2.0));
    if (extended_rotation < 0 || extended_rotation >= 32) {
        extended_rotation = 0;
    }

    extended_rotation += 8;
    if (extended_rotation >= 32) {
        extended_rotation -= 32;
    }

    return extended_rotation;
}

// 0x4B6B10
tig_art_id_t combat_projectile_art_id_rotation_set(tig_art_id_t aid, int projectile_rot)
{
    if (tig_art_type(aid) == TIG_ART_TYPE_ITEM) {
        aid = tig_art_id_rotation_set(aid, projectile_rot / 4);
    } else {
        aid = tig_art_num_set(aid, tig_art_num_get(aid) + projectile_rot / 8);
        aid = tig_art_id_rotation_set(aid, projectile_rot % 8);
    }

    return aid;
}

// 0x4B6B90
void combat_process_taunts(CombatContext* combat)
{
    int64_t pc_obj;

    // NOTE: Verified against 0x4B6B93-0x4B6B9A - original calls player_get_local_pc_obj and passes the handle to combat_taunts_get, whose implementation ignores it; CE keeps the lookup and calls the no-arg CE signature.
    pc_obj = player_get_local_pc_obj();

    if (!combat_taunts_get()) {
        return;
    }

    if (obj_field_int32_get(combat->attacker_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    if (combat->target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (critter_pc_leader_get(combat->target_obj) == combat->attacker_obj) {
        return;
    }

    if ((combat->flags & CF_HIT) == 0) {
        return;
    }

    if (combat->target_obj == combat->field_28) {
        if ((combat->flags & CF_CRITICAL) != 0) {
            ai_npc_witness_pc_critical(combat->attacker_obj, AI_NPC_WITNESS_PC_CRITICAL_HIT);
        }
    } else {
        if ((combat->flags & 0x08) != 0) {
            ai_npc_witness_pc_critical(combat->attacker_obj, AI_NPC_WITNESS_PC_CRITICAL_MISS);
        }
    }
}

// 0x4B6C40
bool combat_set_callbacks(CombatCallbacks* callbacks)
{
    combat_callbacks = *callbacks;

    return true;
}

// 0x4B6C80
bool combat_is_turn_based(void)
{
    return combat_turn_based;
}

// 0x4B6C90
bool combat_turn_based_set(bool turn_based)
{
    int64_t pc;

    if (combat_turn_based == turn_based) {
        return true;
    }

    pc = player_get_local_pc_obj();
    if (!anim_interrupt_all_goals_of_obj(pc, 3, 0, 1)) {
        return false;
    }

    if (!combat_critter_is_combat_mode_active(pc)) {
        combat_turn_based = turn_based;
        return true;
    }

    if (!turn_based) {
        combat_callbacks.field_8();
        combat_turn_based_end();
        combat_turn_based = turn_based;
        return true;
    }

    combat_callbacks.field_4();
    if (combat_turn_based_start()) {
        combat_turn_based = turn_based;
        return true;
    }

    combat_callbacks.field_8();
    return 1;
}

// 0x4B6D20
void combat_turn_based_toggle(void)
{
    if (combat_is_turn_based()) {
        settings_set_value(&settings, TURN_BASED_KEY, 0);
    } else {
        if (critter_is_active(player_get_local_pc_obj())) {
            settings_set_value(&settings, TURN_BASED_KEY, 1);
        }
    }
}

// 0x4B6D70
bool combat_turn_based_is_active(void)
{
    return combat_turn_based_active;
}

// 0x4B6D80
int64_t combat_turn_based_whos_turn_get(void)
{
    if (combat_whos_turn_node != NULL) {
        return combat_whos_turn_node->obj;
    } else {
        return OBJ_HANDLE_NULL;
    }
}

// 0x4B6DA0
void combat_debug(int64_t obj, const char* msg)
{
    char* name = NULL;

    if (obj == OBJ_HANDLE_NULL) {
        obj = combat_debug_obj;
    }

    if (obj != OBJ_HANDLE_NULL) {
        if (obj_handle_is_valid(obj)) {
            if (player_is_pc_obj(obj)) {
                obj_field_string_get(obj, OBJ_F_PC_PLAYER_NAME, &name);
            } else {
                obj_field_string_get(obj, OBJ_F_NAME, &name);
            }
        } else {
            combat_debug_obj = OBJ_HANDLE_NULL;
        }
    }

    tig_debug_printf("Combat: TB: DBG: %s: %s, Idx: %d, APs Left: %d\n",
        msg,
        name != NULL ? name : " ",
        combat_turn_index,
        combat_action_points);

    if (name != NULL) {
        FREE(name);
    }
}

// 0x4B6E70
void combat_turn_based_whos_turn_set(int64_t obj)
{
    if (!combat_turn_based_active) {
        return;
    }

    combat_debug(obj, "Whos Turn Set");

    if (!player_is_local_pc_obj(combat_whos_turn_obj)) {
        anim_interrupt_all_goals_of_obj(combat_whos_turn_obj, 3, 0, 1);
    }

    if (combat_whos_turn_node->obj != obj) {
        combat_whos_turn_node = combat_critter_list.head;
        while (combat_whos_turn_node != NULL) {
            if (combat_whos_turn_node->obj == obj) {
                break;
            }
            combat_whos_turn_node = combat_whos_turn_node->next;
        }

        if (combat_whos_turn_node == NULL) {
            tig_debug_printf("Combat: TB: Note: Couldn't change to 'Who's' Turn, inserting them in list.\n");
            combat_turn_based_add_critter(obj);
            combat_whos_turn_node = combat_critter_list.head;
            while (combat_whos_turn_node != NULL) {
                if (combat_whos_turn_node->obj == obj) {
                    break;
                }
                combat_whos_turn_node = combat_whos_turn_node->next;
            }

            if (combat_whos_turn_node == NULL) {
                tig_debug_printf("Combat: TB: ERROR: Couldn't change to 'Who's' Turn AFTER inserting them in list...DISABLING TB-COMBAT!\n");
                settings_set_value(&settings, TURN_BASED_KEY, 0);
                return;
            }
        }
    }

    // CE (real-time session C): through the shared rule, which the real-time
    // budget divides by - the same `max(5, Speed)` the original computed here.
    combat_action_points = combat_action_points_per_turn(obj);
    // CE (criticals, milestone CR-2): a Staggered critter's next turn grants
    // half.
    combat_action_points = crit_effects_turn_action_points(obj, combat_action_points);

    tig_debug_printf("Combat: TB: Action Points Available: %d.\n", combat_action_points);

    if (combat_observer_is_enabled()) {
        char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

        combat_observer_object_id(obj, actor);
        combat_observer_turn_started(actor, combat_action_points);
        combat_observer_snapshot_obj(obj);
    }

    if (player_is_local_pc_obj(obj)) {
        combat_callbacks.field_C(combat_action_points);

        if (!ci_lock_count()) {
            ci_lock();
        }
    } else {
        combat_callbacks.field_C(0);
        ci_update();
        combat_turn_based_npc_turn(obj);

        if (ci_lock_count()) {
            ci_unlock();
        }
    }

    combat_whos_turn_obj = obj;

    if (combat_callbacks.field_10 != NULL) {
        combat_callbacks.field_10(obj);
    }
}

// 0x4B7010
void combat_turn_based_npc_turn(int64_t obj)
{
    if (obj == combat_whos_turn_node->obj
        && !player_is_pc_obj(obj)
        && !gamelib_in_load()) {
        if (object_script_execute(obj, obj, OBJ_HANDLE_NULL, SAP_HEARTBEAT, 0) == 1) {
            ai_process(obj);
        }

        if (!anim_is_busy(obj, NULL)) {
            combat_action_points = 0;
            combat_turn_based_anims_complete();
        }
    }
}

// 0x4B7080
void combat_turn_based_anims_complete(void)
{
    DateTime datetime;
    TimeEvent timeevent;

    if (!combat_turn_based_active) {
        return;
    }

    combat_debug(OBJ_HANDLE_NULL, "TB Anims Complete");

    if (gamelib_in_reset()) {
        return;
    }

    if (combat_whos_turn_node != NULL) {
        if (player_is_pc_obj(combat_whos_turn_node->obj) && combat_action_points > 0) {
            return;
        }

        if (combat_whos_turn_node->obj == combat_last_turn_obj) {
            if (combat_action_points == combat_last_action_points) {
                combat_turn_stalled = true;
            }
            combat_last_action_points = combat_action_points;
        } else {
            combat_last_turn_obj = combat_whos_turn_node->obj;
        }
    }

    if (!timeevent_type_exists(TIMEEVENT_TYPE_TB_COMBAT)) {
        timeevent.type = TIMEEVENT_TYPE_TB_COMBAT;
        datetime_from_milliseconds(&datetime, 2);
        timeevent_add_delay(&timeevent, &datetime);
    }
}

// 0x4B7150
bool combat_tb_timeevent_process(TimeEvent* timeevent)
{
    (void)timeevent;

    combat_debug(OBJ_HANDLE_NULL, "TimeEvent Process");

    if (combat_turn_based_active) {
        if (combat_whos_turn_node != NULL
            && player_is_pc_obj(combat_whos_turn_node->obj)
            && combat_action_points > 0) {
            return true;
        }

        if (combat_turn_stalled) {
            combat_turn_stalled = false;
            combat_consume_action_points(combat_whos_turn_node->obj, combat_action_points);
        }

        // UAP (0x4B71BE): the call below targets 0x4B7774 in the patched
        // binary instead of 0x4B7780 because UAP relocated the getter to make
        // room for its drag override. Same function, no behaviour change.
        if (combat_action_points_get() <= 0) {
            combat_turn_based_next_subturn();
        }
    }

    return true;
}

// 0x4B71E0
bool combat_turn_based_start(void)
{
    int64_t loc;
    LocRect loc_rect;
    ObjectNode* node;

    combat_debug(OBJ_HANDLE_NULL, "TB Start");

    if (combat_turn_based_active) {
        return true;
    }

    combat_turn_based_turn = 0;

    if (!anim_goal_interrupt_all_for_tb_combat()) {
        tig_debug_printf("Combat: TB_Start: Anim-Goal-Interrupt FAILED!\n");
    }

    combat_turn_index = 0;

    anim_all_goals_finished_callback_set(combat_turn_based_anims_complete);

    loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    combat_recalc_perception_range();

    loc_rect.x1 = LOCATION_GET_X(loc) - combat_perception_range;
    loc_rect.x2 = LOCATION_GET_X(loc) + combat_perception_range;
    loc_rect.y1 = LOCATION_GET_Y(loc) - combat_perception_range;
    loc_rect.y2 = LOCATION_GET_Y(loc) + combat_perception_range;
    object_list_rect(&loc_rect, OBJ_TM_NPC | OBJ_TM_PC, &combat_critter_list);
    sort_combat_list();

    node = combat_critter_list.head;
    while (node != NULL) {
        anim_interrupt_all_goals_of_obj(node->obj, 3, 0, 1);
        node = node->next;
    }

    combat_turn_based_active = true;

    return combat_turn_based_begin_turn();
}

// 0x4B7300
void combat_recalc_perception_range(void)
{
    combat_perception_range = stat_level_get(player_get_local_pc_obj(), STAT_PERCEPTION) / 2 + 5;
    if (combat_perception_range < 10) {
        combat_perception_range = 10;
    }
}

// 0x4B7330
void combat_turn_based_end(void)
{
    ObjectNode* node;

    combat_debug(OBJ_HANDLE_NULL, "TB End");

    if (combat_turn_based_active) {
        combat_turn_based_active = false;

        anim_all_goals_finished_callback_set(NULL);

        if (!in_combat_reset) {
            node = combat_critter_list.head;
            while (node != NULL) {
                animfx_remove(&combat_eye_candies, node->obj, 0, -1);
                node = node->next;
            }
        }

        object_list_destroy(&combat_critter_list);
    }
}

// 0x4B73A0
bool combat_turn_based_begin_turn(void)
{
    int64_t pc_obj;
    int64_t pc_loc;
    LocRect loc_rect;
    ObjectList objects;

    combat_turn_index = 0;
    pc_obj = player_get_local_pc_obj();
    combat_turn_based_turn++;
    pc_loc = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
    combat_recalc_perception_range();

    loc_rect.x1 = LOCATION_GET_X(pc_loc) - combat_perception_range;
    loc_rect.y1 = LOCATION_GET_Y(pc_loc) - combat_perception_range;
    loc_rect.x2 = LOCATION_GET_X(pc_loc) + combat_perception_range;
    loc_rect.y2 = LOCATION_GET_Y(pc_loc) + combat_perception_range;
    object_list_rect(&loc_rect, OBJ_TM_PC | OBJ_TM_NPC, &objects);
    object_list_copy(&combat_critter_list, &objects);
    object_list_destroy(&objects);

    sort_combat_list();
    combat_summons_round_began();

    combat_turn_index = 0;
    combat_whos_turn_node = combat_critter_list.head;
    while (combat_turn_based_critter_skip(combat_whos_turn_node) && combat_whos_turn_node != NULL) {
        combat_turn_index++;
        combat_whos_turn_node = combat_whos_turn_node->next;
    }

    if (combat_whos_turn_node == NULL) {
        if (critter_is_active(pc_obj)) {
            combat_turn_index = 0;
            combat_whos_turn_node = combat_critter_list.head;
            while (combat_whos_turn_node != NULL && combat_whos_turn_node->obj != pc_obj) {
                combat_whos_turn_node = combat_whos_turn_node->next;
                combat_turn_index++;
            }

            if (combat_whos_turn_node == OBJ_HANDLE_NULL) {
                tig_debug_printf("Combat: combat_turn_based_begin_turn: ERROR: Couldn't start TB Combat Turn due to no Active Critters!\n");
                combat_turn_based_end();
                return false;
            }
        }
    }

    combat_debug(combat_whos_turn_node != NULL ? combat_whos_turn_node->obj : OBJ_HANDLE_NULL, "TB Begin Turn");
    combat_last_turn_obj = OBJ_HANDLE_NULL;
    combat_last_action_points = 0;
    combat_turn_stalled = 0;
    combat_turn_based_subturn_start();

    return true;
}

// 0x4B7580
bool combat_turn_based_critter_skip(ObjectNode* object_node)
{
    if (combat_whos_turn_node == NULL) {
        return true;
    }

    // CE (magic, milestone MG-3): this critter's turn has come round, taken
    // or skipped, so the creatures it summoned are ready.
    magictech_summons_turn_reached(object_node->obj);

    if (!critter_is_active(object_node->obj)) {
        anim_advance_stun_goal(object_node->obj);
        return true;
    }

    if (combat_turn_based_critter_is_dontdraw(object_node->obj)) {
        return true;
    }

    // CE (magic, milestone MG-3): summoning sickness - no turn until its
    // caster's next one has come round.
    if (magictech_summon_is_sick(object_node->obj)) {
        return true;
    }

    return false;
}

// 0x4B75D0
void combat_turn_based_subturn_start(void)
{
    if (combat_whos_turn_node != NULL) {
        combat_debug(combat_whos_turn_node->obj, "SubTurn Start");
        combat_turn_based_whos_turn_set(combat_whos_turn_node->obj);
        combat_check_action_points(combat_whos_turn_node->obj, 0);
        // CE (criticals, milestone CR-1): a turn that begins with them up
        // ends their protection when it ends.
        crit_guard_turn_started(combat_whos_turn_node->obj);
        crit_effects_turn_started(combat_whos_turn_node->obj);
    } else {
        tig_debug_printf("Combat: combat_turn_based_subturn_start: ERROR: Couldn't start TB Combat Turn due to no Active Critters!\n");
        combat_turn_based_end();
    }
}

// 0x4B7630
void combat_turn_based_subturn_end(void)
{
    combat_debug(combat_whos_turn_node->obj, "SubTurn End");
    // CE (criticals, milestones CR-1 and CR-2).
    crit_guard_turn_ended(combat_whos_turn_node->obj);
    crit_effects_turn_ended(combat_whos_turn_node->obj);
    if (combat_observer_is_enabled()) {
        char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

        combat_observer_object_id(combat_whos_turn_node->obj, actor);
        combat_observer_turn_ended(actor, combat_action_points);
    }
    if (player_is_local_pc_obj(combat_whos_turn_node->obj)) {
        combat_callbacks.field_C(0);
    } else {
        object_script_execute(combat_whos_turn_node->obj, combat_whos_turn_node->obj, OBJ_HANDLE_NULL, SAP_END_COMBAT, 0);
    }
}

// 0x4B7690
void combat_turn_based_next_subturn(void)
{
    combat_debug(combat_whos_turn_node->obj, "Next SubTurn");

    combat_turn_index++;
    combat_turn_based_subturn_end();

    combat_whos_turn_node = combat_whos_turn_node->next;
    if (combat_whos_turn_node == NULL) {
        combat_turn_based_end_turn();
        return;
    }

    if (combat_turn_based_critter_skip(combat_whos_turn_node)) {
        while (combat_whos_turn_node != NULL) {
            combat_whos_turn_node = combat_whos_turn_node->next;
            combat_turn_index++;
            if (combat_whos_turn_node == NULL) {
                break;
            }
            if (!combat_turn_based_critter_skip(combat_whos_turn_node)) {
                break;
            }
        }
        if (combat_whos_turn_node == NULL) {
            combat_turn_based_end_turn();
        }
    }

    if (combat_whos_turn_node != NULL) {
        combat_turn_based_subturn_start();
        return;
    }

    tig_debug_printf("Combat: combat_turn_based_next_subturn: ERROR: Couldn't start TB Combat Turn due to no Active Critters!\n");
    combat_turn_based_end();
}

// 0x4B7740
void combat_turn_based_end_turn(void)
{
    DateTime datetime;

    // UAP (0x4B7774): the changed bytes sit in the NOP padding behind this
    // function's `ret`, not in the body. UAP puts a duplicate of
    // `combat_action_points_get` there and reuses the old getter's address for
    // the drag-override prologue; the override itself is ported in
    // `combat_check_action_points` below.
    combat_debug(OBJ_HANDLE_NULL, "TB End Turn");
    datetime_from_milliseconds(&datetime, 1000);
    timeevent_inc_datetime(&datetime);
    combat_turn_based_begin_turn();
}

// 0x4B7780
int combat_action_points_get(void)
{
    // UAP relocates this two-instruction getter from 0x4B7780 to 0x4B7774 and
    // repoints its callers; the function's meaning is unchanged, so CE keeps it
    // at its logical source address.
    return combat_action_points;
}

// CE (turn-based rebalance): `exertion`, default off (zsogg, 2026-09-20, built
// 2026-09-26). Stock lets the player finish any action with too few AP - one AP
// left and a 7 AP shot alike - for 2 fatigue taken as damage, and so reduced by
// fatigue resistance. Exertion narrows that to what zsogg chose: only a shortfall
// of **exactly 1 AP**, with at least 1 AP left and at least 3 fatigue; the 2
// fatigue is spent directly, which no armor or resistance touches, and the
// remaining AP becomes zero. The player character only, and automatic.
#define COMBAT_EXERTION_FATIGUE 2
#define COMBAT_EXERTION_MIN_FATIGUE 3

static bool combat_exertion_covers(int64_t obj, int required_action_points)
{
    return player_is_local_pc_obj(obj)
        && combat_action_points >= 1
        && required_action_points - combat_action_points == 1
        && critter_fatigue_current(obj) >= COMBAT_EXERTION_MIN_FATIGUE;
}

// 0x4B7790
bool combat_check_action_points(int64_t obj, int required_action_points)
{
    bool is_pc;
    bool accepted;

    if (!combat_turn_based_active) {
        return true;
    }

    if (combat_whos_turn_node->obj != obj) {
        return true;
    }

    // UAP (0x4B77BD): while an item is being dragged in the inventory UI, the
    // requirement becomes a flat 4 - the cost the engine charges elsewhere for
    // one discrete action (`combat_check_cast_spell`, `combat_check_use_skill`
    // and `item_ui_activate` all pass a bare 4) - instead of whatever the
    // caller worked out. So a drag is not blocked by, say, the movement cost
    // `combat_check_move_to` computed.
    //
    // Note this overrides *every* caller while a drag is in progress,
    // including the `combat_check_action_points(obj, 0)` "can this critter act
    // at all" queries, which start demanding 4 points. That is what the patch
    // does; it hooks the shared function rather than the call sites.
    //
    // The patch reads `inven_ui_drag_item_obj` directly. This file cannot -
    // see `uap_drag_item_query_set` for why the query is installed from the UI
    // side instead.
    if (uap_fixes_enabled() && uap_drag_item_active()) {
        required_action_points = 4;
    }

    combat_required_action_points = required_action_points;

    is_pc = player_is_local_pc_obj(obj);
    if (is_pc) {
        combat_callbacks.field_C(combat_action_points);
    }

    accepted = combat_action_points >= required_action_points;
    if (!accepted && feature_enabled(FEATURE_EXERTION)) {
        accepted = combat_exertion_covers(obj, required_action_points);
    } else if (!accepted
        && combat_action_points > 0
        && is_pc
        && critter_fatigue_current(obj) > 1) {
        accepted = true;
    }

    if (combat_observer_is_enabled()) {
        char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

        combat_observer_object_id(obj, actor);
        combat_observer_ap_checked(actor, required_action_points,
            combat_action_points, accepted);
    }

    return accepted;
}

// 0x4B7830
bool combat_check_move_to(int64_t source_obj, int64_t target_loc)
{
    int type;

    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    type = obj_field_int32_get(source_obj, OBJ_F_TYPE);
    if (obj_type_is_item(type)
        && (obj_field_int32_get(source_obj, OBJ_F_FLAGS) & OF_INVENTORY) != 0
        && obj_field_handle_get(source_obj, OBJ_F_ITEM_PARENT) != OBJ_HANDLE_NULL) {
        return combat_check_action_points(source_obj, 0);
    }

    return combat_check_action_points(source_obj, combat_move_cost(source_obj, target_loc, false));
}

// 0x4B78D0
bool combat_check_attack(int64_t source_obj, int64_t target_obj)
{
    int action_points;
    int64_t weapon_obj;
    int64_t source_loc;
    int64_t target_loc;

    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    action_points = combat_attack_cost(source_obj);

    weapon_obj = item_wield_get(source_obj, ITEM_INV_LOC_WEAPON);
    if (weapon_obj == OBJ_HANDLE_NULL
        || item_weapon_range(weapon_obj, source_obj) <= 1) {
        source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
        target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
        if (location_dist(source_loc, target_loc) > 1) {
            action_points += combat_move_cost(source_obj, target_loc, true);
        }
    }

    return combat_check_action_points(source_obj, action_points);
}

// 0x4B79A0
bool combat_check_use_obj(int64_t source_obj, int64_t target_obj)
{
    int64_t source_loc;
    int64_t target_loc;
    int action_points;

    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    // CE (real-time session C): the shared cost - the original's 2.
    action_points = combat_use_object_cost(source_obj, target_obj);

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    if (source_loc != target_loc) {
        action_points += combat_move_cost(source_obj, target_loc, true);
    }

    return combat_check_action_points(source_obj, action_points);
}

// 0x4B7A20
bool combat_check_pick_item(int64_t source_obj, int64_t target_obj)
{
    int64_t source_loc;
    int64_t target_loc;
    int action_points;

    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    // CE (real-time session C): the shared cost - the original's 1.
    action_points = combat_pickup_cost(source_obj, target_obj);

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    if (source_loc != target_loc) {
        action_points += combat_move_cost(source_obj, target_loc, false);
    }

    return combat_check_action_points(source_obj, action_points);
}

// 0x4B7AA0
bool combat_check_cast_spell(int64_t source_obj)
{
    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    // CE (real-time session C): the shared cast cost; the preview does not
    // know which spell yet.
    return combat_check_action_points(source_obj, combat_cast_cost(source_obj, -1));
}

// CE (magic, milestone MG-1): the preview for a known spell, cast by
// `source_obj` or by an item it carries (`item_obj`, or none).
bool combat_check_cast_spell_for(int64_t source_obj, int spell, int64_t item_obj)
{
    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    return combat_check_action_points(source_obj,
        combat_spell_cast_cost(source_obj, spell, item_obj));
}

// 0x4B7AE0
bool combat_check_use_skill(int64_t source_obj)
{
    if (!combat_turn_based_active) {
        return combat_check_action_points(source_obj, 0);
    }

    // CE (real-time session C): the shared skill cost; the preview does not
    // know which skill yet.
    return combat_check_action_points(source_obj, combat_skill_cost(source_obj, -1));
}

// 0x4B7BA0
int combat_move_cost(int64_t source_obj, int64_t target_loc, bool adjacent)
{
    int mult;

    mult = 2;
    if (obj_field_int32_get(source_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
        && get_always_run(source_obj)) {
        mult = 1;
    }

    // CE (criticals, milestone CR-2): Hobbled doubles it, as
    // `anim_spend_move_cost` charges.
    mult *= crit_effects_move_multiplier(source_obj);

    if (adjacent) {
        return mult * anim_straight_path_length_to_tile(source_obj, target_loc);
    } else {
        return mult * anim_path_length_to_tile(source_obj, target_loc);
    }
}

// 0x4B7C20
int combat_required_action_points_get(void)
{
    return combat_required_action_points;
}

// 0x4B7C30
int combat_attack_cost(int64_t obj)
{
    // CE (combat AI, the get-up fix's follow-up): the body is
    // `combat_weapon_attack_cost`, so the AI can price a weapon it is not
    // holding. Same rule for the weapon in hand.
    return combat_weapon_attack_cost(obj, item_wield_get(obj, ITEM_INV_LOC_WEAPON));
}

// CE: what an attack with `weapon_obj` (OBJ_HANDLE_NULL for unarmed) costs
// `obj`, the rule `combat_attack_cost` applies to the weapon in hand.
int combat_weapon_attack_cost(int64_t obj, int64_t weapon_obj)
{
    int speed;
    int action_points;

    // CE (turn-based rebalance): a weapon's `rules/weapons.mes` FireAP, under
    // `weapon rules`, replaces the cost its speed gives. zsogg chose the rules
    // file over a speed-factor edit (2026-09-26): speed also sets how fast the
    // attack animates in real time, which a firing cost should not move.
    if (weapon_rules_fire_ap(obj, weapon_obj, &action_points)) {
        return action_points;
    }

    speed = item_weapon_magic_speed(weapon_obj, obj);
    if (speed > 24) {
        action_points = 1;
    } else if (speed > 20) {
        action_points = 2;
    } else {
        action_points = 8 - speed / 3;
        if (action_points < 1) {
            action_points = 1;
        }
    }

    return action_points;
}

// CE (real-time session C): what a throw costs, in one place.
//
// The original has no such function: the throw goal's last node,
// `anim_spend_action_points` (0x433270), charges the literal it shares with
// about twenty other goals - nothing for the player and 2 for anyone else in
// 1.0.7.4, and 4 for everyone under UAP (MODDING-NOTES.md quirk 51). The throw
// goal now charges through here, with the same numbers, and the real-time
// budget reads the same function, so a later change to what a throw costs
// moves turn-based and real time together.
int combat_throw_cost(int64_t obj)
{
    return combat_throw_cost_for(player_is_local_pc_obj(obj));
}

// CE (real-time session C): the action points a turn grants. The original
// computed `max(5, Speed)` inline where each turn starts; it lives here so the
// real-time budget, which divides an action's cost by it, reads the same rule.
// A rebalance that changes what a turn grants changes it here, and real time
// follows.
int combat_action_points_per_turn(int64_t obj)
{
    return combat_action_points_for_speed(stat_level_get(obj, STAT_SPEED));
}

int combat_action_points_for_speed(int speed)
{
    return speed < 5 ? 5 : speed;
}

// CE (real-time session C): what an ordinary cast costs. The original charges
// a flat 4 in `anim_pay_spell_cost` (0x429B50) and previews the same 4 in
// `combat_check_cast_spell`; both now ask here, and so does the real-time
// budget. `spell` is passed although today's cost ignores it - the rebalance
// plans spell costs that vary, and this is where they would go. -1 when the
// caller cannot say which spell.
//
// CE (magic, milestone MG-1): under `spell costs` a spell costs its AP in the
// spell table (`spell_rules.h`), 4 or more; a caster who knows all five
// spells of its college pays 1 less, never below 3 (zsogg, 2026-10-10: "Magic
// is generally too strong in Arcanum"). Unlisted, or with the switch off, the
// stock 4. An item that casts pays `COMBAT_ITEM_CAST_COST` instead
// (`combat_spell_cast_cost`).
int combat_cast_cost(int64_t obj, int spell)
{
    int listed;
    bool complete_college;

    if (!feature_enabled(FEATURE_SPELL_COSTS)) {
        return COMBAT_STOCK_CAST_COST;
    }

    listed = spell_rules_ap(spell);
    if (listed < SPELL_RULE_AP_MIN) {
        return COMBAT_STOCK_CAST_COST;
    }

    complete_college = obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && spell_college_level_get(obj, COLLEGE_FROM_SPELL(spell)) >= 5;
    return combat_spell_ap_rule(listed, complete_college);
}

int combat_spell_ap_rule(int listed_ap, bool complete_college)
{
    if (!complete_college) {
        return listed_ap;
    }

    return listed_ap - 1 > COMBAT_COMPLETE_COLLEGE_MIN ? listed_ap - 1 : COMBAT_COMPLETE_COLLEGE_MIN;
}

// CE (magic, milestone MG-1): what a cast costs the critter that pays for it.
// An item - a scroll, a wand, a remedy - casts for 4 whatever the spell (zsogg,
// 2026-10-10); a caster pays `combat_cast_cost`.
int combat_spell_cast_cost(int64_t caster_obj, int spell, int64_t source_obj)
{
    if (source_obj != OBJ_HANDLE_NULL
        && source_obj != caster_obj
        && obj_type_is_item(obj_field_int32_get(source_obj, OBJ_F_TYPE))) {
        return COMBAT_ITEM_CAST_COST;
    }

    return combat_cast_cost(caster_obj, spell);
}

// CE (real-time session C): the three other literal costs, lifted the same way
// with the same numbers, so the rebalance and the real-time budget read one
// place each. Each takes the action's inputs though today's rule ignores them.
//
// A skill: 4, charged in `anim_run_skill_invocation` (0x42A2A0) - skills and
// pickpocketing - and for disarming traps directly in `skill_ui.c`, previewed by
// `combat_check_use_skill`. -1 when the caller cannot say which skill.
int combat_skill_cost(int64_t obj, int skill)
{
    (void)obj;
    (void)skill;

    return 4;
}

// Using an object in the world - a door, a container, a lever: 2, charged in
// `anim_use_object` (0x428A10) and previewed by `combat_check_use_obj`, which
// adds the walk to it.
int combat_use_object_cost(int64_t obj, int64_t target_obj)
{
    (void)obj;
    (void)target_obj;

    return 2;
}

// Using an item from the pack that casts nothing - food, a written item: 4,
// charged in `item_use_on_obj` and `item_use_on_loc` and previewed in
// `item_ui.c`. Charged at once, with no animation goal.
int combat_use_item_cost(int64_t obj, int64_t item_obj)
{
    (void)obj;

    // CE (`restorative cost`, combat AI milestone AI-4, zsogg 2026-10-05): a
    // restorative - a food item, which is what every potion and remedy is -
    // costs 6 for the player and NPCs alike. Anything else stays at 4.
    if (feature_enabled(FEATURE_RESTORATIVE_COST)
        && item_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_FOOD) {
        return COMBAT_RESTORATIVE_COST;
    }

    return 4;
}

// Picking an item up: 1, charged in `anim_pickup_item` (0x42AA70) and
// previewed by `combat_check_pick_item`, which adds the walk to it.
int combat_pickup_cost(int64_t obj, int64_t item_obj)
{
    (void)obj;
    (void)item_obj;

    return 1;
}

// Getting up after a knockdown: 5, charged in turn-based by
// `AGbeginGetUpAnim`. Not a chosen action, so the real-time budget does not
// time it; lifted all the same so the rebalance tunes it in one place.
int combat_get_up_cost(int64_t obj)
{
    (void)obj;

    return 5;
}

// CE (reload prototype session 2): what loading one ready shot costs, in AP.
// The weapon's `rules/weapons.mes` `LoadAP`, or `LoadAPApprentice` at Apprentice
// training or better in its skill (`weapon_rules_trained`). 0 for a weapon
// whose rule names no Capacity. Turn-based charges it and the real-time budget times it, so a change
// here moves both modes, as with every other `combat_*_cost`.
int combat_reload_cost(int64_t obj, int64_t weapon_obj)
{
    const WeaponRule* rule;

    rule = weapon_rule_for(weapon_obj);
    if (rule == NULL || rule->capacity == WEAPON_RULE_UNSET) {
        return 0;
    }

    return weapon_rules_trained(obj, weapon_obj) ? rule->load_ap_apprentice : rule->load_ap;
}

// CE (criticals, milestone CR-4.1): what clearing a jam costs, in AP, under
// `firearm fumbles`. The weapon's `rules/weapons.mes` `ClearAP`, or without
// one the cost of loading a shot - one more reload, as agreed on 2026-09-18.
// Both modes read it, as they read the loading cost.
int combat_jam_clear_cost(int64_t obj, int64_t weapon_obj)
{
    const WeaponRule* rule;

    rule = weapon_rule_for(weapon_obj);
    if (rule != NULL && rule->capacity != WEAPON_RULE_UNSET && rule->clear_ap != WEAPON_RULE_UNSET) {
        return rule->clear_ap;
    }

    return combat_reload_cost(obj, weapon_obj);
}

// CE (reload prototype session 2): `weapon swap cost`, default off - drawing a
// weapon or switching to another costs 4 AP in the player's turn-based turn;
// putting one away is free (zsogg, 2026-09-20 and 2026-09-26; raised from 3 to
// 4 in tuning, 2026-09-27). NPCs swap for free (zsogg, 2026-09-27). Stock charges
// nothing for any weapon change. Real time leaves it untimed, with the other
// instant actions.
//
// **A swap is a different weapon entering the weapon slot** than the one last
// there. The inventory screen lifts an item out of its slot when a drag starts
// and puts it back if the drag is dropped where it began, so charging every
// arrival would bill an abandoned drag; and the hotkey bar unequips before it
// equips, so charging departures and arrivals would bill one swap twice. The
// weapon that last left the slot is remembered here.
#define COMBAT_WEAPON_SWAP_COST 4

int combat_weapon_swap_cost(int64_t obj)
{
    (void)obj;

    return COMBAT_WEAPON_SWAP_COST;
}

static bool combat_weapon_swap_applies(int64_t obj)
{
    return feature_enabled(FEATURE_WEAPON_SWAP_COST)
        && combat_turn_based_active
        && player_is_local_pc_obj(obj)
        && combat_whos_turn_node != NULL
        && combat_whos_turn_node->obj == obj;
}

// Whether `weapon_obj` entering the slot would be a swap: it is a weapon, and
// not the one in hand - or, with the hand empty, not the one last in it.
static bool combat_weapon_swap_is_swap(int64_t obj, int64_t weapon_obj)
{
    int64_t current_obj;

    if (weapon_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    current_obj = item_wield_get(obj, ITEM_INV_LOC_WEAPON);
    return weapon_obj != (current_obj != OBJ_HANDLE_NULL ? current_obj : combat_weapon_swap_last);
}

// Asked before a weapon is put in the player's hand. False, with a word over
// the player's head, when the swap costs more AP than the turn has left - it
// never draws on fatigue.
bool combat_check_weapon_swap(int64_t obj, int64_t weapon_obj)
{
    if (!combat_weapon_swap_applies(obj)
        || !combat_weapon_swap_is_swap(obj, weapon_obj)) {
        return true;
    }

    if (combat_action_points >= combat_weapon_swap_cost(obj)) {
        return true;
    }

    tf_add(obj, TF_TYPE_WHITE, "Not enough action points");
    return false;
}

// `item_force_remove`: a weapon left the player's weapon slot.
void combat_weapon_swap_left(int64_t obj, int64_t weapon_obj)
{
    if (player_is_local_pc_obj(obj)) {
        combat_weapon_swap_last = weapon_obj;
    }
}

// `item_insert`: a weapon entered the player's weapon slot. Charged here, once,
// if it is not the one that left. Never more than the turn has left: a path
// that did not ask `combat_check_weapon_swap` first is not given fatigue.
void combat_weapon_swap_entered(int64_t obj, int64_t weapon_obj)
{
    int cost;

    if (!player_is_local_pc_obj(obj)) {
        return;
    }

    if (combat_weapon_swap_applies(obj) && weapon_obj != combat_weapon_swap_last) {
        cost = combat_weapon_swap_cost(obj);
        if (cost > combat_action_points) {
            cost = combat_action_points;
        }
        if (cost > 0) {
            combat_consume_action_points(obj, cost);
        }
        tig_debug_printf("combat: weapon swap, %d AP\n", cost);
    }

    combat_weapon_swap_last = weapon_obj;
}

// The rule itself, apart from asking who the player is, so it can be tested
// without building one.
int combat_throw_cost_for(bool local_pc)
{
    // UAP (0x4332C0, 0x4332C2): the player stops being exempt and the cost
    // doubles from 2 to 4.
    if (uap_fixes_enabled()) {
        return 4;
    }

    return local_pc ? 0 : 2;
}

// 0x4B7C90
void combat_turn_based_end_critter_turn(int64_t obj)
{
    if (combat_turn_based_active
        && combat_whos_turn_node->obj == obj) {
        combat_action_points = 0;
        combat_turn_based_next_subturn();
    }
}

// CE (`restorative cost`, combat AI milestone AI-4): whether
// `combat_consume_action_points` would accept this charge, without making it
// or telling anyone. A restorative's effect runs before its cost is charged,
// so one that could not be paid for has to be refused first.
bool combat_can_pay_action_points(int64_t obj, int action_points)
{
    if (!combat_turn_based_active || combat_whos_turn_node->obj != obj) {
        return true;
    }

    return combat_action_points >= action_points
        || (feature_enabled(FEATURE_EXERTION) && combat_exertion_covers(obj, action_points));
}

// 0x4B7CD0
bool combat_consume_action_points(int64_t obj, int action_points)
{
    bool is_pc;
    CombatContext combat;
    int action_points_before;
    int fatigue_before = -1;

    if (!combat_turn_based_active) {
        return true;
    }

    if (combat_whos_turn_node->obj != obj) {
        return true;
    }

    is_pc = player_is_local_pc_obj(obj);
    action_points_before = combat_action_points;
    if (combat_observer_is_enabled()) {
        fatigue_before = critter_fatigue_damage_get(obj);
    }
    if (combat_action_points >= action_points) {
        combat_action_points -= action_points;

        if (is_pc) {
            combat_callbacks.field_C(combat_action_points);
        }

        if (combat_observer_is_enabled()) {
            char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

            combat_observer_object_id(obj, actor);
            combat_observer_ap_committed(actor, action_points,
                action_points_before, combat_action_points, true,
                fatigue_before, critter_fatigue_damage_get(obj));
        }

        return true;
    }

    if (feature_enabled(FEATURE_EXERTION)) {
        if (combat_exertion_covers(obj, action_points)) {
            critter_fatigue_damage_set(obj,
                critter_fatigue_damage_get(obj) + COMBAT_EXERTION_FATIGUE);
            combat_action_points = 0;
            combat_callbacks.field_C(0);
            tf_add(obj, TF_TYPE_YELLOW, "Exertion");
            tig_debug_printf("combat: exertion, %d AP short, %d fatigue\n",
                action_points - action_points_before, COMBAT_EXERTION_FATIGUE);

            if (combat_observer_is_enabled()) {
                char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

                combat_observer_object_id(obj, actor);
                combat_observer_ap_committed(actor, action_points,
                    action_points_before, combat_action_points, true,
                    fatigue_before, critter_fatigue_damage_get(obj));
            }

            return true;
        }

        // Refused, and the player keeps the AP they have. Stock never reached
        // the tail below for the player - its overreach accepted any shortfall -
        // and that tail empties the turn; an action that did not happen must
        // not cost the rest of it.
        if (is_pc) {
            tf_add(obj, TF_TYPE_WHITE, "Not enough action points");
            if (combat_observer_is_enabled()) {
                char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

                combat_observer_object_id(obj, actor);
                combat_observer_ap_committed(actor, action_points,
                    action_points_before, combat_action_points, false,
                    fatigue_before, critter_fatigue_damage_get(obj));
            }
            return false;
        }
    } else if (combat_action_points > 0
        && is_pc
        && critter_fatigue_current(obj) > 1) {
        combat_context_init(OBJ_HANDLE_NULL, obj, &combat);
        combat.flags |= 0x80;
        combat.dam[DAMAGE_TYPE_FATIGUE] = 2;
        combat_dmg(&combat);

        combat_action_points = 0;
        combat_callbacks.field_C(0);

        if (combat_observer_is_enabled()) {
            char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

            combat_observer_object_id(obj, actor);
            combat_observer_ap_committed(actor, action_points,
                action_points_before, combat_action_points, true,
                fatigue_before, critter_fatigue_damage_get(obj));
        }

        return true;
    }

    combat_action_points = 0;
    if (combat_observer_is_enabled()) {
        char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

        combat_observer_object_id(obj, actor);
        combat_observer_ap_committed(actor, action_points,
            action_points_before, combat_action_points, false,
            fatigue_before, critter_fatigue_damage_get(obj));
    }
    combat_turn_based_next_subturn();

    return false;
}

// 0x4B7DC0
bool combat_turn_based_critter_is_dontdraw(int64_t obj)
{
    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_DONTDRAW) != 0) {
        return !critter_is_sleeping(obj);
    } else {
        return false;
    }
}

// 0x4B7E00
void combat_turn_based_add_critter(int64_t obj)
{
    ObjectNode* prev = NULL;
    ObjectNode* curr;

    if (!combat_turn_based_active) {
        return;
    }

    curr = combat_critter_list.head;
    while (curr != NULL && curr->obj != obj) {
        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL) {
        // Already added.
        return;
    }

    if (combat_turn_based_critter_is_dontdraw(obj)) {
        tig_debug_printf("Combat: combat_turn_based_add_critter: WARNING: Attempt to add critter that is OF_DONTDRAW!\n");
    }

    curr = object_node_create();
    curr->obj = obj;
    curr->next = NULL;
    combat_debug(obj, "Adding Critter to List");

    if (prev != NULL) {
        prev->next = curr;
    } else {
        tig_debug_printf("Combat: combat_turn_based_add_critter: ERROR: Base list is EMPTY!\n");
        combat_critter_list.head = curr;
    }

    sort_combat_list();
}

static bool combat_critter_list_holds(int64_t obj)
{
    ObjectNode* node;

    for (node = combat_critter_list.head; node != NULL; node = node->next) {
        if (node->obj == obj) {
            return true;
        }
    }

    return false;
}

// CE (magic, milestone MG-3): under `summoning sickness` a summon takes its
// turn straight after its caster's - several in the order they were made -
// rather than wherever the round's list happens to put it (stock put the
// player's summon first, before the player). One whose caster is not in the
// fight keeps its own place. Only summons move, and the nodes themselves, so
// the turn in progress stays where it is.
static void combat_order_summons(void)
{
    ObjectNode* prev = NULL;
    ObjectNode* node;
    ObjectNode* next;
    ObjectNode* moved = NULL;
    ObjectNode** link;
    int64_t summoner;
    unsigned int seq;
    unsigned int other_seq;

    if (!feature_enabled(FEATURE_SUMMONING_SICKNESS)) {
        return;
    }

    // Take out every summon whose caster is in the list, in the order made.
    for (node = combat_critter_list.head; node != NULL; node = next) {
        next = node->next;
        summoner = magictech_summon_summoner(node->obj, &seq);
        if (summoner == OBJ_HANDLE_NULL
            || summoner == node->obj
            || !combat_critter_list_holds(summoner)) {
            prev = node;
            continue;
        }

        if (prev != NULL) {
            prev->next = next;
        } else {
            combat_critter_list.head = next;
        }

        link = &moved;
        while (*link != NULL) {
            magictech_summon_summoner((*link)->obj, &other_seq);
            if (other_seq > seq) {
                break;
            }
            link = &((*link)->next);
        }
        node->next = *link;
        *link = node;
    }

    // Put each back after its caster, behind that caster's earlier summons.
    while (moved != NULL) {
        node = moved;
        moved = moved->next;
        summoner = magictech_summon_summoner(node->obj, NULL);

        for (prev = combat_critter_list.head; prev != NULL && prev->obj != summoner; prev = prev->next) {
        }

        if (prev == NULL) {
            // Its caster was itself a summon still waiting to go back.
            for (prev = combat_critter_list.head; prev != NULL && prev->next != NULL; prev = prev->next) {
            }
        } else {
            while (prev->next != NULL
                && magictech_summon_summoner(prev->next->obj, NULL) == summoner) {
                prev = prev->next;
            }
        }

        if (prev != NULL) {
            node->next = prev->next;
            prev->next = node;
        } else {
            node->next = NULL;
            combat_critter_list.head = node;
        }
    }
}

// CE (magic, milestone MG-3): a sick summon whose caster is dead or out of
// the fight would wait for a turn that never comes; it is ready from this
// round.
static void combat_summons_round_began(void)
{
    ObjectNode* node;
    int64_t summoner;

    if (!feature_enabled(FEATURE_SUMMONING_SICKNESS)) {
        return;
    }

    for (node = combat_critter_list.head; node != NULL; node = node->next) {
        if (!magictech_summon_is_sick(node->obj)) {
            continue;
        }

        summoner = magictech_summon_summoner(node->obj, NULL);
        if (summoner == OBJ_HANDLE_NULL
            || !obj_handle_is_valid(summoner)
            || critter_is_dead(summoner)
            || !combat_critter_list_holds(summoner)) {
            magictech_summon_ready(node->obj, "caster_gone");
        }
    }
}

// 0x4B7EB0
void sort_combat_list(void)
{
    ObjectNode* node;
    ObjectNode* tail;
    ObjectNode* prev;
    ObjectNode* tmp;
    int64_t leader_obj;
    bool process;
    char* name;
    int index;

    combat_debug(OBJ_HANDLE_NULL, "Sorting List");

    tail = NULL;
    prev = NULL;
    node = combat_critter_list.head;
    if (node != NULL) {
        do {
            process = true;
            if (obj_field_int32_get(node->obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
                leader_obj = critter_pc_leader_get(node->obj);
                if (leader_obj == OBJ_HANDLE_NULL
                    || (obj_field_int32_get(node->obj, OBJ_F_TYPE) != OBJ_TYPE_PC)) {
                    process = false;
                }
            }

            if (process) {
                if (prev != NULL) {
                    prev->next = node->next;
                } else {
                    combat_critter_list.head = node->next;
                }

                if (tail != NULL) {
                    tmp = tail;
                    while (tmp->next != NULL) {
                        tmp = tmp->next;
                    }
                    tmp->next = node;
                } else {
                    tail = node;
                }

                node->next = NULL;
                if (prev != NULL) {
                    node = prev->next;
                } else {
                    node = combat_critter_list.head;
                }
            } else {
                prev = node;
                node = node->next;
            }
        } while (node != NULL);

        if (tail != NULL) {
            if (prev != NULL) {
                prev->next = tail;
            } else {
                combat_critter_list.head = tail;
            }
        }
    }

    combat_order_summons();

    node = combat_critter_list.head;
    while (node != NULL) {
        combat_recalc_reaction(node->obj);
        node = node->next;
    }

    index = 0;
    node = combat_critter_list.head;
    while (node != NULL) {
        name = NULL;
        if (obj_handle_is_valid(node->obj)) {
            if (player_is_pc_obj(node->obj)) {
                obj_field_string_get(node->obj, OBJ_F_PC_PLAYER_NAME, &name);
            } else {
                obj_field_string_get(node->obj, OBJ_F_NAME, &name);
            }
        }

        tig_debug_printf("Combat: TB: DBG: List[%d]: %s\n",
            index,
            name != NULL ? name : " ");

        // NOTE: Original code is slightly different and buggy - it attempts
        // to release `name` depending on a flag which is not reset on every
        // iteration.
        if (name != NULL) {
            FREE(name);
        }

        node = node->next;
        index++;
    }
}

// 0x4B8040
bool combat_turn_based_critter_can_fastforward(int64_t obj)
{
    if (!combat_turn_based_active) {
        return false;
    }

    if (combat_turn_based_whos_turn_get() != obj) {
        return false;
    }

    if (combat_fast_turn_based) {
        return true;
    }

    if (player_is_pc_obj(obj)) {
        return false;
    }

    if (combat_critter_is_combat_mode_active(obj)) {
        return false;
    }

    if (anim_is_current_goal_type(obj, AG_ATTACK, NULL)) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_BACKING_OFF) != 0) {
        return false;
    }

    return true;
}

// 0x4B80D0
int combat_turn_based_turn_get(void)
{
    return combat_turn_based_turn;
}

// 0x4B80E0
void combat_recalc_reaction(int64_t obj)
{
    // 0x5B5818
    static unsigned int dword_5B5818[7] = {
        OCF2_REACTION_0,
        OCF2_REACTION_1,
        OCF2_REACTION_2,
        OCF2_REACTION_3,
        OCF2_REACTION_4,
        OCF2_REACTION_5,
        OCF2_REACTION_6,
    };

    int64_t pc_obj;
    int reaction_level;
    int reaction_type;
    unsigned int flags;
    AnimFxNode node;

    if (!combat_turn_based_active) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    reaction_level = reaction_get(obj, pc_obj);
    reaction_type = reaction_translate(reaction_level);

    flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
    flags &= ~(OCF2_REACTION_0 | OCF2_REACTION_1 | OCF2_REACTION_2 | OCF2_REACTION_3 | OCF2_REACTION_4 | OCF2_REACTION_5 | OCF2_REACTION_6);
    flags |= dword_5B5818[reaction_type];
    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, flags);

    if (critter_is_dead(obj) || combat_turn_based_critter_is_dontdraw(obj)) {
        animfx_remove(&combat_eye_candies, obj, 0, -1);
    } else {
        animfx_node_init(&combat_eye_candies, &node, obj, -1, 0);
        if (!animfx_has(&node)) {
            animfx_add(&node);
        }
    }
}

// 0x4B81B0
bool combat_set_blinded(int64_t obj)
{
    unsigned int flags;
    int tf_type;
    MesFileEntry mes_file_entry;

    flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
    if ((flags & OCF_BLINDED) != 0) {
        return false;
    }

    flags |= OCF_BLINDED;
    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, flags);

    tf_type = obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC
        ? TF_TYPE_RED
        : TF_TYPE_WHITE;

    mes_file_entry.num = 4; // "Blinded"
    mes_get_msg(combat_mes_file, &mes_file_entry);
    tf_add(obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x4B8230
bool combat_auto_attack_get(int64_t obj)
{
    if (combat_turn_based_is_active()) {
        return false;
    }

    return settings_get_value(&settings, AUTO_ATTACK_KEY);
}

// 0x4B8280
void combat_auto_attack_set(bool value)
{
    settings_set_value(&settings, AUTO_ATTACK_KEY, value);
}

// 0x4B82E0
bool combat_taunts_get(void)
{
    return settings_get_value(&settings, COMBAT_TAUNTS_KEY);
}

// 0x4B8300
void combat_taunts_set(bool value)
{
    settings_set_value(&settings, COMBAT_TAUNTS_KEY, value);
}

// 0x4B8330
bool combat_auto_switch_weapons_get(int64_t obj)
{
    return settings_get_value(&settings, AUTO_SWITCH_WEAPON_KEY);
}

// 0x4B8380
void combat_auto_switch_weapons_set(bool value)
{
    settings_set_value(&settings, AUTO_SWITCH_WEAPON_KEY, value);
}

// 0x4B83E0
void pc_switch_weapon(int64_t pc_obj, int64_t target_obj)
{
    if (pc_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(pc_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
        && combat_auto_switch_weapons_get(pc_obj)) {
        item_wield_best(pc_obj, ITEM_INV_LOC_WEAPON, target_obj);
    }
}
