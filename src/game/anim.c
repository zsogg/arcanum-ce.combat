#include "game/anim.h"

#include <stdio.h>

#include "game/ai.h"
#include "game/animfx.h"
#include "game/combat_observer.h"
#include "game/crit_effects.h"
#include "game/critter.h"
#include "game/descriptions.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/light.h"
#include "game/light_scheme.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/mp_utils.h"
#include "game/mt_item.h"
#include "game/multiplayer.h"
#include "game/object.h"
#include "game/path.h"
#include "game/player.h"
#include "game/portal.h"
#include "game/proto.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/reload.h"
#include "game/resistance.h"
#include "game/rt_budget.h"
#include "game/sfx.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/teleport.h"
#include "game/tf.h"
#include "game/tile.h"
#include "game/timeevent.h"
#include "game/trap.h"
#include "game/uap.h"
#include "game/ui.h"
#include "game/weapon_rules.h"

static bool anim_id_matches_run_info(AnimID* anim_id, AnimRunInfo* run_info);
static void violence_filter_changed(void);
static bool anim_run_info_save(AnimRunInfo* run_info, TigFile* stream);
static bool anim_goal_data_save(AnimGoalData* goal_data, TigFile* stream);
static bool anim_run_info_param_save(AnimRunInfoParam* param, ObjectLocator* a2, int type, TigFile* stream);
static bool anim_load_internal(GameLoadInfo* load_info);
static bool anim_run_info_load(AnimRunInfo* run_info, TigFile* stream);
static bool anim_goal_data_load(AnimGoalData* goal_data, TigFile* stream);
static bool anim_run_info_param_load(AnimRunInfoParam* param, ObjectLocator* a2, int type, TigFile* stream);
static bool anim_schedule_next_ping(AnimRunInfo* run_info, DateTime* a2, int delay);
static void anim_pop_goal(AnimRunInfo* run_info, unsigned int* flags_ptr, AnimGoalNode** goal_node_ptr, AnimGoalData** goal_data_ptr, bool* a5);
static int anim_goal_pending_active_goals_count(void);
static bool anim_goal_pick_lock(int64_t obj, int64_t target_obj, int64_t item_obj);
static bool anim_find_blocking_critter(int64_t* source_obj_ptr, int64_t* block_obj_ptr);
static void anim_goal_turn_on_stop_at_blockers(AnimID anim_id);
static void notify_speed_recalc(AnimID* anim_id);
static void anim_modify_data_init(AGModifyData* modify_data);
static bool sub_4246C0(AnimRunInfo* run_info);
static bool anim_is_not_turn_based(AnimRunInfo* run_info);
static bool anim_add_parented_goal(AnimRunInfo* run_info);
static bool anim_create_spell_projectile(AnimRunInfo* run_info);
static bool anim_projectile_create(tig_art_id_t art_id, int64_t self_obj, int64_t target_obj, int64_t loc, int64_t target_loc, int spell, int64_t* obj_ptr, AnimID anim_id, ObjectID oid);
static bool anim_set_projectile_rotation(AnimRunInfo* run_info);
static bool anim_destroy_obj(AnimRunInfo* run_info);
static bool anim_is_at_location(AnimRunInfo* run_info);
static bool anim_pick_step_aside_tile(AnimRunInfo* run_info);
static bool anim_target_in_range(AnimRunInfo* run_info);
static bool anim_tile_in_range(AnimRunInfo* run_info);
static bool anim_target_in_walk_range(AnimRunInfo* run_info);
static bool anim_finish_move(AnimRunInfo* run_info);
static bool anim_finish_move_near(AnimRunInfo* run_info);
static bool anim_finish_move_near_combat(AnimRunInfo* run_info);
static bool anim_has_path(AnimRunInfo* run_info);
static bool anim_tile_is_blocked(int64_t obj, int64_t loc, int64_t adjacent_loc, int rot);
static void anim_get_obj_traversal_flags(int64_t obj, unsigned int* flags_ptr);
static bool anim_target_traversal_is_blocked(int64_t a1, int64_t a2, int64_t a3, int a4, int64_t a5);
static bool anim_create_wander_path(AnimRunInfo* run_info);
static bool anim_set_path_flags(PathCreateInfo* path_create_info, bool a2);
static bool anim_create_wander_dark_path(AnimRunInfo* run_info);
static bool anim_create_path_to_tile(AnimRunInfo* run_info);
static void anim_create_path_max_length(int64_t a1, const char* msg, int value);
static int anim_path_calc_max_length(AnimPath* anim_path, int64_t from, int64_t to, int64_t obj);
static int anim_path_create_from_obj(int64_t obj, int64_t to, AnimPath* path, unsigned int flags);
static bool anim_create_straight_path(AnimRunInfo* run_info);
static bool anim_create_knockback_path(AnimRunInfo* run_info);
static bool anim_create_projectile_path(AnimRunInfo* run_info);
static bool anim_create_path_away_from_obj(AnimRunInfo* run_info);
static bool anim_is_concealed_npc(AnimRunInfo* run_info);
static bool anim_is_prone(AnimRunInfo* run_info);
static bool AGendAnimStunAnim(AnimRunInfo* run_info);
static bool anim_next_step_blocked(AnimRunInfo* run_info);
static bool anim_check_next_step(AnimRunInfo* run_info, int64_t obj, int64_t loc);
static bool anim_find_blocking_door(AnimRunInfo* run_info);
static bool anim_find_wall_at(int64_t obj, int64_t loc, int rotation, int a4, int64_t* obj_ptr);
static bool anim_find_blocking_wall(AnimRunInfo* run_info);
static bool sub_427710(AnimRunInfo* run_info);
static bool sub_427720(AnimRunInfo* run_info);
static bool anim_create_path_near_tile(AnimRunInfo* run_info);
static bool anim_create_path_near_obj(AnimRunInfo* run_info);
static bool anim_create_path_near_obj_combat(AnimRunInfo* run_info);
static bool anim_is_portal(AnimRunInfo* run_info);
static bool anim_portal_is_closed_and_idle(AnimRunInfo* run_info);
static bool anim_try_open_portal(AnimRunInfo* run_info);
static bool anim_portal_is_operable(AnimRunInfo* run_info);
static bool anim_can_open_portal(AnimRunInfo* run_info);
static bool anim_cannot_open_portal(AnimRunInfo* run_info);
static bool anim_attempt_open_portal(AnimRunInfo* run_info);
static bool sub_428890(AnimRunInfo* run_info);
static bool anim_can_jump_window(AnimRunInfo* run_info);
static bool anim_set_use_range(AnimRunInfo* run_info);
static bool anim_use_object(AnimRunInfo* run_info);
static bool anim_use_item_on_obj(AnimRunInfo* run_info);
static bool anim_use_item_on_obj_with_skill(AnimRunInfo* run_info);
static bool anim_use_item_on_tile(AnimRunInfo* run_info);
static bool anim_use_item_on_tile_with_skill(AnimRunInfo* run_info);
static bool sub_429370(AnimRunInfo* run_info);
static bool sub_429380(AnimRunInfo* run_info);
static bool sub_429390(AnimRunInfo* run_info);
static bool sub_4293A0(AnimRunInfo* run_info);
static bool sub_4293B0(AnimRunInfo* run_info);
static bool sub_4293C0(AnimRunInfo* run_info);
static bool anim_set_no_flee(AnimRunInfo* run_info);
static bool sub_429420(AnimRunInfo* run_info);
static bool sub_429430(AnimRunInfo* run_info);
static bool sub_429440(AnimRunInfo* run_info);
static bool anim_target_is_alive(AnimRunInfo* run_info);
static bool anim_can_attack_target(AnimRunInfo* run_info);
static bool anim_can_reach_target(int64_t source_obj, int64_t target_obj);
static bool anim_play_queued_sfx(AnimRunInfo* run_info);
static bool anim_should_auto_attack(AnimRunInfo* run_info);
static bool anim_should_fidget(AnimRunInfo* run_info);
static bool anim_target_in_weapon_range(AnimRunInfo* run_info);
static bool anim_wields_ranged_weapon(AnimRunInfo* run_info);
static bool sub_429B40(AnimRunInfo* run_info);
static bool anim_pay_spell_cost(AnimRunInfo* run_info);
static bool sub_429BB0(AnimRunInfo* run_info);
static bool anim_spell_target_is_valid(AnimRunInfo* run_info);
static bool anim_set_spell_projectile_art(AnimRunInfo* run_info);
static bool anim_begin_spell(AnimRunInfo* run_info);
static bool anim_cleanup_spell(AnimRunInfo* run_info);
static bool anim_cleanup_destroy_obj(AnimRunInfo* run_info);
static bool anim_set_projectile_speed(AnimRunInfo* run_info);
static bool anim_load_spell_visuals(AnimRunInfo* run_info);
static bool anim_begin_previous_anim(AnimRunInfo* run_info);
static bool anim_is_on_target_tile(AnimRunInfo* run_info);
static bool anim_end_spell(AnimRunInfo* run_info);
static bool AGexecuteMagicTechCallback(AnimRunInfo* run_info);
static bool AGexecuteMagicTechEndCallback(AnimRunInfo* run_info);
static bool anim_run_skill_invocation(AnimRunInfo* run_info);
static bool anim_skill_succeeded(AnimRunInfo* run_info);
static bool anim_skill_is_pickpocket(AnimRunInfo* run_info);
static bool anim_skill_is_picklock(AnimRunInfo* run_info);
static bool anim_run_skill_no_cost(AnimRunInfo* run_info);
static bool anim_apply_attack(AnimRunInfo* run_info);
static bool AGapplyFireDmg(AnimRunInfo* run_info);
static bool anim_has_ranged_weapon(AnimRunInfo* run_info);
static bool anim_try_pickup_item(AnimRunInfo* run_info);
static bool anim_pickup_item(int64_t source_obj, int64_t target_obj);
static bool anim_throw_item(AnimRunInfo* run_info);
static bool anim_can_talk_to_target(AnimRunInfo* run_info);
static bool anim_target_in_dialog_range(AnimRunInfo* run_info);
static bool anim_begin_dialog(AnimRunInfo* run_info);
static bool anim_cleanup_unconceal(AnimRunInfo* run_info);
static bool anim_cleanup_stand_anim(AnimRunInfo* run_info);
static bool anim_cleanup_stunned(AnimRunInfo* run_info);
static bool anim_cleanup_throw_item(AnimRunInfo* run_info);
static bool anim_cleanup_door_open(AnimRunInfo* run_info);
static bool anim_cleanup_door_closed(AnimRunInfo* run_info);
static bool anim_portal_is_not_sticky(AnimRunInfo* run_info);
static bool anim_doorway_is_clear(AnimRunInfo* run_info);
static bool anim_try_run(AnimRunInfo* run_info);
static bool anim_begin_attack(AnimRunInfo* run_info);
static bool AGsetObjArtDeathAnim(AnimRunInfo* run_info);
static bool AGsetObjArtAnim(AnimRunInfo* run_info);
static bool anim_set_scratch_obj(AnimRunInfo* run_info);
static bool anim_set_skill(AnimRunInfo* run_info);
static bool anim_set_range(AnimRunInfo* run_info);
static bool anim_update_target_tile(AnimRunInfo* run_info);
static bool anim_set_follow_range(AnimRunInfo* run_info);
static bool anim_is_crowding_target(AnimRunInfo* run_info);
static bool anim_face_target(AnimRunInfo* run_info);
static bool anim_face_away_from_target(AnimRunInfo* run_info);
static bool anim_face_target_tile(AnimRunInfo* run_info);
static bool AGperformRotateAnim(AnimRunInfo* run_info);
static bool anim_set_rotation_from_path(AnimRunInfo* run_info);
static bool anim_cleanup_rotate(AnimRunInfo* run_info);
static bool anim_face_path_direction(AnimRunInfo* run_info);
static bool anim_face_spell_target(AnimRunInfo* run_info);
static bool anim_is_not_animating(AnimRunInfo* run_info);
static bool anim_reached_action_frame(AnimRunInfo* run_info);
static bool anim_cleanup_projectile(AnimRunInfo* run_info);
static bool AGbeginAnimAnim(AnimRunInfo* run_info);
static bool anim_advance_frame(AnimRunInfo* run_info);
static bool anim_begin_fidget_anim(AnimRunInfo* run_info);
static bool anim_advance_fidget_frame(AnimRunInfo* run_info);
static bool AGbeginAnimOpenDoor(AnimRunInfo* run_info);
static bool AGupdateAnimOpenDoor(AnimRunInfo* run_info);
static bool AGbeginAnimCloseDoor(AnimRunInfo* run_info);
static bool AGupdateAnimCloseDoor(AnimRunInfo* run_info);
static bool AGbeginAnimPickLock(AnimRunInfo* run_info);
static bool AGupdateAnimPickLock(AnimRunInfo* run_info);
static bool AGbeginAnimDying(AnimRunInfo* run_info);
static bool AGupdateAnimDying(AnimRunInfo* run_info);
static bool AGbeginAnimJump(AnimRunInfo* run_info);
static bool AGupdateAnimJump(AnimRunInfo* run_info);
static bool AGbeginAnimLoopAnim(AnimRunInfo* run_info);
static bool anim_loop_is_near_pc(AnimRunInfo* run_info);
static bool AGupdateAnimLoopAnim(AnimRunInfo* run_info);
static bool anim_cleanup_loop_sound(AnimRunInfo* run_info);
static bool AGbeginStunAnim(AnimRunInfo* run_info);
static bool AGupdateStunAnim(AnimRunInfo* run_info);
static bool AGbeginKneelMagicHandsAnim(AnimRunInfo* run_info);
static bool AGupdateKneelMagicHandsAnim(AnimRunInfo* run_info);
static bool anim_is_conscious(AnimRunInfo* run_info);
static bool AGbeginKnockDownAnim(AnimRunInfo* run_info);
static bool AGbeginGetUpAnim(AnimRunInfo* run_info);
static bool anim_save_current_aid(AnimRunInfo* run_info);
static bool AGbeginAnimAnimReverse(AnimRunInfo* run_info);
static bool AGupdateAnimAnimReverse(AnimRunInfo* run_info);
static bool AGbeginAnimMove(AnimRunInfo* run_info);
static void anim_move_art_id_get(AnimRunInfo* run_info, int64_t obj, tig_art_id_t* art_id_ptr, bool allow_run, int* run_out);
static void anim_move_pause_time_get(int64_t obj, DateTime* pause_time);
static bool anim_move_tile_reached(int direction, int offset_x, int offset_y);
static bool anim_vault_to_next_tile(AnimRunInfo* run_info);
static bool anim_advance_vault_step(AnimRunInfo* run_info);
static bool AGbeginAnimMoveStraight(AnimRunInfo* run_info);
static bool AGupdateAnimMoveStraight(AnimRunInfo* run_info);
static bool anim_begin_projectile_move(AnimRunInfo* run_info);
static bool AGupdateAnimProjectileMoveStraight(AnimRunInfo* run_info);
static bool AGupdateAnimMoveStraightKnockback(AnimRunInfo* run_info);
static bool anim_knockback_path_blocked(AnimRunInfo* run_info, int64_t obj, AnimPath* path, int64_t from, int64_t to);
static bool sub_42FEA0(AnimRunInfo* run_info);
static bool sub_42FEB0(AnimRunInfo* run_info);
static bool sub_42FEC0(AnimRunInfo* run_info);
static bool anim_add_death_blood(AnimRunInfo* run_info);
static bool anim_finish_death(AnimRunInfo* run_info);
static bool anim_cleanup_dying(AnimRunInfo* run_info);
static int anim_collect_fidget_candidates(LocRect* loc_rect, ObjectList* objects);
static bool anim_can_fidget(int64_t obj);
static bool AGupdateAnimMove(AnimRunInfo* run_info);
static bool anim_cleanup_move_path(AnimRunInfo* run_info);
static int anim_spend_move_cost(AnimRunInfo* run_info);
static bool anim_float_not_started(AnimRunInfo* run_info);
static bool anim_float_bob(AnimRunInfo* run_info);
static bool anim_float_begin(AnimRunInfo* run_info);
static bool anim_float_end(AnimRunInfo* run_info);
static bool anim_eye_candy_not_applied(AnimRunInfo* run_info);
static bool AGupdateAnimEyeCandy(AnimRunInfo* run_info);
static void anim_eye_candy_sound_restart(AnimRunInfo* run_info, int64_t obj);
static bool AGbeginAnimEyeCandy(AnimRunInfo* run_info);
static void anim_eye_candy_sound_begin(AnimRunInfo* run_info, int64_t obj);
static void anim_eye_candy_sound_move(AnimRunInfo* run_info, int64_t obj);
static bool AGendAnimEyeCandy(AnimRunInfo* run_info);
static bool anim_cleanup_eye_candy(AnimRunInfo* run_info);
static bool AGupdateAnimEyeCandyReverse(AnimRunInfo* run_info);
static bool AGbeginAnimEyeCandyReverse(AnimRunInfo* run_info);
static bool AGendAnimEyeCandyReverse(AnimRunInfo* run_info);
static bool AGupdateAnimEyeCandyFireDmg(AnimRunInfo* run_info);
static bool AGupdateAnimEyeCandyReverseFireDmg(AnimRunInfo* run_info);
static bool AGbeginAnimAttack(AnimRunInfo* run_info);
static bool AGupdateAnimAttack(AnimRunInfo* run_info);
static bool anim_weapon_has_ammo(int64_t critter_obj);
static bool anim_obj_is_valid(AnimRunInfo* run_info);
static bool anim_spend_action_points(AnimRunInfo* run_info);
static bool anim_can_start_move_goal(int64_t obj);
static int anim_get_animation_fps(int64_t obj, tig_art_id_t art_id, int speed);
static bool anim_path_location_after_steps(AnimRunInfo* run_info, int end, int64_t* x, int64_t* y);

// Shared out-parameter for this file's goal helpers: `anim_goal_add` and
// `anim_is_current_goal_type` write the goal's id here, and the caller reads it
// back immediately to reach `anim_run_info[...]` or to fill a multiplayer
// packet. It is never read before being written, so it carries no state between
// calls.
//
// 0x5A1908
static AnimID anim_scratch_goal_id = { -1, -1, 0 };

// 0x5A59D0
static AnimGoalNode anim_goal_node_animate = {
    6,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginAnimAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  6 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A5BD0
static AnimGoalNode anim_goal_node_animate_loop = {
    4,
    PRIORITY_1,
    0,
    1,
    1,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  2 */ { anim_loop_is_near_pc, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_YIELD, 800, 3, ANIM_DELAY_NONE },
        /*  3 */ { AGupdateAnimLoopAnim, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGbeginAnimLoopAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_loop_sound, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A5DD0
static AnimGoalNode anim_goal_node_animate_fidget = {
    5,
    PRIORITY_1,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_fidget_frame, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_begin_fidget_anim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { anim_should_fidget, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A5FD0
static AnimGoalNode anim_goal_node_move_to_tile = {
    7,
    PRIORITY_2,
    0,
    0,
    0,
    { AG_MOVE_NEAR_TILE, AG_RUN_TO_TILE, -1 },
    {
        /*  1 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  5 */ { anim_create_path_to_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 7, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_set_rotation_from_path, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ROTATE, ANIM_DELAY_NONE },
        /*  7 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A61D0
static AnimGoalNode anim_goal_node_run_to_tile = {
    8,
    PRIORITY_2,
    0,
    0,
    0,
    { AG_MOVE_NEAR_TILE, AG_MOVE_TO_TILE, AG_RUN_NEAR_TILE },
    {
        /*  1 */ { anim_try_run, { -1, -1 }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_path_to_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 8, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_rotation_from_path, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ROTATE, ANIM_DELAY_NONE },
        /*  8 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A63D0
static AnimGoalNode anim_goal_node_attempt_move = {
    12,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  2 */ { anim_next_step_blocked, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, 10, ANIM_DELAY_RANDOM },
        /*  3 */ { AGupdateAnimMove, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 12, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_finish_move, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  6 */ { anim_find_blocking_wall, { AGDATA_SELF_OBJ, -1 }, -1, 7, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  7 */ { anim_find_blocking_door, { AGDATA_SELF_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | ANIM_FLOW_RESET_PATH | AG_OPEN_DOOR, 50 },
        /*  9 */ { AGbeginAnimMove, { AGDATA_SELF_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /* 10 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { anim_can_jump_window, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_JUMP_WINDOW, ANIM_DELAY_NONE },
        /* 12 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_move_path, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// Sub-node 1 terminates on both of its edges, so this goal ends the moment it
// starts and sub-nodes 2 and 3 - a pair of stubs - are unreachable. Runs start
// at sub-node 1 and the only other way to move is a numeric jump from the
// table, and nothing jumps here. Troika's: the table matches the exe exactly.
// 0x5A65D0
static AnimGoalNode anim_goal_node_move_to_pause = {
    3,
    PRIORITY_2,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  2 */ { sub_427710, { AGDATA_BLOCK_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, 1000 },
        /*  3 */ { sub_427720, { AGDATA_BLOCK_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, 1000 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A67D0
static AnimGoalNode anim_goal_node_move_near_tile = {
    7,
    PRIORITY_2,
    0,
    0,
    0,
    { AG_RUN_TO_TILE, AG_MOVE_TO_TILE, AG_RUN_NEAR_TILE },
    {
        /*  1 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  5 */ { anim_create_path_near_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 7, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_set_rotation_from_path, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ROTATE, ANIM_DELAY_NONE },
        /*  7 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A69D0
static AnimGoalNode anim_goal_node_move_near_obj = {
    6,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_NEAR, ANIM_DELAY_NONE },
        /*  5 */ { anim_create_path_near_obj, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_NEAR, ANIM_DELAY_NONE },
        /*  6 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A6BD0
static AnimGoalNode anim_goal_node_move_straight = {
    4,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  2 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_STRAIGHT, ANIM_DELAY_NONE },
        /*  3 */ { anim_create_straight_path, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_STRAIGHT, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A6DD0
static AnimGoalNode anim_goal_node_attempt_move_straight = {
    3,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimMoveStraight, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_CALLBACK },
        /*  3 */ { AGbeginAnimMoveStraight, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A6FD0
static AnimGoalNode anim_goal_node_open_door = {
    5,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_portal, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_cannot_open_portal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, -1, ANIM_FLOW_REPLACE_GOAL | ANIM_FLOW_KEEP_PATH | AG_ATTEMPT_OPEN_DOOR, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        // FIX: The success flow code was `0x4000000` (one zero short of the
        // `0x40000000` push-goal bit), which no branch of the interpreter
        // matches - it would leave the state unchanged with a zero delay and
        // spin until the 100-iteration loop guard killed the animation. The
        // binary has `0x4000000D` at 0x5A7068. Unreachable either way, since
        // `anim_attempt_open_portal` always returns false.
        /*  4 */ { anim_attempt_open_portal, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNLOCK_DOOR, ANIM_DELAY_NONE },
        /*  5 */ { sub_428890, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_PICKUP_ITEM, ANIM_DELAY_NONE },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A71D0
static AnimGoalNode anim_goal_node_attempt_open_door = {
    8,
    PRIORITY_3,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_portal, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_can_open_portal, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_cannot_open_portal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, -1, 6, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_attempt_open_portal, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_UNLOCK_DOOR, ANIM_DELAY_NONE },
        /*  6 */ { anim_use_object, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_try_open_portal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_add_parented_goal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, AG_ANIMATE_DOOR_OPEN, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL | ANIM_FLOW_KEEP_PATH, ANIM_DELAY_NONE },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A73D0
static AnimGoalNode anim_goal_node_jump_window = {
    11,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimJump, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_PAUSE_TIME, 7, ANIM_DELAY_NONE },
        /*  3 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0xD, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_face_path_direction, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginAnimJump, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  6 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 9, ANIM_DELAY_NONE },
        /*  7 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_vault_to_next_tile, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  9 */ { anim_advance_vault_step, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL | ANIM_FLOW_KEEP_PATH, ANIM_DELAY_NONE },
        /* 10 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | ANIM_FLOW_RESET_PATH | AG_OPEN_DOOR, 50 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// Eight of this goal's ten sub-nodes call a stub that ignores its arguments and
// returns a constant, so most of the program is decided before it runs. Folding
// those constants in leaves:
//
//     1 -> 2 -> 3 -> at the item?  push AG_ATTEMPT_PICKUP
//                    otherwise     4 -> 5 -> 10 -> push AG_MOVE_NEAR_OBJ
//
// which is the whole behaviour: pick the item up if you are standing on it,
// walk to it if you are not. Sub-nodes 1, 2, 4 and 5 are unconditional
// pass-throughs, and 6 through 9 cannot be reached at all, because 5 always
// jumps to 10.
//
// The dead block is the interesting part. Its edges push AG_KILL and
// AG_PICKPOCKET, so picking an item up off a critter once escalated to killing
// or robbing them. That design is gone; only the wiring is left. This is
// Troika's, not a porting artifact - verify_goal_table.py matches all 10,880
// table fields against the exe's static data.
// 0x5A75D0
static AnimGoalNode anim_goal_node_pickup_item = {
    10,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { sub_429370, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  2 */ { sub_429380, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_at_location, { AGDATA_SELF_OBJ, 0x20 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_PICKUP, ANIM_DELAY_NONE },
        /*  4 */ { sub_429390, { AGDATA_SELF_OBJ, 1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { sub_4293A0, { AGDATA_TARGET_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, 10, ANIM_DELAY_NONE },
        /*  6 */ { sub_4293B0, { AGDATA_NULL_OBJ, -1 }, -1, ANIM_FLOW_PUSH | AG_KILL, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { sub_4293C0, { AGDATA_NULL_OBJ, -1 }, -1, 8, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_PICKPOCKET, ANIM_DELAY_NONE },
        /*  8 */ { sub_429420, { AGDATA_SELF_OBJ, AGDATA_NULL_OBJ }, -1, 9, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_KILL, ANIM_DELAY_NONE },
        /*  9 */ { sub_429430, { AGDATA_SELF_OBJ, AGDATA_NULL_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_PICKPOCKET, ANIM_DELAY_NONE },
        /* 10 */ { anim_set_range, { -1, -1 }, 0, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A77D0
static AnimGoalNode anim_goal_node_attempt_pickup = {
    2,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_try_pickup_item, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A79D0
static AnimGoalNode anim_goal_node_pickpocket = {
    1,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_skill, { -1, -1 }, SKILL_PICK_POCKET, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_USE_SKILL_ON, ANIM_DELAY_NONE },
        /*  2 */ { 0 },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A7BD0
static AnimGoalNode anim_goal_node_attack = {
    8,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_ATTEMPT_ATTACK, -1, -1 },
    {
        /*  1 */ { anim_target_is_alive, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_can_attack_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_target_in_weapon_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | ANIM_FLOW_NO_CLEANUP | AG_ATTEMPT_ATTACK, ANIM_DELAY_NONE },
        /*  6 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_target_in_range, { AGDATA_SELF_OBJ, 1 }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ_COMBAT, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { 0, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A7DD0
static AnimGoalNode anim_goal_node_attempt_attack = {
    13,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_ATTACK, AG_KILL, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimAttack, { AGDATA_SELF_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  3 */ { anim_face_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_target_in_weapon_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 12, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_begin_attack, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { AGbeginAnimAttack, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  7 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 13, ANIM_DELAY_NONE },
        /*  8 */ { anim_apply_attack, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  9 */ { anim_has_ranged_weapon, { AGDATA_SELF_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, 10, ANIM_DELAY_NONE },
        /* 10 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, 11, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /* 11 */ { anim_obj_is_valid, { AGDATA_SELF_OBJ, -1 }, -1, 12, ANIM_DELAY_NONE, 12, ANIM_DELAY_NONE },
        /* 12 */ { anim_should_auto_attack, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | ANIM_FLOW_NO_CLEANUP | AG_ATTACK, 5 },
        /* 13 */ { anim_target_in_weapon_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 12, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A7FD0
static AnimGoalNode anim_goal_node_kill = {
    7,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_ATTEMPT_ATTACK, -1, -1 },
    {
        /*  1 */ { anim_target_is_alive, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_can_attack_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { sub_429B40, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_PICK_WEAPON, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_target_in_weapon_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 7, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_ATTACK, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A81D0
static AnimGoalNode anim_goal_node_talk = {
    6,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_can_talk_to_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_target_in_dialog_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_begin_dialog, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  6 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A83D0
static AnimGoalNode anim_goal_node_chase = {
    4,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_try_run, { -1, -1 }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_set_follow_range, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, 100 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A85D0
static AnimGoalNode anim_goal_node_follow = {
    5,
    PRIORITY_2,
    0,
    0,
    0,
    { AG_RUN_NEAR_OBJ, AG_MOVE_NEAR_OBJ, -1 },
    {
        /*  1 */ { anim_set_follow_range, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_crowding_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 3, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 4, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { anim_target_in_walk_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_REPLACE_GOAL | AG_RUN_NEAR_OBJ, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  5 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, 100 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A87D0
static AnimGoalNode anim_goal_node_flee = {
    4,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_try_run, { -1, -1 }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_set_range, { -1, -1 }, 9, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_AWAY_FROM_OBJ, ANIM_DELAY_NONE },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A89D0
static AnimGoalNode anim_goal_node_throw_spell = {
    10,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_ATTEMPT_SPELL, -1, -1 },
    {
        /*  1 */ { sub_429440, { AGDATA_TARGET_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_pay_spell_cost, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 8, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { sub_429BB0, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_PICK_WEAPON, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_spell_target_is_valid, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, 9, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_range, { -1, -1 }, AGDATA_ANIM_ID, 10, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  8 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, 10, ANIM_DELAY_NONE },
        /*  9 */ { anim_face_spell_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_ATTEMPT_SPELL, ANIM_DELAY_NONE },
        /* 10 */ { anim_cleanup_spell, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A8BD0
static AnimGoalNode anim_goal_node_attempt_spell = {
    12,
    PRIORITY_4,
    0,
    0,
    0,
    { AG_THROW_SPELL, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, 4, ANIM_DELAY_NONE },
        /*  3 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 12, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_set_spell_projectile_art, { AGDATA_SPELL_DATA, -1 }, -1, 9, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_spell_projectile, { AGDATA_SELF_OBJ, AGDATA_SELF_TILE }, 5, 8, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_add_parented_goal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, AG_SHOOT_SPELL, ANIM_FLOW_TERMINATE, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  8 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  9 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, 8, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /* 10 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { anim_load_spell_visuals, { AGDATA_SPELL_DATA, -1 }, -1, 12, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /* 12 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_spell, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A8DD0
static AnimGoalNode anim_goal_node_shoot_spell = {
    9,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_FORCE_TARGET_TILE }, -1, 2, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  2 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  3 */ { anim_create_straight_path, { AGDATA_SELF_OBJ, AGDATA_FORCE_TARGET_TILE }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_on_target_tile, { AGDATA_SELF_OBJ, 1 }, -1, 9, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_destroy_obj, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  7 */ { anim_end_spell, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, -1, 6, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  8 */ { anim_set_projectile_speed, { AGDATA_SPELL_DATA, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_STRAIGHT_SPELL, ANIM_DELAY_NONE },
        /*  9 */ { anim_update_target_tile, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 1, ANIM_DELAY_NONE },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_destroy_obj, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A8FD0
static AnimGoalNode anim_goal_node_hit_by_spell = {
    4,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { sub_42FEC0, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { sub_42FEB0, { AGDATA_SELF_OBJ, AGDATA_ANIM_DATA }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A91D0
static AnimGoalNode anim_goal_node_hit_by_weapon = {
    6,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 23, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { AGbeginAnimAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  5 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  6 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A93D0
static AnimGoalNode anim_goal_node_dying = {
    9,
    PRIORITY_5,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimDying, { AGDATA_SELF_OBJ, -1 }, -1, 7, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGsetObjArtDeathAnim, { AGDATA_SELF_OBJ, -1 }, -1, 7, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { AGbeginAnimDying, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 7, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  7 */ { anim_add_death_blood, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_NONE, 9, ANIM_DELAY_NONE },
        /*  8 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  9 */ { anim_finish_death, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_dying, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A95D0
static AnimGoalNode anim_goal_node_destroy_obj = {
    1,
    PRIORITY_5,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_destroy_obj, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  2 */ { 0 },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A97D0
static AnimGoalNode anim_goal_node_use_skill_on = {
    7,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_target_in_range, { AGDATA_SELF_OBJ, 1 }, 1, 3, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  3 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  4 */ { anim_create_path_near_obj, { AGDATA_SELF_OBJ, 1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  5 */ { anim_skill_is_picklock, { AGDATA_SELF_OBJ, AGDATA_SKILL_DATA }, 1, 7, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_USE_PICKLOCK_SKILL_ON, ANIM_DELAY_NONE },
        /*  6 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  7 */ { anim_skill_is_pickpocket, { AGDATA_SELF_OBJ, AGDATA_SKILL_DATA }, 1, ANIM_FLOW_REPLACE_GOAL | AG_ATTEMPT_USE_SKILL_ON, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_ATTEMPT_USE_PICKPOCKET_SKILL_ON, ANIM_DELAY_NONE },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A99D0
static AnimGoalNode anim_goal_node_attempt_use_skill_on = {
    9,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, 6, ANIM_DELAY_NONE },
        /*  3 */ { anim_face_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0xC, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginAnimAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 9, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  6 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_run_skill_invocation, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  8 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  9 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A9BD0
static AnimGoalNode anim_goal_node_skill_conceal = {
    6,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 5, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { AGbeginAnimAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  5 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 5, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  6 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A9DD0
static AnimGoalNode anim_goal_node_projectile = {
    7,
    PRIORITY_5,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimProjectileMoveStraight, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_CALLBACK },
        /*  3 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_set_projectile_rotation, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_projectile_path, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_begin_projectile_move, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_projectile, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5A9FD0
static AnimGoalNode anim_goal_node_throw_item = {
    9,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, 6, ANIM_DELAY_NONE },
        /*  3 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 14, 8, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_face_target_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 8, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginAnimAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 8, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  6 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_throw_item, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, 5, 8, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  8 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, 9, ANIM_DELAY_NONE, 9, ANIM_DELAY_NONE },
        /*  9 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_throw_item, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AA1D0
static AnimGoalNode anim_goal_node_use_object = {
    6,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_use_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_portal, { AGDATA_TARGET_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { anim_use_object, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { anim_set_scratch_obj, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_portal_is_closed_and_idle, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_REPLACE_GOAL | AG_CLOSE_DOOR, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_OPEN_DOOR, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AA3D0
static AnimGoalNode anim_goal_node_use_item_on_object = {
    3,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_use_item_on_obj, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AA5D0
static AnimGoalNode anim_goal_node_use_item_on_object_with_skill = {
    3,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_use_item_on_obj_with_skill, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AA7D0
static AnimGoalNode anim_goal_node_use_item_on_tile = {
    3,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_tile_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_TILE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_use_item_on_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AA9D0
static AnimGoalNode anim_goal_node_use_item_on_tile_with_skill = {
    3,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_range, { -1, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_tile_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_TILE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_use_item_on_tile_with_skill, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AABD0
static AnimGoalNode anim_goal_node_knockback = {
    4,
    PRIORITY_5,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  2 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_STRAIGHT_KNOCKBACK, ANIM_DELAY_NONE },
        /*  3 */ { anim_create_knockback_path, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_STRAIGHT_KNOCKBACK, ANIM_DELAY_NONE },
        /*  4 */ { sub_42FEA0, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AADD0
static AnimGoalNode anim_goal_node_floating = {
    4,
    PRIORITY_5,
    1,
    1,
    1,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_float_not_started, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_float_bob, { AGDATA_SELF_OBJ, -1 }, -1, 4, 100, ANIM_FLOW_YIELD, 100 },
        /*  3 */ { anim_float_begin, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_float_end, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AAFD0
static AnimGoalNode anim_goal_node_eye_candy = {
    4,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AB1D0
static AnimGoalNode anim_goal_node_eye_candy_reverse = {
    4,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGendAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AB3D0
static AnimGoalNode anim_goal_node_eye_candy_callback = {
    6,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  3 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 6, ANIM_DELAY_NONE },
        /*  6 */ { AGexecuteMagicTechCallback, { AGDATA_SPELL_DATA, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AB5D0
static AnimGoalNode anim_goal_node_eye_candy_reverse_callback = {
    6,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  3 */ { AGbeginAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGendAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 6, ANIM_DELAY_NONE },
        /*  6 */ { AGexecuteMagicTechCallback, { AGDATA_SPELL_DATA, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AB7D0
static AnimGoalNode anim_goal_node_close_door = {
    2,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_portal, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_REPLACE_GOAL | AG_ATTEMPT_CLOSE_DOOR, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AB9D0
static AnimGoalNode anim_goal_node_attempt_close_door = {
    4,
    PRIORITY_3,
    1,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_portal, { AGDATA_SCRATCH_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  3 */ { anim_portal_is_operable, { AGDATA_SCRATCH_OBJ, 0 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_add_parented_goal, { AGDATA_SCRATCH_OBJ, 0 }, AG_ANIMATE_DOOR_CLOSED, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ABBD0
static AnimGoalNode anim_goal_node_animate_reverse = {
    4,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimAnimReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimAnimReverse, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ABDD0
static AnimGoalNode anim_goal_node_move_away_from_obj = {
    7,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  3 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  5 */ { anim_create_path_away_from_obj, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  6 */ { anim_face_away_from_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_no_flee, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ABFD0
static AnimGoalNode anim_goal_node_rotate = {
    1,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { AGperformRotateAnim, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_VAL1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, 30 },
        /*  2 */ { 0 },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_rotate, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_VAL1 }, -1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AC1D0
static AnimGoalNode anim_goal_node_unconceal = {
    5,
    PRIORITY_4,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimAnimReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_save_current_aid, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { AGbeginAnimAnimReverse, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  5 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_unconceal, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AC3D0
static AnimGoalNode anim_goal_node_run_near_tile = {
    8,
    PRIORITY_2,
    0,
    0,
    0,
    { AG_MOVE_NEAR_TILE, AG_RUN_TO_TILE, -1 },
    {
        /*  1 */ { anim_try_run, { -1, -1 }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_path_near_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 8, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_rotation_from_path, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ROTATE, ANIM_DELAY_NONE },
        /*  8 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AC5D0
static AnimGoalNode anim_goal_node_run_near_obj = {
    7,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_try_run, { -1, -1 }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_path_near_obj, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 7, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  7 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AC7D0
static AnimGoalNode anim_goal_node_animate_stunned = {
    7,
    PRIORITY_1,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateStunAnim, { AGDATA_SELF_OBJ, -1 }, -1, 7, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginStunAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  6 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  7 */ { AGendAnimStunAnim, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stunned, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AC9D0
static AnimGoalNode anim_goal_node_eye_candy_end_callback = {
    5,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { AGexecuteMagicTechEndCallback, { AGDATA_SPELL_DATA, -1 }, -1, 5, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ACBD0
static AnimGoalNode anim_goal_node_eye_candy_reverse_end_callback = {
    5,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { AGexecuteMagicTechEndCallback, { AGDATA_SPELL_DATA, -1 }, -1, 5, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGendAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ACDD0
static AnimGoalNode anim_goal_node_animate_kneel_magic_hands = {
    6,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateKneelMagicHandsAnim, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_REPLACE_GOAL | AG_ANIMATE_REVERSE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginKneelMagicHandsAnim, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  6 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ACFD0
static AnimGoalNode anim_goal_node_attempt_move_near = {
    12,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  2 */ { anim_next_step_blocked, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, 10, ANIM_DELAY_RANDOM },
        /*  3 */ { AGupdateAnimMove, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 12, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_finish_move_near, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  6 */ { anim_find_blocking_wall, { AGDATA_SELF_OBJ, -1 }, -1, 7, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  7 */ { anim_find_blocking_door, { AGDATA_SELF_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | ANIM_FLOW_RESET_PATH | AG_OPEN_DOOR, 50 },
        /*  9 */ { AGbeginAnimMove, { AGDATA_SELF_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /* 10 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /* 11 */ { anim_can_jump_window, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_JUMP_WINDOW, ANIM_DELAY_NONE },
        /* 12 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_move_path, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AD1D0
static AnimGoalNode anim_goal_node_knock_down = {
    4,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginKnockDownAnim, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_conscious, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_ANIM_GET_UP, 200 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AD3D0
static AnimGoalNode anim_goal_node_anim_get_up = {
    4,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginGetUpAnim, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AD5D0
static AnimGoalNode anim_goal_node_attempt_move_straight_knockback = {
    4,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimMoveStraightKnockback, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_CALLBACK },
        /*  3 */ { AGbeginAnimMoveStraight, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AD7D0
static AnimGoalNode anim_goal_node_wander = {
    1,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_MOVE_NEAR_TILE, -1, -1 },
    {
        /*  1 */ { anim_create_wander_path, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_YIELD, 300, ANIM_FLOW_REPLACE_GOAL | AG_MOVE_NEAR_TILE, 300 },
        /*  2 */ { 0 },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AD9D0
static AnimGoalNode anim_goal_node_wander_seek_darkness = {
    1,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_MOVE_NEAR_TILE, -1, -1 },
    {
        /*  1 */ { anim_create_wander_dark_path, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_YIELD, 300, ANIM_FLOW_REPLACE_GOAL | AG_MOVE_NEAR_TILE, 300 },
        /*  2 */ { 0 },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ADBD0
static AnimGoalNode anim_goal_node_use_picklock_skill_on = {
    7,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimPickLock, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_face_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0xC, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGbeginAnimPickLock, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 7, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  6 */ { anim_run_skill_no_cost, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  7 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ADDD0
static AnimGoalNode anim_goal_node_please_move = {
    8,
    PRIORITY_2,
    0,
    0,
    0,
    { AG_MOVE_NEAR_TILE, AG_RUN_TO_TILE, AG_MOVE_TO_TILE },
    {
        /*  1 */ { anim_pick_step_aside_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_CALLBACK },
        /*  2 */ { anim_is_at_location, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  4 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  5 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_path_to_tile, { AGDATA_SELF_OBJ, AGDATA_TARGET_TILE }, -1, 8, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_rotation_from_path, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ROTATE, ANIM_DELAY_NONE },
        /*  8 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5ADFD0
static AnimGoalNode anim_goal_node_attempt_spread_out = {
    5,
    PRIORITY_2,
    1,
    0,
    0,
    { AG_RUN_NEAR_OBJ, AG_MOVE_NEAR_OBJ, -1 },
    {
        /*  1 */ { anim_set_follow_range, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_crowding_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_AWAY_FROM_OBJ, ANIM_DELAY_NONE },
        /*  3 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 4, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { anim_target_in_walk_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_REPLACE_GOAL | AG_RUN_NEAR_OBJ, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  5 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, 100 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AE1D0
static AnimGoalNode anim_goal_node_animate_door_open = {
    4,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimOpenDoor, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimOpenDoor, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { anim_portal_is_not_sticky, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_PEND_CLOSING_DOOR, 1500 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_door_open, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AE3D0
static AnimGoalNode anim_goal_node_animate_door_closed = {
    3,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimCloseDoor, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimCloseDoor, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_door_closed, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AE5D0
static AnimGoalNode anim_goal_node_pend_closing_door = {
    1,
    PRIORITY_3,
    0,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_doorway_is_clear, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_YIELD, 1500, ANIM_FLOW_REPLACE_GOAL | AG_ANIMATE_DOOR_CLOSED, 1500 },
        /*  2 */ { 0 },
        /*  3 */ { 0 },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AE7D0
static AnimGoalNode anim_goal_node_throw_spell_friendly = {
    9,
    PRIORITY_3,
    1,
    0,
    0,
    { AG_ATTEMPT_SPELL, -1, -1 },
    {
        /*  1 */ { sub_429440, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_pay_spell_cost, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 8, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { sub_429BB0, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_PICK_WEAPON, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_spell_target_is_valid, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 9, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_range, { -1, -1 }, 8, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  8 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  9 */ { sub_4246C0, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_ATTEMPT_SPELL, ANIM_DELAY_NONE },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AE9D0
static AnimGoalNode anim_goal_node_attempt_spell_friendly = {
    12,
    PRIORITY_4,
    1,
    0,
    0,
    { AG_THROW_SPELL, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, 4, ANIM_DELAY_NONE },
        /*  3 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 12, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_set_spell_projectile_art, { AGDATA_SPELL_DATA, -1 }, -1, 9, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_spell_projectile, { AGDATA_SELF_OBJ, AGDATA_SELF_TILE }, 5, 8, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_add_parented_goal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, AG_SHOOT_SPELL, ANIM_FLOW_TERMINATE, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  8 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  9 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, 8, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /* 10 */ { sub_4246C0, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { anim_load_spell_visuals, { AGDATA_SPELL_DATA, -1 }, -1, 12, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /* 12 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_spell, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AEBD0
static AnimGoalNode anim_goal_node_eye_candy_fire_dmg = {
    5,
    PRIORITY_5,
    1,
    1,
    1,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandyFireDmg, { AGDATA_SELF_OBJ, AGDATA_PARENT_OBJ }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { AGapplyFireDmg, { AGDATA_SELF_OBJ, AGDATA_PARENT_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AEDD0
static AnimGoalNode anim_goal_node_eye_candy_reverse_fire_dmg = {
    5,
    PRIORITY_5,
    1,
    1,
    1,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandyReverseFireDmg, { AGDATA_SELF_OBJ, AGDATA_PARENT_OBJ }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { AGbeginAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { AGendAnimEyeCandyReverse, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  5 */ { AGapplyFireDmg, { AGDATA_SELF_OBJ, AGDATA_PARENT_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AEFD0
static AnimGoalNode anim_goal_node_animate_loop_fire_dmg = {
    5,
    PRIORITY_1,
    0,
    1,
    1,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  2 */ { anim_loop_is_near_pc, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_YIELD, 800, 3, ANIM_DELAY_NONE },
        /*  3 */ { AGupdateAnimLoopAnim, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGbeginAnimLoopAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { AGapplyFireDmg, { AGDATA_SELF_OBJ, AGDATA_PARENT_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_loop_sound, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AF1D0
static AnimGoalNode anim_goal_node_attempt_move_straight_spell = {
    3,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimMoveStraight, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_CALLBACK },
        /*  3 */ { AGbeginAnimMoveStraight, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { 0 },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AF3D0
static AnimGoalNode anim_goal_node_move_near_obj_combat = {
    8,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 2, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  5 */ { anim_create_path_near_obj_combat, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  6 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /*  7 */ { anim_play_queued_sfx, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_wields_ranged_weapon, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_NEAR, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ATTEMPT_MOVE_NEAR_COMBAT, ANIM_DELAY_NONE },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AF5D0
static AnimGoalNode anim_goal_node_attempt_move_near_combat = {
    12,
    PRIORITY_2,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  2 */ { anim_next_step_blocked, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, 10, ANIM_DELAY_RANDOM },
        /*  3 */ { AGupdateAnimMove, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { anim_has_path, { AGDATA_SELF_OBJ, -1 }, -1, 12, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_finish_move_near_combat, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, ANIM_FLOW_POP_3_GOALS, ANIM_DELAY_NONE },
        /*  6 */ { anim_find_blocking_wall, { AGDATA_SELF_OBJ, -1 }, -1, 7, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  7 */ { anim_find_blocking_door, { AGDATA_SELF_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  8 */ { anim_portal_is_closed_and_idle, { AGDATA_SCRATCH_OBJ, -1 }, -1, 9, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | ANIM_FLOW_RESET_PATH | AG_OPEN_DOOR, 50 },
        /*  9 */ { AGbeginAnimMove, { AGDATA_SELF_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /* 10 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { anim_can_jump_window, { AGDATA_SELF_OBJ, AGDATA_SCRATCH_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_JUMP_WINDOW, ANIM_DELAY_NONE },
        /* 12 */ { anim_is_not_turn_based, { -1, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | ANIM_FLOW_YIELD | AG_MOVE_TO_PAUSE, 1000 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_move_path, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AF7D0
static AnimGoalNode anim_goal_node_use_container = {
    6,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_set_use_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_portal, { AGDATA_TARGET_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  4 */ { anim_use_object, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /*  5 */ { anim_set_scratch_obj, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_portal_is_closed_and_idle, { AGDATA_TARGET_OBJ, -1 }, -1, ANIM_FLOW_REPLACE_GOAL | AG_CLOSE_DOOR, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_OPEN_DOOR, ANIM_DELAY_NONE },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AF9D0
static AnimGoalNode anim_goal_node_throw_spell_w_cast_anim = {
    10,
    PRIORITY_3,
    0,
    0,
    0,
    { AG_ATTEMPT_SPELL, -1, -1 },
    {
        /*  1 */ { sub_429440, { AGDATA_TARGET_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_is_prone, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_ANIM_GET_UP, ANIM_DELAY_NONE },
        /*  3 */ { anim_is_concealed_npc, { AGDATA_SELF_OBJ, -1 }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_UNCONCEAL, ANIM_DELAY_NONE },
        /*  4 */ { anim_pay_spell_cost, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 8, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { sub_429BB0, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_PUSH | AG_PICK_WEAPON, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_spell_target_is_valid, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, 9, ANIM_DELAY_NONE },
        /*  7 */ { anim_set_range, { -1, -1 }, 8, 10, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_NEAR_OBJ, ANIM_DELAY_NONE },
        /*  8 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 10, ANIM_DELAY_NONE, 10, ANIM_DELAY_NONE },
        /*  9 */ { anim_face_spell_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 10, ANIM_DELAY_NONE, ANIM_FLOW_REPLACE_GOAL | AG_ATTEMPT_SPELL_W_CAST_ANIM, ANIM_DELAY_NONE },
        /* 10 */ { anim_cleanup_spell, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AFBD0
static AnimGoalNode anim_goal_node_attempt_spell_w_cast_anim = {
    14,
    PRIORITY_4,
    0,
    0,
    0,
    { AG_THROW_SPELL, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 11, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, 4, ANIM_DELAY_NONE },
        /*  3 */ { sub_4246C0, { AGDATA_SELF_OBJ, -1 }, -1, 13, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  4 */ { anim_reached_action_frame, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_set_spell_projectile_art, { AGDATA_SPELL_DATA, -1 }, -1, 9, ANIM_DELAY_NONE, 6, ANIM_DELAY_NONE },
        /*  6 */ { anim_create_spell_projectile, { AGDATA_SELF_OBJ, AGDATA_SELF_TILE }, 5, 8, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { anim_add_parented_goal, { AGDATA_SCRATCH_OBJ, AGDATA_SELF_OBJ }, AG_SHOOT_SPELL, ANIM_FLOW_TERMINATE, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  8 */ { sub_4246C0, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  9 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, 8, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /* 10 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 11 */ { sub_4246C0, { AGDATA_SPELL_DATA, -1 }, -1, 13, ANIM_DELAY_NONE, 12, ANIM_DELAY_NONE },
        /* 12 */ { anim_begin_previous_anim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID_PREVIOUS }, -1, 13, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /* 13 */ { anim_begin_spell, { AGDATA_SPELL_DATA, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 14 */ { sub_4246C0, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, 4, ANIM_DELAY_NONE },
        /* 15 */ { anim_cleanup_spell, { AGDATA_SELF_OBJ, AGDATA_SPELL_DATA }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AFDD0
static AnimGoalNode anim_goal_node_throw_spell_w_cast_anim_secondary = {
    5,
    PRIORITY_5,
    1,
    1,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_eye_candy_not_applied, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { AGupdateAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 5, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_load_spell_visuals, { AGDATA_SPELL_DATA, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_PAUSE_TIME },
        /*  4 */ { AGbeginAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  5 */ { AGendAnimEyeCandy, { AGDATA_SELF_OBJ, -1 }, -1, 3, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_eye_candy, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5AFFD0
static AnimGoalNode anim_goal_node_back_off_from = {
    4,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_try_run, { -1, -1 }, -1, 2, ANIM_DELAY_NONE, 2, ANIM_DELAY_NONE },
        /*  2 */ { anim_set_range, { -1, -1 }, 9, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  3 */ { anim_target_in_range, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 4, ANIM_DELAY_NONE, ANIM_FLOW_PUSH | AG_MOVE_AWAY_FROM_OBJ, ANIM_DELAY_NONE },
        /*  4 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_PAUSE_TIME },
        /*  5 */ { 0 },
        /*  6 */ { 0 },
        /*  7 */ { 0 },
        /*  8 */ { 0 },
        /*  9 */ { 0 },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { NULL, { -1, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5B01D0
static AnimGoalNode anim_goal_node_attempt_use_pickpocket_skill_on = {
    9,
    PRIORITY_3,
    0,
    0,
    0,
    { -1, -1, -1 },
    {
        /*  1 */ { anim_is_not_animating, { AGDATA_SELF_OBJ, -1 }, -1, 2, ANIM_DELAY_NONE, 3, ANIM_DELAY_NONE },
        /*  2 */ { anim_advance_frame, { AGDATA_SELF_OBJ, -1 }, -1, 8, ANIM_DELAY_PAUSE_TIME, ANIM_FLOW_YIELD, ANIM_DELAY_PAUSE_TIME },
        /*  3 */ { anim_face_target, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 4, ANIM_DELAY_NONE },
        /*  4 */ { anim_run_skill_invocation, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 5, ANIM_DELAY_NONE },
        /*  5 */ { anim_skill_succeeded, { AGDATA_SELF_OBJ, AGDATA_TARGET_OBJ }, -1, 6, ANIM_DELAY_NONE, 8, ANIM_DELAY_NONE },
        /*  6 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0xC, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, 7, ANIM_DELAY_NONE },
        /*  7 */ { AGbeginAnimAnim, { AGDATA_SELF_OBJ, AGDATA_ANIM_ID }, -1, 9, ANIM_DELAY_NONE, ANIM_FLOW_YIELD, ANIM_DELAY_NONE },
        /*  8 */ { AGsetObjArtAnim, { AGDATA_SELF_OBJ, -1 }, 0, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_POP_GOAL, ANIM_DELAY_NONE },
        /*  9 */ { anim_spend_action_points, { AGDATA_SELF_OBJ, -1 }, 1, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE, ANIM_FLOW_TERMINATE, ANIM_DELAY_NONE },
        /* 10 */ { 0 },
        /* 11 */ { 0 },
        /* 12 */ { 0 },
        /* 13 */ { 0 },
        /* 14 */ { 0 },
        /* 15 */ { anim_cleanup_stand_anim, { AGDATA_SELF_OBJ, -1 }, 1, 0, ANIM_DELAY_NONE, 0, ANIM_DELAY_NONE },
    },
};

// 0x5B03D0
AnimGoalNode* anim_goal_nodes[] = {
    /*                          AG_ANIMATE */ &anim_goal_node_animate,
    /*                     AG_ANIMATE_LOOP */ &anim_goal_node_animate_loop,
    /*                      AG_ANIM_FIDGET */ &anim_goal_node_animate_fidget,
    /*                     AG_MOVE_TO_TILE */ &anim_goal_node_move_to_tile,
    /*                      AG_RUN_TO_TILE */ &anim_goal_node_run_to_tile,
    /*                     AG_ATTEMPT_MOVE */ &anim_goal_node_attempt_move,
    /*                    AG_MOVE_TO_PAUSE */ &anim_goal_node_move_to_pause,
    /*                   AG_MOVE_NEAR_TILE */ &anim_goal_node_move_near_tile,
    /*                    AG_MOVE_NEAR_OBJ */ &anim_goal_node_move_near_obj,
    /*                    AG_MOVE_STRAIGHT */ &anim_goal_node_move_straight,
    /*            AG_ATTEMPT_MOVE_STRAIGHT */ &anim_goal_node_attempt_move_straight,
    /*                        AG_OPEN_DOOR */ &anim_goal_node_open_door,
    /*                AG_ATTEMPT_OPEN_DOOR */ &anim_goal_node_attempt_open_door,
    /*                      AG_UNLOCK_DOOR */ NULL,
    /*                      AG_JUMP_WINDOW */ &anim_goal_node_jump_window,
    /*                      AG_PICKUP_ITEM */ &anim_goal_node_pickup_item,
    /*                   AG_ATTEMPT_PICKUP */ &anim_goal_node_attempt_pickup,
    /*                       AG_PICKPOCKET */ &anim_goal_node_pickpocket,
    /*                           AG_ATTACK */ &anim_goal_node_attack,
    /*                   AG_ATTEMPT_ATTACK */ &anim_goal_node_attempt_attack,
    /*                             AG_KILL */ &anim_goal_node_kill,
    /*                             AG_TALK */ &anim_goal_node_talk,
    /*                      AG_PICK_WEAPON */ NULL,
    /*                            AG_CHASE */ &anim_goal_node_chase,
    /*                           AG_FOLLOW */ &anim_goal_node_follow,
    /*                             AG_FLEE */ &anim_goal_node_flee,
    /*                      AG_THROW_SPELL */ &anim_goal_node_throw_spell,
    /*                    AG_ATTEMPT_SPELL */ &anim_goal_node_attempt_spell,
    /*                      AG_SHOOT_SPELL */ &anim_goal_node_shoot_spell,
    /*                     AG_HIT_BY_SPELL */ &anim_goal_node_hit_by_spell,
    /*                    AG_HIT_BY_WEAPON */ &anim_goal_node_hit_by_weapon,
    /*                            AG_DYING */ &anim_goal_node_dying,
    /*                      AG_DESTROY_OBJ */ &anim_goal_node_destroy_obj,
    /*                     AG_USE_SKILL_ON */ &anim_goal_node_use_skill_on,
    /*             AG_ATTEMPT_USE_SKILL_ON */ &anim_goal_node_attempt_use_skill_on,
    /*                    AG_SKILL_CONCEAL */ &anim_goal_node_skill_conceal,
    /*                       AG_PROJECTILE */ &anim_goal_node_projectile,
    /*                       AG_THROW_ITEM */ &anim_goal_node_throw_item,
    /*                       AG_USE_OBJECT */ &anim_goal_node_use_object,
    /*               AG_USE_ITEM_ON_OBJECT */ &anim_goal_node_use_item_on_object,
    /*    AG_USE_ITEM_ON_OBJECT_WITH_SKILL */ &anim_goal_node_use_item_on_object_with_skill,
    /*                 AG_USE_ITEM_ON_TILE */ &anim_goal_node_use_item_on_tile,
    /*      AG_USE_ITEM_ON_TILE_WITH_SKILL */ &anim_goal_node_use_item_on_tile_with_skill,
    /*                        AG_KNOCKBACK */ &anim_goal_node_knockback,
    /*                         AG_FLOATING */ &anim_goal_node_floating,
    /*                        AG_EYE_CANDY */ &anim_goal_node_eye_candy,
    /*                AG_EYE_CANDY_REVERSE */ &anim_goal_node_eye_candy_reverse,
    /*               AG_EYE_CANDY_CALLBACK */ &anim_goal_node_eye_candy_callback,
    /*       AG_EYE_CANDY_REVERSE_CALLBACK */ &anim_goal_node_eye_candy_reverse_callback,
    /*                       AG_CLOSE_DOOR */ &anim_goal_node_close_door,
    /*               AG_ATTEMPT_CLOSE_DOOR */ &anim_goal_node_attempt_close_door,
    /*                  AG_ANIMATE_REVERSE */ &anim_goal_node_animate_reverse,
    /*               AG_MOVE_AWAY_FROM_OBJ */ &anim_goal_node_move_away_from_obj,
    /*                           AG_ROTATE */ &anim_goal_node_rotate,
    /*                        AG_UNCONCEAL */ &anim_goal_node_unconceal,
    /*                    AG_RUN_NEAR_TILE */ &anim_goal_node_run_near_tile,
    /*                     AG_RUN_NEAR_OBJ */ &anim_goal_node_run_near_obj,
    /*                  AG_ANIMATE_STUNNED */ &anim_goal_node_animate_stunned,
    /*           AG_EYE_CANDY_END_CALLBACK */ &anim_goal_node_eye_candy_end_callback,
    /*   AG_EYE_CANDY_REVERSE_END_CALLBACK */ &anim_goal_node_eye_candy_reverse_end_callback,
    /*        AG_ANIMATE_KNEEL_MAGIC_HANDS */ &anim_goal_node_animate_kneel_magic_hands,
    /*                AG_ATTEMPT_MOVE_NEAR */ &anim_goal_node_attempt_move_near,
    /*                       AG_KNOCK_DOWN */ &anim_goal_node_knock_down,
    /*                      AG_ANIM_GET_UP */ &anim_goal_node_anim_get_up,
    /*  AG_ATTEMPT_MOVE_STRAIGHT_KNOCKBACK */ &anim_goal_node_attempt_move_straight_knockback,
    /*                           AG_WANDER */ &anim_goal_node_wander,
    /*             AG_WANDER_SEEK_DARKNESS */ &anim_goal_node_wander_seek_darkness,
    /*            AG_USE_PICKLOCK_SKILL_ON */ &anim_goal_node_use_picklock_skill_on,
    /*                      AG_PLEASE_MOVE */ &anim_goal_node_please_move,
    /*               AG_ATTEMPT_SPREAD_OUT */ &anim_goal_node_attempt_spread_out,
    /*                AG_ANIMATE_DOOR_OPEN */ &anim_goal_node_animate_door_open,
    /*              AG_ANIMATE_DOOR_CLOSED */ &anim_goal_node_animate_door_closed,
    /*                AG_PEND_CLOSING_DOOR */ &anim_goal_node_pend_closing_door,
    /*             AG_THROW_SPELL_FRIENDLY */ &anim_goal_node_throw_spell_friendly,
    /*           AG_ATTEMPT_SPELL_FRIENDLY */ &anim_goal_node_attempt_spell_friendly,
    /*               AG_EYE_CANDY_FIRE_DMG */ &anim_goal_node_eye_candy_fire_dmg,
    /*       AG_EYE_CANDY_REVERSE_FIRE_DMG */ &anim_goal_node_eye_candy_reverse_fire_dmg,
    /*            AG_ANIMATE_LOOP_FIRE_DMG */ &anim_goal_node_animate_loop_fire_dmg,
    /*      AG_ATTEMPT_MOVE_STRAIGHT_SPELL */ &anim_goal_node_attempt_move_straight_spell,
    /*             AG_MOVE_NEAR_OBJ_COMBAT */ &anim_goal_node_move_near_obj_combat,
    /*         AG_ATTEMPT_MOVE_NEAR_COMBAT */ &anim_goal_node_attempt_move_near_combat,
    /*                    AG_USE_CONTAINER */ &anim_goal_node_use_container,
    /*          AG_THROW_SPELL_W_CAST_ANIM */ &anim_goal_node_throw_spell_w_cast_anim,
    /*        AG_ATTEMPT_SPELL_W_CAST_ANIM */ &anim_goal_node_attempt_spell_w_cast_anim,
    /*   AG_THROW_SPELL_W_CAST_ANIM_2NDARY */ &anim_goal_node_throw_spell_w_cast_anim_secondary,
    /*                    AG_BACK_OFF_FROM */ &anim_goal_node_back_off_from,
    /*  AG_ATTEMPT_USE_PICKPOCKET_SKILL_ON */ &anim_goal_node_attempt_use_pickpocket_skill_on,
};

// NOTE: The thirteen `dword_5DE6xx` placeholders below, plus `dword_739E40`
// and `dword_739E44` from anim_private.c, are **savegame ballast**. Each is
// written by `anim_save` and read by `anim_load_internal` and touched by
// nothing else - and that is true of the original too, not just of this port.
// Checked against the exe with `search_instructions` on nine of them
// (0x5DE608, 5DE640, 5DE650, 5DE668, 5DE69C, 5DE6A0, 5DE6B0, 5DE6B8, 5DE6C0,
// 5DE6C4): every one is named by exactly two instructions in the whole image,
// the PUSH in `anim_save` and the PUSH in `anim_load_internal`. So Troika kept
// serialising state that nothing consumes, and CE is faithful in doing the
// same. They cannot be named individually because there is no use to infer a
// meaning from; they are documented here as a class instead.
//
// **Do not reorder them.** The save file's field order is load-bearing, and it
// is not address order. It was verified against the original: the `anim_save`
// PUSH sites run 0x421F3A, 0x421F6E, 0x421F88, 0x421FD6, 0x421FF0, 0x42200A,
// 0x422024, 0x42203E, 0x422058, 0x422072 - strictly increasing in the order CE
// writes them, including the three descending-address pairs that look like
// transcription slips and are not (6B8 before 6B0, 6A0 before 69C, 6C4 before
// 6C0). The three not queried are pinned by the uniform 0x1A stride between
// consecutive fwrite calls.
//
// 0x5DE608
static int dword_5DE608;

// 0x5DE610
static AnimFxList weapon_eye_candies;

// 0x5DE640
static int dword_5DE640;

// 0x5DE648
static int dword_5DE648;

// 0x5DE650
static int dword_5DE650;

// 0x5DE658
static int dword_5DE658;

// 0x5DE660
static int dword_5DE660;

// 0x5DE668
static int dword_5DE668;

// 0x5DE670
static AnimFxList anim_eye_candies;

// 0x5DE69C
static int dword_5DE69C;

// 0x5DE6A0
static int dword_5DE6A0;

// 0x5DE6A4
static int violence_filter;

// 0x5DE6B0
static int dword_5DE6B0;

// 0x5DE6B8
static int dword_5DE6B8;

// 0x5DE6C0
static int dword_5DE6C0;

// 0x5DE6C4
static int dword_5DE6C4;

/**
 * The delay a sub-node callback asks the interpreter for, in milliseconds,
 * picked up by the `ANIM_DELAY_CALLBACK` sentinel in the goal tables.
 *
 * The movement callbacks set it: 35ms between animation steps, 1000ms after
 * asking a blocking critter to move aside, 0 to carry straight on.
 */
// 0x5DE6CC
static int anim_callback_delay;

// NOTE: It's `bool`, but needs to be 4 byte integer because of saving/reading
// compatibility.
//
// 0x5DE6D0
static int anim_catch_up;

// 0x5DE6D4
static bool anim_editor;

// 0x5DE6D8
static int64_t anim_fidget_obj;

// Set while the game clock is paused (see `timeevent_pause`). Suppresses new
// fidget animations for the duration.
//
// 0x5DE6E0
static bool anim_paused;

// 0x5DE6E4
static int anim_float_offset_y;

// 0x421B00
bool anim_init(GameInitInfo* init_info)
{
    anim_editor = init_info->editor;

    if (!anim_editor) {
        if (!animfx_list_init(&anim_eye_candies)) {
            return false;
        }

        anim_eye_candies.path = "Rules\\AnimEyeCandy.mes";
        anim_eye_candies.capacity = 11;
        if (!animfx_list_load(&anim_eye_candies)) {
            return false;
        }

        if (!animfx_list_init(&weapon_eye_candies)) {
            return false;
        }

        weapon_eye_candies.path = "Rules\\WeaponEyeCandy.mes";
        weapon_eye_candies.capacity = 750;
        weapon_eye_candies.initial = 1;
        weapon_eye_candies.num_fields = 5;
        weapon_eye_candies.step = 10;
        if (!animfx_list_load(&weapon_eye_candies)) {
            return false;
        }
    }

    settings_register(&settings, VIOLENCE_FILTER_KEY, "0", violence_filter_changed);
    violence_filter_changed();

    settings_register(&settings, ALWAYS_RUN_KEY, "0", NULL);

    return true;
}

// 0x421BF0
void anim_exit(void)
{
    if (!anim_editor) {
        animfx_list_exit(&weapon_eye_candies);
        animfx_list_exit(&anim_eye_candies);
    }
}

// 0x421C20
void anim_reset(void)
{
    anim_num_allocated_run_indices = 0;
}

// 0x421C30
bool anim_id_to_run_info(AnimID* anim_id, AnimRunInfo** run_info_ptr)
{
    int index;

    ASSERT(anim_id != NULL); // pAnimID != NULL
    ASSERT(run_info_ptr != NULL); // ppRunInfo != NULL

    if (anim_id->slot_num != -1) {
        for (index = 0; index < 216; index++) {
            if (anim_id_matches_run_info(anim_id, &(anim_run_info[index]))) {
                *run_info_ptr = &(anim_run_info[index]);
                return true;
            }
        }
    }

    *run_info_ptr = NULL;
    return false;
}

// 0x421CE0
// True when `run_info` carries `anim_id`. The slot number is part of
// the identity only in single-player; the network path trusts the unique id.
bool anim_id_matches_run_info(AnimID* anim_id, AnimRunInfo* run_info)
{
    ASSERT(anim_id != NULL); // pAnimID != NULL
    ASSERT(run_info != NULL); // pRunInfo != NULL

    if (run_info->id.unique_id != anim_id->unique_id) {
        return false;
    }

    if (!tig_net_is_active()) {
        if (run_info->id.slot_num != anim_id->slot_num) {
            return false;
        }
    }

    return true;
}

// 0x421D60
bool anim_id_is_equal(AnimID* a, AnimID* b)
{
    ASSERT(a != NULL); // pAnimID1 != NULL
    ASSERT(b != NULL); // pAnimID2 != NULL

    if (a->unique_id != b->unique_id) {
        return false;
    }

    if (!tig_net_is_active()) {
        if (a->slot_num != b->slot_num) {
            return false;
        }
    }

    return true;
}

// 0x421DE0
void anim_id_init(AnimID* anim_id)
{
    ASSERT(anim_id != NULL); // pAnimID != NULL

    anim_id->slot_num = -1;
    anim_id->unique_id = -1;
    anim_id->field_8 = 0;
}

// 0x421E20
void anim_id_to_str(AnimID* anim_id, char* buffer)
{
    ASSERT(anim_id != NULL); // pAnimID != NULL
    ASSERT(buffer != NULL); // str != NULL

    snprintf(buffer, ANIM_ID_STR_SIZE,
        "[%d:%dr%d]",
        anim_id->slot_num,
        anim_id->unique_id,
        anim_id->field_8);
}

// 0x421EA0
bool anim_save(TigFile* stream)
{
    int cnt;
    int idx;
    int start;
    int extent_size;

    if (tig_file_fwrite(&anim_next_unique_id, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&animNumActiveGoals, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&anim_catch_up, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_739E44, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_739E40, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE650, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE658, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE608, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE640, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE648, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE660, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE668, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE6B8, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE6B0, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE6A0, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE69C, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE6C4, 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&dword_5DE6C0, 4, 1, stream) != 1) return false;

    cnt = 216;
    if (tig_file_fwrite(&cnt, 4, 1, stream) != 1) return false;

    idx = 0;
    while (idx < cnt) {
        start = idx;

        while (idx < cnt) {
            if ((anim_run_info[idx].flags & 0x01) == 0) {
                break;
            }
            idx++;
        }

        extent_size = idx - start;
        if (extent_size != 0) {
            if (tig_file_fwrite(&extent_size, sizeof(extent_size), 1, stream) != 1) {
                return false;
            }

            while (start < idx) {
                if (!anim_run_info_save(&(anim_run_info[start]), stream)) {
                    return false;
                }
                start++;
            }
        }

        while (idx < cnt) {
            if ((anim_run_info[idx].flags & 0x01) != 0) {
                break;
            }
            idx++;
        }

        extent_size = idx - start;
        if (extent_size > 0) {
            extent_size = -extent_size;
            if (tig_file_fwrite(&extent_size, sizeof(extent_size), 1, stream) != 1) {
                return false;
            }
        }
    }

    return true;
}

// 0x4221A0
void violence_filter_changed(void)
{
    violence_filter = settings_get_value(&settings, VIOLENCE_FILTER_KEY);
}

// 0x4221C0
bool anim_run_info_save(AnimRunInfo* run_info, TigFile* stream)
{
    int idx;

    if (stream == NULL) {
        return false;
    }

    if (tig_file_fwrite(&(run_info->id.slot_num), 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->id.unique_id), 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->id.field_8), 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->flags), 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->current_state), 4, 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->path_attached_to_stack_index), 4, 1, stream) != 1) return false;
    if (!object_save_obj_handle_safe(&(run_info->goals[0].params[0].obj), &(run_info->goals[0].field_B0[0]), stream)) return false;
    if (tig_file_fwrite(&(run_info->extra_target_tile), 8, 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->current_goal), 4, 1, stream) != 1) return false;

    for (idx = 0; idx <= run_info->current_goal; idx++) {
        if (!anim_goal_data_save(&(run_info->goals[idx]), stream)) {
            return false;
        }
    }

    if (tig_file_fwrite(&(run_info->path), sizeof(run_info->path), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->pause_time), sizeof(run_info->pause_time), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->next_ping_time), sizeof(run_info->next_ping_time), 1, stream) != 1) return false;

    return true;
}

// 0x422350
bool anim_goal_data_save(AnimGoalData* goal_data, TigFile* stream)
{
    int idx;
    AnimRunInfoParam param;

    if (stream == NULL) {
        return false;
    }

    if (tig_file_fwrite(&(goal_data->type), sizeof(goal_data->type), 1, stream) != 1) {
        return false;
    }

    for (idx = 0; idx < 5; idx++) {
        if (!anim_run_info_param_save(&(goal_data->params[idx]), &(goal_data->field_B0[idx]), anim_goal_data_param_types[idx], stream)) {
            return false;
        }
    }

    for (; idx < 20; idx++) {
        if (!anim_run_info_param_save(&(goal_data->params[idx]), NULL, anim_goal_data_param_types[idx], stream)) {
            return false;
        }
    }

    // Special case - sound handle is volatile, it's not intended to be
    // serialized.
    param.data = -1;
    if (!anim_run_info_param_save(&param, NULL, anim_goal_data_param_types[AGDATA_SOUND_HANDLE], stream)) {
        return false;
    }

    return true;
}

// 0x422430
bool anim_run_info_param_save(AnimRunInfoParam* param, ObjectLocator* a2, int type, TigFile* stream)
{
    if (stream == NULL) {
        return false;
    }

    switch (type) {
    case AGDATATYPE_INT:
        if (tig_file_fwrite(&(param->data), sizeof(param->data), 1, stream) != 1) return false;
        return true;
    case AGDATATYPE_OBJ:
        // __FILE__: C:\Troika\Code\Game\GameLibX\Anim.c
        // __LINE__: 750
        // pObjSafeData != NULL
        ASSERT(a2 != NULL);

        if (!object_save_obj_handle_safe(&(param->obj), a2, stream)) return false;
        return true;
    case AGDATATYPE_LOC:
        if (tig_file_fwrite(&(param->loc), sizeof(param->loc), 1, stream) != 1) return false;
        return true;
    case AGDATATYPE_SOUND:
        // Special case - reading 8 bytes even though sound handle is only 4
        // bytes.
        if (tig_file_fwrite(param, 8, 1, stream) != 1) return false;
        return true;
    }

    return false;
}

// 0x4224E0
bool anim_load(GameLoadInfo* load_info)
{
    bool loaded;

    in_anim_load = true;
    loaded = anim_load_internal(load_info);
    in_anim_load = false;

    anim_validate_active_goals("Anim Load");

    return loaded;
}

// 0x422520
bool anim_load_internal(GameLoadInfo* load_info)
{
    int cnt;
    int idx;
    int extent_size;

    if (load_info == NULL) {
        return false;
    }

    if (tig_file_fread(&anim_next_unique_id, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&animNumActiveGoals, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&anim_catch_up, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_739E44, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_739E40, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE650, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE658, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE608, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE640, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE648, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE660, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE668, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE6B8, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE6B0, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE6A0, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE69C, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE6C4, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&dword_5DE6C0, 4, 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&cnt, 4, 1, load_info->stream) != 1) return false;

    idx = 0;
    while (idx < cnt) {
        if (tig_file_fread(&extent_size, sizeof(extent_size), 1, load_info->stream) != 1) {
            return false;
        }

        if (extent_size > 0) {
            while (extent_size > 0) {
                if (!anim_run_info_load(&(anim_run_info[idx]), load_info->stream)) {
                    return false;
                }
                idx++;
                extent_size--;
            }
        } else if (extent_size < 0) {
            idx += -extent_size;
        }
    }

    return true;
}

// 0x4227F0
bool anim_run_info_load(AnimRunInfo* run_info, TigFile* stream)
{
    int index;

    if (stream == NULL) {
        return false;
    }

    if (tig_file_fread(&(run_info->id.slot_num), 4, 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->id.unique_id), 4, 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->id.field_8), 4, 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->flags), 4, 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->current_state), 4, 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->path_attached_to_stack_index), 4, 1, stream) != 1) return false;
    if (!object_load_obj_handle_safe(&(run_info->anim_obj), 0, stream)) return false;
    if (tig_file_fread(&(run_info->extra_target_tile), 8, 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->current_goal), 4, 1, stream) != 1) return false;

    for (index = 0; index <= run_info->current_goal; index++) {
        if (!anim_goal_data_load(&(run_info->goals[index]), stream)) {
            return false;
        }
    }

    if (tig_file_fread(&(run_info->path), sizeof(run_info->path), 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->pause_time), sizeof(run_info->pause_time), 1, stream) != 1) return false;
    if (tig_file_fread(&(run_info->next_ping_time), sizeof(run_info->next_ping_time), 1, stream) != 1) return false;

    if (run_info->current_goal >= 0) {
        run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
    }

    return true;
}

// 0x4229A0
bool anim_goal_data_load(AnimGoalData* goal_data, TigFile* stream)
{
    int idx;

    if (stream == NULL) {
        return false;
    }

    if (tig_file_fread(&(goal_data->type), sizeof(goal_data->type), 1, stream) != 1) {
        return false;
    }

    for (idx = 0; idx < 5; idx++) {
        if (!anim_run_info_param_load(&(goal_data->params[idx]), &(goal_data->field_B0[idx]), anim_goal_data_param_types[idx], stream)) {
            return false;
        }
    }

    for (; idx < AGDATA_COUNT; idx++) {
        if (!anim_run_info_param_load(&(goal_data->params[idx]), NULL, anim_goal_data_param_types[idx], stream)) {
            return false;
        }
    }

    return true;
}

// 0x422A50
bool anim_run_info_param_load(AnimRunInfoParam* param, ObjectLocator* a2, int type, TigFile* stream)
{
    if (stream == NULL) {
        return false;
    }

    switch (type) {
    case AGDATATYPE_INT:
        if (tig_file_fread(&(param->data), sizeof(param->data), 1, stream) != 1) return false;
        return true;
    case AGDATATYPE_OBJ:
        // __FILE__: C:\Troika\Code\Game\GameLibX\Anim.c
        // __LINE__: 949
        // pObjSafeData != NULL
        ASSERT(a2 != NULL);

        if (!object_load_obj_handle_safe(&(param->obj), a2, stream)) return false;
        return true;
    case AGDATATYPE_LOC:
        if (tig_file_fread(&(param->loc), sizeof(param->loc), 1, stream) != 1) return false;
        return true;
    case AGDATATYPE_SOUND:
        // Special case - reading 8 bytes even though sound handle is only 4
        // bytes.
        if (tig_file_fread(param, 8, 1, stream) != 1) return false;
        param->data = -1;
        return true;
    }

    return false;
}

// 0x422B10
void anim_break_nodes_to_map(const char* map)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    bool exists;
    int cnt;
    int idx;
    AnimRunInfo* run_info;

    snprintf(path, sizeof(path), "Save\\Current\\maps\\%s\\Anim.dat", map);

    exists = tig_file_exists(path, NULL);
    if (exists) {
        stream = tig_file_fopen(path, "r+b");
    } else {
        stream = tig_file_fopen(path, "wb");
    }

    if (stream == NULL) {
        tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Couldn't create TimeEvent data file for map!\n");
        ASSERT(0); // 1001, "0"
        return;
    }

    cnt = 0;

    if (!exists) {
        if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Writing Header to data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1011, "0"
            return;
        }
    } else {
        if (tig_file_fseek(stream, 0, SEEK_SET) != 0) {
            tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Seeking to start of data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1042, "0"
            return;
        }

        if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Reading Header to data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1025, "0"
            return;
        }

        if (tig_file_fseek(stream, 0, SEEK_END) != 0) {
            tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Seeking to end of data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1034, "0"
            return;
        }
    }

    for (idx = 0; idx < 216; idx++) {
        run_info = &(anim_run_info[idx]);
        if ((run_info->flags & 0x1) != 0) {
            if (!teleport_is_teleporting_obj(run_info->anim_obj)
                || !anim_goal_nodes[run_info->goals[0].type]->passive) {
                anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
            } else if (anim_run_info_save(run_info, stream)) {
                cnt++;
                anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
            } else {
                ASSERT(0); // 1067, "0"
                break;
            }
        }
    }

    if (idx < 216) {
        tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Failed to save out nodes!\n");
        ASSERT(0); // 1089, "0"
        tig_file_fclose(stream);
        tig_file_remove(path);
        return;
    }

    if (tig_file_fseek(stream, 0, SEEK_SET) != 0) {
        tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Writing Header to data file for map!\n");
        tig_file_fclose(stream);
        ASSERT(0); // 1111, "0"
        return;
    }

    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        tig_debug_printf("Anim: anim_break_nodes_to_map: ERROR: Writing Header to data file for map!\n");
        tig_file_fclose(stream);
        ASSERT(0); // 1103, "0"
        return;
    }

    tig_file_fclose(stream);
}

// 0x422DF0
void anim_save_nodes_to_map(const char* map)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    bool exists;
    int cnt;
    int idx;
    AnimRunInfo* run_info;

    snprintf(path, sizeof(path), "Save\\Current\\maps\\%s\\Anim.dat", map);

    exists = tig_file_exists(path, NULL);
    if (exists) {
        stream = tig_file_fopen(path, "r+b");
    } else {
        stream = tig_file_fopen(path, "wb");
    }

    if (stream == NULL) {
        tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Couldn't create TimeEvent data file for map!\n");
        ASSERT(0); // 1138, "0"
        return;
    }

    cnt = 0;

    if (!exists) {
        if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1148, "0"
            return;
        }
    } else {
        if (tig_file_fseek(stream, 0, SEEK_SET) != 0) {
            tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Seeking to start of data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1179, "0"
            return;
        }

        if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Reading Header to data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1162, "0"
            return;
        }

        if (tig_file_fseek(stream, 0, SEEK_END) != 0) {
            tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Seeking to end of data file for map!\n");
            tig_file_fclose(stream);
            ASSERT(0); // 1171, "0"
            return;
        }
    }

    for (idx = 0; idx < 216; idx++) {
        run_info = &(anim_run_info[idx]);
        if ((run_info->flags & 0x1) != 0) {
            if (!anim_run_info_save(run_info, stream)) {
                ASSERT(0); // 1199, "0"
                break;
            }

            cnt++;
        }
    }

    if (idx < 216) {
        tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Failed to save out nodes!\n");
        ASSERT(0); // 1208, "0"
        tig_file_fclose(stream);
        tig_file_remove(path);
        return;
    }

    if (tig_file_fseek(stream, 0, SEEK_SET) != 0) {
        tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
        tig_file_fclose(stream);
        ASSERT(0); // 1230, "0"
        return;
    }

    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        tig_debug_printf("Anim: anim_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
        tig_file_fclose(stream);
        ASSERT(0); // 1222, "0"
        return;
    }

    tig_file_fclose(stream);
}

// 0x4230A0
void anim_load_nodes_from_map(const char* map)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    int cnt;
    int idx;
    AnimID anim_id;
    AnimRunInfo run_info;

    snprintf(path, sizeof(path), "Save\\Current\\maps\\%s\\Anim.dat", map);

    if (!tig_file_exists(path, NULL)) {
        return;
    }

    stream = tig_file_fopen(path, "rb");
    if (stream == NULL) {
        tig_debug_printf("Anim: anim_load_nodes_from_map: ERROR: Couldn't open TimeEvent data file for map!\n");
        ASSERT(0); // 1255, "0"
        return;
    }

    if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
        tig_debug_printf("Anim: anim_load_nodes_from_map: ERROR: Reading Header to data file for map!\n");
        tig_file_fclose(stream);
        ASSERT(0); // 1264, "0"
        return;
    }

    for (idx = 0; idx < cnt; idx++) {
        if (!anim_run_info_load(&run_info, stream)) {
            break;
        }

        if ((anim_run_info[run_info.id.slot_num].flags & 0x1) != 0) {
            if (!anim_allocate_new_run_index(&anim_id)) {
                tig_debug_printf("Anim: anim_load_nodes_from_map: ERROR: Failed to allocate a run slot!\n");
                ASSERT(0); // 1282, "0"
                break;
            }
        } else {
            anim_id = run_info.id;
        }

        anim_run_info[anim_id.slot_num] = run_info;
        anim_run_info[anim_id.slot_num].id = anim_id;
        anim_run_info[anim_id.slot_num].cur_stack_data = &(anim_run_info[anim_id.slot_num].goals[anim_run_info[anim_id.slot_num].current_goal]);
        anim_goal_restart(&anim_id);
    }

    tig_file_fclose(stream);

    if (idx < cnt) {
        tig_debug_printf("Anim: anim_load_nodes_from_map: ERROR: Failed to load all nodes!\n");
        ASSERT(0); // 1307, "0"
    }

    tig_file_remove(path);
}

// 0x4232F0
void anim_debug_enable(void)
{
}

// 0x423300
bool anim_is_busy(int64_t obj, AnimID* anim_id)
{
    int prev = -1;
    int slot;
    AnimGoalNode* goal_node;

    slot = anim_find_first(obj);
    while (slot != -1 && slot != prev) {
        prev = slot;

        goal_node = anim_goal_nodes[anim_run_info[slot].goals[0].type];
        ASSERT(goal_node != NULL); // 1345, "pGoalNode != NULL"

        if (!goal_node->concurrent) {
            if (anim_id != NULL) {
                *anim_id = anim_run_info[slot].id;
            }
            return true;
        }

        slot = anim_find_next(slot, obj);
    }

    if (anim_id != NULL) {
        anim_id_init(anim_id);
    }

    return false;
}

// 0x4233D0
int anim_busy_priority(int64_t obj)
{
    int prev = -1;
    int slot;
    AnimGoalNode* goal_node;

    slot = anim_find_first(obj);
    while (slot != -1 && slot != prev) {
        prev = slot;

        goal_node = anim_goal_nodes[anim_run_info[slot].goals[0].type];
        ASSERT(goal_node != NULL); // 1383, "pGoalNode != NULL"

        if (!goal_node->concurrent) {
            return goal_node->priority_level;
        }

        slot = anim_find_next(slot, obj);
    }

    return 0;
}

// 0x423470
bool anim_is_idle(int64_t obj)
{
    int index;
    AnimRunInfo* run_info;
    tig_art_id_t art_id;

    index = anim_find_first(obj);
    run_info = &(anim_run_info[index]);
    if (run_info->current_goal != 0) {
        return false;
    }

    if (run_info->cur_stack_data == NULL) {
        run_info->cur_stack_data = &(run_info->goals[0]);
    }

    if (run_info->cur_stack_data->type > 2) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_id_anim_get(art_id) != TIG_ART_ANIM_STAND) {
        return false;
    }

    return true;
}

// 0x4234F0
bool anim_is_fidgeting(int64_t obj)
{
    return anim_is_current_goal_type(obj, AG_ANIM_FIDGET, NULL);
}

// 0x423530
// Empty in the original. Called once per anim timeevent when the run's
// object handle is still valid, before handle recovery.
void anim_timeevent_pre_obj_hook(AnimRunInfo* run_info)
{
    (void)run_info;
}

// 0x423540
// Empty in the original. Called immediately before each goal sub-node
// callback runs.
void anim_subnode_pre_callback(AnimRunInfo* run_info)
{
    (void)run_info;
}

// 0x423550
// Empty in the original. Called immediately after each goal sub-node
// callback, with the callback's return value.
void anim_subnode_post_callback(AnimRunInfo* run_info, int a2)
{
    (void)run_info;
    (void)a2;
}

// 0x423560
bool anim_timeevent_process(TimeEvent* timeevent)
{
    int run_index;
    AnimRunInfo* run_info;
    AnimGoalData* goal_data;
    AnimGoalNode* goal_node;
    AnimGoalSubNode* goal_subnode;
    char str[128];
    unsigned int state_change;
    int delay;
    bool err = false;
    int num_loops = 0;

    if (anim_out_of_slots) {
        anim_goal_interrupt_all_goals_of_priority(3);
        anim_out_of_slots = false;
    }

    run_index = timeevent->params[0].integer_value;

    ASSERT(run_index < 216); // 1965, "animRunIndex < ANIM_MAX_CURRENT_ANIMS"

    run_info = &(anim_run_info[run_index]);
    if (run_info->id.slot_num != run_index) {
        anim_id_to_str(&(run_info->id), str);
        tig_debug_printf("%s != %d:%d:%d\n",
            str,
            run_index,
            timeevent->params[1].integer_value,
            timeevent->params[2].integer_value);
        return true;
    }

    if ((run_info->flags & 0x01) == 0) {
        return true;
    }

    if ((run_info->flags & 0x10000) != 0) {
        unsigned int delay = run_info->pause_time.milliseconds;
        if (delay < 100) {
            delay = 100;
        }

        return anim_schedule_next_ping(run_info, &(timeevent->datetime), delay);
    }

    ASSERT(run_info->current_goal >= 0); // 2022, "pRunInfo->current_goal >= 0"

    if (run_info->current_goal < 0) {
        run_info->current_goal = 0;
    }

    goal_data = &(run_info->goals[run_info->current_goal]);
    run_info->cur_stack_data = goal_data;

    if (goal_data->type >= 0 && goal_data->type < ANIM_GOAL_MAX) {
        goal_node = anim_goal_nodes[goal_data->type];

        ASSERT(goal_node != NULL); // 2035, "pGoalNode != NULL"
    } else {
        run_info->flags |= 0x02;
        err = true;
    }

    if (!anim_recover_handles(run_info, NULL)) {
        return true;
    }

    if (run_info->anim_obj != OBJ_HANDLE_NULL) {
        if ((obj_field_int32_get(run_info->anim_obj, OBJ_F_FLAGS) & OF_DESTROYED) != 0) {
            ASSERT(0); // 2063, "!(object_flags_get(pRunInfo->animObj) & OF_DESTROYED)"
        }

        anim_timeevent_pre_obj_hook(run_info);
    } else {
        run_info->flags |= 0x02;
        err = true;
    }

    anim_current_run_index = run_index;

    while (!err) {
        if (++num_loops > 100) {
            tig_debug_printf("Anim: anim_timeevent_process: ERROR: Infinite loop detected in goal: %d, RunIdx: %d!\n",
                run_info->cur_stack_data->type,
                run_info->id.slot_num);

            ASSERT(num_loops < 100); // 2088, "numLoops < ANIM_MAX_LOOPS_ALLOWED"

            combat_turn_based_end_critter_turn(run_info->anim_obj);
            anim_current_run_index = -1;
            anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
            return true;
        }

        goal_subnode = &(goal_node->subnodes[run_info->current_state]);
        if (!anim_recover_handles(run_info, goal_subnode)) {
            return true;
        }

        anim_subnode_pre_callback(run_info);

        bool rc = goal_subnode->func(run_info);

        anim_subnode_post_callback(run_info, rc);

        if ((run_info->flags & 0x10000) != 0) {
            err = true;
        }

        if ((run_info->flags & 0x01) == 0) {
            anim_current_run_index = -1;
            return true;
        }

        if ((run_info->flags & 0x02) != 0) {
            err = true;
            break;
        }

        state_change = rc ? goal_subnode->success_state : goal_subnode->failure_state;
        delay = rc ? goal_subnode->success_delay : goal_subnode->failure_delay;

        if ((state_change & ANIM_FLOW_MASK) != 0) {
            if ((state_change & ANIM_FLOW_YIELD) != 0) {
                run_info->current_state = 0;
                err = true;
            }

            // NOTE: The two pops here are on top of the one below, which this
            // pattern also matches - `ANIM_FLOW_POP_3_GOALS` pops three goals.
            if ((state_change & ANIM_FLOW_POP_3_GOALS) == ANIM_FLOW_POP_3_GOALS) {
                anim_pop_goal(run_info, &state_change, &goal_node, &goal_data, &err);
                anim_pop_goal(run_info, &state_change, &goal_node, &goal_data, &err);
            }

            if ((state_change & ANIM_FLOW_POP_GOAL) == ANIM_FLOW_POP_GOAL) {
                anim_pop_goal(run_info, &state_change, &goal_node, &goal_data, &err);
            }

            if ((state_change & ANIM_FLOW_PUSH) != 0) {
                if (run_info->current_goal < 7) {
                    run_info->current_state = 0;
                    run_info->current_goal++;

                    goal_data = &(run_info->goals[run_info->current_goal]);
                    run_info->cur_stack_data = goal_data;

                    // A replace reuses the data the pop just left behind at
                    // this depth; a plain push starts from a copy of the
                    // caller's.
                    if (run_info->current_goal > 0 && (state_change & ANIM_FLOW_POP_GOAL) != ANIM_FLOW_POP_GOAL) {
                        *goal_data = run_info->goals[run_info->current_goal - 1];
                    }

                    goal_data->type = state_change & ANIM_FLOW_GOAL_MASK;
                    goal_node = anim_goal_nodes[goal_data->type];
                    anim_active_goal_count_increment(run_info, goal_node);
                    anim_validate_active_goals("Running: PushGoal");
                } else {
                    anim_id_to_str(&(run_info->id), str);
                    tig_debug_printf("Anim: ERROR: Attempt to PushGoal: Goal Stack too LARGE!!!  Killing the Animation Slot: AnimID: %s!\n", str);

                    for (int idx = 0; idx < run_info->current_goal; idx++) {
                        tig_debug_printf("\t[%d]: Goal: %s\n", idx, anim_goal_names[run_info->goals[idx].type]);
                    }

                    run_info->current_state = 0;
                    run_info->flags |= 0x02;

                    err = true;
                }
            }

            if ((state_change & ANIM_FLOW_TERMINATE) == ANIM_FLOW_TERMINATE) {
                run_info->flags |= 0x02;
                if (combat_turn_based_is_active() && !player_is_local_pc_obj(run_info->anim_obj)) {
                    combat_consume_action_points(run_info->anim_obj, combat_required_action_points_get());
                }

                for (int idx = 1; idx <= run_info->current_goal; idx++) {
                    anim_validate_active_goals("Running: Goal Terminate");

                    goal_subnode = &(anim_goal_nodes[run_info->goals[idx].type]->subnodes[14]);
                    if (goal_subnode->func != NULL
                        && anim_recover_handles(run_info, goal_subnode)) {
                        goal_subnode->func(run_info);
                    }
                }

                goal_subnode = &(anim_goal_nodes[run_info->goals[0].type]->subnodes[14]);
                if (goal_subnode->func != NULL
                    && anim_recover_handles(run_info, goal_subnode)) {
                    goal_subnode->func(run_info);
                }

                run_info->current_state = 0;
                run_info->path_attached_to_stack_index = -1;
                run_info->path.flags |= 0x01;

                err = true;
            }
        } else {
            ASSERT(run_info->current_state != state_change - 1); // 2334, "pRunInfo->current_state != (stateChange - 1)"

            run_info->current_state = state_change - 1;
        }

        if (delay != ANIM_DELAY_NONE) {
            err = true;
            if (delay == ANIM_DELAY_PAUSE_TIME) {
                delay = run_info->pause_time.milliseconds;
                break;
            }

            if (delay == ANIM_DELAY_CALLBACK) {
                delay = anim_callback_delay;
                break;
            }

            if (delay != ANIM_DELAY_RANDOM) {
                break;
            }

            if (tig_net_is_active()
                && !tig_net_is_host()) {
                delay = 0;
                break;
            }

            delay = random_between(0, 300);
        }
    }

    anim_current_run_index = -1;

    if ((run_info->flags & 0x02) != 0) {
        int64_t anim_obj = run_info->anim_obj;

        if (anim_obj == OBJ_HANDLE_NULL) {
            return true;
        }

        bool rc = anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);

        if (!combat_turn_based_is_active()) {
            combat_critter_fidget_restart(anim_obj);
            return rc;
        }

        if (obj_type_is_critter(obj_field_int32_get(anim_obj, OBJ_F_TYPE))) {
            combat_turn_based_npc_turn(anim_obj);
        }

        // UAP (0x423B96): call retargeted to UAP's relocated
        // `combat_action_points_get` at 0x4B7774; same getter, no behaviour
        // change.
        if (combat_action_points_get() > 0) {
            combat_critter_fidget_restart(anim_obj);
        }

        return rc;
    }

    if ((run_info->flags & 0x01) != 0) {
        return anim_schedule_next_ping(run_info, &(timeevent->datetime), delay);
    }

    return true;
}

// 0x423C80
// Schedules the run's next animation timeevent. The delay is expressed in
// game-time milliseconds and scaled by the anim timeevent's factor of 8.
bool anim_schedule_next_ping(AnimRunInfo* run_info, DateTime* a2, int delay)
{
    TimeEvent timeevent;
    DateTime datetime;

    datetime_from_milliseconds(&datetime, delay);
    datetime.milliseconds *= 8;

    timeevent.type = TIMEEVENT_TYPE_ANIM;
    timeevent.params[0].integer_value = run_info->id.slot_num;
    timeevent.params[1].integer_value = run_info->id.unique_id;
    timeevent.params[2].integer_value = 1111;

    if (anim_catch_up) {
        return timeevent_add_delay_base_at(&timeevent, &datetime, a2, &(run_info->next_ping_time));
    } else {
        return timeevent_add_delay_at(&timeevent, &datetime, &(run_info->next_ping_time));
    }
}

// 0x423D10
void anim_pop_goal(AnimRunInfo* run_info, unsigned int* state_change_ptr, AnimGoalNode** goal_node_ptr, AnimGoalData** goal_data_ptr, bool* err_ptr)
{
    // Popping the bottom goal ends the run, unless the flow code pushes a
    // replacement on the way out.
    if (run_info->current_goal == 0
        && (*state_change_ptr & ANIM_FLOW_PUSH) == 0) {
        run_info->flags |= 0x02;
    }

    // NOTE: The first test is dead - the caller only reaches here with
    // `ANIM_FLOW_POP_GOAL` set, which is inside `ANIM_FLOW_REPLACE_GOAL`.
    if ((*goal_node_ptr)->subnodes[14].func != NULL
        && ((*state_change_ptr & ANIM_FLOW_REPLACE_GOAL) == 0
            || (*state_change_ptr & ANIM_FLOW_NO_CLEANUP) == 0)
        && anim_recover_handles(run_info, &((*goal_node_ptr)->subnodes[14]))) {
        (*goal_node_ptr)->subnodes[14].func(run_info);
    }

    if ((*state_change_ptr & ANIM_FLOW_KEEP_PATH) == 0) {
        run_info->path.maxPathLength = 0;
        run_info->flags &= ~0x83C;
    }

    if ((*state_change_ptr & ANIM_FLOW_RESET_PATH) != 0) {
        run_info->path.flags = 0x01;
    }

    anim_active_goal_count_decrement(run_info, *goal_node_ptr);

    run_info->current_goal--;

    if (run_info->current_goal >= 0) {
        run_info->cur_stack_data = *goal_data_ptr = &(run_info->goals[run_info->current_goal]);
        *goal_node_ptr = anim_goal_nodes[(*goal_data_ptr)->type];
        *err_ptr = false;
    } else {
        if ((*state_change_ptr & ANIM_FLOW_PUSH) == 0) {
            run_info->flags |= 0x02;
        }
    }
}

// 0x423E60
void anim_validate_active_goals(const char* msg)
{
    int cnt;

    cnt = anim_goal_pending_active_goals_count();
    if (cnt != animNumActiveGoals) {
        tig_debug_printf("Anim: Num Active Goals Failure: %s, Expected: %d, Actual: %d!\n",
            msg,
            animNumActiveGoals,
            cnt);

        if (!in_anim_load) {
            ASSERT(0); // 2517, "0"
        }
    }
}

// 0x423EB0
int anim_goal_pending_active_goals_count(void)
{
    int index;
    AnimRunInfo* run_info;
    int cnt = 0;
    int stack_index;

    for (index = 0; index < 216; index++) {
        run_info = &(anim_run_info[index]);
        if ((run_info->flags & 0x1) != 0) {
            for (stack_index = 0; stack_index <= run_info->current_goal; stack_index++) {
                ASSERT(run_info->goals[stack_index].type >= 0); // pRunInfo->goal_stack[j].goal_type >= 0
                ASSERT(run_info->goals[stack_index].type < ANIM_GOAL_MAX); // pRunInfo->goal_stack[j].goal_type < anim_goal_max

                if (run_info->goals[stack_index].type >= 0 && run_info->goals[stack_index].type < ANIM_GOAL_MAX) {
                    if (anim_goal_nodes[run_info->goals[stack_index].type]->priority_level >= 2
                        && !anim_goal_nodes[run_info->goals[stack_index].type]->concurrent) {
                        cnt++;
                    }
                } else {
                    tig_debug_printf("Anim: anim_goal_pending_active_goals_count: ERROR: goal Type invalid: %d, Slot: %d, Stack Index: %d!\n",
                        run_info->goals[stack_index].type,
                        index,
                        stack_index);
                    mp_deallocate_run_index(&(run_info->id));
                }
            }
        }
    }

    return cnt;
}

// 0x423FB0
// Recomputes animNumActiveGoals from the live run table. CE has no caller:
// the original calls it from the slot-reset walk at 0x44D2B6, which is not
// ported, but the helper is kept as the binary's address anchor.
void anim_refresh_active_goal_count(void)
{
    animNumActiveGoals = anim_goal_pending_active_goals_count();
}

// 0x423FC0
void anim_catch_up_enable(void)
{
    anim_catch_up = true;
}

// 0x423FD0
void anim_catch_up_disable(void)
{
    anim_catch_up = false;
}

// 0x423FE0
void anim_all_goals_finished_callback_set(void (*func)(void))
{
    anim_all_goals_finished_callback = func;
}

/**
 * Force-interrupts every animation slot belonging to `obj`, regardless of goal
 * priority. Sibling of `anim_interrupt_all_goals_of_obj`, which respects
 * priorities.
 *
 * 0x423FF0
 */
bool anim_force_interrupt_all_goals_of_obj(int64_t obj)
{
    int prev = -1;
    int slot;

    if (light_scheme_is_changing()) {
        return true;
    }

    slot = anim_find_first(obj);
    while (slot != -1 && slot != prev) {
        prev = slot;
        if (!anim_force_interrupt(&(anim_run_info[slot].id))) {
            return false;
        }

        slot = anim_find_next(slot, obj);
    }

    return true;
}

/**
 * Interrupts every animation slot belonging to `obj` whose current goal is
 * beaten by `priority_level` (see `anim_interrupt`). Callers use it to clear a
 * critter's running animations before starting a new goal, and pass the new
 * goal's priority. The per-object counterpart of the global
 * `anim_goal_interrupt_all_goals_of_priority`; compare
 * `anim_interrupt_all_goals_of_type`, which filters by root goal type instead
 * of priority.
 *
 * `concurrent` mirrors `AnimGoalNode.concurrent` of the goal about to start
 * (see the identical idiom in `anim_interrupt_all_goals_of_type`): a
 * concurrent goal must not disturb the animations it will run alongside, so
 * the sweep runs at PRIORITY_NONE. `a4` suppresses the multiplayer broadcast
 * (Packet9); it is set when the caller is itself reacting to a remote event or
 * performing a local-only cleanup.
 *
 * 0x424070
 */
bool anim_interrupt_all_goals_of_obj(int64_t obj, int priority_level, bool concurrent, bool a4)
{
    int prev = -1;
    int slot;

    ASSERT(priority_level >= PRIORITY_NONE && priority_level < PRIORITY_HIGHEST); // (priorityLevel >= priorityNone)&&(priorityLevel <= priorityHighest)

    if (!a4) {
        if (tig_net_is_active() && player_is_pc_obj(obj)) {
            Packet9 pkt;

            pkt.type = 9;
            pkt.field_4 = 0;

            if (tig_net_is_host()) {
                object_ref_init(obj, &(pkt.field_18));
                pkt.priority_level = priority_level;
                pkt.field_48 = concurrent;
                pkt.loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
                pkt.art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
                pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
                pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
                tig_net_send_app_all(&pkt, sizeof(pkt));
            } else {
                object_ref_init(obj, &(pkt.field_18));
                pkt.priority_level = priority_level;
                pkt.field_48 = concurrent;
                pkt.loc = 0;
                pkt.art_id = TIG_ART_ID_INVALID;
                pkt.offset_x = 0;
                pkt.offset_y = 0;
                tig_net_send_app_all(&pkt, sizeof(pkt));
            }
        }
    }

    if (concurrent) {
        priority_level = PRIORITY_NONE;
    }

    slot = anim_find_first(obj);
    while (slot != -1 && slot != prev) {
        prev = slot;
        if (!anim_interrupt(&(anim_run_info[slot].id), priority_level)) {
            return false;
        }

        slot = anim_find_next(slot, obj);
    }

    return true;
}

// 0x424250
bool anim_goal_interrupt_all_goals(void)
{
    int index;

    if (anim_num_allocated_run_indices > 0) {
        for (index = 0; index < 216; index++) {
            if ((anim_run_info[index].flags & 0x1) != 0
                && !anim_interrupt(&(anim_run_info[index].id), PRIORITY_HIGHEST)) {
                return false;
            }
        }
    }

    return true;
}

// 0x424290
bool anim_goal_interrupt_all_goals_of_priority(int priority_level)
{
    int index;

    ASSERT(priority_level >= PRIORITY_NONE && priority_level < PRIORITY_HIGHEST); // (priorityLevel >= priorityNone)&&(priorityLevel <= priorityHighest)

    for (index = 0; index < 216; index++) {
        if ((anim_run_info[index].flags & 0x1) != 0
            && !anim_interrupt(&(anim_run_info[index].id), priority_level)) {
            tig_debug_printf("Anim: anim_goal_interrupt_all_goals_of_priority: ERROR: Failed to interrupt slot: %d!\n", index);
        }
    }

    return true;
}

// 0x424310
bool anim_goal_interrupt_all_for_tb_combat(void)
{
    int index = 0;
    AnimRunInfo* run_info;

    for (index = 0; index < 216; index++) {
        run_info = &(anim_run_info[index]);
        if ((run_info->flags & 0x1) != 0
            && !anim_goal_is_endless(run_info)
            && !anim_interrupt(&(run_info->id), PRIORITY_3)) {
            tig_debug_printf("Anim: anim_goal_interrupt_all_for_tb_combat: ERROR: Failed to interrupt slot: %d!\n", index);
        }
    }

    return true;
}

// 0x4243E0
bool anim_eye_candy_interrupt(int64_t obj, tig_art_id_t eye_candy_id, int mt_id)
{
    AnimID prev_anim_id;
    AnimID cur_anim_id;
    unsigned int num;
    AnimRunInfo* run_info;

    anim_id_init(&prev_anim_id);

    if (obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if (!anim_find_first_of_type(obj, AG_EYE_CANDY, &cur_anim_id)) {
        return true;
    }

    num = tig_art_num_get(eye_candy_id);

    do {
        if (anim_id_is_equal(&prev_anim_id, &cur_anim_id)) {
            break;
        }

        run_info = &(anim_run_info[cur_anim_id.slot_num]);

        ASSERT(run_info->current_goal > -1); // 2839, "pRunInfo->current_goal > -1"
        ASSERT(run_info->current_goal < 14); // 2840, "pRunInfo->current_goal < ANIM_GOAL_MAX_SUBNODES"

        if (run_info->current_goal > -1 && run_info->current_goal < 14) {
            if (run_info->cur_stack_data == NULL) {
                run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
            }

            if (run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data == mt_id
                && tig_art_num_get(run_info->cur_stack_data->params[AGDATA_ANIM_ID].data) == num
                && anim_interrupt(&(run_info->id), PRIORITY_HIGHEST)) {
                break;
            }
        }

        prev_anim_id = cur_anim_id;
    } while (anim_find_next_of_type(obj, AG_EYE_CANDY, &cur_anim_id));

    return true;
}

// 0x424560
bool anim_eye_candy_is_active(int64_t obj, tig_art_id_t eye_candy_id, int mt_id)
{
    AnimID prev_anim_id;
    AnimID cur_anim_id;
    unsigned int num;
    AnimRunInfo* run_info;

    anim_id_init(&prev_anim_id);

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_find_first_of_type(obj, AG_EYE_CANDY, &cur_anim_id)) {
        return false;
    }

    num = tig_art_num_get(eye_candy_id);

    do {
        if (anim_id_is_equal(&prev_anim_id, &cur_anim_id)) {
            break;
        }

        run_info = &(anim_run_info[cur_anim_id.slot_num]);
        if (run_info->cur_stack_data == NULL) {
            run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
        }

        if (run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data == mt_id
            && tig_art_num_get(run_info->cur_stack_data->params[AGDATA_ANIM_ID].data) == num) {
            return true;
        }

        prev_anim_id = cur_anim_id;
    } while (anim_find_next_of_type(obj, AG_EYE_CANDY, &cur_anim_id));

    return false;
}

// 0x4246C0
bool sub_4246C0(AnimRunInfo* run_info)
{
    (void)run_info;

    return 1;
}

// 0x4246D0
bool anim_is_not_turn_based(AnimRunInfo* run_info)
{
    (void)run_info;

    return combat_turn_based_is_active() == 0;
}

// 0x4246E0
bool anim_add_parented_goal(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t parent_obj;
    AnimGoalData goal_data;
    AnimID anim_id;
    int idx;

    if (tig_net_is_active()
        && !tig_net_is_host()
        && !multiplayer_is_locked()) {
        return true;
    }

    obj = run_info->params[0].obj;
    parent_obj = run_info->params[1].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 3001, "obj != OBJ_HANDLE_NULL"
    ASSERT(parent_obj != OBJ_HANDLE_NULL); // 3002, "parentObj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL || parent_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    anim_goal_data_init(&goal_data, obj, run_info->params[2].data);

    for (idx = 1; idx < AGDATA_COUNT; idx++) {
        goal_data.params[idx] = run_info->cur_stack_data->params[idx];
    }

    goal_data.params[AGDATA_ANIM_ID].data = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    goal_data.params[AGDATA_PARENT_OBJ].obj = parent_obj;

    if (!anim_goal_add(&goal_data, &anim_id)) {
        return false;
    }

    return true;
}

// 0x424820
bool anim_create_spell_projectile(AnimRunInfo* run_info)
{
    ObjectID oid;

    oid.type = OID_TYPE_NULL;

    return anim_projectile_create(run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data,
        run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj,
        run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj,
        run_info->params[1].loc,
        run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc,
        run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data,
        &(run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj),
        run_info->id,
        oid);
}

// 0x4248A0
bool anim_projectile_create(tig_art_id_t art_id, int64_t self_obj, int64_t target_obj, int64_t loc, int64_t target_loc, int spell, int64_t* obj_ptr, AnimID anim_id, ObjectID oid)
{
    int64_t proto_obj;
    int64_t self_loc;
    int rotation;

    ASSERT(obj_ptr != NULL); // 3074, "obj != NULL"

    if (multiplayer_is_locked() || tig_net_is_host()) {
        proto_obj = proto_get_by_description(BP_PROJECTILE);

        if (tig_net_is_active()
            && !tig_net_is_host()) {
            if (!object_create_ex(proto_obj, loc, oid, obj_ptr)) {
                ASSERT(0); // 3085, "0"
                exit(EXIT_FAILURE);
            }
        } else {
            if (!object_create(proto_obj, loc, obj_ptr)) {
                ASSERT(0); // 3090, "0"
                exit(EXIT_FAILURE);
            }
        }

        if (tig_net_is_active()
            && tig_net_is_host()) {
            Packet6 pkt;

            pkt.type = 6;
            pkt.subtype = 0;
            pkt.art_id = art_id;

            if (self_obj != OBJ_HANDLE_NULL) {
                pkt.self_oid = obj_get_id(self_obj);
            } else {
                pkt.self_oid.type = OID_TYPE_NULL;
            }

            if (target_obj != OBJ_HANDLE_NULL) {
                pkt.target_oid = obj_get_id(self_obj);
            } else {
                pkt.target_oid.type = OID_TYPE_NULL;
            }

            pkt.loc = loc;
            pkt.target_loc = target_loc;
            pkt.spell = spell;
            pkt.anim_id = anim_id;
            pkt.obj_oid = obj_get_id(*obj_ptr);
            tig_net_send_app_all(&pkt, sizeof(pkt));
        }

        object_flags_set(*obj_ptr, OF_DONTLIGHT);

        if (art_id != TIG_ART_ID_INVALID) {
            if (target_obj != OBJ_HANDLE_NULL) {
                target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
            }

            self_loc = obj_field_int64_get(self_obj, OBJ_F_LOCATION);
            rotation = combat_projectile_rot(self_loc, target_loc);
            art_id = combat_projectile_art_id_rotation_set(art_id, rotation);

            obj_field_int32_set(*obj_ptr, OBJ_F_AID, art_id);
            obj_field_int32_set(*obj_ptr, OBJ_F_CURRENT_AID, art_id);
        } else {
            ASSERT(0); // 3126, "0"
        }

        magictech_projectile_impact_fx(spell, *obj_ptr);
    }

    return true;
}

// 0x424BC0
bool anim_set_projectile_rotation(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t parent_obj;
    int64_t target_loc;
    tig_art_id_t art_id;
    int64_t parent_loc;
    int rotation;

    source_obj = run_info->params[0].obj;

    if (run_info->cur_stack_data == NULL) {
        run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
    }

    parent_obj = run_info->cur_stack_data->params[AGDATA_PARENT_OBJ].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 3202, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(parent_obj != OBJ_HANDLE_NULL); // 3203, "parentObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (parent_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (run_info->params[1].obj != OBJ_HANDLE_NULL) {
        target_loc = obj_field_int64_get(run_info->params[1].obj, OBJ_F_LOCATION);
    } else {
        target_loc = run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc;
    }

    // NOTE: Looks meaningless - `combat_projectile_art_id_rotation_set` returns
    // the rotated aid and nothing here stores it - but it is Troika's: 0x424BC0
    // in the exe drops the return value in exactly the same way. Faithful, so
    // left alone.
    art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
    parent_loc = obj_field_int64_get(parent_obj, OBJ_F_LOCATION);
    rotation = combat_projectile_rot(parent_loc, target_loc);
    combat_projectile_art_id_rotation_set(art_id, rotation);

    return true;
}

// 0x424D00
bool anim_destroy_obj(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int p_piece;

    obj = run_info->params[0].obj;
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_DESTROYED) != 0) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    p_piece = tig_art_wall_id_p_piece_get(art_id);
    if (tig_art_type(art_id) != TIG_ART_TYPE_WALL
        || p_piece == 0
        || p_piece == 2) {
        object_flags_set(obj, OF_OFF);
    }

    object_destroy(obj);

    return true;
}

// 0x424D90
bool anim_is_at_location(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t loc;

    obj = run_info->params[0].obj;
    loc = run_info->params[1].loc;

    ASSERT(obj != OBJ_HANDLE_NULL); // 3321, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    return obj_field_int64_get(obj, OBJ_F_LOCATION) == loc;
}

// 0x424E00
bool anim_pick_step_aside_tile(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t source_loc;
    int64_t target_loc;
    int rot;
    int64_t adjacent_locs[8];
    int64_t adjacent_objs[8];
    int idx;

    source_obj = run_info->params[0].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 3344, "sourceObj != OBJ_HANDLE_NULL"

    // TODO: Unclear if it checks loc or obj.
    if (run_info->params[1].loc != 0) {
        anim_callback_delay = 0;
        return true;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);

    if (run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj != OBJ_HANDLE_NULL) {
        target_loc = obj_field_int64_get(run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj, OBJ_F_LOCATION);
    } else {
        target_loc = 0;
    }

    rot = random_between(0, 8);

    for (idx = rot; idx < 8; idx++) {
        if (location_in_dir(source_loc, idx, &(adjacent_locs[idx]))) {
            adjacent_objs[idx] = OBJ_HANDLE_NULL;
            if (!anim_tile_is_blocked(source_obj, source_loc, adjacent_locs[idx], rot)) {
                run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = adjacent_locs[idx];
                return true;
            }
        }
    }

    for (idx = 0; idx < rot; idx++) {
        if (location_in_dir(source_loc, idx, &(adjacent_locs[idx]))) {
            adjacent_objs[idx] = OBJ_HANDLE_NULL;
            if (!anim_tile_is_blocked(source_obj, source_loc, adjacent_locs[idx], idx)) {
                run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = adjacent_locs[idx];
                return true;
            }
        }
    }

    for (idx = rot; idx < 8; idx++) {
        if (adjacent_locs[idx] != target_loc) {
            if (anim_goal_please_move(source_obj, adjacent_objs[idx])) {
                anim_callback_delay = 1000;
                run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = adjacent_locs[idx];
                return true;
            }
        }
    }

    for (idx = 0; idx < rot; idx++) {
        if (adjacent_locs[idx] != target_loc) {
            if (anim_goal_please_move(source_obj, adjacent_objs[idx])) {
                anim_callback_delay = 1000;
                run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = adjacent_locs[idx];
                return true;
            }
        }
    }

    return false;
}

// 0x425130
bool anim_target_in_range(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int range;
    int64_t source_loc;
    int64_t target_loc;
    int rot;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 3532, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 3533, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);

    if (location_dist(source_loc, target_loc) > range) {
        return false;
    }

    rot = location_rot(source_loc, target_loc);
    if (anim_target_traversal_is_blocked(source_obj, source_loc, target_loc, rot, target_obj)) {
        return false;
    }

    return true;
}

// 0x425270
bool anim_tile_in_range(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_loc;
    int range;
    int64_t source_loc;

    source_obj = run_info->params[0].obj;
    target_loc = run_info->params[1].loc;
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 3584, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_loc != 0); // 3585, "targetLoc != 0"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_loc == 0) {
        return false;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    if (location_dist(source_loc, target_loc) > range) {
        return false;
    }

    return true;
}

// 0x425340
bool anim_target_in_walk_range(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t range;
    int64_t source_loc;
    int64_t target_loc;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data + 2;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 3613, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 3614, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);

    if (location_dist(source_loc, target_loc) > range) {
        return false;
    }

    return true;
}

// 0x425430
bool anim_finish_move(AnimRunInfo* run_info)
{
    if ((run_info->path.flags & 0xC) == 0) {
        return false;
    }

    anim_spend_move_cost(run_info);

    if (run_info->current_goal > 0) {
        run_info->goals[run_info->current_goal - 1].params[AGDATA_SCRATCH_VAL4].data = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data;
    }

    run_info->path.flags &= ~0x4;
    run_info->path.flags |= 0x1;

    run_info->flags &= ~0x30;

    if ((run_info->path.flags & 0x8) != 0
        && (!tig_net_is_active()
            || tig_net_is_host())) {
        run_info->flags |= 0x2;
    }

    return true;
}

// 0x4254C0
bool anim_finish_move_near(AnimRunInfo* run_info)
{
    if (run_info->params[1].obj != OBJ_HANDLE_NULL
        && run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc != obj_field_int64_get(run_info->params[1].obj, OBJ_F_LOCATION)) {
        run_info->path.flags |= 0x4;
    }

    if ((run_info->path.flags & 0xC) == 0) {
        return false;
    }

    anim_spend_move_cost(run_info);

    if (run_info->current_goal > 0) {
        run_info->goals[run_info->current_goal - 1].params[AGDATA_SCRATCH_VAL4].data = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data;
    }

    run_info->path.flags &= ~0x4;
    run_info->path.flags |= 0x1;

    run_info->flags &= ~0x30;

    if ((run_info->path.flags & 0x8) != 0
        && (!tig_net_is_active()
            || tig_net_is_host())) {
        run_info->flags |= 0x2;
    }

    return true;
}

// 0x425590
bool anim_finish_move_near_combat(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t target_loc = 0;
    int64_t v1;
    int64_t weapon_obj;
    int range;
    int goal;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    if (target_obj != OBJ_HANDLE_NULL) {
        target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
        if (run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc != target_loc) {
            run_info->path.flags |= 0x04;
        }
    }

    if ((run_info->path.flags & 0x04) == 0 && target_loc != 0) {
        if (anim_can_reach_target(source_obj, target_obj)) {
            return false;
        }

        if (ai_line_of_fire_cost(source_obj, target_loc, &v1) < 26
            && (v1 == OBJ_HANDLE_NULL || v1 == target_obj)) {
            weapon_obj = combat_critter_weapon(source_obj);
            if (weapon_obj != OBJ_HANDLE_NULL) {
                range = item_weapon_range(weapon_obj, source_obj);
                for (goal = 0; goal < run_info->current_goal; goal++) {
                    run_info->goals[goal].params[AGDATA_RANGE_DATA].data = range;
                }
                run_info->path.flags |= 0x04;
            }
        }
    }

    if ((run_info->path.flags & 0x0C) == 0) {
        return false;
    }

    anim_spend_move_cost(run_info);

    if (run_info->current_goal > 0) {
        // NOTE: Verified at 0x4256CF-0x4256ED - the original copies
        // AGDATA_SCRATCH_VAL4 from the current stack data the same way.
        run_info->goals[run_info->current_goal - 1].params[AGDATA_SCRATCH_VAL4].data = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data;
    }

    run_info->path.flags &= ~0x04;
    run_info->path.flags |= 0x01;

    run_info->path.flags &= ~0x30;

    if ((run_info->path.flags & 0x08) != 0
        && (!tig_net_is_active()
            || tig_net_is_host())) {
        run_info->flags |= 0x02;
    }

    return true;
}

/**
 * The run has a path to walk: `path.flags` bit 0x1 marks the path slot empty,
 * and is cleared once one is computed.
 */
// 0x425740
bool anim_has_path(AnimRunInfo* run_info)
{
    return (run_info->path.flags & 0x1) == 0;
}

// 0x425760
// True when this critter cannot step from `loc` into `adjacent_loc`: the tile
// itself blocks, or object traversal blocks with the critter's portal/window
// capabilities taken into account.
bool anim_tile_is_blocked(int64_t obj, int64_t loc, int64_t adjacent_loc, int rot)
{
    unsigned int flags = 0;

    anim_get_obj_traversal_flags(obj, &flags);

    if (tile_is_blocking(adjacent_loc, false)) {
        return true;
    }

    return object_traversal_is_blocked(obj, loc, rot, flags, NULL);
}

// 0x4257E0
// Collects the OBJ_TRAVERSAL_* flags implied by the critter's current form
// and abilities (polymorphed, cannot open portals, cannot jump windows).
void anim_get_obj_traversal_flags(int64_t obj, unsigned int* flags_ptr)
{
    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_POLYMORPHED) != 0) {
        *flags_ptr |= OBJ_TRAVERSAL_PORTAL_BLOCKS | OBJ_TRAVERSAL_WINDOW_BLOCKS;
    }

    if (!critter_can_open_portals(obj)) {
        *flags_ptr |= OBJ_TRAVERSAL_PORTAL_BLOCKS;
    }

    if (!critter_can_jump_window(obj)) {
        *flags_ptr |= OBJ_TRAVERSAL_WINDOW_BLOCKS;
    }
}

// 0x425840
// True when traversal from `a2` to `a3` is blocked. `a5` (the target object)
// is temporarily given OF_NO_BLOCK so a container or NPC does not block the
// movement that is trying to reach it.
bool anim_target_traversal_is_blocked(int64_t a1, int64_t a2, int64_t a3, int a4, int64_t a5)
{
    unsigned int flags = 0;
    bool v1 = true;
    bool v2 = false;
    int64_t v3;
    int v4;

    anim_get_obj_traversal_flags(a1, &flags);

    if (tile_is_blocking(a3, false)) {
        return true;
    }

    if ((obj_field_int32_get(a5, OBJ_F_FLAGS) & OF_NO_BLOCK) == 0) {
        object_flags_set(a5, OF_NO_BLOCK);
        v1 = false;
    }

    if ((object_calc_traversal_cost(a1, a2, a4, flags, &v3, &v4, NULL) || v3) && !v4) {
        v2 = true;
    }

    if (!v1) {
        object_flags_unset(a5, OF_NO_BLOCK);
    }

    return v2;
}

// 0x425930
bool anim_create_wander_path(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t loc;
    int range;
    int x;
    int y;
    int64_t target_loc;
    PathCreateInfo path_create_info;

    ASSERT(run_info != NULL); // 4071, "pRunInfo != NULL"

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4075, "obj != OBJ_HANDLE_NULL"

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        run_info->path.flags = 0x01;
        return true;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;
    x = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    y = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;

    x += random_between(-range, range);
    y += random_between(-range, range);
    target_loc = location_make(x, y);

    run_info->path_attached_to_stack_index = run_info->current_goal + 1;

    path_create_info.obj = obj;
    path_create_info.max_rotations = anim_path_calc_max_length(&(run_info->path), loc, target_loc, obj);
    path_create_info.from = loc;
    path_create_info.to = target_loc;
    path_create_info.rotations = run_info->path.rotations;
    path_create_info.flags = 0;

    if (anim_set_path_flags(&path_create_info, 1)) {
        run_info->path.max = path_make(&path_create_info);
    } else {
        run_info->path.max = 0;
    }

    if (run_info->path.max == 0 || run_info->path.max > range) {
        path_create_info.flags = PATH_FLAG_0x0001;
        if (!anim_set_path_flags(&path_create_info, 1)) {
            if (!player_is_pc_obj(obj)) {
                combat_turn_based_end_critter_turn(obj);
            }
            return false;
        }

        run_info->path.max = path_make(&path_create_info);
        if (run_info->path.max == 0 || run_info->path.max > range) {
            if (!player_is_pc_obj(obj)) {
                combat_turn_based_end_critter_turn(obj);
            }
            return false;
        }
    }

    run_info->path.flags &= ~0x03;
    run_info->path.field_E8 = path_create_info.from;
    run_info->path.field_F0 = path_create_info.to;
    run_info->path.curr = 0;
    run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = target_loc;

    if (tig_net_is_active()) {
        run_info->path.flags |= 0x01;
    }

    return true;
}

// 0x425BF0
bool anim_set_path_flags(PathCreateInfo* path_create_info, bool a2)
{
    ASSERT(path_create_info != NULL); // 3923, "pPathData != NULL"
    ASSERT(path_create_info->obj != OBJ_HANDLE_NULL); // 3924, "pPathData->movingObj != OBJ_HANDLE_NULL"

    if (obj_type_is_critter(obj_field_int32_get(path_create_info->obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(path_create_info->obj, OBJ_F_SPELL_FLAGS) & OSF_ENTANGLED) != 0) {
            return false;
        }

        if (!critter_can_open_portals(path_create_info->obj)) {
            path_create_info->flags |= PATH_FLAG_0x0002;
        }

        if (!critter_can_jump_window(path_create_info->obj)) {
            path_create_info->flags |= PATH_FLAG_0x0004;
        }

        if (critter_is_concealed(path_create_info->obj)
            && basic_skill_training_get(path_create_info->obj, BASIC_SKILL_PROWLING) <= 0) {
            path_create_info->flags |= PATH_FLAG_0x0200;
        }

        if (object_get_resistance(path_create_info->obj, RESISTANCE_TYPE_FIRE, false) < 45
            && stat_level_get(path_create_info->obj, STAT_STRENGTH) != 0) {
            path_create_info->flags |= PATH_FLAG_0x0400;
        }

        if (a2) {
            if (!combat_critter_is_combat_mode_active(path_create_info->obj)) {
                path_create_info->flags |= PATH_FLAG_0x0010;
            }
        }
    }

    return true;
}

// 0x425D60
bool anim_create_wander_dark_path(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t loc;
    int range;
    int x;
    int y;
    int64_t target_loc;
    PathCreateInfo path_create_info;

    ASSERT(run_info != NULL); // 4169, "pRunInfo != NULL"

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4173, "obj != OBJ_HANDLE_NULL"

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        run_info->path.flags = 0x01;
        return true;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;
    x = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    y = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;

    x += random_between(-range, range);
    y += random_between(-range, range);
    target_loc = location_make(x, y);

    if (light_level_at(target_loc, 0, 0) > light_level_at(loc, 0, 0)) {
        return false;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal + 1;

    path_create_info.obj = obj;
    path_create_info.max_rotations = anim_path_calc_max_length(&(run_info->path), loc, target_loc, obj);
    path_create_info.from = loc;
    path_create_info.to = target_loc;
    path_create_info.rotations = run_info->path.rotations;
    path_create_info.flags = 0;

    if (anim_set_path_flags(&path_create_info, 1)) {
        run_info->path.max = path_make(&path_create_info);
    } else {
        run_info->path.max = 0;
    }

    if (run_info->path.max == 0 || run_info->path.max > range) {
        path_create_info.flags = PATH_FLAG_0x0001;
        if (!anim_set_path_flags(&path_create_info, 1)) {
            if (!player_is_pc_obj(obj)) {
                combat_turn_based_end_critter_turn(obj);
            }
            return false;
        }

        run_info->path.max = path_make(&path_create_info);
        if (run_info->path.max == 0 || run_info->path.max > range) {
            if (!player_is_pc_obj(obj)) {
                combat_turn_based_end_critter_turn(obj);
            }
            return false;
        }
    }

    // FIX: Was `&= 0x03`, which keeps only the two bits it means to clear and
    // wipes every other path flag. 0x425D60 in the exe is `and 0xfffffffc`, and
    // the near-identical twin `anim_create_wander_path` already spells it
    // `&= ~0x03`.
    //
    // Leaving bit 0x01 set matters: the move steps read it as "use the object's
    // current rotation instead of the planned one", so a critter wandering into
    // darkness walked its fresh path facing whichever way it happened to be
    // pointing.
    run_info->path.flags &= ~0x03;
    run_info->path.field_E8 = path_create_info.from;
    run_info->path.field_F0 = path_create_info.to;
    run_info->path.curr = 0;
    run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = target_loc;

    if (tig_net_is_active()) {
        run_info->path.flags |= 0x01;
    }

    return true;
}

// 0x426040
bool anim_create_path_to_tile(AnimRunInfo* run_info)
{
    int64_t obj;
    unsigned int flags = 0;
    bool rc;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4297, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        anim_path_mark_net_update_pending(run_info);
        return true;
    }

    if ((run_info->flags & 0x400) != 0) {
        flags |= PATH_FLAG_0x0040 | PATH_FLAG_0x0020 | PATH_FLAG_0x0010 | PATH_FLAG_0x0008;
    }

    if (run_info->params[1].loc == 0 || run_info->params[1].loc == -1) {
        return false;
    }

    if (anim_path_create_from_obj(obj, run_info->params[1].loc, &(run_info->path), flags)) {
        return true;
    }

    if ((run_info->flags & 0x400) != 0 || !player_is_pc_obj(obj)) {
        return false;
    }

    run_info->flags |= 0x400;

    rc = anim_path_create_from_obj(obj, run_info->params[1].loc, &(run_info->path), flags | PATH_FLAG_0x0040 | PATH_FLAG_0x0020 | PATH_FLAG_0x0010 | PATH_FLAG_0x0008);

    return rc;
}

// 0x4261E0
// Path length from `a1`'s current tile to `a2`, using the normal
// pathfinding flags. Used by combat_move_cost for non-adjacent moves.
int anim_path_length_to_tile(int64_t a1, int64_t a2)
{
    AnimPath path;

    path.flags = 0;
    path.curr = 0;
    path.max = 0;
    path.absMaxPathLength = 0;
    path.maxPathLength = 0;
    path.baseRot = 0;
    path.field_CC = sizeof(path.rotations); // FIX: Initialize to prevent errors in `anim_path_calc_max_length`.

    if (!anim_path_create_from_obj(a1, a2, &path, 0)) {
        return 0;
    }

    return path.max;
}

// 0x426250
// Straight-line path length from `a1` to `a2` (PATH_FLAG_0x0001, no
// pathfinding), used by combat_move_cost for adjacent moves.
int anim_straight_path_length_to_tile(int64_t a1, int64_t a2)
{
    AnimPath path;

    path.flags = 1;
    path.curr = 0;
    path.max = 0;
    path.absMaxPathLength = 0;
    path.maxPathLength = 0;
    path.baseRot = 0;
    path.field_CC = sizeof(path.rotations); // FIX: Initialize to prevent errors in `anim_path_calc_max_length`.

    if (!anim_path_create_from_obj(a1, a2, &path, PATH_FLAG_0x0001)) {
        return 0;
    }

    return path.max;
}

// 0x4262D0
void anim_create_path_max_length(int64_t a1, const char* msg, int value)
{
    char str[MAX_STRING];

    object_examine(a1, a1, str);
    tig_debug_printf("Anim: anim_create_path_max_length: Error: %s: %d!  [%s]\n", msg, value, str);
}

// 0x426320
int anim_path_calc_max_length(AnimPath* anim_path, int64_t from, int64_t to, int64_t obj)
{
    int estimate;

    ASSERT(anim_path->maxPathLength >= 0); // 4404, "pAnimPath->maxPathLength >= 0"

    if (anim_path->maxPathLength < 0) {
        anim_path->maxPathLength = 0;
    }

    if (anim_path->maxPathLength != 0) {
        anim_path->maxPathLength = anim_path->absMaxPathLength + 5;
        if (anim_path->maxPathLength > anim_path->absMaxPathLength) {
            if (anim_path->absMaxPathLength > 0) {
                anim_path->maxPathLength = anim_path->absMaxPathLength;
            } else {
                estimate = (int)location_dist(from, to);
                if (estimate > anim_path->field_CC) {
                    anim_create_path_max_length(obj, "Estimated Distance is too large", estimate);
                }

                anim_path->absMaxPathLength = 4 * estimate + 5;
                if (anim_path->absMaxPathLength > anim_path->field_CC) {
                    anim_path->absMaxPathLength = anim_path->field_CC;
                }

                if (anim_path->maxPathLength > anim_path->absMaxPathLength) {
                    if (anim_path->absMaxPathLength > 0) {
                        anim_path->maxPathLength = anim_path->absMaxPathLength;
                    } else {
                        anim_path->absMaxPathLength = anim_path->field_CC;
                    }
                }
            }
        }
    } else {
        estimate = (int)location_dist(from, to);
        if (estimate > anim_path->field_CC) {
            anim_create_path_max_length(obj, "Estimated Distance is too large", estimate);
            tig_debug_printf("   SrcLocAxis: (%d x %d)", (int)location_get_x(from), (int)location_get_y(from));
            tig_debug_printf(", DstLocAxis: (%d x %d)\n", (int)location_get_x(to), (int)location_get_y(to));
        }

        anim_path->maxPathLength = 4 * estimate + 5;
        if (anim_path->absMaxPathLength > anim_path->field_CC) {
            anim_path->absMaxPathLength = anim_path->field_CC;
        }
    }

    if (anim_path->maxPathLength == 0) {
        anim_create_path_max_length(obj, "Path Length is 0", anim_path->maxPathLength);
    } else if (anim_path->maxPathLength > anim_path->field_CC) {
        anim_create_path_max_length(obj, "Path Length is out of range", anim_path->maxPathLength);
    }

    if (anim_path->maxPathLength > anim_path->field_CC) {
        anim_path->maxPathLength = anim_path->field_CC;
    }

    return anim_path->maxPathLength;
}

// 0x426500
// Creates a path for `obj` from its current tile to `to`.
int anim_path_create_from_obj(int64_t obj, int64_t to, AnimPath* path, unsigned int flags)
{
    ASSERT(obj != OBJ_HANDLE_NULL); // 4493, "obj != OBJ_HANDLE_NULL"

    return anim_path_create(obj, obj_field_int64_get(obj, OBJ_F_LOCATION), to, path, flags);
}

// 0x426560
// Low-level path creation from an explicit `from` tile into `path`. Shared
// by the run_info path creators above and the combat move-cost helpers.
bool anim_path_create(int64_t obj, int64_t from, int64_t to, AnimPath* path, unsigned int flags)
{
    int v1;
    int8_t* rotations;
    tig_art_id_t art_id;
    int rot;
    int offset_x;
    int offset_y;
    bool v2 = true;
    int64_t adjacent_loc;
    PathCreateInfo path_create_info;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4511, "obj != OBJ_HANDLE_NULL"

    v1 = anim_path_calc_max_length(path, from, to, obj);

    if ((flags & PATH_FLAG_0x1000) != 0) {
        v1 = 200;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_POLYMORPHED) != 0) {
        flags |= PATH_FLAG_0x0004 | PATH_FLAG_0x0002;
    }

    if (!critter_can_open_portals(obj)) {
        flags |= PATH_FLAG_0x0002;
    }

    if (!critter_can_jump_window(obj)) {
        flags |= PATH_FLAG_0x0004;
    }

    if (critter_is_concealed(obj)
        && basic_skill_training_get(obj, BASIC_SKILL_PROWLING) <= 0) {
        flags |= PATH_FLAG_0x0200;
    }

    rotations = path->rotations;
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    rot = tig_art_id_rotation_get(art_id);
    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);

    if (offset_x != 0 || offset_y != 0) {
        v1--;
        rotations++;
        v2 = false;

        if (!location_in_dir(from, rot, &adjacent_loc)) {
            return false;
        }

        if (!tile_is_blocking(adjacent_loc, false)
            && !object_traversal_is_blocked(obj, from, rot, path_translate_flags(flags), NULL)) {
            from = adjacent_loc;
        } else {
            if (from != adjacent_loc) {
                return false;
            }
        }
    }

    path_create_info.to = to;
    path_create_info.obj = obj;
    path_create_info.from = from;
    path_create_info.max_rotations = v1;
    path_create_info.rotations = rotations;
    path_create_info.flags = flags;

    if (anim_set_path_flags(&path_create_info, true)) {
        path->max = path_make(&path_create_info);
    } else {
        path->max = 0;
    }

    if (path->max == 0) {
        if (!player_is_pc_obj(obj)) {
            combat_turn_based_end_critter_turn(obj);
        }
        return false;
    }

    path->curr = 0;
    path->flags &= ~0x03;
    path->field_E8 = from;
    path->field_F0 = to;

    if (!v2) {
        path->rotations[0] = rot;
    }

    return true;
}

// 0x426840
bool anim_create_straight_path(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t target_loc;
    int64_t source_loc;

    obj = run_info->params[0].obj;
    target_loc = run_info->params[1].loc;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4643, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;

    if (target_loc == 0) {
        return false;
    }

    if (target_loc == -1) {
        return false;
    }

    source_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    run_info->path.max = path_make_straight(source_loc, target_loc, run_info->path.rotations);
    if (run_info->path.max == 0) {
        return false;
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x3;

    return true;
}

// 0x4268F0
bool anim_create_knockback_path(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t target_loc;
    int64_t source_loc;

    obj = run_info->params[0].obj;
    target_loc = run_info->params[1].loc;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4689, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;

    if (target_loc == 0) {
        return false;
    }

    source_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    run_info->path.baseRot = location_rot(source_loc, target_loc);
    run_info->path.max = path_make_straight(source_loc, target_loc, run_info->path.rotations);
    if (run_info->path.max == 0) {
        return false;
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x3;

    return true;
}

// 0x4269D0
bool anim_create_projectile_path(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4738, "obj != OBJ_HANDLE_NULL"
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;

    if (run_info->params[1].loc == 0
        || run_info->params[1].loc == -1) {
        return false;
    }

    run_info->path.max = path_make_straight(obj_field_int64_get(obj, OBJ_F_LOCATION),
        run_info->params[1].loc,
        run_info->path.rotations);
    if (run_info->path.max == 0) {
        return false;
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x03;

    return true;
}

// 0x426A80
bool anim_create_path_away_from_obj(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int range;
    int64_t source_loc;
    int64_t target_loc;
    int rot;
    int dist;
    int v1;
    unsigned int path_create_flags;
    int8_t* rotations;
    int offset_x;
    int offset_y;
    PathCreateInfo path_create_info;
    tig_art_id_t art_id;
    bool v2 = true;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 4782, "sourceObj != OBJ_HANLDE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 4783, "targetObj != OBJ_HANLDE_NULL"

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        anim_path_mark_net_update_pending(run_info);
        return true;
    }

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;
    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    rot = location_rot(target_loc, source_loc);
    for (dist = 0; dist < range; dist++) {
        if (!location_in_dir(target_loc, rot, &target_loc)) {
            ASSERT(0); // 4812, "0"
            exit(EXIT_FAILURE);
        }
    }

    run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = target_loc;
    run_info->path_attached_to_stack_index = run_info->current_goal;

    v1 = anim_path_calc_max_length(&(run_info->path), source_loc, target_loc, source_obj);

    path_create_flags = 0;
    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_POLYMORPHED) != 0) {
        path_create_flags |= PATH_FLAG_0x0004 | PATH_FLAG_0x0002;
    }

    if ((run_info->flags & 0x400) != 0) {
        path_create_flags |= PATH_FLAG_0x0040 | PATH_FLAG_0x0020 | PATH_FLAG_0x0010 | PATH_FLAG_0x0008;
    }

    rotations = run_info->path.rotations;
    offset_x = obj_field_int32_get(source_obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(source_obj, OBJ_F_OFFSET_Y);
    if (offset_x != 0 || offset_y != 0) {
        v1--;
        rotations++;
        v2 = false;
    }

    path_create_info.obj = source_obj;
    path_create_info.from = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    path_create_info.max_rotations = v1;
    path_create_info.to = target_loc;
    path_create_info.rotations = rotations;
    path_create_info.flags = path_create_flags;

    if (!anim_set_path_flags(&path_create_info, true)) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    run_info->path.max = path_make(&path_create_info);
    run_info->path.field_E8 = path_create_info.from;
    run_info->path.field_F0 = path_create_info.to;

    if (run_info->path.max == 0) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    if (path_create_info.field_24) {
        run_info->path.flags |= 0x20;
        ai_set_no_flee(source_obj);
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x03;

    if (!v2) {
        art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
        rot = tig_art_id_rotation_get(art_id);
        if (rot == run_info->path.rotations[1]) {
            run_info->path.curr = 1;
        } else {
            rot = (rot + 4) % 8;
            tig_art_id_rotation_set(art_id, rot);
            object_set_current_aid(source_obj, art_id);
            run_info->path.rotations[0] = rot;
        }
    }

    return true;
}

// 0x426E80
bool anim_is_concealed_npc(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4919, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return false;
    }

    if (tig_art_id_anim_get(obj_field_int32_get(obj, OBJ_F_CURRENT_AID)) != TIG_ART_ANIM_CONCEAL_FIDGET) {
        return false;
    }

    if (player_is_pc_obj(obj)) {
        return false;
    }

    return true;
}

// 0x426F10
bool anim_is_prone(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4946, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    return critter_is_prone(obj);
}

// 0x426F60
bool AGendAnimStunAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    unsigned int flags;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4961, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!combat_turn_based_is_active()) {
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data--;
    }

    if (run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data > 0) {
        return false;
    }

    flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
    if ((flags & OCF_STUNNED) != 0) {
        flags &= ~OCF_STUNNED;
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, flags);
    }

    return true;
}

// 0x427000
// Called when a stunned critter's TB turn arrives: shortens the
// AG_ANIMATE_STUNNED goal by three ticks and interrupts it when exhausted.
bool anim_advance_stun_goal(int64_t obj)
{
    AnimID anim_id;
    AnimRunInfo* run_info;

    if (combat_turn_based_is_active()
        && combat_turn_based_whos_turn_get() == obj
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_STUNNED) != 0
        && anim_is_current_goal_type(obj, AG_ANIMATE_STUNNED, &anim_id)
        && anim_id_to_run_info(&anim_id, &run_info)) {
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data -= 3;
        if (run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data <= 0) {
            anim_interrupt(&anim_id, PRIORITY_HIGHEST);
        }
    }

    return true;
}

// 0x4270B0
bool anim_next_step_blocked(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 4961, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    return anim_check_next_step(run_info,
        obj,
        obj_field_int64_get(obj, OBJ_F_LOCATION));
}

// 0x427110
bool anim_check_next_step(AnimRunInfo* run_info, int64_t obj, int64_t loc)
{
    tig_art_id_t art_id;
    int rot;
    int64_t adjacent_loc;
    ObjectList objects;
    ObjectNode* node;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if ((run_info->path.flags & 0x01) != 0) {
        rot = tig_art_id_rotation_get(art_id);
    } else {
        rot = run_info->path.rotations[run_info->path.curr];
    }

    if ((run_info->path.flags & 0x02) != 0) {
        return false;
    }

    if (!location_in_dir(loc, rot, &adjacent_loc)) {
        return false;
    }

    if (run_info->path.curr > run_info->path.max - 2) {
        object_list_location(adjacent_loc, OBJ_TM_CRITTER, &objects);
        node = objects.head;
        while (node != NULL) {
            if (!critter_is_dead(node->obj)) {
                run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj = node->obj;
                break;
            }
            node = node->next;
        }
        object_list_destroy(&objects);
    }

    if ((run_info->flags & 0x400) != 0
        && anim_tile_is_blocked(obj, loc, adjacent_loc, rot)) {
        return true;
    }

    if (run_info->path.curr < run_info->path.max) {
        object_list_location(adjacent_loc, OBJ_TM_TRAP, &objects);
        node = objects.head;
        while (node != NULL) {
            if (!trap_is_spotted(obj, node->obj)
                && trap_attempt_spot(obj, node->obj)) {
                break;
            }
            node = node->next;
        }
        object_list_destroy(&objects);

        if (node != NULL) {
            return true;
        }
    }

    return false;
}

// 0x4272E0
bool anim_find_blocking_door(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int64_t loc;
    int rotation;
    int64_t door_obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 5202, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);

    if ((run_info->path.flags & 0x1) != 0) {
        rotation = tig_art_id_rotation_get(art_id);
    } else {
        rotation = run_info->path.rotations[run_info->path.curr];
    }

    if (!object_traversal_check_blocking_door(obj, loc, rotation, 0, &door_obj)) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj = door_obj;

    return true;
}

// 0x4273B0
bool anim_find_wall_at(int64_t obj, int64_t loc, int rotation, int a4, int64_t* obj_ptr)
{
    ObjectList objects;
    ObjectNode* node;
    tig_art_id_t art_id;
    int obj_rot;
    bool v1;

    if ((rotation & 1) == 0) {
        return false;
    }

    v1 = false;
    if (obj_ptr != NULL) {
        *obj_ptr = OBJ_HANDLE_NULL;
    }

    object_list_location(loc, OBJ_TM_WALL, &objects);
    node = objects.head;
    while (node != NULL) {
        art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
        obj_rot = tig_art_id_rotation_get(art_id);
        if ((obj_rot & 1) == 0) {
            obj_rot++;
        }
        if (obj_rot == rotation) {
            switch (tig_art_wall_id_p_piece_get(art_id)) {
            case 9:
            case 11:
            case 12:
            case 15:
            case 16:
            case 20:
                object_list_destroy(&objects);
                return false;
            case 10:
            case 13:
            case 14:
            case 17:
            case 18:
            case 19:
                v1 = true;
                break;
            }
        }
        node = node->next;
    }
    object_list_destroy(&objects);

    if (v1) {
        object_list_location(loc, OBJ_TM_PORTAL, &objects);
        if (objects.head != NULL) {
            if (obj_ptr != NULL) {
                *obj_ptr = objects.head->obj;
            }
        }
        object_list_destroy(&objects);
        return true;
    }

    if (!location_in_dir(loc, rotation, &loc)) {
        return false;
    }

    object_list_location(loc, OBJ_TM_WALL, &objects);
    node = objects.head;
    while (node != NULL) {
        art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
        obj_rot = tig_art_id_rotation_get(art_id);
        if ((obj_rot & 1) == 0) {
            obj_rot++;
        }
        if (obj_rot == (rotation + 4) % 8) {
            switch (tig_art_wall_id_p_piece_get(art_id)) {
            case 9:
            case 11:
            case 12:
            case 15:
            case 16:
            case 20:
                object_list_destroy(&objects);
                return false;
            case 10:
            case 13:
            case 14:
            case 17:
            case 18:
            case 19:
                v1 = true;
                break;
            }
        }
        node = node->next;
    }
    object_list_destroy(&objects);

    if (v1) {
        object_list_location(loc, OBJ_TM_PORTAL, &objects);
        if (objects.head != NULL) {
            if (obj_ptr != NULL) {
                *obj_ptr = objects.head->obj;
            }
        }
        object_list_destroy(&objects);
        return true;
    }

    return false;
}

// 0x427640
bool anim_find_blocking_wall(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int64_t loc;
    int rotation;
    int64_t v1;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 5401, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);

    if ((run_info->path.flags & 0x1) != 0) {
        rotation = tig_art_id_rotation_get(art_id);
    } else {
        rotation = run_info->path.rotations[run_info->path.curr];
    }

    if (!anim_find_wall_at(obj, loc, rotation, 0, &v1)) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj = v1;

    return true;
}

// 0x427710
bool sub_427710(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x427720
bool sub_427720(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x427730
bool anim_create_path_near_tile(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t source_loc;
    int64_t target_loc;
    PathCreateInfo path_create_info;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL);

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        anim_path_mark_net_update_pending(run_info);
        return true;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;
    target_loc = run_info->params[1].loc;
    source_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);

    if (target_loc == 0
        || target_loc == -1) {
        return false;
    }

    path_create_info.max_rotations = anim_path_calc_max_length(&(run_info->path), source_loc, target_loc, obj);
    path_create_info.from = source_loc;
    path_create_info.to = target_loc;
    path_create_info.obj = obj;
    path_create_info.rotations = run_info->path.rotations;

    if ((run_info->flags & 0x4000) == 0) {
        path_create_info.flags = 0;
        if (anim_set_path_flags(&path_create_info, true)) {
            run_info->path.max = path_make(&path_create_info);
        } else {
            run_info->path.max = 0;
        }
        run_info->path.field_E8 = path_create_info.from;
        run_info->path.field_F0 = path_create_info.to;
    } else {
        run_info->path.max = 0;
    }

    if (run_info->path.max == 0) {
        path_create_info.flags = PATH_FLAG_0x0001;
        if (anim_set_path_flags(&path_create_info, true)) {
            run_info->path.max = path_make(&path_create_info);
            run_info->path.field_E8 = path_create_info.from;
            run_info->path.field_F0 = path_create_info.to;
        }

        if (run_info->path.max == 0) {
            if (!player_is_pc_obj(obj)) {
                combat_turn_based_end_critter_turn(obj);
            }
            return false;
        }
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x03;

    return true;
}

// 0x427990
bool anim_create_path_near_obj(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t orig_range;
    int64_t range;
    int64_t source_loc;
    int64_t target_loc;
    unsigned int path_create_flags;
    PathCreateInfo path_create_info;
    int max_rotations;
    bool v1 = false;
    int v2 = 0;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 556, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 5557, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        anim_path_mark_net_update_pending(run_info);
        return true;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;
    orig_range = range;

    switch (obj_field_int32_get(target_obj, OBJ_F_TYPE)) {
    case OBJ_TYPE_CONTAINER:
    case OBJ_TYPE_SCENERY:
    case OBJ_TYPE_PC:
    case OBJ_TYPE_NPC:
        v1 = true;
        v2 = 1;
        range = 0;
        object_flags_set(target_obj, OF_OFF);
        break;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    max_rotations = anim_path_calc_max_length(&(run_info->path), source_loc, target_loc, source_obj);

    run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = target_loc;

    path_create_flags = 0;
    if (range == -1 || range != 0) {
        path_create_flags = PATH_FLAG_0x0001;
    }

    path_create_info.obj = source_obj;
    path_create_info.max_rotations = max_rotations;
    path_create_info.from = source_loc;
    path_create_info.to = target_loc;
    path_create_info.rotations = run_info->path.rotations;
    path_create_info.flags = path_create_flags;

    if ((run_info->flags & 0x4000) == 0) {
        path_create_info.flags &= ~PATH_FLAG_0x0001;
    }

    if (anim_set_path_flags(&path_create_info, false)) {
        run_info->path.max = path_make(&path_create_info);
    } else {
        run_info->path.max = 0;
    }

    run_info->path.field_E8 = source_loc;
    run_info->path.field_F0 = target_loc;

    if (run_info->path.max > v2) {
        run_info->path.curr = 0;
        run_info->path.flags &= ~0x03;

        if (v1) {
            object_flags_unset(target_obj, OF_OFF);
            run_info->path.max--;
        }

        return true;
    }

    if (path_create_flags != path_create_info.flags
        && (run_info->flags & 0x4000) == 0) {
        path_create_info.flags = path_create_flags;

        if (!anim_set_path_flags(&path_create_info, true)) {
            if (!player_is_pc_obj(source_obj)) {
                combat_turn_based_end_critter_turn(source_obj);
            }
            return false;
        }

        run_info->path.max = path_make(&path_create_info);
        run_info->path.field_E8 = path_create_info.from;
        run_info->path.field_F0 = path_create_info.to;

        if (run_info->path.max > v2) {
            run_info->path.curr = 0;
            run_info->path.flags &= ~0x03;

            if (range > 0) {
                run_info->path.max -= (int)range - 1;
                if (run_info->path.max < 1) {
                    if (!player_is_pc_obj(source_obj)) {
                        combat_turn_based_end_critter_turn(source_obj);
                    }
                    return false;
                }
            }
        }

        if (v1) {
            object_flags_unset(target_obj, OF_OFF);
            run_info->path.max--;
        }

        return true;
    }

    if (v1) {
        object_flags_unset(target_obj, OF_OFF);
    }

    if (range != 0 || orig_range == 0) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    path_create_info.obj = source_obj;
    path_create_info.rotations = run_info->path.rotations;
    path_create_info.from = source_loc;
    path_create_info.to = target_loc;
    path_create_info.max_rotations = max_rotations;
    path_create_info.flags = (run_info->flags & 0x4000) != 0 ? PATH_FLAG_0x0001 : 0;

    if (anim_set_path_flags(&path_create_info, true)) {
        run_info->path.max = path_make(&path_create_info);
    } else {
        run_info->path.max = 0;
    }

    run_info->path.field_E8 = source_loc;
    run_info->path.field_F0 = target_loc;

    if (run_info->path.max > 0) {
        run_info->path.curr = 0;
        run_info->path.flags &= ~0x03;

        return true;
    }

    if (path_create_info.flags == PATH_FLAG_0x0001 || (run_info->flags & 0x4000) != 0) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    path_create_info.flags = PATH_FLAG_0x0001;

    if (!anim_set_path_flags(&path_create_info, true)) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    run_info->path.max = path_make(&path_create_info);
    run_info->path.field_E8 = path_create_info.from;
    run_info->path.field_F0 = path_create_info.to;

    if (run_info->path.max <= 0) {
        if (!player_is_pc_obj(source_obj)) {
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x03;

    if (orig_range >= 0) {
        run_info->path.max -= (int)orig_range - 1;
        if (run_info->path.max < 1) {
            return false;
        }
    }

    return true;
}

// 0x4280D0
bool anim_create_path_near_obj_combat(AnimRunInfo* run_info)
{
    // CE: the three exits that end a non-PC's turn because no path was found
    // first call `ai_note_unreachable`, so under `ai targeting` the NPC can
    // turn to a target it can reach (combat AI milestone AI-3). The fourth,
    // a path shorter than the weapon's range, is not a failure to reach. It
    // records nothing with the switch off.
    int64_t source_obj;
    int64_t target_obj;
    int64_t range;
    int64_t source_loc;
    int64_t target_loc;
    unsigned int path_create_flags;
    PathCreateInfo path_create_info;
    int max_rotations;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 5870, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 5871, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        anim_path_mark_net_update_pending(run_info);
        return true;
    }

    run_info->path_attached_to_stack_index = run_info->current_goal;

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;
    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    max_rotations = anim_path_calc_max_length(&(run_info->path), source_loc, target_loc, source_obj);

    run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = target_loc;

    path_create_flags = 0;
    if (range == -1 || range != 0) {
        path_create_flags = PATH_FLAG_0x0001;
    }

    path_create_info.obj = source_obj;
    path_create_info.max_rotations = max_rotations;
    path_create_info.from = source_loc;
    path_create_info.to = target_loc;
    path_create_info.rotations = run_info->path.rotations;
    path_create_info.flags = path_create_flags;

    if ((run_info->flags & 0x4000) == 0) {
        path_create_info.flags &= ~PATH_FLAG_0x0001;
    }

    if (anim_set_path_flags(&path_create_info, false)) {
        run_info->path.max = path_make(&path_create_info);
    } else {
        run_info->path.max = 0;
    }

    run_info->path.field_E8 = path_create_info.from;
    run_info->path.field_F0 = path_create_info.to;

    if (run_info->path.max != 0) {
        run_info->path.curr = 0;
        run_info->path.flags &= ~0x03;

        return true;
    }

    if (path_create_flags == path_create_info.flags
        || (run_info->flags & 0x4000) != 0) {
        if (!player_is_pc_obj(source_obj)) {
            ai_note_unreachable(source_obj, target_obj);
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    path_create_info.flags = path_create_flags;
    if (!anim_set_path_flags(&path_create_info, false)) {
        if (!player_is_pc_obj(source_obj)) {
            ai_note_unreachable(source_obj, target_obj);
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    run_info->path.max = path_make(&path_create_info);
    run_info->path.field_E8 = path_create_info.from;
    run_info->path.field_F0 = path_create_info.to;

    if (run_info->path.max == 0) {
        if (!player_is_pc_obj(source_obj)) {
            ai_note_unreachable(source_obj, target_obj);
            combat_turn_based_end_critter_turn(source_obj);
        }
        return false;
    }

    run_info->path.flags &= ~0x03;
    run_info->path.curr = 0;

    if (range > 0) {
        run_info->path.max -= (int)range - 1;
        if (run_info->path.max < 1) {
            if (!player_is_pc_obj(source_obj)) {
                combat_turn_based_end_critter_turn(source_obj);
            }
            return false;
        }
    }

    return true;
}

// 0x4284A0
bool anim_is_portal(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_PORTAL) {
        return false;
    }

    return true;
}

// 0x4284F0
bool anim_portal_is_closed_and_idle(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (portal_is_open(obj)) {
        return false;
    }

    if (anim_is_busy(obj, NULL)) {
        return false;
    }

    return true;
}

// 0x428550
bool anim_try_open_portal(AnimRunInfo* run_info)
{
    int64_t door_obj;
    int64_t self_obj;
    int sound_id;

    door_obj = run_info->params[0].obj;
    self_obj = run_info->params[1].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 6093, "doorObj != OBJ_HANDLE_NULL"
    ASSERT(self_obj != OBJ_HANDLE_NULL); // 6093, "selfObj != OBJ_HANDLE_NULL"

    if (door_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (self_obj == OBJ_HANDLE_NULL) {
        // NOTE: Verified at 0x4285BE-0x4285C8/0x428611 - the original also
        // returns true here.
        return true;
    }

    if ((obj_field_int32_get(door_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (ai_attempt_open_portal(self_obj, door_obj, object_rot(self_obj, door_obj)) != AI_ATTEMPT_OPEN_PORTAL_OK) {
        sound_id = sfx_portal_sound(door_obj, PORTAL_SOUND_LOCKED);
        gsound_play_sfx_on_obj(sound_id, 1, door_obj);
        return false;
    }

    return true;
}

// 0x428620
bool anim_portal_is_operable(AnimRunInfo* run_info)
{
    int64_t door_obj;

    door_obj = run_info->params[0].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 6125, "doorObj != OBJ_HANDLE_NULL"

    if (door_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(door_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if ((obj_field_int32_get(door_obj, OBJ_F_PORTAL_FLAGS) & OPF_MAGICALLY_HELD) != 0) {
        return false;
    }

    return true;
}

// 0x428690
bool anim_can_open_portal(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t portal_obj;
    int rot;

    source_obj = run_info->params[0].obj;
    portal_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6151, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(portal_obj != OBJ_HANDLE_NULL); // 6152, "portalObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (portal_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!critter_can_open_portals(source_obj)) {
        return false;
    }

    rot = object_rot(source_obj, portal_obj);
    if (ai_attempt_open_portal(source_obj, portal_obj, rot) != AI_ATTEMPT_OPEN_PORTAL_OK) {
        return false;
    }

    return true;
}

// 0x428750
bool anim_cannot_open_portal(AnimRunInfo* run_info)
{
    int64_t door_obj;
    int64_t obj;
    int rot;

    door_obj = run_info->params[0].obj;
    obj = run_info->params[1].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 6183, "doorObj != OBJ_HANDLE_NULL"

    if ((obj_field_int32_get(door_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    rot = object_rot(obj, door_obj);
    if (ai_attempt_open_portal(obj, door_obj, rot) == AI_ATTEMPT_OPEN_PORTAL_OK) {
        return false;
    }

    return true;
}

// 0x4287E0
bool anim_attempt_open_portal(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t portal_obj;
    int rot;

    source_obj = run_info->params[0].obj;
    portal_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6221, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(portal_obj != OBJ_HANDLE_NULL); // 6222, "portalObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (portal_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(portal_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    rot = object_rot(source_obj, portal_obj);
    ai_attempt_open_portal(source_obj, portal_obj, rot);

    // NOTE: Returns `false` unconditionally, and the original does too -
    // 0x4287E0 ends in `return 0` with no path that yields anything else. Both
    // cells that name this callback are therefore half dead: `open_door` sub-node
    // 4 always falls to 5 and `attempt_open_door` sub-node 5 always terminates,
    // so neither of their `AG_UNLOCK_DOOR` success edges can ever be taken.
    // Kept as-is; this mirrors the shipped game.
    return false;
}

// 0x428890
bool sub_428890(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x4288A0
bool anim_can_jump_window(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t portal_obj;
    int rot;

    source_obj = run_info->params[0].obj;
    portal_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6259, "sourceObj != OBJ_HANDLE_NULL"

    if (!critter_can_jump_window(source_obj)) {
        return false;
    }

    if (portal_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    rot = object_rot(source_obj, portal_obj);
    if (ai_attempt_open_portal(source_obj, portal_obj, rot) != AI_ATTEMPT_OPEN_PORTAL_OK) {
        return false;
    }

    return true;
}

// 0x428930
bool anim_set_use_range(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int range = 2;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6288, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 6289, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    switch (obj_field_int32_get(target_obj, OBJ_F_TYPE)) {
    case OBJ_TYPE_CONTAINER:
    case OBJ_TYPE_PC:
    case OBJ_TYPE_NPC:
        range = 1;
        break;
    }

    run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data = range;

    return true;
}

// 0x428A10
bool anim_use_object(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    unsigned int spell_flags;
    int64_t source_loc;
    int64_t target_loc;
    int rot;
    int obj_type;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6318, "sourceObj != OBJ_HANDLE_NULL"

    if (target_obj == OBJ_HANDLE_NULL) {
        target_obj = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;

        ASSERT(target_obj != OBJ_HANDLE_NULL); // 6321, "targetObj != OBJ_HANDLE_NULL"
    }

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    // CE (real-time session C): the shared cost - the original's 2.
    if (!combat_consume_action_points(source_obj, combat_use_object_cost(source_obj, target_obj))) {
        return false;
    }

    spell_flags = obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS);

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    rot = location_rot(source_loc, target_loc);

    if (anim_target_traversal_is_blocked(source_obj, source_loc, target_loc, rot, target_obj)) {
        if (tig_net_is_active()
            && tig_net_is_host()) {
            anim_interrupt_all_goals_of_obj(source_obj, 2, false, false);
        }
        return false;
    }

    obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);
    switch (obj_type) {
    case OBJ_TYPE_PORTAL:
        if (!object_script_execute(source_obj, target_obj, source_obj, SAP_USE, 0)) {
            return false;
        }
        return true;
    case OBJ_TYPE_CONTAINER:
        if (!tig_net_is_active()
            || tig_net_is_host()) {
            mp_ui_show_inven_loot(source_obj, target_obj);

            if (tig_net_is_active()) {
                anim_interrupt_all_goals_of_obj(source_obj, 2, false, false);
            }
        }
        return true;
    case OBJ_TYPE_SCENERY:
        if (tig_net_is_active()
            && tig_net_is_host()) {
            anim_interrupt_all_goals_of_obj(source_obj, 2, false, false);
        }
        if (tig_art_scenery_id_type_get(obj_field_int32_get(target_obj, OBJ_F_CURRENT_AID)) == TIG_ART_SCENERY_TYPE_BEDS) {
            ui_sleep_toggle(target_obj);
            return true;
        }
        if (!object_script_execute(source_obj, target_obj, source_obj, SAP_USE, 0)) {
            return false;
        }
        return true;
    case OBJ_TYPE_PC:
    case OBJ_TYPE_NPC:
        if ((spell_flags & OSF_POLYMORPHED) != 0
            || anim_is_busy(target_obj, NULL)) {
            return false;
        }
        if (!tig_net_is_active()
            || tig_net_is_host()) {
            mp_ui_show_inven_loot(source_obj, target_obj);

            if (tig_net_is_active()) {
                anim_interrupt_all_goals_of_obj(source_obj, 2, false, false);
            }
        }
        return true;
    default:
        if (tig_net_is_active()
            && tig_net_is_host()) {
            anim_interrupt_all_goals_of_obj(source_obj, 2, false, false);
        }
        if (!object_script_execute(source_obj, target_obj, source_obj, SAP_USE, 0)) {
            return false;
        }
        return true;
    }
}

// 0x428CD0
bool anim_use_item_on_obj(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t item_obj;
    int64_t actual_parent_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;
    item_obj = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6415, "sourceObj != OBJ_HANDLE_NULL"

    if (target_obj == OBJ_HANDLE_NULL) {
        target_obj = item_obj;
        ASSERT(target_obj != OBJ_HANDLE_NULL); // 6418, "targetObj != OBJ_HANDLE_NULL"
    }

    ASSERT(item_obj != OBJ_HANDLE_NULL); // 6419, "itemObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL
        || item_obj == OBJ_HANDLE_NULL
        || !item_parent(item_obj, &actual_parent_obj)
        || source_obj != actual_parent_obj) {
        return false;
    }

    item_use_on_obj(source_obj, item_obj, target_obj);

    return true;
}

// 0x428E10
bool anim_use_item_on_obj_with_skill(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t item_obj;
    SkillInvocation skill_invocation;
    unsigned int flags;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;
    item_obj = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6453, "sourceObj != OBJ_HANDLE_NULL"

    if (target_obj == OBJ_HANDLE_NULL) {
        target_obj = item_obj;

        ASSERT(target_obj != OBJ_HANDLE_NULL); // 6456, "targetObj != OBJ_HANDLE_NULL"
    }

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL
        || (obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (item_obj != OBJ_HANDLE_NULL) {
        int64_t actual_parent_obj;

        if ((obj_field_int32_get(item_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0
            || !item_parent(item_obj, &actual_parent_obj)
            || actual_parent_obj != source_obj) {
            return false;
        }
    }

    skill_invocation_init(&skill_invocation);
    skill_invocation.skill = run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data;
    skill_invocation.modifier = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data;

    if (target_obj == -1) {
        target_obj = OBJ_HANDLE_NULL;
    }

    object_ref_init(source_obj, &(skill_invocation.source));
    object_ref_init(target_obj, &(skill_invocation.target));
    object_ref_init(item_obj, &(skill_invocation.item));

    if (item_obj != -1) {
        object_ref_init(item_obj, &(skill_invocation.item));
    }

    flags = run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data;
    if ((flags & 0x01) != 0) {
        skill_invocation.flags |= SKILL_INVOCATION_0x02;
    }
    if ((flags & 0x2000) != 0) {
        skill_invocation.flags |= SKILL_INVOCATION_FORCED;
    }

    skill_invocation_run(&skill_invocation);

    return true;
}

// 0x429040
bool anim_use_item_on_tile(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_loc;
    int64_t item_obj;
    int64_t parent_obj;

    source_obj = run_info->params[0].obj;
    target_loc = run_info->params[1].loc;
    item_obj = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6526, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_loc != 0); // 6527, "targetLoc != 0"
    ASSERT(item_obj != OBJ_HANDLE_NULL); // 6528, "itemObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || target_loc == 0
        || item_obj == OBJ_HANDLE_NULL
        || !item_parent(item_obj, &parent_obj)
        || source_obj != parent_obj) {
        return false;
    }

    item_use_on_loc(source_obj, item_obj, target_loc);

    return true;
}

// 0x429160
bool anim_use_item_on_tile_with_skill(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_loc;
    int64_t item_obj;
    int64_t parent_obj;
    SkillInvocation skill_invocation;

    source_obj = run_info->params[0].obj;
    target_loc = run_info->params[1].loc;
    item_obj = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6563, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_loc != 0); // 6564, "targetLoc != 0"
    ASSERT(item_obj != OBJ_HANDLE_NULL); // 6565, "itemObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || target_loc == 0
        || item_obj == OBJ_HANDLE_NULL
        || (obj_field_int32_get(item_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0
        || !item_parent(item_obj, &parent_obj)
        || source_obj != parent_obj) {
        return false;
    }

    skill_invocation_init(&skill_invocation);
    skill_invocation.skill = run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data;
    skill_invocation.modifier = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data;
    object_ref_init(source_obj, &(skill_invocation.source));
    skill_invocation.target_loc = target_loc;
    object_ref_init(item_obj, &(skill_invocation.item));

    if (item_obj != -1) {
        object_ref_init(item_obj, &(skill_invocation.item));
    }

    if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x01) != 0) {
        skill_invocation.flags |= SKILL_INVOCATION_0x02;
    }

    skill_invocation_run(&skill_invocation);

    return true;
}

// The goal table names 18 callbacks that ignore their arguments and return a
// constant. They are deliberately left as `sub_` here.
//
// Every other callback in the table has been given a name from its body; these
// have no body to name. Calling one `anim_pickup_item_check_target` would
// invent a meaning the code does not have, in a port whose value is that its
// names can be trusted. What they do is fixed by the constant alone: a stub
// that returns true can only take its cell's success edge, one that returns
// false only the failure edge, so each is an unconditional transition written
// as a call.
//
// The full set, with the constant each returns:
//
//   true   sub_4246C0  sub_427720  sub_429380  sub_429390  sub_4293A0
//          sub_429440  sub_429B40  sub_429BB0  sub_42FEA0
//   false  sub_427710  sub_428890  sub_429370  sub_4293B0  sub_4293C0
//          sub_429420  sub_429430  sub_42FEB0  sub_42FEC0
//
// Folding the constants into the tables makes three blocks unreachable, all of
// them Troika's own: pickup_item 6-9 and move_to_pause 2-3, documented on those
// goal nodes, and attempt_spell_w_cast_anim sub-node 14, whose own stub is
// reached only from a cell that jumps past it.
//
// sub_437CF0 has the same constant-true shape but is not referenced by a goal
// table; it is also deliberately left as `sub_` for the same reason.
//
// 0x429370
bool sub_429370(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x429380
bool sub_429380(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x429390
bool sub_429390(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x4293A0
bool sub_4293A0(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x4293B0
bool sub_4293B0(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x4293C0
bool sub_4293C0(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x4293D0
bool anim_set_no_flee(AnimRunInfo* run_info)
{
    int64_t critter_obj;

    critter_obj = run_info->params[0].obj;

    ASSERT(critter_obj != OBJ_HANDLE_NULL); // 6633, "critObj != OBJ_HANDLE_NULL"

    if (critter_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    ai_set_no_flee(critter_obj);

    return true;
}

// 0x429420
bool sub_429420(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x429430
bool sub_429430(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x429440
bool sub_429440(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x429450
bool anim_target_is_alive(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && critter_is_dead(obj)) {
        return false;
    }

    return true;
}

// 0x4294A0
bool anim_can_attack_target(AnimRunInfo* run_info)
{
    bool rc;

    rc = anim_can_reach_target(run_info->params[0].obj, run_info->params[1].obj);
    if (!rc) {
        if (combat_turn_based_is_active()) {
            combat_turn_based_end_critter_turn(run_info->params[0].obj);
        }
    }

    return rc;
}

// 0x4294F0
bool anim_can_reach_target(int64_t source_obj, int64_t target_obj)
{
    int64_t target_loc;
    int64_t v1;
    AnimPath path;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6747, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 6748, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || source_obj == OBJ_HANDLE_NULL
        || !critter_is_active(source_obj)
        || (obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(source_obj, OBJ_F_TYPE))
        && !critter_is_active(source_obj)) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))
        && critter_is_dead(target_obj)) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_BODY_OF_AIR) != 0
        && (obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_ELEMENTAL) == 0) {
        return false;
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);

    ai_line_of_fire_cost(source_obj, target_loc, &v1);

    if (v1 != OBJ_HANDLE_NULL && v1 != target_obj) {
        path.flags = 0;
        path.maxPathLength = 0;
        path.absMaxPathLength = 0;
        path.curr = 0;
        path.max = 0;
        path.baseRot = 0;
        path.field_CC = sizeof(path.rotations); // FIX: Initialize to prevent errors in `anim_path_calc_max_length`.

        if (!anim_path_create_from_obj(source_obj, target_loc, &path, PATH_FLAG_0x0001)) {
            return false;
        }
    }

    return true;
}

// 0x4296D0
bool anim_play_queued_sfx(AnimRunInfo* run_info)
{
    int64_t self_obj;
    int goal;

    self_obj = run_info->params[0].obj;

    ASSERT(self_obj != OBJ_HANDLE_NULL); // 6826, "selfObj != OBJ_HANDLE_NULL"

    if (self_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if (run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data != -1) {
        gsound_play_sfx_on_obj(run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data, 1, self_obj);
        for (goal = 0; goal <= run_info->current_goal; goal++) {
            run_info->goals[goal].params[AGDATA_SCRATCH_VAL5].data = -1;
        }
    }

    return true;
}

// 0x429760
bool anim_should_auto_attack(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6854, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 6855, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!combat_critter_is_combat_mode_active(source_obj)) {
        return false;
    }

    if (!player_is_pc_obj(source_obj)) {
        return false;
    }

    if (!combat_auto_attack_get(source_obj)) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (critter_is_dead(target_obj)) {
        return false;
    }

    if (object_is_busted(target_obj)) {
        return false;
    }

    if ((run_info->flags & 0x800) != 0) {
        return false;
    }

    if (combat_turn_based_is_active()) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & OCF_NON_LETHAL_COMBAT) != 0) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_USING_BOOMERANG) != 0) {
        return false;
    }

    run_info->path.maxPathLength = 0;
    run_info->flags &= ~0x83C;

    return true;
}

// 0x4298D0
bool anim_should_fidget(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t pc_obj;

    source_obj = run_info->params[0].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6918, "sourceObj != OBJ_HANDLE_NULL"

    if (combat_critter_is_combat_mode_active(source_obj)) {
        return true;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_AUTO_ANIMATES) == 0) {
        return false;
    }

    pc_obj = player_get_local_pc_obj();
    if (object_dist(source_obj, pc_obj) >= 30) {
        return false;
    }

    return true;
}

// 0x429960
bool anim_target_in_weapon_range(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 6949, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 6950, "targetObj != OBJ_HANDLE_NULL"

    return anim_target_in_weapon_range_of(source_obj, target_obj);
}

// CE (real-time session D): the body of `anim_target_in_weapon_range`, taking
// the two objects rather than a node's parameter slots, so the real-time
// budget can ask the game's own question at an attack's strike frame - where
// the slots hold the animation id, not the target. Unchanged otherwise.
bool anim_target_in_weapon_range_of(int64_t source_obj, int64_t target_obj)
{
    int64_t source_loc;
    int64_t target_loc;
    int64_t weapon_obj;
    int range;
    int64_t v1;

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL
        || (obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    weapon_obj = combat_critter_weapon(source_obj);

    if (weapon_obj == OBJ_HANDLE_NULL) {
        range = 1;
    } else {
        range = item_weapon_range(weapon_obj, source_obj);
        if (range < 0) {
            range = 1;
        }
    }

    if (location_dist(source_loc, target_loc) > range) {
        return false;
    }

    if (ai_line_of_fire_cost(source_obj, target_loc, &v1) >= 26) {
        return false;
    }

    if (v1 != OBJ_HANDLE_NULL && v1 != target_obj) {
        return false;
    }

    return true;
}

// 0x429AD0
bool anim_wields_ranged_weapon(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t weapon_obj;

    source_obj = run_info->params[0].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7008, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    weapon_obj = combat_critter_weapon(source_obj);
    if (weapon_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (item_weapon_range(weapon_obj, source_obj) <= 1) {
        return false;
    }

    return true;
}

// 0x429B40
bool sub_429B40(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x429B50
bool anim_pay_spell_cost(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 7039, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    // CE (real-time session C): the shared cast cost, which the real-time
    // budget reads too - the original's flat 4 today.
    {
        MagicTechRunInfo* mt_run_info = NULL;
        int spell = -1;
        int64_t source_obj = OBJ_HANDLE_NULL;
        int64_t target_obj = OBJ_HANDLE_NULL;
        int mt_id = -1;
        bool on_hit = false;

        if (magictech_id_to_run_info(run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data, &mt_run_info)) {
            spell = mt_run_info->spell;
            source_obj = mt_run_info->source_obj.obj;
            target_obj = mt_run_info->target_obj.obj;
            mt_id = mt_run_info->id;
            on_hit = (mt_run_info->flags & MAGICTECH_RUN_CE_ON_HIT) != 0;
        }

        // FIX (`on-hit spells free`, zsogg 2026-10-02): the original charges
        // the wielder 4 AP for every spell a weapon casts on a hit, as if he
        // had cast it. In turn-based the Grenade Launcher's explosion is a
        // second such cast and is refused for want of AP, so it never lands -
        // a bug in the original game. Off by default, so CE matches the
        // original unless a player opts in. The mark is set only while the
        // feature is on.
        // CE (magic, milestone MG-2): an Unfocused caster cannot cast its own
        // spells; an item still works for it.
        if (!on_hit && crit_effects_unfocused(obj)
            && (source_obj == OBJ_HANDLE_NULL || source_obj == obj)) {
            tf_add(obj, TF_TYPE_WHITE, "Unfocused");
            if (mt_run_info != NULL) {
                mt_run_info->flags |= MAGICTECH_RUN_CE_REFUSED;
            }
            return false;
        }

        // CE (magic, milestone MG-3): under `spell no stacking` a second
        // copy of a buff, or of Drain Will, is refused before anything is
        // paid (zsogg, 2026-10-11).
        if (!on_hit && spell != -1 && magictech_recast_refused(obj, spell, target_obj, mt_id)) {
            tf_add(obj, TF_TYPE_WHITE, "Already in effect");
            mt_run_info->flags |= MAGICTECH_RUN_CE_REFUSED;
            return false;
        }

        // CE (magic, milestone MG-1): an item that casts pays the item cost.
        if (!on_hit && !combat_consume_action_points(obj, combat_spell_cast_cost(obj, spell, source_obj))) {
            return false;
        }
    }

    if (!magictech_charge_run_fatigue(run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data)) {
        return false;
    }

    return true;
}

// 0x429BB0
bool sub_429BB0(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x429BC0
bool anim_spell_target_is_valid(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7070, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj != OBJ_HANDLE_NULL) {
        if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
            return false;
        }

        // NOTE: Verified against 0x429C22 - original calls obj_field_int64_get(target_obj, OBJ_F_LOCATION) and discards the result; transcription faithful.
        obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    }

    return true;
}

// 0x429C40
bool anim_set_spell_projectile_art(AnimRunInfo* run_info)
{
    tig_art_id_t art_id;

    art_id = magictech_projectile_art_id_get(run_info->params[0].data);
    if (art_id == TIG_ART_ID_INVALID) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = art_id;

    magictech_add_casting_eye_candy(run_info->params[0].data);

    return true;
}

// 0x429C80
bool anim_begin_spell(AnimRunInfo* run_info)
{
    if (!magictech_target_is_valid(run_info->params[0].data)) {
        return false;
    }

    if (magictech_projectile_art_id_get(run_info->params[0].data) == TIG_ART_ID_INVALID) {
        magictech_add_casting_eye_candy(run_info->params[0].data);
    }

    run_info->flags |= 0xC;

    magictech_begin(run_info->params[0].data);

    return true;
}

// 0x429CD0
bool anim_cleanup_spell(AnimRunInfo* run_info)
{
    int64_t obj;
    int fore;
    int back;
    int light;
    int spell;
    tig_art_id_t art_id;
    int goal;

    obj = run_info->params[0].obj;
    spell = run_info->params[1].data;

    ASSERT(obj != OBJ_HANDLE_NULL); // 7156, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0
        || map_is_clearing_objects()
        || obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data &= ~0x40;

    if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x80) != 0) {
        return false;
    }

    fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (back != -5) {
        if (fore != -1) {
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, fore, TIG_ART_ID_INVALID);
        }

        if (back != -1) {
            object_overlay_set(obj, OBJ_F_OVERLAY_BACK, back, TIG_ART_ID_INVALID);
        }
    } else {
        object_overlay_set(obj, OBJ_F_UNDERLAY, fore, TIG_ART_ID_INVALID);
    }

    if (light != -1) {
        object_set_overlay_light(obj, light, 0, TIG_ART_ID_INVALID, 0);
    }

    if ((run_info->flags & 0x08) == 0 && spell != -1) {
        if (!tig_net_is_active()
            || tig_net_is_host()) {
            magictech_release(spell, 1);
        }

        run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data = -1;

        for (goal = 0; goal < run_info->current_goal; goal++) {
            if (run_info->goals[goal].params[AGDATA_SPELL_DATA].data == spell) {
                run_info->goals[goal].params[AGDATA_SPELL_DATA].data = -1;
            }
        }
    }

    if ((run_info->flags & 0x200000) == 0) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(obj, art_id);

        anim_set_obj_offset(obj, 0, 0);
    }

    return true;
}

// 0x429E70
bool anim_cleanup_destroy_obj(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 7243, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_DESTROYED) == 0) {
        object_destroy(obj);
    }

    return true;
}

// 0x429ED0
bool anim_set_projectile_speed(AnimRunInfo* run_info)
{
    if (run_info->params[0].data == -1) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data = magictech_projectile_speed_get(run_info->params[0].data);

    return true;
}

// 0x429F00
bool anim_load_spell_visuals(AnimRunInfo* run_info)
{
    tig_art_id_t art_id;
    tig_art_id_t light_art_id;
    tig_color_t light_color;
    int overlay_fore_idx;
    int overlay_back_idx;
    int overlay_light_idx;
    int fx_idx;

    if (!magictech_target_is_valid(run_info->params[0].data)) {
        return false;
    }

    if (!magictech_eye_candy_overlay_add(run_info->params[0].data, &art_id, &light_art_id, &light_color, &overlay_fore_idx, &overlay_back_idx, &overlay_light_idx, &fx_idx)) {
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = -1;
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data = -1;
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data = -1;
        return false;
    }

    if (run_info->cur_stack_data == NULL) {
        run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
    }

    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data = fx_idx;
    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data = light_art_id;
    run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data = light_color;
    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = overlay_fore_idx;
    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data = overlay_back_idx;
    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data = overlay_light_idx;
    run_info->flags |= 0x2000;

    return true;
}

// 0x42A010
bool anim_begin_previous_anim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int anim;
    int sound_id;
    TigArtAnimData art_anim_data;

    // CE (real-time session E): a new cast, counted before the step that holds it.
    rt_budget_cast_begun(run_info);

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 7327, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        anim = tig_art_id_anim_get(art_id);

        if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0
            && !IS_BLOODY_DEATH_ANIM(anim)) {
            return false;
        }

        if (run_info->cur_stack_data->type == AG_DYING) {
            if (anim != TIG_ART_ANIM_FALL_DOWN) {
                sound_id = sfx_critter_sound(obj, CRITTER_SOUND_DYING_GRUESOME);
            } else {
                sound_id = sfx_critter_sound(obj, CRITTER_SOUND_DYING);
            }
            gsound_play_sfx_on_obj(sound_id, 1, obj);
        }
    }

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags &= ~0x0C;
    run_info->flags |= 0x10;

    return true;
}

// 0x42A180
bool anim_is_on_target_tile(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7393, "sourceObj!= OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj != OBJ_HANDLE_NULL) {
        if (obj_field_int64_get(source_obj, OBJ_F_LOCATION) != obj_field_int64_get(target_obj, OBJ_F_LOCATION)) {
            return false;
        }
    }

    return true;
}

// 0x42A200
bool anim_end_spell(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 7416, "obj != OBJ_HANDLE_NULL"

    magictech_release(run_info->params[1].data, 0x1);

    return true;
}

// 0x42A260
bool AGexecuteMagicTechCallback(AnimRunInfo* run_info)
{
    magictech_run_action(run_info->params[0].data, MAGICTECH_ACTION_CALLBACK);

    return true;
}

// 0x42A280
bool AGexecuteMagicTechEndCallback(AnimRunInfo* run_info)
{
    magictech_run_action(run_info->params[0].data, MAGICTECH_ACTION_END_CALLBACK);

    return true;
}

// 0x42A2A0
bool anim_run_skill_invocation(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    SkillInvocation skill_invocation;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7461, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7462, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    // CE (real-time session C): the shared cost - the original's 4.
    if (!combat_consume_action_points(source_obj,
            combat_skill_cost(source_obj, run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data))) {
        return false;
    }

    skill_invocation_init(&skill_invocation);

    skill_invocation.skill = run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data;

    if (target_obj == -1) {
        target_obj = OBJ_HANDLE_NULL;
    }

    object_ref_init(source_obj, &(skill_invocation.source));
    object_ref_init(target_obj, &(skill_invocation.target));

    if (run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj != -1) {
        object_ref_init(run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj, &(skill_invocation.item));
    }

    if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x1) != 0) {
        skill_invocation.flags |= SKILL_INVOCATION_0x02;
    }

    skill_invocation_run(&skill_invocation);

    if ((skill_invocation.flags & SKILL_INVOCATION_SUCCESS) != 0) {
        run_info->flags |= 0x40000;
    }

    return true;
}

// 0x42A430
bool anim_skill_succeeded(AnimRunInfo* run_info)
{
    return (run_info->flags & 0x40000) != 0;
}

// 0x42A440
bool anim_skill_is_pickpocket(AnimRunInfo* run_info)
{
    int64_t source_obj;

    source_obj = run_info->params[0].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7519, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    return run_info->params[1].data == SKILL_PICK_POCKET;
}

// 0x42A490
bool anim_skill_is_picklock(AnimRunInfo* run_info)
{
    int64_t source_obj;

    source_obj = run_info->params[0].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7540, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    return run_info->params[1].data == SKILL_PICK_LOCKS;
}

// 0x42A4E0
bool anim_run_skill_no_cost(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    SkillInvocation skill_invocation;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7563, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7564, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    skill_invocation_init(&skill_invocation);

    skill_invocation.skill = run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data;

    if (target_obj == -1) {
        target_obj = OBJ_HANDLE_NULL;
    }

    object_ref_init(source_obj, &(skill_invocation.source));
    object_ref_init(target_obj, &(skill_invocation.target));

    if (run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj != -1) {
        object_ref_init(run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj, &(skill_invocation.item));
    }

    if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x1) != 0) {
        skill_invocation.flags |= SKILL_INVOCATION_0x02;
    }

    skill_invocation_run(&skill_invocation);

    return true;
}

// 0x42A630
bool anim_apply_attack(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int hit_loc;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;
    hit_loc = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7694, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7605, "targetObj != OBJ_HANDLE_NULL"
    ASSERT(hit_loc != -1); // 7606, "hitLoc != -1"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (hit_loc == -1) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) == 0) {
        combat_attack_weapon_wear(source_obj, target_obj, hit_loc);
    }

    return true;
}

// 0x42A720
bool AGapplyFireDmg(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t parent_obj;
    int64_t source_loc;
    ObjectList objects;
    ObjectNode* node;
    CombatContext combat;
    int dam;
    bool update_turn;
    bool is_normal_dam;
    int aptitude;

    source_obj = run_info->params[0].obj;
    parent_obj = run_info->params[1].obj;
    update_turn = false;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7650, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    object_list_location(source_loc, OBJ_TM_CRITTER | OBJ_TM_ITEM, &objects);
    node = objects.head;
    while (node != NULL) {
        combat_context_init(source_obj, node->obj, &combat);
        combat.field_30 = parent_obj;
        if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x4000) != 0) {
            dam = 1;
            is_normal_dam = true;
        } else {
            dam = 3;
            is_normal_dam = false;
        }

        if (parent_obj != OBJ_HANDLE_NULL) {
            aptitude = stat_level_get(parent_obj, STAT_MAGICK_TECH_APTITUDE);
            if (aptitude > 0) {
                dam += 5 * aptitude / 100 - 1;
            }
        }

        if (is_normal_dam) {
            combat.dam[DAMAGE_TYPE_NORMAL] = dam;
        } else {
            combat.dam[DAMAGE_TYPE_FIRE] = dam;
        }

        if ((parent_obj == OBJ_HANDLE_NULL || parent_obj != node->obj)
            && critter_pc_leader_get(node->obj) != parent_obj) {
            ai_attack(parent_obj, node->obj, LOUDNESS_NORMAL, 0);

            if (combat_turn_based_is_active()
                && run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL6].data != combat_turn_based_turn_get()) {
                update_turn = true;
                combat.dam[DAMAGE_TYPE_NORMAL] *= 2;
                combat.dam[DAMAGE_TYPE_FIRE] *= 2;
                combat_dmg(&combat);
            }
        } else {
            combat_dmg(&combat);
        }

        node = node->next;
    }
    object_list_destroy(&objects);

    if (update_turn) {
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL6].data = combat_turn_based_turn_get();
    }

    return true;
}

// 0x42A930
bool anim_has_ranged_weapon(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t weapon_obj;

    source_obj = run_info->params[0].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7752, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    weapon_obj = combat_critter_weapon(source_obj);
    return item_weapon_range(weapon_obj, source_obj) > 1;
}

// 0x42A9B0
bool anim_try_pickup_item(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7778, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7779, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (!anim_pickup_item(source_obj, target_obj)) {
        return false;
    }

    return true;
}

// 0x42AA70
bool anim_pickup_item(int64_t source_obj, int64_t target_obj)
{
    int64_t parent_obj;
    int sound_id;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7803, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7804, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!item_decay_process_is_enabled()) {
        return false;
    }

    if (item_parent(target_obj, &parent_obj)) {
        if (parent_obj == OBJ_HANDLE_NULL || proto_get_description(parent_obj) != BP_JUNK_PILE) {
            return false;
        }
    }

    // CE (real-time session C): the shared cost - the original's 1.
    if (!combat_consume_action_points(source_obj, combat_pickup_cost(source_obj, target_obj))) {
        return false;
    }

    if (player_is_local_pc_obj(source_obj)) {
        sound_id = sfx_item_sound(target_obj, source_obj, OBJ_HANDLE_NULL, ITEM_SOUND_PICKUP);
        gsound_play_sfx_on_obj(sound_id, 1, source_obj);
    }

    if (!tig_net_is_active()
        || tig_net_is_host()) {
        return item_transfer(target_obj, source_obj);
    }

    return true;
}

// 0x42AB90
bool anim_throw_item(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t item_obj;
    int64_t target_obj;
    int64_t target_loc;
    int hit_loc;

    source_obj = run_info->params[0].obj;
    item_obj = run_info->params[1].obj;
    target_obj = run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj;
    target_loc = run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc;

    // CE (card S2): the aim this throw was committed with, read from the same
    // goal slot the attack goal uses (`anim_apply_attack`). The original passes
    // the literal `HIT_LOC_TORSO` here, and `HIT_LOC_TORSO` is 0, which is what
    // `anim_goal_data_init_no_interrupt` leaves in the slot - so with nothing
    // setting it this reads exactly what the original passed.
    //
    // Validated rather than trusted. `AGDATA_SCRATCH_VAL3` is shared: one of
    // its other users writes the `-1` that `anim_apply_attack` asserts against,
    // and this value indexes `hit_loc_penalties`, a four-element table. An
    // out-of-range aim is an ordinary throw, not a read off the end.
    hit_loc = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;
    if (hit_loc < 0 || hit_loc >= HIT_LOC_COUNT) {
        hit_loc = HIT_LOC_TORSO;
    }

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7861, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(item_obj != OBJ_HANDLE_NULL); // 7861, "itemObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj != OBJ_HANDLE_NULL) {
        if (target_obj == -1) {
            target_obj = OBJ_HANDLE_NULL;
        }
    } else {
        if (target_loc == 0) {
            return false;
        }
    }

    if ((obj_field_int32_get(item_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    combat_throw(source_obj, item_obj, target_obj, target_loc, hit_loc);

    run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj = OBJ_HANDLE_NULL;

    return true;
}

// 0x42ACD0
bool anim_can_talk_to_target(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int target_type;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7905, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7906, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(source_obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    target_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(target_type)) {
        if ((obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }
    }

    if (!obj_type_is_critter(target_type)) {
        return false;
    }

    return true;
}

// 0x42AE10
bool anim_target_in_dialog_range(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t source_loc;
    int64_t target_loc;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7963, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7964, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    return location_dist(source_loc, target_loc) < ai_max_dialog_distance(source_obj);
}

// 0x42AF00
bool anim_begin_dialog(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 7997, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 7998, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return false;
    }

    if (!tig_net_is_active()
        || tig_net_is_host()) {
        ui_talk_to(source_obj, target_obj);
    }

    // NOTE: Returns `false` on every path, and so does 0x42AF00 in the exe, so
    // `talk` sub-node 5 always terminates and its ANIM_FLOW_POP_GOAL success
    // edge is unreachable. Harmless here - the dialog ui has taken over by this
    // point and the goal has nothing left to do - but see
    // `anim_attempt_open_portal` for the same shape with two more dead edges.
    return false;
}

// 0x42AFB0
bool anim_cleanup_unconceal(AnimRunInfo* run_info)
{
    int64_t obj;
    int type;
    tig_art_id_t art_id;
    int64_t leader_obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8028, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0) {
        return false;
    }

    if (map_is_clearing_objects()) {
        return false;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    type = obj_field_int32_get(obj, OBJ_F_TYPE);

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
    art_id = tig_art_id_frame_set(art_id, 0);
    object_set_current_aid(obj, art_id);

    if (obj_type_is_critter(type)) {
        anim_set_obj_offset(obj, 0, 0);

        leader_obj = critter_leader_get(obj);
        if (leader_obj == OBJ_HANDLE_NULL
            || !critter_is_concealed(leader_obj)) {
            critter_set_concealed(obj, false);
        }
    }

    return true;
}

/**
 * The cleanup callback (sub-node 15) 22 goals share: drop the critter back to
 * its idle art - the conceal fidget when it is hiding, otherwise a standing
 * frame - and tell the clients about it.
 */
// 0x42B090
bool anim_cleanup_stand_anim(AnimRunInfo* run_info)
{
    int64_t obj;
    int obj_type;
    tig_art_id_t art_id;

    if (map_is_clearing_objects()) {
        return true;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        return true;
    }

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8068, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0
        || map_is_clearing_objects()
        || obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (obj_type_is_critter(obj_type)) {
        if (critter_is_concealed(obj)) {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_CONCEAL_FIDGET);
        } else {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        }

        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(obj, art_id);
        anim_set_obj_offset(obj, 0, 0);
    }

    if (tig_net_is_active()
        && tig_net_is_host()) {
        Packet10 pkt;

        pkt.type = 10;
        pkt.anim_id = run_info->id;
        pkt.oid = obj_get_id(obj);
        pkt.loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
        pkt.art_id = art_id;
        pkt.flags = run_info->flags;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    return true;
}

// 0x42B250
bool anim_cleanup_stunned(AnimRunInfo* run_info)
{
    int64_t obj;
    int obj_type;
    tig_art_id_t art_id;
    unsigned int critter_flags;

    if (map_is_clearing_objects()) {
        return true;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        return true;
    }

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8202, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0
        || map_is_clearing_objects()
        || obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (obj_type_is_critter(obj_type)) {
        if (critter_is_concealed(obj)) {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_CONCEAL_FIDGET);
        } else {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        }

        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(obj, art_id);

        anim_set_obj_offset(obj, 0, 0);

        if (!tig_net_is_active()
            || tig_net_is_host()) {
            critter_flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
            if ((critter_flags & OCF_STUNNED) != 0) {
                critter_flags &= ~OCF_STUNNED;
                obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, critter_flags);
            }
        }
    }

    if (tig_net_is_active()
        && tig_net_is_host()) {
        Packet10 pkt;

        pkt.type = 10;
        pkt.anim_id = run_info->id;
        pkt.oid = obj_get_id(obj);
        pkt.loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
        pkt.art_id = art_id;
        pkt.flags = run_info->flags;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    return true;
}

// 0x42B440
bool anim_cleanup_throw_item(AnimRunInfo* run_info)
{
    int64_t obj;
    int obj_type;
    tig_art_id_t art_id;
    int64_t item_obj;
    int inventory_location;

    if (map_is_clearing_objects()) {
        return true;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        return true;
    }

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8276, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0
        || map_is_clearing_objects()
        || obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (obj_type_is_critter(obj_type)) {
        if (critter_is_concealed(obj)) {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_CONCEAL_FIDGET);
        } else {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        }

        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(obj, art_id);

        anim_set_obj_offset(obj, 0, 0);

        if (obj_type == OBJ_TYPE_PC) {
            item_obj = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;
            if (item_obj != OBJ_HANDLE_NULL) {
                if (!tig_net_is_active()
                    || tig_net_is_host()) {
                    inventory_location = item_find_free_inv_loc_for_insertion(item_obj, obj);
                    if (inventory_location != -1) {
                        item_insert(item_obj, obj, inventory_location);
                    } else {
                        item_drop_at_loc(item_obj, obj_field_int64_get(obj, OBJ_F_LOCATION));
                    }
                }
            }
        }
    }

    if (tig_net_is_active()
        && tig_net_is_host()) {
        Packet10 pkt;

        pkt.type = 10;
        pkt.anim_id = run_info->id;
        pkt.oid = obj_get_id(obj);
        pkt.loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
        pkt.art_id = art_id;
        pkt.flags = run_info->flags;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    return true;
}

// 0x42B640
bool anim_cleanup_door_open(AnimRunInfo* run_info)
{
    int64_t door_obj;

    door_obj = run_info->params[0].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 8351, "doorObj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0) {
        return false;
    }

    if (map_is_clearing_objects()) {
        return false;
    }

    if (door_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (obj_field_int32_get(door_obj, OBJ_F_TYPE) != OBJ_TYPE_PORTAL) {
        return false;
    }

    if (portal_is_open(door_obj)) {
        return true;
    }

    if ((obj_field_int32_get(door_obj, OBJ_F_PORTAL_FLAGS) & OPF_MAGICALLY_HELD) != 0) {
        return false;
    }

    // NOTE: Why twice? Unclear, but it is Troika's - 0x42B640 in the exe emits
    // the same two back-to-back `portal_toggle` calls on this branch. Faithful,
    // so left alone.
    portal_toggle(door_obj);
    portal_toggle(door_obj);

    return true;
}

// 0x42B6F0
bool anim_cleanup_door_closed(AnimRunInfo* run_info)
{
    int64_t door_obj;

    door_obj = run_info->params[0].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 8386, "doorObj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0) {
        return false;
    }

    if (map_is_clearing_objects()) {
        return false;
    }

    if (door_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (obj_field_int32_get(door_obj, OBJ_F_TYPE) != OBJ_TYPE_PORTAL) {
        return false;
    }

    if (!portal_is_open(door_obj)) {
        return true;
    }

    if ((obj_field_int32_get(door_obj, OBJ_F_PORTAL_FLAGS) & OPF_MAGICALLY_HELD) != 0) {
        return false;
    }

    portal_toggle(door_obj);

    return true;
}

// 0x42B790
bool anim_portal_is_not_sticky(AnimRunInfo* run_info)
{
    int64_t door_obj;

    door_obj = run_info->params[0].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 8418, "doorObj != OBJ_HANDLE_NULL"

    if (door_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    return (obj_field_int32_get(door_obj, OBJ_F_PORTAL_FLAGS) & OPF_STICKY) == 0;
}

// 0x42B7F0
bool anim_doorway_is_clear(AnimRunInfo* run_info)
{
    int64_t door_obj;
    ObjectList objects;
    ObjectNode* node;

    door_obj = run_info->params[0].obj;

    ASSERT(door_obj != OBJ_HANDLE_NULL); // 8438, "doorObj != OBJ_HANDLE_NULL"

    if (door_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (combat_turn_based_is_active()) {
        return false;
    }

    object_list_location(obj_field_int64_get(door_obj, OBJ_F_LOCATION),
        OBJ_TM_NPC | OBJ_TM_PC,
        &objects);
    node = objects.head;
    while (node != NULL) {
        if (!critter_is_dead(node->obj)) {
            object_list_destroy(&objects);
            return false;
        }
        node = node->next;
    }

    object_list_destroy(&objects);
    return true;
}

// 0x42B8B0
void anim_goal_reset_position_mp(AnimID* anim_id, int64_t obj, int64_t loc, tig_art_id_t art_id, unsigned int flags, int offset_x, int offset_y)
{
    AnimRunInfo* run_info;
    char str[ANIM_ID_STR_SIZE];

    if (art_id != TIG_ART_ID_INVALID) {
        object_set_current_aid(obj, art_id);
    }

    object_move_to(obj, loc, offset_x, offset_y);

    if (anim_id_to_run_info(anim_id, &run_info)) {
        run_info->flags = flags;
    } else {
        anim_id_to_str(anim_id, str);
        tig_debug_printf("Anim: anim_goal_reset_position_mp: Could not convert ID (%s) to slot!\n", str);
    }
}

/**
 * Switch the run to running rather than walking (`run_info` flag 0x40), which
 * fails outright once the critter is carrying significant weight.
 */
// 0x42B940
bool anim_try_run(AnimRunInfo* run_info)
{
    ASSERT(run_info != OBJ_HANDLE_NULL); // 8503, "pRunInfo != NULL"
    ASSERT(run_info->anim_obj); // 8504, "pRunInfo->animObj != OBJ_HANDLE_NULL"

    if (critter_encumbrance_level_get(run_info->anim_obj) >= ENCUMBRANCE_LEVEL_SIGNIFICANT) {
        return false;
    }

    run_info->flags |= 0x40;

    return true;
}

// 0x42B9C0
bool anim_begin_attack(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t leader_obj;
    tig_art_id_t art_id;
    unsigned int flags;
    int64_t weapon_obj;
    int anim;
    int new_anim;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 8525, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 8526, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    combat_critter_activate_combat_mode(source_obj);

    if (player_is_pc_obj(target_obj)) {
        combat_critter_activate_combat_mode(target_obj);
    } else {
        leader_obj = critter_pc_leader_get(target_obj);
        if (leader_obj != OBJ_HANDLE_NULL && !critter_is_dead(leader_obj)) {
            combat_critter_activate_combat_mode(leader_obj);
        }
    }

    art_id = obj_field_int32_get(target_obj, OBJ_F_CURRENT_AID);
    anim = tig_art_id_anim_get(art_id);

    flags = obj_field_int32_get(target_obj, OBJ_F_FLAGS);
    weapon_obj = item_wield_get(source_obj, ITEM_INV_LOC_WEAPON);
    if (weapon_obj != OBJ_HANDLE_NULL
        && (obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_BOOMERANGS) != 0) {
        new_anim = TIG_ART_ANIM_THROW;
    } else if ((flags & OF_SHRUNK) != 0) {
        new_anim = TIG_ART_ANIM_ATTACK_LOW;
    } else {
        switch (anim) {
        case TIG_ART_ANIM_FALL_DOWN:
        case TIG_ART_ANIM_GET_UP:
        case TIG_ART_ANIM_9:
        case TIG_ART_ANIM_10:
        case TIG_ART_ANIM_11:
            new_anim = TIG_ART_ANIM_ATTACK_LOW;
            break;
        default:
            new_anim = tig_art_critter_id_size_get(art_id) != TIG_ART_CRITTER_SIZE_SMALL
                ? TIG_ART_ANIM_ATTACK
                : TIG_ART_ANIM_ATTACK_LOW;
            break;
        }
    }

    if (!combat_critter_is_combat_mode_active(source_obj)) {
        return false;
    }

    art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, new_anim);
    art_id = tig_art_id_frame_set(art_id, 0);
    if (tig_art_exists(art_id) != TIG_OK) {
        // NOTE: Verified against 0x42BBAC - original repeats tig_art_exists on the missing-art path and discards the result; transcription faithful.
        tig_art_exists(art_id);

        tig_debug_printf("Anim: ERROR: Missing art: %u, List: %d,",
            art_id,
            tig_art_type(art_id));
        tig_debug_printf(" Num: %d, Anim: %d,",
            tig_art_num_get(art_id),
            tig_art_id_anim_get(art_id));
        tig_debug_printf(" Weapon: %d, Specie: %d!\n",
            tig_art_critter_id_weapon_get(art_id),
            tig_art_monster_id_specie_get(art_id));

        return false;
    }

    object_set_current_aid(source_obj, art_id);
    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;

    return true;
}

// 0x42BC10
bool AGsetObjArtDeathAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    int anim;
    int64_t pc_obj;
    tig_art_id_t art_id;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8623, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: AGsetObjArtDeathAnim: Failed to set art to Death art!!!\n");
        return false;
    }

    anim = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;

    // NOTE: Verified against 0x42BC68-0x42BC74 - original fetches the local PC and reads OBJ_F_PC_FLAGS without using the result; transcription faithful.
    pc_obj = player_get_local_pc_obj();
    obj_field_int32_get(pc_obj, OBJ_F_PC_FLAGS);

    if (violence_filter != 0) {
        anim = 7;
    }

    combat_critter_deactivate_combat_mode(obj);

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, anim);
    art_id = tig_art_id_frame_set(art_id, 0);
    if (tig_art_exists(art_id) != TIG_OK) {
        if (violence_filter != 0) {
            ASSERT(0); // 8647, "0"
            return false;
        }

        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_BLOWN_OUT_CHUNK);
        tig_debug_printf("Anim: Error: Missing Death Art: %d: Using Default\n", anim);
    }

    if (tig_art_exists(art_id) != TIG_OK) {
        tig_debug_printf("Anim: AGsetObjArtDeathAnim: Failed to set art to Death art!!!\n");
        return false;
    }

    object_set_current_aid(obj, art_id);
    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;

    return true;
}

// 0x42BD40
bool AGsetObjArtAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8676, "obj != OBJ_HANDLE_NULL"
    ASSERT(run_info->params[2].data != -1); // 8677, "pRunInfo->params[2].dataVal != -1"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, run_info->params[2].data);
    art_id = tig_art_id_frame_set(art_id, 0);
    if (tig_art_exists(art_id) != TIG_OK) {
        return false;
    }

    object_set_current_aid(obj, art_id);

    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGsetObjArtAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    return true;
}

// 0x42BE50
bool anim_set_scratch_obj(AnimRunInfo* run_info)
{
    run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj = run_info->params[0].obj;

    return true;
}

// 0x42BE80
bool anim_set_skill(AnimRunInfo* run_info)
{
    run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data = run_info->params[2].data;

    return true;
}

// 0x42BEA0
bool anim_set_range(AnimRunInfo* run_info)
{
    run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data = run_info->params[2].data;

    return true;
}

// 0x42BEC0
bool anim_update_target_tile(AnimRunInfo* run_info)
{
    int64_t target_obj;
    int64_t target_loc;

    target_obj = run_info->params[0].obj;

    ASSERT(target_obj != OBJ_HANDLE_NULL); // 8742, "targetObj != OBJ_HANDLE_NULL"

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);

    if (run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc == target_loc) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = target_loc;

    run_info->path.flags |= 0x1;

    return true;
}

// 0x42BF40
bool anim_set_follow_range(AnimRunInfo* run_info)
{
    int64_t obj;
    unsigned int flags;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8765, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    flags = 0;
    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
    }

    run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data = (flags & ONF_AI_SPREAD_OUT) != 0 ? 5 : 2;

    return true;
}

// 0x42BFD0
bool anim_is_crowding_target(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    unsigned int flags;
    int v2;
    int64_t source_loc;
    int64_t target_loc;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 8792, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 8793, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    flags = 0;
    if (obj_field_int32_get(source_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        flags = obj_field_int32_get(source_obj, OBJ_F_NPC_FLAGS);
    }

    v2 = (flags & ONF_AI_SPREAD_OUT) != 0 ? 3 : 1;

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    return location_dist(source_loc, target_loc) < v2;
}

// 0x42C0F0
bool anim_face_target(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t source_loc;
    int64_t target_loc;
    int rot;
    tig_art_id_t art_id;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 8832, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 8833, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(source_obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    rot = location_rot(source_loc, target_loc);

    art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_rotation_set(art_id, rot);
    if (tig_art_exists(art_id) == TIG_OK) {
        object_set_current_aid(source_obj, art_id);
        run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    }

    return true;
}

// 0x42C240
bool anim_face_away_from_target(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t source_loc;
    int64_t target_loc;
    int rot;
    tig_art_id_t art_id;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 8832, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 8833, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(source_obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    rot = location_rot(target_loc, source_loc);
    art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_rotation_set(art_id, rot);
    if (tig_art_exists(art_id) == TIG_OK) {
        object_set_current_aid(source_obj, art_id);
        run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    }

    return true;
}

// 0x42C390
bool anim_face_target_tile(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t source_loc;
    int64_t target_loc;
    int rot;
    tig_art_id_t art_id;

    source_obj = run_info->params[0].obj;
    target_loc = run_info->params[1].loc;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 8927, "sourceObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    rot = location_rot(source_loc, target_loc);
    art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_rotation_set(art_id, rot);
    if (tig_art_exists(art_id) == TIG_OK) {
        object_set_current_aid(source_obj, art_id);
        run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    }

    return true;
}

// 0x42C440
bool AGperformRotateAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    int new_rot;
    int rot;
    int next_rot;
    int delta;
    tig_art_id_t art_id;
    bool rc = true;

    obj = run_info->params[0].obj;
    new_rot = run_info->params[1].data;

    ASSERT(obj != OBJ_HANDLE_NULL); // 8959, "obj != OBJ_HANDLE_NULL"
    ASSERT(new_rot >= 0); // 8960, "newRot >= 0"
    ASSERT(new_rot < 8); // 8961, "newRot < tig_art_rotation_max"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    rot = tig_art_id_rotation_get(art_id);
    if (rot == new_rot) {
        return false;
    }

    if (combat_turn_based_is_active()
        && combat_turn_based_whos_turn_get() != obj) {
        return true;
    }

    if (combat_turn_based_critter_can_fastforward(obj)) {
        next_rot = new_rot;
        rc = false;
    } else {
        // NOTE: Original code is probably different.
        delta = rot - new_rot;

        if (delta <= 0) {
            next_rot = delta >= -4 ? rot + 1 : rot + 7;
        } else {
            next_rot = delta < 4 ? rot + 7 : rot + 1;
        }

        if (next_rot < 0) {
            next_rot = 8 - (next_rot % 8);
        } else {
            next_rot = next_rot % 8;
        }
    }

    art_id = tig_art_id_rotation_set(art_id, next_rot);

    if (tig_art_exists(art_id) != TIG_OK) {
        return false;
    }

    object_set_current_aid(obj, art_id);
    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;

    return rc;
}

// 0x42C610
bool anim_set_rotation_from_path(AnimRunInfo* run_info)
{
    if (run_info->cur_stack_data == NULL) {
        run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = run_info->path.rotations[0];

    return true;
}

// 0x42C650
bool anim_cleanup_rotate(AnimRunInfo* run_info)
{
    int64_t obj;
    int new_rot;
    tig_art_id_t art_id;

    obj = run_info->params[0].obj;
    new_rot = run_info->params[1].data;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9055, "obj != OBJ_HANDLE_NULL"
    ASSERT(new_rot >= 0); // 9056, "newRot >= 0"
    ASSERT(new_rot < 8); // 9057, "newRot < tig_art_rotation_max"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_id_rotation_get(art_id) != new_rot) {
        art_id = tig_art_id_rotation_set(art_id, new_rot);
        if (tig_art_exists(art_id) == TIG_OK) {
            object_set_current_aid(obj, art_id);
            run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
        }
    }

    return true;
}

// 0x42C780
bool anim_face_path_direction(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9099, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_rotation_set(art_id, run_info->path.rotations[run_info->path.curr]);
    if (tig_art_exists(art_id) == TIG_OK) {
        object_set_current_aid(obj, art_id);
        run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    }

    return true;
}

// 0x42C850
bool anim_face_spell_target(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int64_t source_loc;
    int64_t target_loc;
    tig_art_id_t art_id;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 9141, "sourceObj != OBJ_HANDLE_NULL"

    if (target_obj != OBJ_HANDLE_NULL && source_obj != target_obj) {
        if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
            return false;
        }

        if (obj_type_is_critter(obj_field_int32_get(source_obj, OBJ_F_TYPE))
            && (obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return false;
        }

        source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
        target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);

        art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_rotation_set(art_id, location_rot(source_loc, target_loc));
        if (tig_art_exists(art_id) == TIG_OK) {
            object_set_current_aid(source_obj, art_id);
        }
    }

    return true;
}

/**
 * No animation started by this run is still playing (`run_info` flag 0x10,
 * raised by the `AGbeginAnim*` callbacks and dropped when the last frame is
 * reached). The entry sub-node of 30 goals, branching between beginning an
 * animation and updating the one already running.
 */
// 0x42CA90
bool anim_is_not_animating(AnimRunInfo* run_info)
{
    return (run_info->flags & 0x10) == 0;
}

/**
 * The current animation has reached its action frame - the moment the blow
 * lands or the item changes hands - and this is the first sub-node to notice.
 * Flag 0x04 is raised by the update callbacks; 0x08 latches so the action
 * fires once.
 */
// 0x42CAA0
bool anim_reached_action_frame(AnimRunInfo* run_info)
{
    // CE (real-time combat session 1): the release, observed where it is
    // decided rather than inferred from a later damage event.
    //
    // This is the one place that answers "how often did this actor get to
    // act". The ranged repeat in `AGupdateAnimAttack` rewinds the animation
    // and clears 0x0C, which re-arms the latch below, so a bow firing four
    // arrows passes through here four times while the attack animation is
    // never restarted. Counting `damage.committed` instead would miss every
    // release that hit nothing, and counting frames would not know which ones
    // were action frames.
    char actor[COMBAT_OBSERVER_IDENTITY_SIZE];
    AnimGoalData* goal_data;
    int64_t self_obj;
    int64_t weapon_obj;
    bool is_critter;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    if ((run_info->flags & 0x4) == 0) {
        return false;
    }

    if ((run_info->flags & 0x8) != 0) {
        return false;
    }

    run_info->flags |= 0x8;

    // CE (real-time session B): the release, where the action budget starts
    // counting recovery rather than wind-up.
    rt_budget_released(run_info);

    if (combat_observer_is_enabled()) {
        goal_data = run_info->cur_stack_data;
        self_obj = goal_data != NULL
            ? goal_data->params[AGDATA_SELF_OBJ].obj
            : OBJ_HANDLE_NULL;
        if (self_obj != OBJ_HANDLE_NULL) {
            combat_observer_object_id(self_obj, actor);
            art_id = obj_field_int32_get(self_obj, OBJ_F_CURRENT_AID);
            // A frame the art system cannot describe is reported as -1 rather
            // than left as whatever was on the stack: an unreadable frame is a
            // fact about the run, and silently emitting a plausible number
            // would put it in the baseline as data.
            //
            // CE (card RT1): the timing inputs are read, not recomputed into
            // the run. `anim_get_animation_fps` and `item_weapon_magic_speed`
            // only read fields, and `step_ms` is the pause the animation is
            // running at now, after both clamps.
            is_critter = obj_type_is_critter(obj_field_int32_get(self_obj, OBJ_F_TYPE));
            weapon_obj = is_critter
                ? item_wield_get(self_obj, ITEM_INV_LOC_WEAPON)
                : OBJ_HANDLE_NULL;
            if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                combat_observer_action_released(actor, goal_data->type, -1, -1,
                    tig_art_critter_id_weapon_get(art_id), -1, -1,
                    item_weapon_magic_speed(weapon_obj, self_obj),
                    run_info->pause_time.milliseconds);
            } else {
                combat_observer_action_released(actor, goal_data->type,
                    tig_art_id_frame_get(art_id), art_anim_data.action_frame,
                    tig_art_critter_id_weapon_get(art_id), art_anim_data.fps,
                    is_critter
                        ? anim_get_animation_fps(self_obj, art_id,
                              stat_level_get(self_obj, STAT_SPEED))
                        : art_anim_data.fps,
                    item_weapon_magic_speed(weapon_obj, self_obj),
                    run_info->pause_time.milliseconds);
            }
        }
    }

    return true;
}

// 0x42CAC0
bool anim_cleanup_projectile(AnimRunInfo* run_info)
{
    AnimGoalData* goal_data;
    int64_t self_obj;
    int64_t parent_obj;

    goal_data = run_info->cur_stack_data;
    if (goal_data != NULL) {
        self_obj = goal_data->params[AGDATA_SELF_OBJ].obj;
        parent_obj = goal_data->params[AGDATA_PARENT_OBJ].obj;
        if (self_obj != OBJ_HANDLE_NULL
            && (obj_field_int32_get(self_obj, OBJ_F_FLAGS) & OF_DESTROYED) == 0) {
            // CE (card RT1): the flight ended without arriving anywhere. Until
            // 2026-09-22 this was the only place a projectile was observed, so
            // it was reported as an impact - on 3 of 34 releases, never with a
            // target, because `combat` is always NULL here.
            combat_projectile_observe(parent_obj,
                goal_data->params[AGDATA_TARGET_OBJ].obj, OBJ_HANDLE_NULL,
                "abandoned");
            combat_projectile_goal_end(self_obj, parent_obj, NULL);
        }
    }

    return true;
}

// 0x42CB10
bool AGbeginAnimAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int anim;
    int sound_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9273, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        anim = tig_art_id_anim_get(art_id);
        if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0
            && !IS_BLOODY_DEATH_ANIM(anim)) {
            return false;
        }

        if (run_info->cur_stack_data->type == AG_DYING) {
            if (anim != TIG_ART_ANIM_FALL_DOWN) {
                sound_id = sfx_critter_sound(obj, CRITTER_SOUND_DYING_GRUESOME);
            } else {
                sound_id = sfx_critter_sound(obj, CRITTER_SOUND_DYING);
            }
            gsound_play_sfx_on_obj(sound_id, 1, obj);
        }
    }

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags &= ~0x0C;
    run_info->flags |= 0x10;

    return true;
}

// 0x42CC80
bool anim_advance_frame(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int anim;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9345, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    // CE (real-time session B): the same hold for a spell with a casting
    // animation; every other goal advancing frames here passes straight through.
    if (rt_budget_hold(run_info)) {
        return true;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        anim = tig_art_id_anim_get(art_id);
        if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0
            && !IS_BLOODY_DEATH_ANIM(anim)
            && anim != TIG_ART_ANIM_FALL_DOWN
            && anim != TIG_ART_ANIM_11) {
            return false;
        }

        if (anim == 14
            && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_USING_BOOMERANG) != 0) {
            return true;
        }
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x42CDA0 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42CDF0
bool anim_begin_fidget_anim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9416, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    if (object_dist(obj, player_get_local_pc_obj()) > 30) {
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    // NOTE: Useless call to `sub_503E20` is omitted.
    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags &= ~0x0C;
    run_info->flags |= 0x10;

    return true;
}

// 0x42CF40
bool anim_advance_fidget_frame(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;
    int sound_id;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9482, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == 1) {
        sound_id = sfx_critter_sound(obj, CRITTER_SOUND_FIDGETING);
        gsound_play_sfx_on_obj(sound_id, 1, obj);
    }

    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x42D032 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42D080
bool AGbeginAnimOpenDoor(AnimRunInfo* run_info)
{
    int64_t door_obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int sound_id;

    door_obj = run_info->params[0].obj;
    ASSERT(door_obj != OBJ_HANDLE_NULL); // 9543, doorObj != OBJ_HANDLE_NULL
    if (door_obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: AGbeginAnimOpenDoor: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = obj_field_int32_get(door_obj, OBJ_F_CURRENT_AID);
    if (!portal_open(door_obj)) {
        return false;
    }

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimOpenDoor: Failed to find Aid: %d, defaulting to 10 fps!\n", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags &= ~0xC;

    sound_id = sfx_portal_sound(door_obj, PORTAL_SOUND_OPEN);
    gsound_play_sfx_on_obj(sound_id, 1, door_obj);

    run_info->flags |= 0x10;

    return true;
}

// 0x42D160
bool AGupdateAnimOpenDoor(AnimRunInfo* run_info)
{
    int64_t door_obj;

    door_obj = run_info->params[0].obj;
    ASSERT(door_obj != OBJ_HANDLE_NULL); // 9580, doorObj != OBJ_HANDLE_NULL
    if (door_obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: AGupdateAnimOpenDoor: Warning: Goal Received NULL Object!\n");
        return false;
    }

    if (!portal_open(door_obj)) {
        return false;
    }

    return true;
}

// 0x42D1C0
bool AGbeginAnimCloseDoor(AnimRunInfo* run_info)
{
    int64_t door_obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int sound_id;

    door_obj = run_info->params[0].obj;
    ASSERT(door_obj != OBJ_HANDLE_NULL); // 9603, doorObj != OBJ_HANDLE_NULL
    if (door_obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: AGbeginAnimCloseDoor: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = obj_field_int32_get(door_obj, OBJ_F_CURRENT_AID);
    if (!portal_close(door_obj)) {
        return false;
    }

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimCloseDoor: Failed to find Aid: %d, defaulting to 10 fps!\n", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags &= ~0xC;

    sound_id = sfx_portal_sound(door_obj, PORTAL_SOUND_CLOSE);
    gsound_play_sfx_on_obj(sound_id, 1, door_obj);

    run_info->flags |= 0x10;

    return true;
}

// 0x42D2A0
bool AGupdateAnimCloseDoor(AnimRunInfo* run_info)
{
    int64_t door_obj;

    door_obj = run_info->params[0].obj;
    ASSERT(door_obj != OBJ_HANDLE_NULL); // 9640, doorObj != OBJ_HANDLE_NULL
    if (door_obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: AGupdateAnimCloseDoor: Warning: Goal Received NULL Object!\n");
        return false;
    }

    if (!portal_close(door_obj)) {
        return false;
    }

    return true;
}

// 0x42D300
bool AGbeginAnimPickLock(AnimRunInfo* run_info)
{
    int64_t obj;
    int obj_type;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9665, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    ASSERT(obj_type_is_critter(obj_type)); // 9674, "obj_type_is_critter(objType)"

    if (!obj_type_is_critter(obj_type)) {
        return false;
    }

    if (run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data < 1) {
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimPickLock: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags &= ~0x0C;
    run_info->flags |= 0x10;

    return true;
}

// 0x42D440
bool AGupdateAnimPickLock(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9717, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    object_inc_current_aid(obj);

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 2
        && run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data > 0) {
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data--;
        art_id = tig_art_id_frame_set(art_id, 2);
        object_set_current_aid(obj, art_id);
        return true;
    }

    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x42D520 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42D570
bool AGbeginAnimDying(AnimRunInfo* run_info)
{
    int64_t obj;
    int obj_type;
    tig_art_id_t art_id;
    int anim;
    int sound_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9779, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    ASSERT(obj_type_is_critter(obj_type)); // 9788, "obj_type_is_critter(objType)"

    if (!obj_type_is_critter(obj_type)) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    anim = tig_art_id_anim_get(art_id);

    if ((IS_BLOODY_DEATH_ANIM(anim) || anim == TIG_ART_ANIM_FALL_DOWN)
        && tig_art_id_frame_get(art_id) > 0) {
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    if (tig_art_id_anim_get(art_id) != TIG_ART_ANIM_FALL_DOWN) {
        sound_id = sfx_critter_sound(obj, CRITTER_SOUND_DYING_GRUESOME);
    } else {
        sound_id = sfx_critter_sound(obj, CRITTER_SOUND_DYING);
    }
    gsound_play_sfx_on_obj(sound_id, 1, obj);

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimDying: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags |= 0x10;

    return true;
}

// 0x42D6F0
bool AGupdateAnimDying(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9843, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x42D787 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42D7D0
bool AGbeginAnimJump(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int anim;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9889, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        anim = tig_art_id_anim_get(art_id);
        if (!IS_BLOODY_DEATH_ANIM(anim)) {
            return false;
        }
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimJump: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags |= 0x10;

    return true;
}

// 0x42D910
bool AGupdateAnimJump(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int anim;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 9948, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    // NOTE: Verified against 0x42D967 - original calls player_is_local_pc_obj(obj) and discards the result; transcription faithful.
    player_is_local_pc_obj(obj);

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        anim = tig_art_id_anim_get(art_id);
        if (!IS_BLOODY_DEATH_ANIM(anim)) {
            return false;
        }
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x42DA09 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42DA50
bool AGbeginAnimLoopAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int anim;
    TigArtAnimData art_anim_data;
    int sound_id;
    int max_sound_distance;
    tig_sound_handle_t sound_handle;
    int64_t loc;
    int64_t player_loc;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10027, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id == TIG_ART_ID_INVALID) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        anim = tig_art_id_anim_get(art_id);
        if (!IS_BLOODY_DEATH_ANIM(anim)) {
            return false;
        }
    }

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimLoopAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    ASSERT(run_info->cur_stack_data != NULL); // 10074, "pRunInfo->pCurStackData != NULL"

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = 1;
    if (!tig_net_is_active()) {
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = object_dist(obj, player_get_local_pc_obj()) < 30;
    }

    run_info->flags |= 0x10;

    if ((run_info->flags & 0x20000) != 0) {
        return true;
    }

    sound_id = sfx_misc_sound(obj, MISC_SOUND_ANIMATING);
    if (sound_id == -1) {
        return true;
    }

    max_sound_distance = gsound_range(obj);
    sound_handle = run_info->goals[0].params[AGDATA_SOUND_HANDLE].data;
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    player_loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    if (location_dist(player_loc, loc) > max_sound_distance) {
        if (sound_handle != TIG_SOUND_HANDLE_INVALID) {
            tig_sound_destroy(sound_handle);
            run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data = TIG_SOUND_HANDLE_INVALID;
        }
    } else if (sound_handle == TIG_SOUND_HANDLE_INVALID) {
        sound_handle = gsound_play_sfx_on_obj(sound_id, 1, obj);
        if (sound_handle != TIG_SOUND_HANDLE_INVALID) {
            run_info->goals[0].params[AGDATA_SOUND_HANDLE].data = sound_handle;
        } else {
            tig_debug_printf("Anim: ERROR: Animate Forever: Sound Failed to Start!\n");
        }
    }

    return true;
}

// 0x42DCF0
bool anim_loop_is_near_pc(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10145, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) {
        tig_debug_printf("Anim: Warning: Goal Received NULL Object!\n");
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = 0;
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data++;
    if (run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data < 3) {
        return run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data;
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data = 0;

    if (!tig_net_is_active()) {
        if (object_dist(obj, player_get_local_pc_obj()) >= 30) {
            run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = 0;
            return false;
        }
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = 1;
    return true;
}

// 0x42DDE0
bool AGupdateAnimLoopAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10193, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) return false;

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    // NOTE: Verified against 0x42DE4D - original calls obj_field_int32_get(obj, OBJ_F_TYPE) and discards the result; transcription faithful.
    obj_field_int32_get(obj, OBJ_F_TYPE);

    object_inc_current_aid(obj);

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        frame = tig_art_id_frame_get(art_id);
        if (frame == art_anim_data.num_frames - 1) {
            run_info->flags &= ~0x4;
        }
        if (frame == art_anim_data.action_frame - 1) {
            run_info->flags |= 0x4;
        }
        return frame % 3 != 1;
    } else {
        run_info->flags &= ~0x10;
        return false;
    }
}

// 0x42DED0
bool anim_cleanup_loop_sound(AnimRunInfo* run_info)
{
    ASSERT(run_info != NULL); // 10259, pRunInfo != NULL

    if (run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data != TIG_SOUND_HANDLE_INVALID
        && (run_info->flags & 0x20000) == 0) {
        tig_sound_destroy(run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data);
        run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data = TIG_SOUND_HANDLE_INVALID;
    }

    return true;
}

// 0x42DF40
bool AGbeginStunAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int anim;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10292, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STUNNED);
    art_id = tig_art_id_frame_set(art_id, 0);
    object_set_current_aid(obj, art_id);

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_PARALYZED) != 0) {
            anim = tig_art_id_anim_get(art_id);
            if (!IS_BLOODY_DEATH_ANIM(anim)) {
                return false;
            }
        }
    }

    // NOTE: Verified against 0x42DFA6 and 0x42E004 - original sets current aid twice; transcription faithful.
    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        tig_debug_printf("Anim: AGbeginStunAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
        // NOTE: Faithful. The inconsistency with the sibling handlers is
        // Troika's. The original sets the flag only on the success path
        // (0x42E02E), and its failure path stores 100 and returns 0 without
        // touching it:
        //
        //     42E017: test eax, eax
        //     42E019: jne  0x42E041          ; not TIG_OK
        //     42E01B: mov eax, 0x3E8 / div   ; 1000 / fps
        //     42E028: mov [ebp+0xCFC], eax   ; pause_time.milliseconds
        //     42E02E: mov eax, [ebp+0xC]
        //     42E031: or  al, 0x10           ; flags |= 0x10, success only
        //     42E037: mov eax, 1             ; return true
        //     42E041: ... tig_debug_printf
        //     42E04F: mov DWORD PTR [ebp+0xCFC], 0x64   ; = 100
        //     42E05C: xor eax, eax           ; return false
        return false;
    }

    run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    run_info->flags |= 0x10;

    return true;
}

// 0x42E070
bool AGupdateStunAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    int anim;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10354, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_PARALYZED) != 0) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        anim = tig_art_id_anim_get(art_id);
        if (!IS_BLOODY_DEATH_ANIM(anim)) {
            return false;
        }
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x42E169 - original calls tig_art_type and discards the result; CE previously omitted the call.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42E1B0
bool AGbeginKneelMagicHandsAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10416, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_9);
    art_id = tig_art_id_frame_set(art_id, 0);
    object_set_current_aid(obj, art_id);

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_PARALYZED) != 0) {
        return false;
    }

    object_set_current_aid(obj, art_id);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginKneelMHAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags |= 0x10;
    return true;
}

// 0x42E2D0
bool AGupdateKneelMagicHandsAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10463, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_PARALYZED) != 0) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 1) {
        if (tig_art_id_anim_get(art_id) != TIG_ART_ANIM_9) {
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_9);
            if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                return false;
            }

            art_id = tig_art_id_frame_set(art_id, art_anim_data.num_frames - 1);
            object_set_current_aid(obj, art_id);
            run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = TIG_ART_ID_INVALID;
            run_info->flags &= ~0x04;
            return false;
        } else {
            art_id = tig_art_id_frame_set(art_id, 0);
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_10);
            object_set_current_aid(obj, art_id);
            run_info->flags &= ~0x04;
            return true;
        }
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_inc_current_aid(obj);
    return true;
}

// 0x42E460
bool anim_is_conscious(AnimRunInfo* run_info)
{
    int64_t self_obj;

    self_obj = run_info->params[0].obj;

    ASSERT(self_obj != OBJ_HANDLE_NULL); // 10534, "selfObj != OBJ_HANDLE_NULL"
    if (self_obj == OBJ_HANDLE_NULL) return false;

    return !critter_is_unconscious(self_obj);
}

// 0x42E4B0
bool AGbeginKnockDownAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10553, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_FALL_DOWN);
    art_id = tig_art_id_frame_set(art_id, 0);
    object_set_current_aid(obj, art_id);

    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginKnockDownAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->flags |= 0x10;

    return true;
}

// 0x42E590
bool AGbeginGetUpAnim(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10585, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!critter_is_active(obj)) {
        return false;
    }

    // FIX (porting bug #55): in turn-based a critter gets up only on its own
    // turn, paying for it, as the original refuses when it is not
    // (0x42E5F2-0x42E5F8: `jne` to the failure exit). CE had the test
    // inverted, so a knocked-down critter got up for free on anyone else's
    // turn and was refused the animation on its own.
    if (combat_turn_based_is_active()
        // CE (real-time session C): the shared cost - the original's 5.
        && (combat_turn_based_whos_turn_get() != obj || !combat_consume_action_points(obj, combat_get_up_cost(obj)))) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_GET_UP);
    art_id = tig_art_id_frame_set(art_id, 0);

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginGetUpAnim: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
        ASSERT(0); // 10617, "0"
    }

    object_set_current_aid(obj, art_id);

    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = art_id;
    run_info->flags |= 0x10;

    return true;
}

// 0x42E6B0
bool anim_save_current_aid(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10638, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) return false;

    if (!critter_is_active(obj)) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_ANIM_ID].data = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    return true;
}

// 0x42E720
bool AGbeginAnimAnimReverse(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10664, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    art_id = run_info->params[1].data;
    if (art_id != TIG_ART_ID_INVALID) {
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
        } else {
            tig_debug_printf("Anim: AGbeginAnimAnimReverse: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
            run_info->pause_time.milliseconds = 100;
        }
    } else {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
        } else {
            tig_debug_printf("Anim: AGbeginAnimAnimReverse: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
            run_info->pause_time.milliseconds = 100;
        }

        art_id = tig_art_id_frame_set(art_id, art_anim_data.num_frames - 1);
    }

    object_set_current_aid(obj, art_id);

    run_info->flags |= 0x10;

    return true;
}

// 0x42E8B0
bool AGupdateAnimAnimReverse(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10730, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == 0) {
        // NOTE: Verified against 0x42E962 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    object_dec_current_aid(obj);
    return true;
}

// 0x42E9B0
bool AGbeginAnimMove(AnimRunInfo* run_info)
{
    int64_t obj;
    unsigned int spell_flags;
    int obj_type;
    tig_art_id_t art_id;
    int anim;
    int v2;
    TigArtAnimData art_anim_data;
    int64_t loc;
    int64_t next_loc;
    int rot;

    if ((run_info->flags & 0x100) != 0) {
        return false;
    }

    if ((run_info->flags & 0x20) != 0) {
        run_info->flags |= 0x10;
        return true;
    }

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 10798, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    spell_flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);
    if ((spell_flags & OSF_STONED) != 0) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if (obj_type_is_critter(obj_type)
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    anim = tig_art_id_anim_get(art_id);
    run_info->cur_stack_data->params[AGDATA_ANIM_ID_PREVIOUS].data = art_id;
    if (tig_art_id_anim_get(art_id) == TIG_ART_ANIM_CONCEAL_FIDGET
        && critter_is_concealed(obj)) {
        run_info->cur_stack_data->params[AGDATA_ANIM_ID_PREVIOUS].data = tig_art_id_anim_set(run_info->cur_stack_data->params[AGDATA_ANIM_ID_PREVIOUS].data, 0);
    }

    v2 = 2;
    anim_move_art_id_get(run_info, obj, &art_id, (run_info->flags & 0x40) != 0, &v2);

    if (!combat_check_action_points(obj, v2)) {
        anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
        return false;
    }

    if (combat_turn_based_critter_can_fastforward(obj)) {
        loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        // UAP (0x42EB4E, 0x42EBBE): both fast-forward reads of
        // `combat_action_points_get` are retargeted from 0x4B7780 to the
        // relocated copy at 0x4B7774. Same getter, no behaviour change.
        while (combat_action_points_get() > 0) {
            if (!anim_spend_move_cost(run_info)) {
                break;
            }

            if (anim_check_next_step(run_info, obj, loc)) {
                break;
            }

            rot = run_info->path.rotations[run_info->path.curr++];
            location_in_dir(loc, rot, &next_loc);
            loc = next_loc;

            if (run_info->path.curr >= run_info->path.max) {
                break;
            }
        }

        object_move_to(obj, loc, 0, 0);
        run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = loc;
        run_info->goals[0].params[AGDATA_TARGET_TILE].loc = loc;

        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_rotation_set(art_id, run_info->path.rotations[run_info->path.curr - 1]);
        object_set_current_aid(obj, art_id);

        return false;
    }

    art_id = tig_art_id_rotation_set(art_id, run_info->path.rotations[0]);
    if (anim != tig_art_id_anim_get(art_id)) {
        art_id = tig_art_id_frame_set(art_id, 0);
    }
    object_set_current_aid(obj, art_id);

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    if (!location_in_dir(loc, run_info->path.rotations[0], &next_loc)) {
        return false;
    }

    if ((run_info->path.rotations[0] & 1) != 0) {
        run_info->extra_target_tile = 0;
    } else {
        run_info->extra_target_tile = next_loc;
    }

    if ((run_info->flags & 0x400) != 0) {
        if ((run_info->path.flags & 0x01) != 0) {
            rot = tig_art_id_rotation_get(art_id);
        } else {
            rot = run_info->path.rotations[run_info->path.curr];
        }

        if (anim_tile_is_blocked(obj, loc, next_loc, rot)) {
            return false;
        }
    }

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimMove: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    anim_move_pause_time_get(obj, &(run_info->pause_time));

    if ((spell_flags & OSF_SHRUNK) != 0) {
        run_info->pause_time.milliseconds *= 2;
    }

    run_info->path.curr = 0;
    run_info->path.flags &= ~0x03;
    run_info->flags |= 0x30;

    return true;
}

// 0x42EDC0
// Chooses the movement art id for `obj`: stealth-walk while concealed below
// expert prowling, otherwise walk, promoted to run when the caller permits it
// and the critter still has fatigue. Callers are AGbeginAnimMove and
// AGupdateAnimMove; `run_out` reports whether run was selected.
void anim_move_art_id_get(AnimRunInfo* run_info, int64_t obj, tig_art_id_t* art_id_ptr, bool allow_run, int* run_out)
{
    tig_art_id_t art_id;
    bool concealed;

    art_id = *art_id_ptr;
    concealed = critter_is_concealed(obj);

    if (concealed && basic_skill_training_get(obj, BASIC_SKILL_PROWLING) < TRAINING_EXPERT) {
        *art_id_ptr = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STEALTH_WALK);
        return;
    }

    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_WALK);
    if (run_info->current_goal <= 0
        || !allow_run
        || (concealed
            && basic_skill_training_get(obj, BASIC_SKILL_PROWLING) < TRAINING_MASTER)) {
        *art_id_ptr = art_id;
        return;
    }

    if (critter_fatigue_current(obj) <= 0) {
        run_info->flags &= ~0x40;
        *art_id_ptr = art_id;
        return;
    }

    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_RUN);
    if (run_out != NULL) {
        *run_out = 1;
    }
    *art_id_ptr = art_id;
}

// 0x42EE90
// Milliseconds per animation frame for the critter's current art and speed,
// clamped to 30..800. Shared by the move and attack animation updates.
void anim_move_pause_time_get(int64_t obj, DateTime* pause_time)
{
    int speed;
    tig_art_id_t art_id;
    int ms;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11052, "obj != OBJ_HANDLE_NULL"
    ASSERT(pause_time != NULL); // 11053, "pPauseTime != NULL"

    speed = stat_level_get(obj, STAT_SPEED);
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    ms = 1000 / anim_get_animation_fps(obj, art_id, speed);
    if (ms < 30) {
        ms = 30;
    } else if (ms > 800) {
        ms = 800;
    }
    pause_time->milliseconds = ms;
}

// 0x42EF60
// Whether the signed pixel offsets have reached the boundary for movement
// direction 0..7. The switch is the 8-direction version of the path
// step-completion test used by AGupdateAnimMove.
bool anim_move_tile_reached(int direction, int offset_x, int offset_y)
{
    switch (direction) {
    case 0:
        return offset_y > 0 ? false : true;
    case 1:
        if (offset_y > 0) {
            return false;
        } else {
            return offset_x < 0 ? false : true;
        }
    case 2:
        return offset_x < 0 ? false : true;
    case 3:
        return offset_y < 0 || offset_x < 0 ? false : true;
    case 4:
        return offset_y < 0 ? false : true;
    case 5:
        if (offset_y < 0) {
            return false;
        } else {
            return offset_x > 0 ? false : true;
        }
    case 6:
        return offset_x > 0 ? false : true;
    case 7:
        if (offset_y > 0) {
            return false;
        } else {
            return offset_x > 0 ? false : true;
        }
    }

    return false;
}

// 0x42F000
bool anim_vault_to_next_tile(AnimRunInfo* run_info)
{
    int x_shifts[] = {
        0,
        40,
        80,
        40,
        0,
        -40,
        -80,
        -40,
    };

    int y_shifts[] = {
        -40,
        -20,
        0,
        20,
        40,
        20,
        0,
        -20,
    };

    int64_t obj;
    int64_t loc;
    int offset_x;
    int offset_y;
    int rot;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11248, "obj != OBJ_HANDLE_NULL";

    // NOTE: Verified against 0x42F0A0 - original calls obj_field_int32_get(obj, OBJ_F_CURRENT_AID) and discards the result; transcription faithful.
    obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
    rot = run_info->path.rotations[run_info->path.curr];

    if (!location_in_dir(loc, rot, &loc)) {
        return false;
    }

    object_move_to(obj, loc, offset_x - x_shifts[rot], offset_y - y_shifts[rot]);

    return true;
}

// 0x42F140
bool anim_advance_vault_step(AnimRunInfo* run_info)
{
    int64_t obj;
    int offset_x;
    int offset_y;
    tig_art_id_t art_id;
    int64_t loc;
    int rot;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11279, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);

    run_info->path.curr++;
    if (run_info->path.curr >= run_info->path.max) {
        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(obj, art_id);

        run_info->flags &= ~0x30;
        anim_set_obj_offset(obj, 0, 0);

        return false;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    rot = run_info->path.rotations[run_info->path.curr];
    if ((rot & 0x1) != 0) {
        run_info->extra_target_tile = 0;
    } else if (!location_in_dir(loc, rot, &(run_info->extra_target_tile))) {
        run_info->path.curr = run_info->path.max + 1;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_VAULT);
    art_id = tig_art_id_rotation_set(art_id, rot);
    anim_set_obj_offset(obj, -offset_x, -offset_y);

    if ((run_info->flags & 0x40) != 0) {
        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_RUN);
    } else {
        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_WALK);
    }

    art_id = tig_art_id_frame_set(art_id, 0);
    object_set_current_aid(obj, art_id);

    run_info->flags |= 0x10;

    return true;
}

// 0x42F2D0
bool AGbeginAnimMoveStraight(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11351, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) return false;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimMoveStraight: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->path.flags &= ~0x1;
    run_info->path.curr = 0;
    run_info->flags |= 0x10;

    return true;
}

// 0x42F390
bool AGupdateAnimMoveStraight(AnimRunInfo* run_info)
{
    int64_t obj;
    int v1;
    int idx;
    int offset_x;
    int offset_y;
    int64_t loc;
    tig_art_id_t art_id;
    int64_t loc_x;
    int64_t loc_y;
    int64_t new_loc;
    int64_t new_loc_x;
    int64_t new_loc_y;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11385, "obj != OBJ_HANDLE_NULL"

    v1 = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data;
    anim_callback_delay = 35;

    if (v1 == 0) {
        v1 = 4;
        anim_callback_delay = 35;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    for (idx = 0; idx < v1; idx++) {
        offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
        loc = obj_field_int64_get(obj, OBJ_F_LOCATION);

        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_inc(art_id);
        object_set_current_aid(obj, art_id);

        // NOTE: Verified at 0x42F459-0x42F483 - the original adds the
        // sign-extended int8 rotations the same way.
        offset_x += run_info->path.rotations[run_info->path.curr];
        offset_y += run_info->path.rotations[run_info->path.curr + 1];

        location_xy(loc, &loc_x, &loc_y);
        if (!location_at(loc_x + offset_x + 40, loc_y + offset_y + 20, &new_loc)) {
            ASSERT(0); // 11433, "0"
            exit(EXIT_FAILURE);
        }

        if (new_loc != loc) {
            location_xy(new_loc, &new_loc_x, &new_loc_y);
            offset_x += (int)(loc_x - new_loc_x);
            offset_y += (int)(loc_y - new_loc_y);
        }

        object_move_to(obj, new_loc, offset_x, offset_y);

        run_info->path.curr += 2;

        if (run_info->path.curr >= run_info->path.max) {
            run_info->flags &= ~0x10;
            return false;
        }
    }

    return true;
}

// 0x42F5C0
bool anim_begin_projectile_move(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11453, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) return false;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimMoveStraight: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    run_info->path.flags &= ~0x1;
    run_info->path.curr = 0;
    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data = 0;
    run_info->cur_stack_data->params[AGDATA_ORIGINAL_TILE].loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    run_info->flags |= 0x10;

    return true;
}

// 0x42F6A0
bool AGupdateAnimProjectileMoveStraight(AnimRunInfo* run_info)
{
    int64_t projectile_obj;
    int64_t target_obj;
    int idx;
    int cnt;
    int64_t parent_obj;
    int64_t target_loc;
    int64_t v1;
    int offset_x;
    int offset_y;
    int64_t projectile_loc;
    tig_art_id_t art_id;
    int64_t projectile_loc_x;
    int64_t projectile_loc_y;
    int64_t new_loc;
    int64_t new_loc_x;
    int64_t new_loc_y;

    ASSERT(run_info != NULL); // 11492, "pRunInfo != NULL"

    projectile_obj = run_info->params[0].obj;
    target_obj = run_info->params[1].obj;

    ASSERT(projectile_obj != OBJ_HANDLE_NULL); // 11496, "projObj != OBJ_HANDLE_NULL"

    cnt = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data;
    anim_callback_delay = 35;
    if (cnt == 0) {
        cnt = 4;
        anim_callback_delay = 35;
    }

    if (projectile_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    parent_obj = run_info->cur_stack_data->params[AGDATA_PARENT_OBJ].obj;
    target_loc = run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc;
    v1 = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;

    if ((run_info->path.flags & 0x01) != 0) {
        return true;
    }

    for (idx = 0; idx < cnt; idx++) {
        offset_x = obj_field_int32_get(projectile_obj, OBJ_F_OFFSET_X);
        offset_y = obj_field_int32_get(projectile_obj, OBJ_F_OFFSET_Y);
        projectile_loc = obj_field_int64_get(projectile_obj, OBJ_F_LOCATION);

        art_id = obj_field_int32_get(projectile_obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_inc(art_id);
        object_set_current_aid(projectile_obj, art_id);

        offset_x += run_info->path.rotations[run_info->path.curr];
        offset_y += run_info->path.rotations[run_info->path.curr + 1];

        location_xy(projectile_loc, &projectile_loc_x, &projectile_loc_y);

        new_loc_x = projectile_loc_x + offset_x + 40;
        new_loc_y = projectile_loc_y + offset_y + 20;

        if (!location_at(new_loc_x, new_loc_y, &new_loc)) {
            tig_debug_printf("Anim: AGupdateAnimProjectileMoveStraight: ERROR: location_at() failed: Loc: (%lld x %lld)!\n",
                (long long)new_loc_x,
                (long long)new_loc_y);
            ASSERT(0); // 11594, "0"
            return false;
        }

        if (new_loc != projectile_loc) {
            int range = (int)location_dist(run_info->cur_stack_data->params[AGDATA_ORIGINAL_TILE].loc,
                obj_field_int64_get(projectile_obj, OBJ_F_LOCATION));
            combat_projectile_arrive(parent_obj, target_obj, target_loc, projectile_obj, range, new_loc, v1);

            if ((run_info->flags & 0x02) != 0) {
                return false;
            }

            if ((run_info->flags & 0x01) == 0) {
                return false;
            }

            if ((obj_field_int32_get(projectile_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
                return false;
            }

            location_xy(new_loc, &new_loc_x, &new_loc_y);
            offset_x += (int)(projectile_loc_x - new_loc_x);
            offset_y += (int)(projectile_loc_y - new_loc_y);
        }

        object_move_to(projectile_obj, new_loc, offset_x, offset_y);

        run_info->path.curr += 2;

        if (run_info->path.curr >= run_info->path.max) {
            location_in_dir(new_loc, run_info->path.rotations[0], &new_loc);
            run_info->cur_stack_data->params[AGDATA_TARGET_TILE].loc = new_loc;
            run_info->path.curr = 0;
        }
    }

    return true;
}

// 0x42FA50
bool AGupdateAnimMoveStraightKnockback(AnimRunInfo* run_info)
{
    int64_t obj;
    int idx;
    int offset_x;
    int offset_y;
    int64_t loc;
    tig_art_id_t art_id;
    int64_t loc_x;
    int64_t loc_y;
    int64_t new_loc;
    int64_t new_loc_x;
    int64_t new_loc_y;
    CombatContext combat;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 11622, "obj != OBJ_HANDLE_NULL"

    anim_callback_delay = 35;

    if (obj == OBJ_HANDLE_NULL) {
        run_info->flags &= ~0x10;
        return false;
    }

    for (idx = 0; idx < 4; idx++) {
        offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
        loc = obj_field_int64_get(obj, OBJ_F_LOCATION);

        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        if (tig_art_id_anim_get(art_id) == TIG_ART_ANIM_STAND) {
            art_id = tig_art_id_frame_inc(art_id);
            object_set_current_aid(obj, art_id);
        }

        offset_x += run_info->path.rotations[run_info->path.curr];
        offset_y += run_info->path.rotations[run_info->path.curr + 1];

        location_xy(loc, &loc_x, &loc_y);
        if (!location_at(loc_x + offset_x + 40, loc_y + offset_y + 20, &new_loc)) {
            ASSERT(0); // 11691, "0"
            exit(EXIT_FAILURE);
        }

        if (new_loc != loc) {
            if (anim_knockback_path_blocked(run_info, obj, &(run_info->path), loc, new_loc)) {
                object_move_to(obj, loc, 0, 0);
                combat_context_init(OBJ_HANDLE_NULL, obj, &combat);
                combat.dam[DAMAGE_TYPE_NORMAL] = random_between(1, (run_info->path.max - run_info->path.curr) / 2);
                combat.dam[DAMAGE_TYPE_FATIGUE] = random_between(1, (run_info->path.max - run_info->path.curr) / 2);
                combat.field_30 = run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj;
                combat_dmg(&combat);

                if ((run_info->flags & 0x1) != 0) {
                    run_info->flags &= ~0x10;
                }

                return false;
            }

            location_xy(new_loc, &new_loc_x, &new_loc_y);
            offset_x += (int)(loc_x - new_loc_x);
            offset_y += (int)(loc_y - new_loc_y);
        }

        object_move_to(obj, new_loc, offset_x, offset_y);

        run_info->path.curr += 2;
        if (run_info->path.curr >= run_info->path.max) {
            run_info->flags &= ~0x10;
            return false;
        }
    }

    return true;
}

// 0x42FD70
// Whether a knockback move cannot enter the next tile: the tile is blocking,
// traversal is blocked, or a live critter already occupies the destination.
// Used by AGupdateAnimMoveStraightKnockback to end the knockback early.
bool anim_knockback_path_blocked(AnimRunInfo* run_info, int64_t obj, AnimPath* path, int64_t from, int64_t to)
{
    ObjectList objects;
    ObjectNode* node;

    if ((path->flags & 0x01) != 0) {
        if (!location_in_dir(from, path->baseRot, &to)) {
            return false;
        }

        if (path->curr > path->max - 2) {
            object_list_location(to, OBJ_TM_CRITTER, &objects);
            node = objects.head;
            while (node != NULL) {
                if (!critter_is_dead(node->obj)) {
                    break;
                }
                node = node->next;
            }
            object_list_destroy(&objects);

            if (node != NULL) {
                run_info->cur_stack_data->params[AGDATA_SCRATCH_OBJ].obj = node->obj;
                return true;
            }
        }

        // NOTE: Verified against 0x42FE20 - original calls obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) and discards the result; transcription faithful.
        obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);

        if (!tile_is_blocking(to, false)
            && !object_traversal_is_blocked(obj, from, path->baseRot, 0x03, NULL)) {
            return false;
        }
    }

    return true;
}

// 0x42FEA0
bool sub_42FEA0(AnimRunInfo* run_info)
{
    (void)run_info;

    return true;
}

// 0x42FEB0
bool sub_42FEB0(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x42FEC0
bool sub_42FEC0(AnimRunInfo* run_info)
{
    (void)run_info;

    return false;
}

// 0x42FED0
bool anim_add_death_blood(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t loc;

    obj = run_info->params[0].obj;
    if (obj == OBJ_HANDLE_NULL) {
        ASSERT(obj != OBJ_HANDLE_NULL); // obj != OBJ_HANDLE_NULL
        return false;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    if (!tile_is_blocking(loc, false)) {
        anim_create_blood_splat(obj);
    }

    return true;
}

// 0x42FF40
bool anim_finish_death(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;
    if (obj == OBJ_HANDLE_NULL) {
        ASSERT(obj != OBJ_HANDLE_NULL); // obj != OBJ_HANDLE_NULL
        return false;
    }

    if ((run_info->flags & 0x200) == 0 && critter_is_dead(obj)) {
        object_flags_set(obj, OF_FLAT | OF_NO_BLOCK);

        if (!tig_net_is_active()) {
            if (player_is_local_pc_obj(obj)) {
                ui_end_death();
            }
        }

        mt_item_notify_parent_dying(OBJ_HANDLE_NULL, obj);
    }

    return true;
}

// 0x42FFE0
bool anim_cleanup_dying(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int64_t location;

    obj = run_info->params[0].obj;
    ASSERT(obj != OBJ_HANDLE_NULL); // obj != OBJ_HANDLE_NULL

    if ((run_info->flags & 0x8000) != 0) {
        return false;
    }

    if (map_is_clearing_objects()) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        return false;
    }

    if (tig_art_id_frame_get(art_id) != art_anim_data.num_frames - 1) {
        art_id = tig_art_id_frame_set(art_id, art_anim_data.num_frames - 1);
        object_set_current_aid(obj, art_id);
    }

    location = obj_field_int64_get(obj, OBJ_F_LOCATION);
    if (tile_is_blocking(location, false)) {
        object_destroy(obj);
    }

    return true;
}

// 0x4300B0
void anim_pause(void)
{
    anim_paused = true;
}

// 0x4300C0
void anim_unpause(void)
{
    anim_paused = false;
}

// 0x4300D0
bool anim_fidget_timeevent_process(TimeEvent* timeevent)
{
    bool v1 = false;
    TigRect rect;
    TimeEvent next_timeevent;
    DateTime datetime;
    LocRect loc_rect;
    ObjectList objects;
    ObjectNode* node;
    int cnt;
    int skip;
    tig_art_id_t art_id;
    AnimGoalData goal_data;

    if (combat_turn_based_is_active()) {
        return true;
    }

    if (tig_net_is_active()) {
        return true;
    }

    if (anim_paused) {
        return true;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = 800;
    rect.height = 400;

    if (location_screen_rect_to_loc_rect(&rect, &loc_rect)) {
        datetime_from_milliseconds(&datetime, 4000);

        cnt = anim_collect_fidget_candidates(&loc_rect, &objects);
        if (cnt != 0) {
            if (cnt > 1) {
                if (object_list_remove(&objects, anim_fidget_obj)) {
                    cnt--;
                }
            }

            if (cnt > 1) {
                skip = random_between(0, cnt - 1);
            } else {
                skip = 0;
            }

            node = objects.head;
            while (skip > 0) {
                node = node->next;
                skip--;
            }

            ASSERT(node != NULL); // 12021, "pCurNode != NULL"

            anim_fidget_obj = node->obj;
            if (anim_fidget_obj != OBJ_HANDLE_NULL) {
                art_id = obj_field_int32_get(anim_fidget_obj, OBJ_F_CURRENT_AID);
                art_id = tig_art_id_frame_set(art_id, 0);
                if (magictech_is_under_influence_of(anim_fidget_obj, 172)) {
                    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STUNNED);
                    v1 = true;
                }

                object_set_current_aid(anim_fidget_obj, art_id);

                anim_goal_data_init(&goal_data, anim_fidget_obj, AG_ANIM_FIDGET);
                if (!anim_goal_add(&goal_data, NULL)) {
                    if (v1) {
                        art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
                        object_set_current_aid(anim_fidget_obj, art_id);
                    }
                }
            }
        }

        object_list_destroy(&objects);
    }

    next_timeevent.type = TIMEEVENT_TYPE_FIDGET_ANIM;
    return timeevent_add_delay(&next_timeevent, &datetime);
}

// 0x4302D0
// Fills `objects` with critters in `loc_rect` that can fidget right now and
// returns how many remain. Ineligible nodes are removed, restarting the scan
// so the returned list contains only candidates.
int anim_collect_fidget_candidates(LocRect* loc_rect, ObjectList* objects)
{
    ObjectNode* node;
    int cnt = 0;

    ASSERT(loc_rect != NULL); // 11919, "pLocRect != NULL"
    ASSERT(objects != NULL); // 11920, "pObjList != NULL"

    object_list_rect(loc_rect, OBJ_TM_PC | OBJ_TM_NPC, objects);
    node = objects->head;
    while (node != NULL) {
        if (anim_can_fidget(node->obj)) {
            cnt++;
            node = node->next;
            continue;
        }

        if (!object_list_remove(objects, node->obj)) {
            ASSERT(0); // 11939, "0"
            object_list_destroy(objects);
            return false;
        }

        node = objects->head;
        cnt = 0;
    }

    return cnt;
}

// 0x4303D0
// A critter can fidget when it is active, idle, out of combat mode, standing,
// and either a PC or an NPC not currently reacting to the local PC.
bool anim_can_fidget(int64_t obj)
{
    if (!anim_is_busy(obj, 0)
        && !combat_critter_is_combat_mode_active(obj)
        && critter_is_active(obj)) {
        if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC
            || !player_is_local_pc_obj(reaction_current_pc_get(obj))) {
            if (tig_art_id_anim_get(obj_field_int32_get(obj, OBJ_F_CURRENT_AID)) == TIG_ART_ANIM_STAND) {
                return true;
            }
        }
    }

    return false;
}

// 0x430460
// Arms the recurring fidget timeevent; called once when the game starts.
void anim_fidget_schedule(void)
{
    DateTime datetime;
    TimeEvent timeevent;

    datetime_from_milliseconds(&datetime, 4000);
    timeevent.type = TIMEEVENT_TYPE_FIDGET_ANIM;
    timeevent_add_delay(&timeevent, &datetime);
}

// 0x430490
void anim_set_obj_offset(int64_t obj, int offset_x, int offset_y)
{
    object_set_offset(obj, offset_x, offset_y + anim_float_offset_y);
}

// 0x4304C0
bool get_always_run(int64_t obj)
{
    int client_id;
    bool always_run;

    if (tig_net_is_active()) {
        client_id = multiplayer_find_slot_from_obj(obj);
        if (client_id != -1) {
            return (multiplayer_flags_get(client_id) & MULTIPLAYER_ALWAYS_RUN) != 0;
        }
        return false;
    }

    always_run = settings_get_value(&settings, ALWAYS_RUN_KEY);

    if (player_is_local_pc_obj(obj)) {
        if (always_run) {
            if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                return false;
            }
        } else {
            if (tig_kb_get_modifier(SDL_KMOD_CTRL)
                || tig_kb_get_modifier(SDL_KMOD_NUM)) {
                return true;
            }
        }
    }

    return always_run;
}

// 0x430580
void set_always_run(bool value)
{
    int client_id;

    if (tig_net_is_active()) {
        client_id = multiplayer_find_slot_from_obj(player_get_local_pc_obj());
        if (client_id != -1) {
            if (value) {
                multiplayer_flags_set(client_id, MULTIPLAYER_ALWAYS_RUN);
            } else {
                multiplayer_flags_unset(client_id, MULTIPLAYER_ALWAYS_RUN);
            }
        }
    }

    settings_set_value(&settings, ALWAYS_RUN_KEY, value);
}

// 0x4305D0
bool AGupdateAnimMove(AnimRunInfo* run_info)
{
    int64_t obj;
    bool v1;
    int64_t loc;
    tig_art_id_t art_id;
    int anim;
    int rot;
    unsigned int spell_flags;
    int64_t new_loc;
    int offset_x;
    int offset_y;
    AnimID anim_id;
    TigArtAnimData art_anim_data;
    int frame;
    int sound_id;
    int v2;
    int v3;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 12268, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    // NOTE: Verified against 0x430627 - original calls player_is_local_pc_obj(obj) and discards the result; transcription faithful.
    player_is_local_pc_obj(obj);

    v1 = (run_info->flags & 0x40) != 0;
    run_info->flags |= 0x100000;

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (tig_art_id_anim_get(art_id) == TIG_ART_ANIM_STAND) {
        return false;
    }

    rot = run_info->path.rotations[run_info->path.curr];
    run_info->flags |= 0x10;

    if (tig_net_is_active()
        && tig_net_is_host()) {
        if (run_info->path.field_E8 != 0
            && run_info->path.curr > 0) {
            int64_t distance;
            char str[40];
            int64_t x;
            int64_t y;

            distance = location_dist(run_info->path.field_E8, loc);
            object_examine(obj, obj, str);
            tig_debug_printf("AGUpdateAnimMove: ERROR %s (%lld tiles away @ %lld, %lld) are more than\n",
                str,
                (long long)distance,
                (long long)LOCATION_GET_X(loc),
                (long long)LOCATION_GET_Y(loc));
            tig_debug_printf("                  anim_path_data.curr(%d) tiles away from where you started (%lld,%lld)\n",
                run_info->path.curr,
                (long long)LOCATION_GET_X(run_info->path.field_E8),
                (long long)LOCATION_GET_Y(run_info->path.field_E8));

            anim_path_location_after_steps(run_info, run_info->path.curr, &x, &y);
            tig_debug_printf("                  interrupting your animation at %lld, %lld\n", (long long)x, (long long)y);

            art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
            if (critter_is_concealed(art_id)) {
                art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_CONCEAL_FIDGET);
            } else {
                art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
            }
            object_set_current_aid(obj, art_id);

            mp_object_move_to(obj, loc, 0, 0, false);
            anim_interrupt_all_goals_of_obj(obj, 2, false, false);

            return false;
        }
    }

    if (v1) {
        if (tig_art_id_anim_get(art_id) != TIG_ART_ANIM_RUN) {
            anim_move_art_id_get(run_info, obj, &art_id, true, NULL);
        }
    }

    spell_flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);
    if ((spell_flags & OSF_FLOATING) != 0) {
        if (!anim_find_first_of_type(obj, AG_FLOATING, &anim_id)) {
            ASSERT(0); // 12354, "0"
            exit(EXIT_FAILURE);
        }

        v2 = anim_run_info[anim_id.slot_num].cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
        anim_float_offset_y = 15 - 2 * v2;
    }

    new_loc = loc;
    object_inc_current_aid(obj);

    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);

    if (!critter_is_monstrous(obj)) {
        anim = tig_art_id_anim_get(art_id);
        if (anim == TIG_ART_ANIM_WALK || anim == TIG_ART_ANIM_RUN) {
            if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
                frame = tig_art_id_frame_get(art_id);
                if (frame == 3 || frame == 8) {
                    sound_id = sfx_critter_sound(obj, CRITTER_SOUND_FOOTSTEPS);
                    gsound_play_sfx_on_obj(sound_id, 1, obj);
                }
            }
        }
    }

    if ((run_info->path.flags & 0x02) != 0) {
        if (anim_move_tile_reached(rot, offset_x, offset_y)) {
            if (run_info->extra_target_tile == 0 || run_info->extra_target_tile == new_loc) {
                run_info->path.flags &= ~0x02;
                run_info->path.curr++;

                v3 = false;

                rot = run_info->path.rotations[run_info->path.curr];
                if ((rot & 1) != 0) {
                    run_info->extra_target_tile = 0;
                } else if (!location_in_dir(loc, rot, &(run_info->extra_target_tile))) {
                    run_info->path.curr = run_info->path.max + 1;
                }

                if ((spell_flags & OSF_FLOATING) != 0) {
                    offset_y = 15 - 2 * v2;
                } else {
                    offset_y = 0;
                }

                if ((spell_flags & (OSF_FLOATING | OSF_BODY_OF_AIR)) == 0) {
                    ObjectList traps;

                    object_list_location(new_loc, OBJ_TM_TRAP, &traps);
                    if (traps.head != NULL) {
                        object_script_execute(obj, traps.head->obj, traps.head->obj, 1, 0);
                        run_info->path.flags |= 0x08;
                        run_info->path.curr = run_info->path.max;
                    }
                    object_list_destroy(&traps);
                }

                if ((!tig_net_is_active()
                        || tig_net_is_host())
                    && (spell_flags & (OSF_FLOATING | OSF_BODY_OF_AIR)) == 0
                    && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NO_SLIP) == 0) {
                    if (tile_is_slippery(new_loc)) {
                        int dexterity = stat_level_get(obj, STAT_DEXTERITY);
                        if (dexterity < 20 && random_between(1, 20) > dexterity + 6) {
                            v3 = true;
                            run_info->path.curr = run_info->path.max;
                        }
                    }
                }

                if (run_info->path.curr >= run_info->path.max) {
                    anim = tig_art_id_anim_get(run_info->cur_stack_data->params[AGDATA_ANIM_ID_PREVIOUS].data);
                    art_id = tig_art_id_anim_set(art_id, anim);
                    art_id = tig_art_id_frame_set(art_id, 0);
                    object_set_current_aid(obj, art_id);
                    anim_set_obj_offset(obj, 0, 0);

                    run_info->flags &= ~0x30;

                    anim_float_offset_y = 0;

                    if (v3) {
                        anim_goal_knockdown(obj);
                    }

                    return false;
                }

                rot = run_info->path.rotations[run_info->path.curr];
                if (rot != run_info->path.rotations[run_info->path.curr - 1]) {
                    anim_set_obj_offset(obj, 0, 0);
                    art_id = tig_art_id_rotation_set(art_id, rot);
                    object_set_current_aid(obj, art_id);
                }

                run_info->flags &= ~0x10;

                v3 = 2;
                if (run_info->current_goal > 0
                    && (run_info->flags & 0x40) != 0
                    && critter_fatigue_current(obj) > 0) {
                    v3 = 1;
                }

                if (!combat_check_action_points(obj, v3)) {
                    anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
                    return false;
                }
            } else {
                run_info->path.flags &= ~0x02;
            }
        }
    } else {
        if (location_normalize(&new_loc, &offset_x, &offset_y)) {
            if ((spell_flags & OSF_FLOATING) != 0) {
                offset_y += 2 * v2 - 15;
            }

            object_move_to(obj, new_loc, offset_x, offset_y);
            run_info->path.flags |= 0x02;

            if (tig_net_is_active()
                && tig_net_is_host()) {
                // TODO: Incomplete.
            }

            if ((run_info->flags & 0x80000) != 0
                && player_is_local_pc_obj(obj)
                && new_loc != loc) {
                int64_t x;
                int64_t y;

                location_calc_dist_from_screen_center(new_loc, &x, &y);

                // NOTE: Original code compares by casting to double.
                if (llabs(x) > 360) {
                    location_origin_set(new_loc);
                }

                // NOTE: Original code compares by casting to double.
                if (llabs(y) > 180) {
                    location_origin_set(new_loc);
                }
            }

            if (!anim_spend_move_cost(run_info)) {
                return false;
            }
        }
    }

    anim_float_offset_y = 0;

    if ((run_info->flags & 0x80) != 0) {
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
        } else {
            tig_debug_printf("Anim: AGupdateAnimMove: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
            run_info->pause_time.milliseconds = 100;
        }

        if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
            anim_move_pause_time_get(obj, &(run_info->pause_time));
        }

        if ((spell_flags & OSF_SHRUNK) != 0) {
            run_info->pause_time.milliseconds *= 2;
        }

        run_info->flags &= ~0x80;
    }

    return true;
}

// 0x430F20
bool anim_cleanup_move_path(AnimRunInfo* run_info)
{
    int64_t obj;
    tig_art_id_t art_id;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 12721, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0
        || map_is_clearing_objects()
        || obj == OBJ_HANDLE_NULL) {
        return false;
    }

    // NOTE: Verified against 0x430F75-0x430F85 - the original computes the stand art id, and discards the tig_art_id_frame_set result at 0x430F85; transcription faithful.
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_STAND);
    tig_art_id_frame_set(art_id, 0);

    if (run_info->path_attached_to_stack_index == run_info->current_goal
        || run_info->path_attached_to_stack_index == run_info->current_goal - 1) {
        run_info->path.flags |= 0x01;
    }

    return true;
}

// 0x430FC0
int anim_spend_move_cost(AnimRunInfo* run_info)
{
    int64_t obj;
    int action_points;
    tig_art_id_t art_id;
    int v1;
    unsigned int critter_flags;
    unsigned int critter_flags2;
    int fatigue_dam;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 12753, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    action_points = 2;

    if (run_info->current_goal > 0 && (run_info->flags & 0x40) != 0) {
        if (critter_fatigue_current(obj) > 0) {
            if (combat_critter_is_combat_mode_active(obj)) {
                v1 = 5;

                run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data++;

                critter_flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
                critter_flags2 = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);

                if ((critter_flags & OCF_FATIGUE_LIMITING) != 0) {
                    v1 *= 4;
                }

                if ((critter_flags2 & OCF2_FATIGUE_DRAINING) != 0) {
                    v1 /= 4;
                    if (v1 == 0) {
                        v1 = 1;
                    }
                }

                if (run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data >= v1) {
                    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data = 0;

                    // FIX (porting bug #30): the original stores damage + 1
                    // (0x43010BA get, 0x43010BF INC EAX, 0x43010C3 set),
                    // so each combat running interval drains one fatigue.
                    fatigue_dam = critter_fatigue_damage_get(obj);

                    if (critter_fatigue_damage_set(obj, fatigue_dam + 1) == 0) {
                        return false;
                    }
                }
            }

            action_points = 1;
        } else {
            art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
            art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_WALK);
            object_set_current_aid(obj, art_id);

            run_info->flags &= ~0x40;
        }
    }

    // CE (criticals, milestone CR-2): a Hobbled critter pays double for each
    // step in turn-based.
    action_points *= crit_effects_move_multiplier(obj);

    if (!combat_consume_action_points(obj, action_points)) {
        anim_float_offset_y = 0;
        anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
        return false;
    }

    return true;
}

// 0x431130
bool anim_float_not_started(AnimRunInfo* run_info)
{
    return (run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x20) == 0;
}

// 0x431150
bool anim_float_bob(AnimRunInfo* run_info)
{
    int64_t obj;
    int v1;
    int dy;
    int offset_x;
    int offset_y;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 12839, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    v1 = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    if (v1 > 0) {
        dy = 2;
        v1++;
        if (v1 > 5) {
            v1 = -1;
        }
    } else {
        dy = -2;
        v1--;
        if (v1 < -5) {
            v1 = 1;
        }
    }

    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = v1;
    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
    object_set_offset(obj, offset_x, offset_y + dy);

    return true;
}

// 0x4311F0
bool anim_float_begin(AnimRunInfo* run_info)
{
    int64_t obj;
    int offset_x;
    int offset_y;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 12872, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) return false;

    // FIX: Was `|= ~0x20`, which sets every bit *except* 0x20 - the one bit it
    // means to set. 0x4311F0 in the exe is a plain `or 0x20`.
    //
    // Two things went wrong with the original spelling. Sub-node 1 routes on
    // `(FLAGS_DATA & 0x20) == 0`, so with the bit never set the goal returned
    // here every tick and lifted the object another 15 pixels instead of
    // handing over to the bob in sub-node 2. And the stray bits included 0x80,
    // which `anim_goal_is_endless` reads, so the run also stopped counting
    // toward animNumActiveGoals.
    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data |= 0x20;
    run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = 1;
    run_info->pause_time.milliseconds = 100;
    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
    object_set_offset(obj, offset_x, offset_y - 15);

    return true;
}

// 0x431290
bool anim_float_end(AnimRunInfo* run_info)
{
    int64_t obj;
    int offset_x;
    int offset_y;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 12885, obj != OBJ_HANDLE_NULL
    if (obj == OBJ_HANDLE_NULL) return false;

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data &= ~0x20;
    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
    object_set_offset(obj, offset_x, 15 - 2 * run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data + offset_y);

    return true;
}

/**
 * The goal's eye-candy overlays have not been applied yet
 * (`AGDATA_FLAGS_DATA` bit 0x40, raised when they go on and cleared by
 * `anim_cleanup_eye_candy`).
 */
// 0x431320
bool anim_eye_candy_not_applied(AnimRunInfo* run_info)
{
    return (run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x40) == 0;
}

// 0x431340
bool AGupdateAnimEyeCandy(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 13035, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    // CE (real-time session B): a cast whose caster is still recovering holds
    // before its casting effect advances. Only for the spell-attempt goals;
    // every other eye-candy goal passes straight through.
    if (rt_budget_hold(run_info)) {
        return true;
    }

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, overlay_fore);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandy: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return true;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame == art_anim_data.num_frames - 1) {
                    return false;
                }

                object_eye_candy_aid_inc(obj, OBJ_F_OVERLAY_FORE, overlay_fore);

                if (frame == art_anim_data.action_frame - 1) {
                    run_info->flags |= 0x04;
                }
            }

            if (overlay_back != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_BACK, overlay_back);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandy: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return true;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame == art_anim_data.num_frames - 1) {
                    return false;
                }

                object_eye_candy_aid_inc(obj, OBJ_F_OVERLAY_BACK, overlay_back);
            }
        } else {
            art_id = obj_arrayfield_uint32_get(obj, OBJ_F_UNDERLAY, overlay_fore);
            if (art_id == TIG_ART_ID_INVALID) {
                tig_debug_printf("Anim: AGupdateAnimEyeCandy: Error: No Art!\n");
                return false;
            }

            if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                return true;
            }

            frame = tig_art_id_frame_get(art_id);
            if (frame == art_anim_data.num_frames - 1) {
                return false;
            }

            object_eye_candy_aid_inc(obj, OBJ_F_UNDERLAY, overlay_fore);

            if (frame == art_anim_data.action_frame - 1) {
                run_info->flags |= 0x04;
            }
        }
    } else {
        if (overlay_light != -1) {
            return false;
        }
    }

    if (overlay_light != -1) {
        object_overlay_light_frame_inc(obj, overlay_light);
    }

    anim_eye_candy_sound_restart(run_info, obj);

    return true;
}

// 0x431550
// Restarts the eye-candy animation sound when there is no live handle and the
// previous animation id is still valid. The four AGupdateAnimEyeCandy* variants
// all funnel here.
void anim_eye_candy_sound_restart(AnimRunInfo* run_info, int64_t obj)
{
    AnimGoalData* goal_data;
    tig_sound_handle_t sound_handle;

    goal_data = run_info->cur_stack_data;
    // FIX (porting bug #33): two defects on one condition. The handle test was
    // inverted - the original at 0x43155B is `cmp [eax+0xA8], -1` / `jne`, so
    // it proceeds only when there is *no* handle yet, which is what makes this
    // the function that restarts a stopped loop. And the mask was 0x80000000,
    // which nothing ever sets; the original tests `test BYTE PTR [eax+0x70],
    // 0x80` at 0x43156C (bytes `f6 40 70 80` - a *byte* test, so bit 7 of the
    // low byte, not bit 31). See the note on anim_eye_candy_sound_begin.
    if (goal_data->params[AGDATA_SOUND_HANDLE].data == TIG_SOUND_HANDLE_INVALID
        && goal_data->params[AGDATA_ANIM_ID_PREVIOUS].data != -1
        && (goal_data->params[AGDATA_FLAGS_DATA].data & 0x80) != 0) {
        sound_handle = gsound_play_sfx_on_obj(goal_data->params[AGDATA_ANIM_ID_PREVIOUS].data, 0, obj);
        if (sound_handle != TIG_SOUND_HANDLE_INVALID) {
            goal_data->params[AGDATA_SOUND_HANDLE].data = sound_handle;
        } else {
            goal_data->params[AGDATA_ANIM_ID_PREVIOUS].data = -1;
        }
    }
}

// 0x4315B0
bool AGbeginAnimEyeCandy(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;
    tig_art_id_t art_id;
    int range;
    tig_art_id_t light_art_id;
    TigArtAnimData art_anim_data;
    int frame = 0;

    // CE (real-time session E): a new cast, counted before the step that holds it.
    rt_budget_cast_begun(run_info);

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 13155, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data |= 0x40;
    run_info->pause_time.milliseconds = 100;

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;
    light_art_id = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data;
    art_id = run_info->cur_stack_data->params[AGDATA_ANIM_ID].data;
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;

    if (overlay_fore == -1
        && overlay_back == -1
        && overlay_light == -1) {
        return true;
    }

    if ((run_info->flags & 0x2000) == 0) {
        AnimFxList* animfx_list;
        AnimFxNode node;

        animfx_list = animfx_list_get(run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data);
        animfx_node_init(animfx_list, &node, obj, -1, run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data);
        node.art_id_ptr = &art_id;
        if (!animfx_overlay_slots_find(&node)) {
            return false;
        }

        overlay_fore = node.overlay_fore_index;
        overlay_back = node.overlay_back_index;
        overlay_light = node.overlay_light_index;

        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = overlay_fore;
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data = overlay_back;
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data = overlay_light;

        run_info->flags |= 0x2000;
    }

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        tig_debug_printf("Anim: AGbeginAnimEyeCandy: ERROR: aid %d failed to load!\n", art_id);
        ASSERT(0); // 13208, "0"
        return false;
    }

    run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;

    if ((run_info->flags & 0x800) != 0) {
        frame = random_between(0, art_anim_data.num_frames - 1);
    }

    if (overlay_back != -5) {
        if (overlay_fore == -1) {
            return false;
        }

        if (art_id != TIG_ART_ID_INVALID) {
            art_id = tig_art_eye_candy_id_type_set(art_id, 0);
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, art_id);
        }

        art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, overlay_fore);
        if (art_id != TIG_ART_ID_INVALID
            && tig_art_id_frame_get(art_id) != 0) {
            art_id = tig_art_id_frame_set(art_id, frame);
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, art_id);
        }

        if (overlay_back != -1) {
            art_id = tig_art_eye_candy_id_type_set(art_id, 1);
            object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, art_id);

            art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_BACK, overlay_back);
            if (art_id != TIG_ART_ID_INVALID
                && tig_art_id_frame_get(art_id) != 0) {
                art_id = tig_art_id_frame_set(art_id, frame);
                object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, art_id);
            }
        }
    } else {
        if (overlay_fore == -1) {
            return false;
        }

        art_id = tig_art_eye_candy_id_type_set(art_id, 2);
        object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, art_id);

        art_id = obj_arrayfield_uint32_get(obj, OBJ_F_UNDERLAY, overlay_fore);
        if (tig_art_id_frame_get(art_id) != 0) {
            art_id = tig_art_id_frame_set(art_id, frame);
            object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, art_id);
        }
    }

    if (overlay_light != -1) {
        ASSERT(light_art_id != 0); // 13270, "lightAid != 0"

        if (light_art_id != TIG_ART_ID_INVALID) {
            object_set_overlay_light(obj, overlay_light, 0x20, -1, range);
            object_set_overlay_light(obj, overlay_light, 0x20, light_art_id, range);
            object_overlay_light_frame_set_first(obj, overlay_light);
        }
    }

    anim_eye_candy_sound_begin(run_info, obj);

    return true;
}

// Porting bug #33: the whole sound side of eye candy was dead.
//
// These three functions are it. anim_eye_candy_sound_begin runs at the end of
// every AGbeginAnimEyeCandy*, anim_eye_candy_sound_restart at the end of every
// AGupdateAnimEyeCandy*, and anim_eye_candy_sound_move keeps a playing sound on
// top of its object. All three gated on `AGDATA_FLAGS_DATA & 0x80000000`, and
// **nothing sets bit 31**: the only writers of that field are anim_private.c
// (0) and animfx.c (0x80 from the eye-candy .mes `anim_forever` flag, plus
// 0x800), so the field can only ever hold 0, 0x80, 0x800 or 0x880. CE tests the
// bit correctly everywhere else in this file (AGbeginAnimEyeCandy,
// AGendAnimEyeCandy).
//
// Two slots carry the state and the original's own offsets identify them:
// AGDATA_ANIM_ID_PREVIOUS (+0x50) holds the *sound id* from the .mes, and
// AGDATA_SOUND_HANDLE (+0xA8) the handle once it is playing. This function
// wrote the handle into the id slot and cleared the handle slot on failure -
// the two swapped - so even with the flag fixed there would be no handle left
// to move or stop.
//
// What a player heard: with the loop bit dead, `loops` stayed 1, so the sound
// played **once** where the effect began, did not follow the target, and was
// never restarted. Six shipped effects have both `anim_forever` and a `Sound:`,
// and they are exactly the long-running ones - Vitality of Air, Strength of
// Earth, Agility of Fire, Purity of Water, Tempus Fugit, and the secondary
// destination of Tech: Explode (Fire).
//
// (gsound_play_sfx_on_obj's `loops` reaches AIL_quick_play, where 0 means
// forever and 1 means once - so the flag turning `loops` to 0 is what makes the
// sound loop.)
//
// 0x431960
// Starts the eye-candy sound when the animation begins: looping when the
// anim_forever bit is set, otherwise a one-shot. An already-live sound is moved
// instead (anim_eye_candy_sound_move).
void anim_eye_candy_sound_begin(AnimRunInfo* run_info, int64_t obj)
{
    AnimGoalData* goal_data;
    tig_sound_handle_t sound_handle;
    int loops = 1;

    goal_data = run_info->cur_stack_data;
    if (goal_data->params[AGDATA_SOUND_HANDLE].data == TIG_SOUND_HANDLE_INVALID) {
        if (goal_data->params[AGDATA_ANIM_ID_PREVIOUS].data != -1) {
            // FIX (porting bug #33): was 0x80000000. The original tests
            // `test BYTE PTR [eax+0x70], 0x80` at 0x431982 (`f6 40 70 80`).
            if ((goal_data->params[AGDATA_FLAGS_DATA].data & 0x80) != 0) {
                loops = 0;
            }

            sound_handle = gsound_play_sfx_on_obj(goal_data->params[AGDATA_ANIM_ID_PREVIOUS].data, loops, obj);
            if (sound_handle != TIG_SOUND_HANDLE_INVALID) {
                if (loops != 0) {
                    run_info->flags |= 0x20000;
                } else {
                    // FIX (porting bug #33): the two result slots were swapped.
                    // The original stores the handle to +0xA8 at 0x4319AF
                    // (`89 81 a8 00 00 00` = `mov [ecx+0xA8], eax`) - that is
                    // AGDATA_SOUND_HANDLE - and it has to, or nothing can ever
                    // stop, move or notice this sound again.
                    goal_data->params[AGDATA_SOUND_HANDLE].data = sound_handle;
                }
            } else {
                // FIX (porting bug #33): the other half of that swap. The
                // original clears the *sound id* at 0x4319CC
                // (`c7 42 50 ff ff ff ff` = `mov [edx+0x50], -1`), which is
                // what stops it retrying a sound that would not start.
                goal_data->params[AGDATA_ANIM_ID_PREVIOUS].data = -1;
            }
        }
    } else {
        anim_eye_candy_sound_move(run_info, obj);
    }
}

// 0x4319F0
// Keeps a looping eye-candy sound glued to the object as it moves.
void anim_eye_candy_sound_move(AnimRunInfo* run_info, int64_t obj)
{
    AnimGoalData* goal_data;

    goal_data = run_info->cur_stack_data;
    // FIX (porting bug #33): the same mask. `test BYTE PTR [eax+0x70], 0x80` at
    // 0x431A0F (`f6 40 70 80`).
    if (goal_data->params[AGDATA_SOUND_HANDLE].data != TIG_SOUND_HANDLE_INVALID
        && (run_info->flags & 0x20000) == 0
        && (goal_data->params[AGDATA_FLAGS_DATA].data & 0x80) != 0) {
        gsound_move(goal_data->params[AGDATA_SOUND_HANDLE].data, obj_field_int64_get(obj, OBJ_F_LOCATION));
    }
}

// 0x431A40
bool AGendAnimEyeCandy(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;

    obj = run_info->params[0].obj;
    ASSERT(obj != OBJ_HANDLE_NULL); // 13298, "obj != OBJ_HANDLE_NULL"
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data &= ~0x40;
    if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x80) != 0) {
        return false;
    }

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, TIG_ART_ID_INVALID);
            }
            if (overlay_back != -1) {
                object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, TIG_ART_ID_INVALID);
            }
        } else {
            object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, TIG_ART_ID_INVALID);
        }
        if (overlay_light == -1) {
            return true;
        }
    } else {
        if (overlay_light == -1) {
            return false;
        }
    }

    object_set_overlay_light(obj, overlay_light, 0, TIG_ART_ID_INVALID, 0);

    return true;
}

/**
 * The cleanup callback the eye-candy goals share: take the overlays, underlay
 * and overlay light back off the object and release the sound the effect was
 * holding.
 */
// 0x431B20
bool anim_cleanup_eye_candy(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 13355, "obj != OBJ_HANDLE_NULL"

    if ((run_info->flags & 0x8000) != 0
        || map_is_clearing_objects()
        || obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data &= ~0x40;

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, TIG_ART_ID_INVALID);
            }
            if (overlay_back != -1) {
                object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, TIG_ART_ID_INVALID);
            }
        } else {
            object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, TIG_ART_ID_INVALID);
        }
    } else {
        if (overlay_light == -1) {
            return false;
        }
    }

    if (overlay_light != -1) {
        object_set_overlay_light(obj, overlay_light, 0, TIG_ART_ID_INVALID, 0);
    }

    if (run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data != TIG_SOUND_HANDLE_INVALID) {
        if ((run_info->flags & 0x20000) == 0) {
            tig_sound_destroy(run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data);
        }
        run_info->cur_stack_data->params[AGDATA_SOUND_HANDLE].data = TIG_SOUND_HANDLE_INVALID;
    }

    return true;
}

// 0x431C40
bool AGupdateAnimEyeCandyReverse(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 13427, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, overlay_fore);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandyReverse: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return false;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame == 0) {
                    return false;
                }

                object_eye_candy_aid_dec(obj, OBJ_F_OVERLAY_FORE, overlay_fore);

                if (frame == art_anim_data.action_frame - 1) {
                    run_info->flags |= 0x04;
                }
            }

            if (overlay_back != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_BACK, overlay_back);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandyReverse: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return false;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame == 0) {
                    return false;
                }

                object_eye_candy_aid_dec(obj, OBJ_F_OVERLAY_BACK, overlay_back);
            }
        } else {
            art_id = obj_arrayfield_uint32_get(obj, OBJ_F_UNDERLAY, overlay_fore);
            if (art_id == TIG_ART_ID_INVALID) {
                tig_debug_printf("Anim: AGupdateAnimEyeCandyReverse: Error: No Art!\n");
                return false;
            }

            if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                return false;
            }

            frame = tig_art_id_frame_get(art_id);
            if (frame == 0) {
                return false;
            }

            object_eye_candy_aid_dec(obj, OBJ_F_UNDERLAY, overlay_fore);

            if (frame == art_anim_data.action_frame - 1) {
                run_info->flags |= 0x04;
            }
        }
    } else {
        if (overlay_light == -1) {
            return false;
        }
    }

    if (overlay_light != -1) {
        object_overlay_light_frame_dec(obj, overlay_light);
    }

    anim_eye_candy_sound_restart(run_info, obj);

    return true;
}

// 0x431E50
bool AGbeginAnimEyeCandyReverse(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;
    tig_art_id_t art_id;
    int range;
    tig_art_id_t light_art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data |= 0x40;
    run_info->pause_time.milliseconds = 100;

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;
    light_art_id = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL5].data;
    art_id = run_info->cur_stack_data->params[AGDATA_ANIM_ID].data;
    range = run_info->cur_stack_data->params[AGDATA_RANGE_DATA].data;

    if (overlay_fore == -1
        && overlay_back == -1
        && overlay_light == -1) {
        return true;
    }

    if ((run_info->flags & 0x2000) == 0) {
        AnimFxList* animfx_list;
        AnimFxNode node;

        animfx_list = animfx_list_get(run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data);
        animfx_node_init(animfx_list, &node, obj, -1, run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL4].data);
        node.art_id_ptr = &art_id;
        if (!animfx_overlay_slots_find(&node)) {
            return false;
        }

        overlay_fore = node.overlay_fore_index;
        overlay_back = node.overlay_back_index;
        overlay_light = node.overlay_light_index;

        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data = overlay_fore;
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data = overlay_back;
        run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data = overlay_light;

        run_info->flags |= 0x2000;
    }

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        tig_debug_printf("Anim: AGbeginAnimEyeCandyReverse: ERROR: aid %d failed to load!\n", art_id);
        ASSERT(0); // 13586, "0"
        return false;
    }

    run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;

    if ((run_info->flags & 0x800) != 0) {
        frame = random_between(1, art_anim_data.num_frames - 1);
    } else {
        frame = art_anim_data.num_frames - 1;
    }

    if (overlay_back != -5) {
        if (overlay_fore == -1) {
            return false;
        }

        if (art_id != TIG_ART_ID_INVALID) {
            art_id = tig_art_eye_candy_id_type_set(art_id, 0);
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, art_id);
        }

        art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, overlay_fore);
        if (art_id != TIG_ART_ID_INVALID
            && tig_art_id_frame_get(art_id) != frame) {
            art_id = tig_art_id_frame_set(art_id, frame);
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, art_id);
        }

        if (overlay_back != -1) {
            art_id = tig_art_eye_candy_id_type_set(art_id, 1);
            object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, art_id);

            art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_BACK, overlay_back);
            if (art_id != TIG_ART_ID_INVALID
                && tig_art_id_frame_get(art_id) != frame) {
                art_id = tig_art_id_frame_set(art_id, frame);
                object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, art_id);
            }
        }
    } else {
        if (overlay_fore == -1) {
            return false;
        }

        art_id = tig_art_eye_candy_id_type_set(art_id, 2);
        object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, art_id);

        art_id = obj_arrayfield_uint32_get(obj, OBJ_F_UNDERLAY, overlay_fore);
        if (tig_art_id_frame_get(art_id) != frame) {
            art_id = tig_art_id_frame_set(art_id, frame);
            object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, art_id);
        }
    }

    if (overlay_light != -1) {
        ASSERT(light_art_id != 0); // 13649, "lightAid != 0"

        if (light_art_id != TIG_ART_ID_INVALID) {
            object_set_overlay_light(obj, overlay_light, 0x20, light_art_id, range);
            object_overlay_light_frame_set_last(obj, overlay_light);
        }
    }

    anim_eye_candy_sound_begin(run_info, obj);

    return true;
}

// NOTE: Binary identical to 0x431A40.
//
// 0x4321C0
bool AGendAnimEyeCandyReverse(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;

    obj = run_info->params[0].obj;
    ASSERT(obj != OBJ_HANDLE_NULL); // 13671, "obj != OBJ_HANDLE_NULL"
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data &= ~0x40;
    if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x80) != 0) {
        return false;
    }

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                object_overlay_set(obj, OBJ_F_OVERLAY_FORE, overlay_fore, TIG_ART_ID_INVALID);
            }
            if (overlay_back != -1) {
                object_overlay_set(obj, OBJ_F_OVERLAY_BACK, overlay_back, TIG_ART_ID_INVALID);
            }
        } else {
            object_overlay_set(obj, OBJ_F_UNDERLAY, overlay_fore, TIG_ART_ID_INVALID);
        }
        if (overlay_light == -1) {
            return true;
        }
    } else {
        if (overlay_light == -1) {
            return false;
        }
    }

    object_set_overlay_light(obj, overlay_light, 0, TIG_ART_ID_INVALID, 0);

    return true;
}

// 0x4322A0
bool AGupdateAnimEyeCandyFireDmg(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 13736, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, overlay_fore);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandyFireDmg: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return true;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame % 3 == 1) {
                    AGapplyFireDmg(run_info);
                }

                if (frame == art_anim_data.num_frames - 1) {
                    return false;
                }

                object_eye_candy_aid_inc(obj, OBJ_F_OVERLAY_FORE, overlay_fore);

                if (frame == art_anim_data.action_frame - 1) {
                    run_info->flags |= 0x04;
                }
            }

            if (overlay_back != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_BACK, overlay_back);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandyFireDmg: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return true;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame == art_anim_data.num_frames - 1) {
                    return false;
                }

                object_eye_candy_aid_inc(obj, OBJ_F_OVERLAY_BACK, overlay_back);
            }
        } else {
            art_id = obj_arrayfield_uint32_get(obj, OBJ_F_UNDERLAY, overlay_fore);
            if (art_id == TIG_ART_ID_INVALID) {
                tig_debug_printf("Anim: AGupdateAnimEyeCandyFireDmg: Error: No Art!\n");
                return false;
            }

            if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                return true;
            }

            frame = tig_art_id_frame_get(art_id);
            if (frame == art_anim_data.num_frames - 1) {
                return false;
            }

            object_eye_candy_aid_inc(obj, OBJ_F_UNDERLAY, overlay_fore);

            if (frame == art_anim_data.action_frame - 1) {
                run_info->flags |= 0x04;
            }
        }
    } else {
        if (overlay_light != -1) {
            return false;
        }
    }

    if (overlay_light != -1) {
        object_overlay_light_frame_inc(obj, overlay_light);
    }

    anim_eye_candy_sound_restart(run_info, obj);

    return true;
}

// 0x4324D0
bool AGupdateAnimEyeCandyReverseFireDmg(AnimRunInfo* run_info)
{
    int64_t obj;
    int overlay_fore;
    int overlay_back;
    int overlay_light;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 13860, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    overlay_fore = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL1].data;
    overlay_back = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL2].data;
    overlay_light = run_info->cur_stack_data->params[AGDATA_SCRATCH_VAL3].data;

    if (overlay_fore != -1 || overlay_back != -1) {
        if (overlay_back != -5) {
            if (overlay_fore != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, overlay_fore);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandyReverseFireDmg: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return true;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame % 3 == 1) {
                    AGapplyFireDmg(run_info);
                }

                if (frame == 0) {
                    return false;
                }

                object_eye_candy_aid_dec(obj, OBJ_F_OVERLAY_FORE, overlay_fore);

                if (frame == art_anim_data.action_frame - 1) {
                    run_info->flags |= 0x04;
                }
            }

            if (overlay_back != -1) {
                art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_BACK, overlay_back);
                if (art_id == TIG_ART_ID_INVALID) {
                    tig_debug_printf("Anim: AGupdateAnimEyeCandyReverseFireDmg: Error: No Art!\n");
                    return false;
                }

                if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                    return true;
                }

                frame = tig_art_id_frame_get(art_id);
                if (frame == 0) {
                    return false;
                }

                object_eye_candy_aid_dec(obj, OBJ_F_OVERLAY_BACK, overlay_back);
            }
        } else {
            art_id = obj_arrayfield_uint32_get(obj, OBJ_F_UNDERLAY, overlay_fore);
            if (art_id == TIG_ART_ID_INVALID) {
                tig_debug_printf("Anim: AGupdateAnimEyeCandyReverseFireDmg: Error: No Art!\n");
                return false;
            }

            if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
                return true;
            }

            frame = tig_art_id_frame_get(art_id);
            if (frame == 0) {
                return false;
            }

            object_eye_candy_aid_dec(obj, OBJ_F_UNDERLAY, overlay_fore);

            if (frame == art_anim_data.action_frame - 1) {
                run_info->flags |= 0x04;
            }
        }
    } else {
        if (overlay_light != -1) {
            return false;
        }
    }

    if (overlay_light != -1) {
        object_overlay_light_frame_dec(obj, overlay_light);
    }

    anim_eye_candy_sound_restart(run_info, obj);

    return true;
}

// 0x432700
bool AGbeginAnimAttack(AnimRunInfo* run_info)
{
    int64_t source_obj;
    int64_t target_obj;
    int source_obj_type;
    int action_points;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int64_t weapon_obj;
    int delay;
    int sound_id;

    source_obj = run_info->params[0].obj;
    target_obj = run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj;

    ASSERT(source_obj != OBJ_HANDLE_NULL); // 13985, "sourceObj != OBJ_HANDLE_NULL"
    ASSERT(target_obj != OBJ_HANDLE_NULL); // 13986, "targetObj != OBJ_HANDLE_NULL"

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    source_obj_type = obj_field_int32_get(source_obj, OBJ_F_TYPE);

    if (obj_type_is_critter(source_obj_type)
        && (obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    // CE (reload prototype session 2): a gun under `firearm reloading` with
    // nothing loaded is refused here, before its AP is charged (zsogg,
    // 2026-09-25) - stock charged the attack and only found the gun empty at
    // the shot. Ended as a failed AP charge ends it, so an automatic follow-up
    // attack in real time cannot retry it.
    //
    // And a gun that cannot fire for want of ammunition (zsogg, 2026-10-03):
    // the player's attack is refused before its AP is charged, saying so.
    // Stock charged the attack, found the pack empty at the shot and said
    // nothing. Checked first, so an empty magazine with nothing to load says
    // "No ammo!" rather than "Reload".
    //
    // And a jammed gun (criticals CR-4.1, under `firearm fumbles`): refused
    // first, since clearing it needs no ammunition.
    if (reload_wielded_is_jammed(source_obj)) {
        tig_debug_printf("reload: attack refused, jammed\n");
        tf_add(source_obj, TF_TYPE_WHITE, "Jammed");
        anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
        return false;
    }

    if (reload_wielded_out_of_ammo(source_obj)) {
        tig_debug_printf("reload: attack refused, no ammunition\n");
        tf_add(source_obj, TF_TYPE_WHITE, "No ammo!");
        anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
        return false;
    }

    if (reload_wielded_is_empty(source_obj)) {
        tig_debug_printf("reload: attack refused, nothing loaded\n");
        tf_add(source_obj, TF_TYPE_WHITE, "Reload");
        anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
        return false;
    }

    action_points = combat_attack_cost(source_obj);
    if (!combat_consume_action_points(source_obj, action_points)) {
        anim_interrupt(&(run_info->id), PRIORITY_HIGHEST);
        return false;
    }

    // CE (real-time session B): a new attack attempt is a new action.
    rt_budget_attack_begun(run_info);

    art_id = run_info->params[1].data;
    if (art_id != TIG_ART_ID_INVALID) {
        object_set_current_aid(source_obj, art_id);
    } else {
        art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);
        object_set_current_aid(source_obj, art_id);
    }

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
    } else {
        tig_debug_printf("Anim: AGbeginAnimAttack: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
        run_info->pause_time.milliseconds = 100;
    }

    anim_move_pause_time_get(source_obj, &(run_info->pause_time));

    weapon_obj = item_wield_get(source_obj, ITEM_INV_LOC_WEAPON);

    delay = run_info->pause_time.milliseconds - 10 * (item_weapon_magic_speed(weapon_obj, source_obj) - 10);
    if (delay < 30) {
        delay = 30;
    } else if (delay > 800) {
        delay = 800;
    }
    run_info->pause_time.milliseconds = delay;

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_INVISIBLE) != 0
        && player_is_local_pc_obj(source_obj)) {
        object_flags_unset(source_obj, OF_INVISIBLE);
    }

    run_info->flags |= 0x10;

    if (obj_type_is_critter(source_obj_type) && random_between(1, 4) == 1) {
        sound_id = sfx_critter_sound(source_obj, CRITTER_SOUND_ATTACKING);
        gsound_play_sfx_on_obj(sound_id, 1, source_obj);
    }

    mt_item_notify_parent_attacks_obj(source_obj, target_obj);
    anim_play_weapon_fx(NULL, source_obj, source_obj, ANIM_WEAPON_EYE_CANDY_TYPE_POWER_GATHER);

    if (art_anim_data.action_frame < 1) {
        run_info->flags |= 0x04;
    }

    return true;
}

// 0x432990
bool AGupdateAnimAttack(AnimRunInfo* run_info)
{
    int64_t obj;
    int obj_type;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    int frame;
    int64_t target_obj;
    int weapon_type;
    int64_t weapon_obj;
    int delay;

    obj = run_info->params[0].obj;

    ASSERT(obj != OBJ_HANDLE_NULL); // 14089, "obj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    if (obj_type_is_critter(obj_type)
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return false;
    }

    // CE (rebalance, 2026-09-24): the chase stopgap, behind `real time chase`.
    // An NPC's melee swing waits while its target moves in reach, and is
    // given up - the goal goes back to chasing - once the target is out of it.
    switch (rt_chase_check(run_info)) {
    case RT_CHASE_WAIT:
        return true;
    case RT_CHASE_GIVE_UP:
        return false;
    case RT_CHASE_SWING:
        break;
    }

    // CE (real-time session B): an attack whose actor is still recovering holds
    // here, without advancing a frame, until the action budget allows it.
    if (rt_budget_hold(run_info)) {
        return true;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        run_info->flags &= ~0x10;
        return false;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame == art_anim_data.num_frames - 1) {
        // NOTE: Verified against 0x432A75 - original calls tig_art_type and discards the result at the final-frame exit; transcription faithful.
        tig_art_type(art_id);

        run_info->flags &= ~0x10;
        return false;
    }

    if (frame == art_anim_data.action_frame - 1) {
        run_info->flags |= 0x04;
    }

    target_obj = run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj;

    if (!combat_turn_based_is_active() && obj_type_is_critter(obj_type)) {
        weapon_type = tig_art_critter_id_weapon_get(art_id);
        if ((weapon_type == TIG_ART_WEAPON_TYPE_PISTOL
                || weapon_type == TIG_ART_WEAPON_TYPE_RIFLE)
            && frame == art_anim_data.action_frame + 2
            && anim_weapon_has_ammo(obj)
            && (obj_type == OBJ_TYPE_PC
                || (obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_BACKING_OFF) == 0)
            && anim_can_reach_target(obj, target_obj)) {
            // CE (real-time session B): another shot is another action.
            if (rt_budget_hold_repeat(run_info)) {
                return true;
            }
            art_id = tig_art_id_frame_set(art_id, frame - 3);
            art_id = tig_art_id_rotation_set(art_id, object_rot(obj, target_obj));
            object_set_current_aid(obj, art_id);
            run_info->flags &= ~0x0C;
        } else if (weapon_type == TIG_ART_WEAPON_TYPE_BOW
            && frame == art_anim_data.action_frame
            && anim_weapon_has_ammo(obj)
            && (obj_type == OBJ_TYPE_PC
                || (obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_BACKING_OFF) == 0)
            && anim_can_reach_target(obj, target_obj)) {
            // CE (real-time session B): another arrow is another action. Until
            // the last one's time is up the bow stays drawn.
            if (rt_budget_hold_repeat(run_info)) {
                return true;
            }
            art_id = tig_art_id_frame_set(art_id, frame - 3);
            art_id = tig_art_id_rotation_set(art_id, object_rot(obj, target_obj));
            object_set_current_aid(obj, art_id);
            run_info->flags &= ~0x0C;
        } else {
            object_inc_current_aid(obj);
        }
    } else {
        object_inc_current_aid(obj);
    }

    if ((run_info->flags & 0x80) != 0) {
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            run_info->pause_time.milliseconds = 1000 / art_anim_data.fps;
        } else {
            tig_debug_printf("Anim: AGupdateAnimAttack: Failed to find Aid: %d, defaulting to 10 fps!", art_id);
            run_info->pause_time.milliseconds = 100;
        }

        anim_move_pause_time_get(obj, &(run_info->pause_time));

        weapon_obj = item_wield_get(obj, ITEM_INV_LOC_WEAPON);
        delay = run_info->pause_time.milliseconds - 10 * (item_weapon_magic_speed(weapon_obj, obj) - 10);
        if (delay < 30) {
            delay = 30;
        } else if (delay > 800) {
            delay = 800;
        }

        run_info->pause_time.milliseconds = delay;
        run_info->flags &= ~0x80;
    }

    return true;
}

// 0x432CF0
// Whether the critter's equipped weapon has enough ammo for another shot.
// Unarmed/no-ammo weapons count as ready.
bool anim_weapon_has_ammo(int64_t critter_obj)
{
    int64_t weapon_obj;
    int ammo_type;
    int qty;
    int consumption;

    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
    if (weapon_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    ammo_type = item_weapon_ammo_type(weapon_obj);
    if (ammo_type == 10000) {
        return true;
    }

    // CE (reload prototype session 2): a gun under `firearm reloading` has
    // another shot when one is loaded, whatever the pack holds - so real-time
    // repeat fire stops at an empty gun.
    if (reload_applies(critter_obj, weapon_obj)) {
        return reload_loaded_get(weapon_obj) > 0;
    }

    qty = item_ammo_quantity_get(critter_obj, ammo_type);
    // CE (turn-based rebalance): `rules/weapons.mes` AmmoPerShot.
    consumption = weapon_rules_ammo_per_shot(weapon_obj);
    if (qty >= consumption) {
        return true;
    }

    return false;
}

// 0x432D50
bool anim_obj_is_valid(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;
    if (obj == OBJ_HANDLE_NULL) {
        ASSERT(obj != OBJ_HANDLE_NULL); // obj != OBJ_HANDLE_NULL
        return false;
    }

    return true;
}

// 0x432D90
void anim_create_blood_splat(int64_t obj)
{
    int64_t pc_obj;
    int64_t loc;
    int offset_x;
    int offset_y;
    int64_t blood_obj;
    tig_art_id_t blood_art_id;
    tig_art_id_t obj_art_id;
    TigArtAnimData art_anim_data;
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    pc_obj = player_get_local_pc_obj();

    // NOTE: Verified against 0x432DBE - original calls obj_field_int32_get(pc_obj, OBJ_F_PC_FLAGS) and discards the result; transcription faithful.
    obj_field_int32_get(pc_obj, OBJ_F_PC_FLAGS);

    if (violence_filter != 0) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & (OCF_UNDEAD | OCF_MECHANICAL)) != 0) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_ELEMENTAL) != 0) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()
        && !anim_editor) {
        return;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
    object_create(proto_get_by_description(BP_POOL_OF_BLOOD), loc, &blood_obj);
    blood_art_id = obj_field_int32_get(blood_obj, OBJ_F_CURRENT_AID);

    // NOTE: Verified against 0x432EB5-0x432EBB - original calls tig_art_critter_id_size_get on the object's current aid and discards the result; transcription faithful.
    tig_art_critter_id_size_get(obj_field_int32_get(obj, OBJ_F_CURRENT_AID));

    if (anim_editor) {
        obj_art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        if (tig_art_anim_data(obj_art_id, &art_anim_data) == TIG_OK) {
            obj_art_id = tig_art_id_frame_set(obj_art_id, art_anim_data.num_frames - 1);
        }
        object_set_current_aid(obj, obj_art_id);
        return;
    }

    if (tig_net_is_host()) {
        Packet70 pkt;

        pkt.type = 70;
        pkt.subtype = 5;
        pkt.s5.oid = obj_get_id(obj);
        pkt.s5.loc = loc;
        pkt.s5.offset_x = offset_x;
        pkt.s5.offset_y = offset_y;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    if (anim_goal_data_init(&goal_data, blood_obj, AG_ANIMATE)) {
        goal_data.params[AGDATA_ANIM_ID].data = blood_art_id;
        if (anim_goal_add(&goal_data, NULL)) {
            critter_decay_timeevent_schedule(blood_obj);
        } else {
            tig_debug_printf("Anim: AGapplyBloodEffect: ERROR: Blood object failed to animate!\n");
            object_destroy(blood_obj);
        }
    }
}

// 0x433020
void anim_play_blood_splotch_fx(int64_t obj, int blood_splotch_type, int damage_type, CombatContext* combat)
{
    int fx_id;
    unsigned int critter_flags;
    unsigned int spell_flags;
    AnimFxNode fx;

    (void)damage_type;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (blood_splotch_type == BLOOD_SPLOTCH_TYPE_NONE) {
        return;
    }

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return;
    }

    if (!obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NO_BLOOD_SPLOTCHES) != 0) {
        return;
    }

    fx_id = blood_splotch_type - 1;
    if (fx_id == ANIM_EYE_CANDY_NORMAL_BLOOD_SPLOTCH) {
        // NOTE: Verified against 0x4330A7-0x4330B3 - original fetches the local PC, reads OBJ_F_PC_FLAGS, and discards the result; transcription faithful.
        obj_field_int32_get(player_get_local_pc_obj(), OBJ_F_PC_FLAGS);

        critter_flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
        spell_flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);

        if (violence_filter == 0 && (spell_flags & OSF_STONED) == 0) {
            if ((critter_flags & OCF_UNDEAD) != 0) {
                fx_id = ANIM_EYE_CANDY_UNDEAD_BLOOD_SPLOTCH;
            } else if ((critter_flags & OCF_MECHANICAL) != 0) {
                fx_id = ANIM_EYE_CANDY_STONED_BLOOD_SPLOTCH;
            }
        } else {
            fx_id = ANIM_EYE_CANDY_STONED_BLOOD_SPLOTCH;
        }
    }

    if (combat->total_dam > 5 && fx_id == ANIM_EYE_CANDY_NORMAL_BLOOD_SPLOTCH) {
        if (combat->total_dam < 10) {
            fx_id = ANIM_EYE_CANDY_NORMAL_BLOOD_SPLOTCH_X2;
        } else if (combat->total_dam < 15) {
            fx_id = ANIM_EYE_CANDY_NORMAL_BLOOD_SPLOTCH_X3;
        } else {
            fx_id = ANIM_EYE_CANDY_NORMAL_BLOOD_SPLOTCH_X4;
        }
    }

    animfx_node_init(&anim_eye_candies, &fx, obj, -1, fx_id);
    fx.animate = true;
    fx.max_simultaneous_effects = 3;
    animfx_add(&fx);

    magictech_anim_play_hit_fx(obj, combat);
}

// 0x433170
void anim_lag_icon_add(int64_t obj)
{
    tig_art_id_t art_id;
    int idx;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (tig_art_eye_candy_id_create(0, 0, 0, 0, 0, 0, 4, &art_id) != TIG_OK) {
        ASSERT(0); // 14478, "0"
        return;
    }

    for (idx = 0; idx < 7; idx++) {
        if (obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, idx) == art_id) {
            return;
        }
    }

    for (idx = 0; idx < 7; idx++) {
        if (obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, idx) == TIG_ART_ID_INVALID) {
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, idx, art_id);
            break;
        }
    }
}

// 0x433220
void anim_lag_icon_remove(int64_t obj)
{
    int idx;
    tig_art_id_t art_id;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    for (idx = 0; idx < 7; idx++) {
        art_id = obj_arrayfield_uint32_get(obj, OBJ_F_OVERLAY_FORE, idx);
        if (tig_art_num_get(art_id) == 0) {
            object_overlay_set(obj, OBJ_F_OVERLAY_FORE, idx, TIG_ART_ID_INVALID);
            break;
        }
    }
}

// 0x433270
bool anim_spend_action_points(AnimRunInfo* run_info)
{
    int64_t obj;

    obj = run_info->params[0].obj;
    if (obj == OBJ_HANDLE_NULL) {
        ASSERT(obj != OBJ_HANDLE_NULL); // obj != OBJ_HANDLE_NULL
        return false;
    }

    // CE (real-time session C): a throw pays through `combat_throw_cost_for`,
    // which the real-time budget reads too. The same numbers as below, and the
    // same test of who is exempt - the run's own object, as the original asks.
    if (run_info->cur_stack_data != NULL
        && run_info->cur_stack_data->type == AG_THROW_ITEM) {
        int cost = combat_throw_cost_for(player_is_local_pc_obj(run_info->anim_obj));
        return cost == 0 || combat_consume_action_points(obj, cost);
    }

    // UAP (0x4332C0, 0x4332C2): the local PC stops being exempt, and the cost
    // doubles from 2 to 4. The patch NOPs the `jne` that skipped the charge for
    // the player and rewrites the pushed immediate, so in the patched build
    // every critter - the player included - pays 4.
    if (uap_fixes_enabled()) {
        return combat_consume_action_points(obj, 4);
    }

    if (player_is_local_pc_obj(run_info->anim_obj)) {
        return true;
    }

    return combat_consume_action_points(obj, 2);
}

// 0x4332E0
bool anim_goal_animate(int64_t obj, int anim)
{
    int obj_type;
    AnimGoalData goal_data;
    tig_art_id_t art_id;

    ASSERT(obj != OBJ_HANDLE_NULL); // 14570, obj != OBJ_HANDLE_NULL
    ASSERT(anim >= 0); // 14571, whichAnim >= 0
    ASSERT(anim < 26); // 14572, whichAnim < tig_art_anim_max

    if (obj == OBJ_HANDLE_NULL
        || anim < 0
        || anim >= 26) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if (obj_type_is_critter(obj_type)
        && anim == 10
        && anim_goal_data_init(&goal_data, obj, AG_ANIMATE_KNEEL_MAGIC_HANDS)) {
        anim_goal_add(&goal_data, 0);
        return true;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_ANIMATE)) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    goal_data.params[AGDATA_ANIM_ID].data = tig_art_id_anim_set(art_id, anim);
    if (!anim_goal_add(&goal_data, 0)) {
        return false;
    }

    return true;
}

// 0x433440
bool anim_goal_rotate(int64_t obj, int rot)
{
    tig_art_id_t art_id;
    AnimID anim_id;
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_id_rotation_get(art_id) == rot) {
        return true;
    }

    if (!critter_is_active(obj)) {
        return true;
    }

    if (combat_turn_based_is_active() && combat_turn_based_whos_turn_get() != obj) {
        return true;
    }

    if (anim_is_busy(obj, NULL)) {
        return true;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_ROTATE)) {
        return true;
    }

    goal_data.params[AGDATA_SCRATCH_VAL1].data = rot;
    anim_is_busy(obj, &anim_id);
    if (!anim_goal_add(&goal_data, &anim_id)) {
        return false;
    }

    return true;
}

// 0x433580
bool anim_goal_animate_loop(int64_t obj)
{
    tig_art_id_t art_id;
    int goal_type;
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (anim_is_busy(obj, NULL)) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    goal_type = (obj_field_int32_get(obj, OBJ_F_SCENERY_FLAGS) & OSCF_IS_FIRE)
        ? AG_ANIMATE_LOOP_FIRE_DMG
        : AG_ANIMATE_LOOP;
    anim_goal_data_init(&goal_data, obj, goal_type);
    goal_data.params[AGDATA_ANIM_ID].data = art_id;
    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_run_info[anim_scratch_goal_id.slot_num].goals[0].params[AGDATA_SOUND_HANDLE].data = TIG_SOUND_HANDLE_INVALID;

    return true;
}

// 0x433640
bool anim_goal_move_to_tile(int64_t obj, int64_t loc)
{
    AnimID anim_id;
    AnimRunInfo* run_info;
    AnimGoalData goal_data;

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        Packet4 pkt;

        if (!anim_is_current_goal_type(obj, AG_RUN_TO_TILE, &anim_id)) {
            return false;
        }

        if (anim_id_to_run_info(&anim_id, &run_info)) {
            return false;
        }

        if (run_info->goals[run_info->current_goal].params[AGDATA_TARGET_TILE].loc == loc) {
            return false;
        }

        pkt.type = 4;
        pkt.subtype = 0;
        mp_obj_to_oid(obj, &(pkt.oid));
        pkt.loc = loc;
        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    if (!anim_can_start_move_goal(obj)) {
        return false;
    }

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC
        && get_always_run(obj)
        && critter_encumbrance_level_get(obj) < 4) {
        return anim_goal_run_to_tile(obj, loc);
    }

    if (!anim_is_current_goal_type(obj, AG_MOVE_TO_TILE, &anim_scratch_goal_id)) {
        anim_goal_data_init_no_interrupt(&goal_data, obj, AG_MOVE_TO_TILE);
        goal_data.params[AGDATA_TARGET_TILE].loc = loc;
        if (!anim_interrupt_all_goals_of_obj(obj, 3, false, false)) {
            return false;
        }

        if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
            return false;
        }

        return true;
    }

    run_info = &(anim_run_info[anim_scratch_goal_id.slot_num]);

    if (run_info->goals[0].params[AGDATA_TARGET_TILE].loc == loc) {
        return true;
    }

    if (tig_net_is_active()) {
        Packet8 pkt;
        int64_t self_obj;

        self_obj = run_info->goals[0].params[AGDATA_SELF_OBJ].obj;

        run_info->path.flags |= 0x04;

        if (tig_net_is_host()) {
            run_info->goals[0].params[AGDATA_TARGET_TILE].loc = loc;
            sub_44D0C0(run_info);
        }

        anim_modify_data_init(&(pkt.modify_data));

        pkt.type = 8;
        pkt.modify_data.id = anim_scratch_goal_id;
        pkt.modify_data.flags = run_info->flags;
        pkt.modify_data.path_flags = run_info->path.flags;
        pkt.modify_data.field_14 = 5;
        pkt.modify_data.loc = loc;
        pkt.modify_data.path_curr = run_info->path.curr;
        pkt.modify_data.location = obj_field_int64_get(self_obj, OBJ_F_LOCATION);
        pkt.modify_data.current_aid = obj_field_int32_get(self_obj, OBJ_F_CURRENT_AID);
        pkt.offset_x = obj_field_int32_get(self_obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(self_obj, OBJ_F_OFFSET_Y);

        if (tig_net_is_host()) {
            run_info->id.field_8++;
        }

        tig_net_send_app_all(&pkt, sizeof(pkt));
        return true;
    }

    run_info->path.flags |= 0x04;
    run_info->goals[0].params[AGDATA_TARGET_TILE].loc = loc;

    return true;
}

// 0x4339A0
// Common gate for the move/run-to-tile starters: active, current turn in
// TB combat, and not an NPC reacting to the local PC.
bool anim_can_start_move_goal(int64_t obj)
{
    return obj != OBJ_HANDLE_NULL
        && critter_is_active(obj)
        && (!combat_turn_based_is_active() || combat_turn_based_whos_turn_get() == obj)
        && (player_is_pc_obj(obj) || !reaction_current_pc_get(obj));
}

// 0x433A00
// Move-to-tile starter that appends a waypoint subgoal when a move goal is
// already running. Used for multi-waypoint town-map travel.
bool anim_goal_move_to_tile_append(int64_t obj, int64_t loc, bool a3)
{
    AnimID anim_id;
    AnimGoalData goal_data;
    AnimRunInfo* run_info;

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        if (anim_is_current_goal_type(obj, AG_RUN_TO_TILE, &anim_id)
            && anim_id_to_run_info(&anim_id, &run_info)
            && run_info->goals[run_info->current_goal].params[AGDATA_TARGET_TILE].loc == loc) {
            return false;
        }

        Packet4 pkt;

        pkt.type = 4;
        pkt.subtype = 2;
        mp_obj_to_oid(obj, &(pkt.oid));
        pkt.loc = loc;

        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    if (!anim_can_start_move_goal(obj)) {
        return false;
    }

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC
        && get_always_run(obj)) {
        return anim_goal_run_to_tile_append(obj, loc);
    }

    if (anim_is_current_goal_type(obj, AG_MOVE_TO_TILE, &anim_scratch_goal_id) || a3) {
        run_info = &(anim_run_info[anim_scratch_goal_id.slot_num]);
        if (run_info->goals[0].params[AGDATA_TARGET_TILE].loc == loc) {
            return true;
        }

        anim_goal_data_init_no_interrupt(&goal_data, obj, AG_MOVE_TO_TILE);
        goal_data.params[AGDATA_TARGET_TILE].loc = loc;

        // __FILE__: "C:\Troika\Code\Game\GameLibX\Anim.c"
        // __LINE__: 15016
        if (!anim_subgoal_add(anim_scratch_goal_id, &goal_data, __FILE__, __LINE__)) {
            return false;
        }
    } else {
        anim_goal_data_init_no_interrupt(&goal_data, obj, AG_MOVE_TO_TILE);
        goal_data.params[AGDATA_TARGET_TILE].loc = loc;

        if (!anim_interrupt_all_goals_of_obj(obj, 3, false, false)) {
            return false;
        }

        if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
            return false;
        }
    }

    return true;
}

// 0x433C80
bool anim_goal_run_to_tile(int64_t obj, int64_t loc)
{
    AnimID anim_id;
    AnimRunInfo* run_info;
    AnimGoalData goal_data;

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        if (anim_is_current_goal_type(obj, AG_RUN_TO_TILE, &anim_id)
            && anim_id_to_run_info(&anim_id, &run_info)
            && run_info->goals[run_info->current_goal].params[AGDATA_TARGET_TILE].loc == loc) {
            return false;
        }

        Packet4 pkt;

        pkt.type = 4;
        pkt.subtype = 1;
        mp_obj_to_oid(obj, &(pkt.oid));
        pkt.loc = loc;

        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    if (!anim_can_start_move_goal(obj)) {
        return false;
    }

    if (!anim_is_current_goal_type(obj, AG_RUN_TO_TILE, &anim_scratch_goal_id)) {
        anim_goal_data_init_no_interrupt(&goal_data, obj, AG_RUN_TO_TILE);
        goal_data.params[AGDATA_TARGET_TILE].loc = loc;

        if (!anim_interrupt_all_goals_of_obj(obj, 3, false, false)) {
            return false;
        }

        if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
            return false;
        }

        return true;
    }

    run_info = &(anim_run_info[anim_scratch_goal_id.slot_num]);
    run_info->flags |= 0x40;

    // NOTE: Faithful, and the dead branch is Troika's. The original writes the
    // flag and then tests the same slot for zero, so the encumbrance check can
    // never run:
    //
    //     433F08: mov edx, [eax+0xC] / or edx, 0x40 / mov [eax+0xC], edx
    //     433F15: mov ecx, [eax+0xC]
    //     433F18: test ecx, ecx
    //     433F1A: jne 0x433F42            ; always taken
    //
    // It is not a one-off transcription slip either: the same `|= 0x40` then
    // `== 0` pair appears three times in this one function in the original, and
    // all three are reproduced below. Presumably the test was meant to run
    // before the OR, but that is a guess, so it is left as written.
    if (run_info->flags == 0
        && critter_encumbrance_level_get(run_info->anim_obj) < ENCUMBRANCE_LEVEL_SIGNIFICANT) {
        run_info->flags |= 0x40;
    }

    if (run_info->goals[0].params[AGDATA_TARGET_TILE].loc == loc) {
        return true;
    }

    if (tig_net_is_active()) {
        int64_t self_obj;
        Packet8 pkt;

        self_obj = run_info->goals[0].params[AGDATA_SELF_OBJ].obj;

        run_info->path.flags |= 0x04;

        if (tig_net_is_host()) {
            run_info->goals[0].params[AGDATA_TARGET_TILE].loc = loc;
            run_info->flags |= 0x40;

            if (run_info->flags == 0
                && critter_encumbrance_level_get(run_info->anim_obj) < ENCUMBRANCE_LEVEL_SIGNIFICANT) {
                run_info->flags |= 0x40;
            }

            sub_44D0C0(run_info);
        }

        anim_modify_data_init(&(pkt.modify_data));
        pkt.type = 8;
        pkt.modify_data.id = anim_scratch_goal_id;
        pkt.modify_data.flags = run_info->flags;
        pkt.modify_data.path_flags = run_info->path.flags;
        pkt.modify_data.field_14 = 5;
        pkt.modify_data.loc = loc;
        pkt.modify_data.path_curr = run_info->path.curr;
        pkt.field_40 = run_info->extra_target_tile;
        pkt.modify_data.location = obj_field_int64_get(self_obj, OBJ_F_LOCATION);
        pkt.modify_data.current_aid = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        pkt.offset_x = obj_field_int32_get(self_obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(self_obj, OBJ_F_OFFSET_Y);

        if (tig_net_is_host()) {
            run_info->id.field_8++;
        }

        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    run_info->path.flags |= 0x04;
    run_info->goals[0].params[AGDATA_TARGET_TILE].loc = loc;

    if (run_info->flags == 0
        && critter_encumbrance_level_get(run_info->anim_obj) < ENCUMBRANCE_LEVEL_SIGNIFICANT) {
        run_info->flags |= 0x40;
    }

    return true;
}

// 0x434030
// Run-to-tile twin of anim_goal_move_to_tile_append: starts or appends an
// AG_RUN_TO_TILE subgoal and sets the running flag.
bool anim_goal_run_to_tile_append(int64_t obj, int64_t loc)
{
    AnimRunInfo* run_info;
    AnimGoalData goal_data;

    if (!anim_can_start_move_goal(obj)) {
        return false;
    }

    if (!anim_is_current_goal_type(obj, AG_RUN_TO_TILE, &anim_scratch_goal_id)) {
        anim_goal_data_init_no_interrupt(&goal_data, obj, AG_RUN_TO_TILE);
        goal_data.params[AGDATA_TARGET_TILE].loc = loc;

        if (!anim_interrupt_all_goals_of_obj(obj, 3, false, false)) {
            return false;
        }

        if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
            return false;
        }

        return true;
    }

    run_info = &(anim_run_info[anim_scratch_goal_id.slot_num]);
    run_info->flags |= 0x40;
    if (run_info->goals[0].params[AGDATA_TARGET_TILE].loc != loc) {
        anim_goal_data_init_no_interrupt(&goal_data, obj, 3);
        goal_data.params[AGDATA_TARGET_TILE].loc = loc;

        // __FILE__: C:\Troika\Code\Game\GameLibX\Anim.c
        // __LINE__: 15263
        anim_subgoal_add(anim_scratch_goal_id, &goal_data, __FILE__, __LINE__);
    }

    return true;
}

// 0x4341C0
bool anim_goal_move_near_tile(int64_t source_obj, int64_t target_loc, int range)
{
    AnimRunInfo* run_info;
    AnimGoalData goal_data;

    if (!anim_can_start_move_goal(source_obj)) {
        return false;
    }

    if (!anim_is_current_goal_type(source_obj, AG_RUN_TO_TILE, &anim_scratch_goal_id)) {
        if (anim_goal_data_init(&goal_data, source_obj, AG_MOVE_NEAR_TILE)) {
            goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
            goal_data.params[AGDATA_RANGE_DATA].data = range;
            if (anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
                return true;
            }
        }

        return false;
    }

    run_info = &(anim_run_info[anim_scratch_goal_id.slot_num]);

    if (run_info->goals[0].params[AGDATA_TARGET_TILE].loc == target_loc) {
        return true;
    }

    if (tig_net_is_active()) {
        int64_t obj;
        Packet8 pkt;

        obj = run_info->goals[0].params[AGDATA_SELF_OBJ].obj;

        run_info->path.flags |= 0x04;

        if (tig_net_is_host()) {
            run_info->goals[0].params[AGDATA_TARGET_TILE].loc = target_loc;
            sub_44D0C0(run_info);
        }

        anim_modify_data_init(&(pkt.modify_data));
        pkt.modify_data.id = anim_scratch_goal_id;
        pkt.modify_data.flags = run_info->flags;
        pkt.modify_data.path_flags = run_info->path.flags;

        pkt.type = 8;
        pkt.modify_data.field_14 = 5;
        pkt.modify_data.loc = target_loc;
        pkt.field_40 = run_info->extra_target_tile;
        pkt.modify_data.path_curr = run_info->path.curr;
        pkt.modify_data.location = obj_field_int64_get(obj, OBJ_F_LOCATION);
        pkt.modify_data.current_aid = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);

        if (tig_net_is_host()) {
            run_info->id.field_8++;
        }

        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    run_info->goals[0].params[AGDATA_TARGET_TILE].loc = target_loc;
    run_info->path.flags |= 0x04;

    return true;
}

// 0x434400
bool anim_goal_run_near_tile(int64_t source_obj, int64_t target_loc, int range)
{
    AnimRunInfo* run_info;
    AnimGoalData goal_data;

    if (!anim_can_start_move_goal(source_obj)) {
        return false;
    }

    if (!anim_is_current_goal_type(source_obj, AG_RUN_TO_TILE, &anim_scratch_goal_id)) {
        if (anim_goal_data_init(&goal_data, source_obj, AG_RUN_NEAR_TILE)) {
            goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
            goal_data.params[AGDATA_RANGE_DATA].data = range;
            if (anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
                if (critter_encumbrance_level_get(source_obj) < ENCUMBRANCE_LEVEL_SIGNIFICANT) {
                    turn_on_running(anim_scratch_goal_id);
                }

                return true;
            }
        }

        return false;
    }

    run_info = &(anim_run_info[anim_scratch_goal_id.slot_num]);
    if ((run_info->flags & 0x40) == 0
        && critter_encumbrance_level_get(run_info->anim_obj) < ENCUMBRANCE_LEVEL_SIGNIFICANT) {
        run_info->flags |= 0x40;
    }

    if (run_info->goals[0].params[AGDATA_TARGET_TILE].loc == target_loc) {
        return true;
    }

    if (tig_net_is_active()) {
        int64_t obj;
        Packet8 pkt;

        obj = run_info->goals[0].params[AGDATA_SELF_OBJ].obj;

        run_info->path.flags |= 0x04;

        if (tig_net_is_host()) {
            run_info->goals[0].params[AGDATA_TARGET_TILE].loc = target_loc;
            sub_44D0C0(run_info);
        }

        anim_modify_data_init(&(pkt.modify_data));
        pkt.modify_data.id = anim_scratch_goal_id;
        pkt.modify_data.flags = run_info->flags;
        pkt.modify_data.path_flags = run_info->path.flags;

        pkt.type = 8;
        pkt.modify_data.field_14 = 5;
        pkt.modify_data.loc = target_loc;
        pkt.field_40 = run_info->extra_target_tile;
        pkt.modify_data.path_curr = run_info->path.curr;
        pkt.modify_data.location = obj_field_int64_get(obj, OBJ_F_LOCATION);
        pkt.modify_data.current_aid = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);

        if (tig_net_is_host()) {
            run_info->id.field_8++;
        }

        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    run_info->goals[0].params[AGDATA_TARGET_TILE].loc = target_loc;
    run_info->path.flags |= 0x04;

    return true;
}

// 0x4346A0
bool anim_goal_follow_obj(int64_t source_obj, int64_t target_obj)
{
    AnimID anim_id;
    AnimRunInfo* run_info;
    bool v1 = false;
    AnimGoalData goal_data;
    int range;
    int64_t source_loc;
    int64_t target_loc;

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (obj_field_int32_get(source_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        tig_debug_printf("Anim: anim_goal_follow_obj: ERROR: Goal only valid on NPCs!\n");
        return false;
    }

    if (anim_is_busy(source_obj, &anim_id)) {
        run_info = &(anim_run_info[anim_id.slot_num]);
        if ((run_info->cur_stack_data->params[AGDATA_FLAGS_DATA].data & 0x1000) == 0) {
            if (run_info->cur_stack_data->type != AG_ANIM_FIDGET) {
                return false;
            }

            v1 = true;
        }
    }

    if (!anim_can_start_goal(source_obj, 0)) {
        return false;
    }

    if (!anim_goal_data_init_no_interrupt(&goal_data, source_obj, AG_FOLLOW)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;

    range = 4;
    if ((obj_field_int32_get(source_obj, OBJ_F_NPC_FLAGS) & ONF_AI_SPREAD_OUT) != 0) {
        range = 7;
    }

    if (anim_is_current_goal_ex(source_obj, &goal_data, &anim_id)) {
        run_info = &(anim_run_info[anim_id.slot_num]);
        switch (run_info->cur_stack_data->type) {
        // NOTE: Not sure why this one was specified explicitly.
        case AG_RUN_NEAR_OBJ:
            anim_make_room_for_critter(source_obj);
            return true;
        case AG_MOVE_NEAR_OBJ:
        case AG_ATTEMPT_MOVE_NEAR:
            source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
            target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
            if (location_dist(source_loc, target_loc) <= range) {
                anim_make_room_for_critter(source_obj);
                return true;
            }
            break;
        default:
            anim_make_room_for_critter(source_obj);
            return true;
        }
    } else {
        source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
        target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
        if (location_dist(source_loc, target_loc) <= range) {
            anim_make_room_for_critter(source_obj);
            return true;
        }
    }

    if (v1) {
        anim_interrupt(&anim_id, PRIORITY_HIGHEST);
    }

    if (!anim_interrupt_all_goals_of_obj(source_obj, 3, false, false)) {
        return false;
    }

    goal_data.params[AGDATA_FLAGS_DATA].data |= 0x1000;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

/**
 * Gate check at the top of the `anim_goal_*` starters: `obj` can begin an
 * action costing `action_points` if it is an active critter and - in
 * turn-based combat - it is its turn and it can afford the cost (attack
 * passes `combat_attack_cost`, item use 4, free actions 0). Quirk faithful to
 * the original binary (verified at 0x4348E0): an NPC with more than 35% of
 * its fatigue remaining bypasses the action-point check entirely.
 *
 * 0x4348E0
 */
bool anim_can_start_goal(int64_t obj, int action_points)
{
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!critter_is_active(obj)) {
        return false;
    }

    if (!combat_turn_based_is_active()) {
        return true;
    }

    if (combat_turn_based_whos_turn_get() != obj) {
        return false;
    }

    if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return true;
    }

    // UAP (0x434935): call retargeted to UAP's relocated
    // `combat_action_points_get` at 0x4B7774; same getter, no behaviour change.
    if (action_points <= combat_action_points_get()) {
        return true;
    }

    if (100 * critter_fatigue_current(obj) / critter_fatigue_max(obj) > 35) {
        return true;
    }

    return false;
}

// 0x434980
bool anim_goal_flee(int64_t obj, int64_t from_obj)
{
    AnimGoalData goal_data;

    if (!anim_can_start_goal(obj, 0)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_FLEE)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = from_obj;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_goal_turn_on_stop_at_blockers(anim_scratch_goal_id);

    return true;
}

// 0x434AE0
bool anim_goal_attack(int64_t attacker_obj, int64_t target_obj)
{
    return anim_goal_attack_ex(attacker_obj, target_obj, -1);
}

// 0x434B00
bool anim_goal_attack_ex(int64_t attacker_obj, int64_t target_obj, int sound_id)
{
    int64_t weapon_obj;
    unsigned int spell_flags;
    unsigned int critter_flags2;
    AnimGoalData goal_data;
    int64_t source_obj;
    int64_t block_obj;

    ASSERT(attacker_obj != target_obj); // 15680, "attackerObj != targetObj"

    if (attacker_obj == target_obj) {
        return false;
    }

    weapon_obj = item_wield_get(attacker_obj, ITEM_INV_LOC_WEAPON);
    if (weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
        && (obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_DEFAULT_THROWS) != 0
        && item_check_remove(weapon_obj) == ITEM_CANNOT_OK) {
        item_remove(weapon_obj);

        return anim_goal_throw_item(attacker_obj, weapon_obj, obj_field_int64_get(target_obj, OBJ_F_LOCATION));
    }

    if (!anim_can_start_goal(attacker_obj, combat_attack_cost(attacker_obj))) {
        return false;
    }

    spell_flags = obj_field_int32_get(attacker_obj, OBJ_F_SPELL_FLAGS);
    critter_flags2 = obj_field_int32_get(attacker_obj, OBJ_F_CRITTER_FLAGS2);

    if ((spell_flags & OSF_BODY_OF_AIR) != 0
        && (critter_flags2 & OCF2_ELEMENTAL) == 0) {
        anim_make_room_for_critter(attacker_obj);
        return false;
    }

    if (!anim_goal_data_init_no_interrupt(&goal_data, attacker_obj, AG_ATTACK)) {
        anim_make_room_for_critter(attacker_obj);
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;

    if (anim_is_current_goal(attacker_obj, &goal_data)) {
        anim_make_room_for_critter(attacker_obj);
        return false;
    }

    source_obj = attacker_obj;
    if (anim_find_blocking_critter(&source_obj, &block_obj)) {
        anim_goal_please_move(block_obj, source_obj);
        return false;
    }

    if (!anim_interrupt_all_goals_of_obj(attacker_obj, 3, false, false)) {
        anim_make_room_for_critter(attacker_obj);
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_USING_BOOMERANG) != 0) {
        anim_make_room_for_critter(attacker_obj);
        return false;
    }

    goal_data.params[AGDATA_SCRATCH_VAL5].data = sound_id;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        anim_make_room_for_critter(attacker_obj);
        return false;
    }

    if (obj_field_int32_get(attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        if (!tig_net_is_active()
            && critter_fatigue_current(attacker_obj) > 8) {
            turn_on_running(anim_scratch_goal_id);
        }
    } else {
        if (get_always_run(attacker_obj)) {
            turn_on_running(anim_scratch_goal_id);
        }
    }

    return true;
}

// 0x434DE0
bool anim_goal_get_up(int64_t obj)
{
    AnimGoalData goal_data;
    tig_art_id_t art_id;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_id_anim_get(art_id) != TIG_ART_ANIM_FALL_DOWN) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_ANIM_GET_UP)) {
        return false;
    }

    goal_data.params[AGDATA_ANIM_ID].data = tig_art_id_anim_set(art_id, TIG_ART_ANIM_GET_UP);

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x434E80
bool anim_goal_knockback(int64_t target_obj, int rot, int range, int64_t source_obj)
{
    int distance;
    int64_t loc;
    AnimGoalData goal_data;

    ASSERT(range > 0); // 15830, "range > 0"

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    for (distance = 0; distance < range; distance++) {
        if (!location_in_dir(loc, rot, &loc)) {
            return false;
        }
    }

    if (!anim_goal_data_init(&goal_data, target_obj, AG_KNOCKBACK)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_TILE].loc = loc;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = source_obj;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x434F80
bool anim_goal_throw_item(int64_t obj, int64_t item_obj, int64_t target_loc)
{
    // The original's entry point, unchanged in effect: an unaimed throw, which
    // is what every one of its callers meant when it was the only one there
    // was. `HIT_LOC_TORSO` is how `combat_attack` spells "roll the location
    // normally", so this is not a throw forced at the chest.
    return anim_goal_throw_item_aimed(obj, item_obj, target_loc, HIT_LOC_TORSO);
}

// CE (card S2): a throw that carries the aim its command committed.
//
// Separate from the entry point above rather than a fourth parameter on it,
// because four of the five callers have no aim to pass and never will: an
// NPC's aim is its own, and a throw at a tile has no limb to ask for. The one
// caller that does is the local player attacking a critter with a weapon that
// throws by default, where `intgame.c` had already committed an aim and then
// discarded the goal holding it.
bool anim_goal_throw_item_aimed(int64_t obj, int64_t item_obj, int64_t target_loc, int hit_loc)
{
    AnimGoalData goal_data;

    ASSERT(item_obj != OBJ_HANDLE_NULL); // itemObj != OBJ_HANDLE_NULL

    if (!anim_can_start_goal(obj, 0)) {
        return false;
    }

    if (item_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_goal_data_init_no_interrupt(&goal_data, obj, AG_THROW_ITEM)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;

    // Snapshotted here, at command creation, and never re-read: once the throw
    // is accepted it owns its aim, so changing the selection afterwards cannot
    // retarget a weapon already in the air. The slot is free in this goal -
    // `anim_goal_node_throw_item` names only SELF_OBJ, TARGET_TILE, ANIM_ID
    // and SCRATCH_OBJ, and none of its node functions touches SCRATCH_VAL3.
    if (hit_loc > HIT_LOC_TORSO && hit_loc < HIT_LOC_COUNT) {
        goal_data.params[AGDATA_SCRATCH_VAL3].data = hit_loc;
    }

    if (anim_is_current_goal(obj, &goal_data)) {
        return false;
    }

    if (!anim_interrupt_all_goals_of_obj(obj, 3, false, false)) {
        return false;
    }

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x435080
bool anim_goal_dying(int64_t obj, int anim)
{
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_DYING)) {
        return false;
    }

    goal_data.params[AGDATA_SCRATCH_VAL1].data = anim;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x4350F0
bool anim_goal_use_skill_on(int64_t obj, int64_t target_obj, int64_t item_obj, int skill, unsigned int flags)
{
    AnimGoalData goal_data;

    if (skill == SKILL_PICK_LOCKS) {
        return anim_goal_pick_lock(obj, target_obj, item_obj);
    }

    if (!anim_can_start_goal(obj, 4)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_USE_SKILL_ON)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;
    goal_data.params[AGDATA_SKILL_DATA].data = skill;
    goal_data.params[AGDATA_FLAGS_DATA].data |= flags;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_goal_turn_on_direct_path(anim_scratch_goal_id);

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        if (!tig_net_is_active()
            && critter_fatigue_current(obj) > 8) {
            turn_on_running(anim_scratch_goal_id);
        }
    } else {
        if (get_always_run(obj)) {
            turn_on_running(anim_scratch_goal_id);
        }
    }

    return true;
}

// 0x4352C0
bool anim_goal_use_item_on_obj_with_skill(int64_t obj, int64_t item_obj, int64_t target_obj, int skill, int modifier)
{
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_can_start_goal(obj, 4)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_USE_ITEM_ON_OBJECT_WITH_SKILL)) {
        return false;
    }

    goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;
    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;
    goal_data.params[AGDATA_SKILL_DATA].data = skill;
    goal_data.params[AGDATA_SCRATCH_VAL4].data = modifier;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_goal_turn_on_direct_path(anim_scratch_goal_id);

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        if (!tig_net_is_active()
            && critter_fatigue_current(obj) > 8) {
            turn_on_running(anim_scratch_goal_id);
        }
    } else {
        if (get_always_run(obj)) {
            turn_on_running(anim_scratch_goal_id);
        }
    }

    return true;
}

// 0x435450
bool anim_goal_use_item_on_obj(int64_t obj, int64_t target_obj, int64_t item_obj, unsigned int flags)
{
    AnimGoalData goal_data;

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (item_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_can_start_goal(obj, 4)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_USE_ITEM_ON_OBJECT)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;
    goal_data.params[AGDATA_FLAGS_DATA].data |= flags;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_goal_turn_on_direct_path(anim_scratch_goal_id);

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        if (!tig_net_is_active()
            && critter_fatigue_current(obj) > 8) {
            turn_on_running(anim_scratch_goal_id);
        }
    } else {
        if (get_always_run(obj)) {
            turn_on_running(anim_scratch_goal_id);
        }
    }

    return true;
}

// 0x4355F0
bool anim_goal_use_item_on_loc(int64_t obj, int64_t target_loc, int64_t item_obj, unsigned int flags)
{
    AnimGoalData goal_data;

    if (target_loc == 0) {
        return false;
    }

    if (item_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_can_start_goal(obj, 4)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_USE_ITEM_ON_TILE)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;
    goal_data.params[AGDATA_FLAGS_DATA].data |= flags;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x4356C0
bool anim_goal_pickup_item(int64_t obj, int64_t item_obj)
{
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (item_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_can_start_goal(obj, 4)) {
        return false;
    }

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC
        && !item_decay_process_is_enabled()) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_PICKUP_ITEM)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = item_obj;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_goal_turn_on_direct_path(anim_scratch_goal_id);

    return true;
}

// 0x4357B0
bool anim_goal_animate_stunned(int64_t obj)
{
    AnimGoalData goal_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!critter_is_active(obj)) {
        return false;
    }

    if (!anim_interrupt_all_goals_of_obj(obj, 4, false, false)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_ANIMATE_STUNNED)) {
        return false;
    }

    combat_critter_activate_combat_mode(obj);
    combat_turn_based_end_critter_turn(obj);

    goal_data.params[AGDATA_SCRATCH_VAL5].data = (20 - stat_level_get(obj, STAT_CONSTITUTION)) / 2;
    // CE (magic, milestone MG-2): a spell critical's long stun - one more
    // skipped turn (zsogg, 2026-10-11). Each skipped turn takes 3 off.
    if (crit_effects_take_long_stun(obj)) {
        goal_data.params[AGDATA_SCRATCH_VAL5].data += 3;
    }

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x435870
bool anim_goal_projectile(int64_t source_obj, int64_t missile_obj, tig_art_id_t missile_art_id, int a4, int a5, int64_t target_obj, int64_t target_loc, int64_t weapon_obj)
{
    AnimGoalData goal_data;
    int64_t loc;
    int rotation;
    AnimFxListEntry* v1;
    int projectile_speed;

    if (missile_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_interrupt_all_goals_of_obj(missile_obj, 4, false, true)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, missile_obj, AG_PROJECTILE)) {
        return false;
    }

    loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    rotation = combat_projectile_rot(loc, target_loc);
    mp_object_set_current_aid(missile_obj, combat_projectile_art_id_rotation_set(missile_art_id, rotation));

    // NOTE: Verified against 0x435924-0x43592A - original reads the source's current aid and passes it to tig_art_id_rotation_get, discarding the result; transcription faithful.
    tig_art_id_rotation_get(obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID));

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;
    goal_data.params[AGDATA_PARENT_OBJ].obj = source_obj;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = target_obj;
    goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
    goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
    goal_data.params[AGDATA_SCRATCH_VAL4].data = missile_art_id;

    if (weapon_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
        && animfx_id_get(&weapon_eye_candies, 5 * proto_get_description(weapon_obj) - 30143, &v1)) {
        projectile_speed = v1->projectile_speed;
    } else {
        projectile_speed = 0;
    }
    goal_data.params[AGDATA_SCRATCH_VAL5].data = projectile_speed;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x435A00
// Retargets a live AG_PROJECTILE goal (boomerang return path): new target
// tile and scratch object, path reset for the return trip.
bool anim_goal_projectile_retarget(int64_t proj_obj, int64_t a2, int64_t a3)
{
    AnimID anim_id;
    AnimRunInfo* run_info;

    if (proj_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_is_current_goal_type(proj_obj, AG_PROJECTILE, &anim_id)) {
        return false;
    }

    run_info = &(anim_run_info[anim_id.slot_num]);
    run_info->goals[0].params[AGDATA_TARGET_TILE].loc = a2;
    run_info->goals[0].params[AGDATA_SCRATCH_OBJ].obj = a3;
    run_info->goals[0].params[AGDATA_TARGET_OBJ].obj = run_info->goals[0].params[AGDATA_PARENT_OBJ].obj;
    run_info->path.flags = 1;
    run_info->flags &= ~0x10;

    return true;
}

// 0x435A90
bool anim_goal_knockdown(int64_t critter_obj)
{
    tig_art_id_t art_id;
    AnimGoalData goal_data;

    if (critter_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(critter_obj, OBJ_F_CURRENT_AID);
    if (tig_art_id_anim_get(art_id) == TIG_ART_ANIM_FALL_DOWN) {
        return false;
    }

    if (critter_is_dead(critter_obj)) {
        return false;
    }

    if (!anim_interrupt_all_goals_of_obj(critter_obj, 5, false, false)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, critter_obj, AG_KNOCK_DOWN)) {
        return false;
    }

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x435B30
bool anim_goal_make_knockdown(int64_t obj)
{
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!anim_interrupt_all_goals_of_obj(obj, PRIORITY_4, false, false)) {
        return false;
    }

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_FALL_DOWN);
    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        art_id = tig_art_id_frame_set(art_id, art_anim_data.num_frames - 1);
    } else {
        tig_debug_printf("Anim: anim_goal_make_knockdown: Failed to find Aid: %d, defaulting to frame 0!", art_id);
        art_id = tig_art_id_frame_set(art_id, 0);
    }

    object_set_current_aid(obj, art_id);

    return true;
}

// 0x435BD0
bool anim_goal_fidget(int64_t critter_obj)
{
    int obj_type;
    tig_art_id_t art_id;
    AnimGoalData goal_data;

    if (tig_net_is_active()) {
        return true;
    }

    if (critter_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(critter_obj, OBJ_F_TYPE);

    ASSERT(obj_type_is_critter(obj_type)); // 16405, "obj_type_is_critter(objType)"

    if (obj_type != OBJ_TYPE_NPC) {
        return false;
    }

    if (!anim_can_start_goal(critter_obj, 0)) {
        return false;
    }

    if (anim_is_fidgeting(critter_obj) || anim_is_busy(critter_obj, NULL)) {
        return false;
    }

    art_id = obj_field_int32_get(critter_obj, OBJ_F_CURRENT_AID);
    if (tig_art_id_anim_get(art_id) != TIG_ART_ANIM_STAND) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, critter_obj, AG_ANIM_FIDGET)) {
        return false;
    }

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x435CE0
// Starts a fidget goal for an idle, auto-animating NPC. Real-time only;
// turn-based combat has its own fidget handling.
bool anim_goal_auto_fidget(int64_t critter_obj)
{
    ASSERT(critter_obj != OBJ_HANDLE_NULL); // 16444, "critterObj != OBJ_HANDLE_NULL"
    if (critter_obj == OBJ_HANDLE_NULL) return false;

    if (!obj_type_is_critter(obj_field_int32_get(critter_obj, OBJ_F_TYPE))) {
        return false;
    }

    if ((obj_field_int32_get(critter_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_AUTO_ANIMATES) == 0) {
        return false;
    }

    if (combat_turn_based_is_active()) {
        return false;
    }

    anim_goal_fidget(critter_obj);

    return true;
}

// 0x435D70
bool anim_goal_unconceal(int64_t critter_obj)
{
    int obj_type;
    AnimGoalData goal_data;

    ASSERT(critter_obj != OBJ_HANDLE_NULL); // 16472, "critterObj != OBJ_HANDLE_NULL"

    if (critter_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(critter_obj, OBJ_F_TYPE);

    ASSERT(obj_type_is_critter(obj_type)); // 16478, "obj_type_is_critter(objType)"

    if (obj_type != OBJ_TYPE_NPC) {
        return false;
    }

    if (!critter_is_active(critter_obj)) {
        return false;
    }

    if (anim_is_busy(critter_obj, NULL)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, critter_obj, AG_UNCONCEAL)) {
        return false;
    }

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x435E60
bool anim_goal_wander(int64_t obj, int64_t tether_loc, int radius)
{
    int obj_type;
    int64_t source_obj;
    int64_t block_obj = OBJ_HANDLE_NULL;
    AnimGoalData goal_data;

    ASSERT(tether_loc != 0); // 16507, "tetherLoc != 0"
    ASSERT(radius > 0); // 16508, "radius > 0"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    ASSERT(obj_type_is_critter(obj_type)); // 16513, "obj_type_is_critter(objType)"

    if ((!combat_turn_based_is_active() || combat_turn_based_whos_turn_get() == obj)
        && radius > 0
        && obj_type_is_critter(obj_type)
        && critter_is_active(obj)
        && !anim_is_busy(obj, NULL)) {
        source_obj = obj;
        if (anim_find_blocking_critter(&source_obj, &block_obj)) {
            anim_goal_please_move(block_obj, source_obj);
        } else if (anim_goal_data_init(&goal_data, obj, AG_WANDER)) {
            goal_data.params[AGDATA_RANGE_DATA].data = radius;
            goal_data.params[AGDATA_SCRATCH_VAL1].data = (int)LOCATION_GET_X(tether_loc);
            goal_data.params[AGDATA_SCRATCH_VAL2].data = (int)LOCATION_GET_Y(tether_loc);
            if (anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
                return true;
            }
        }
    }

    return false;
}

// 0x436040
bool anim_goal_wander_seek_darkness(int64_t obj, int64_t tether_loc, int radius)
{
    int obj_type;
    int64_t source_obj;
    int64_t block_obj = OBJ_HANDLE_NULL;
    AnimGoalData goal_data;

    ASSERT(tether_loc != 0); // 16567, "tetherLoc != 0"
    ASSERT(radius > 0); // 16568, "radius > 0"

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    ASSERT(obj_type_is_critter(obj_type)); // 16573, "obj_type_is_critter(objType)"

    if ((!combat_turn_based_is_active() || combat_turn_based_whos_turn_get() == obj)
        && radius > 0
        && obj_type_is_critter(obj_type)
        && critter_is_active(obj)
        && !anim_is_busy(obj, NULL)) {
        source_obj = obj;
        if (anim_find_blocking_critter(&source_obj, &block_obj)) {
            anim_goal_please_move(block_obj, source_obj);
        } else if (anim_goal_data_init(&goal_data, obj, AG_WANDER_SEEK_DARKNESS)) {
            goal_data.params[AGDATA_RANGE_DATA].data = radius;
            goal_data.params[AGDATA_SCRATCH_VAL1].data = (int)LOCATION_GET_X(tether_loc);
            goal_data.params[AGDATA_SCRATCH_VAL2].data = (int)LOCATION_GET_Y(tether_loc);
            if (anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
                return true;
            }
        }
    }

    return false;
}

// 0x436220
// AG_USE_SKILL_ON special case for lockpicks. The goal duration is derived
// from the pick-pocket animation frame count and halved for apprentices.
bool anim_goal_pick_lock(int64_t obj, int64_t target_obj, int64_t item_obj)
{
    AnimGoalData goal_data;
    TigArtAnimData art_anim_data;
    tig_art_id_t art_id;
    int frame;
    int v1;

    if (!anim_can_start_goal(obj, 4)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, obj, AG_USE_SKILL_ON)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;
    goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;
    goal_data.params[AGDATA_SKILL_DATA].data = SKILL_PICK_LOCKS;
    goal_data.params[AGDATA_FLAGS_DATA].data |= 0x400;

    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    art_id = tig_art_id_anim_set(art_id, TIG_ART_ANIM_PICK_POCKET);

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        return false;
    }

    frame = art_anim_data.num_frames - 2;
    if (frame < 1) {
        frame = 1;
    }

    v1 = 10 * art_anim_data.fps / frame + 1;
    if (tech_skill_training_get(obj, TECH_SKILL_PICK_LOCKS) >= TRAINING_APPRENTICE) {
        v1 /= 2;
    }

    goal_data.params[AGDATA_SCRATCH_VAL3].data = v1;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    anim_goal_turn_on_direct_path(anim_scratch_goal_id);

    return true;
}

// 0x4363E0
bool anim_goal_please_move(int64_t obj, int64_t target_obj)
{
    int64_t tmp_obj;
    AnimGoalData goal_data;

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (target_obj == obj) {
        return false;
    }

    if (obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
        && (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC
            && obj > target_obj)) {
        tmp_obj = target_obj;
        target_obj = obj;
        obj = tmp_obj;
    }

    if (!anim_can_start_goal(target_obj, 0)) {
        return false;
    }

    if (anim_is_running_non_passive_goal(target_obj, NULL)) {
        return false;
    }

    if (!anim_goal_data_init(&goal_data, target_obj, AG_PLEASE_MOVE)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = obj;

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// 0x4364D0
// Asks idle critters sharing `obj`'s tile to PLEASE_MOVE, so the tile has
// room for `obj` to act. The lowest-handle idle NPC is the anchor; every
// other idle NPC moves off it, and in hosted multiplayer idle PCs do too.
void anim_make_room_for_critter(int64_t obj)
{
    int64_t loc;
    ObjectList critters;
    ObjectNode* obj_node;
    ObjectNode* new_node;
    ObjectNode* npc_head = NULL;
    ObjectNode* pc_head = NULL;
    int64_t v1 = OBJ_HANDLE_NULL;
    int64_t v2 = OBJ_HANDLE_NULL;
    int cnt = 0;

    ASSERT(obj != OBJ_HANDLE_NULL); // 16752, "sourceObj != OBJ_HANDLE_NULL"

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    object_list_location(loc, OBJ_TM_CRITTER, &critters);

    if (critters.head != NULL) {
        obj_node = critters.head;
        while (obj_node != NULL) {
            if (!critter_is_dead(obj_node->obj)
                && !anim_is_running_non_passive_goal(obj_node->obj, NULL)) {
                cnt++;
            }
            obj_node = obj_node->next;
        }

        if (cnt > 1) {
            obj_node = critters.head;
            while (obj_node != NULL) {
                if (!critter_is_dead(obj_node->obj)
                    && !anim_is_running_non_passive_goal(obj_node->obj, NULL)) {
                    new_node = object_node_create();
                    new_node->obj = obj_node->obj;

                    if (obj_field_int32_get(obj_node->obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
                        new_node->next = npc_head;
                        npc_head = new_node;

                        if (v1 == OBJ_HANDLE_NULL) {
                            if (v2 == OBJ_HANDLE_NULL || v2 > obj_node->obj) {
                                v2 = obj_node->obj;
                            }
                        }
                    } else {
                        new_node->next = pc_head;
                        pc_head = new_node;

                        if (v1 == OBJ_HANDLE_NULL) {
                            v1 = obj_node->obj;
                        } else {
                            if (v1 > obj_node->obj) {
                                v1 = obj_node->obj;
                            }
                        }
                    }
                }
                obj_node = obj_node->next;
            }

            if (v1 == OBJ_HANDLE_NULL) {
                v1 = v2;
            }

            obj_node = npc_head;
            while (obj_node != NULL) {
                if (v1 != obj_node->obj) {
                    anim_goal_please_move(v1, obj_node->obj);
                }
                obj_node = obj_node->next;
            }

            if (pc_head != NULL
                && tig_net_is_active()
                && tig_net_is_host()) {
                obj_node = pc_head;
                while (obj_node != NULL) {
                    anim_goal_please_move(v1, obj_node->obj);
                    obj_node = obj_node->next;
                }
            }

            while (pc_head != NULL) {
                obj_node = pc_head->next;
                object_node_destroy(pc_head);
                pc_head = obj_node;
            }

            while (npc_head != NULL) {
                obj_node = npc_head->next;
                object_node_destroy(npc_head);
                npc_head = obj_node;
            }
        }
    }

    object_list_destroy(&critters);
}

// 0x436720
// Finds an idle NPC on the same tile as `*source_obj_ptr` and returns it in
// `*block_obj_ptr`. When both are NPCs the lower handle becomes the source,
// matching the PLEASE_MOVE convention used by callers.
bool anim_find_blocking_critter(int64_t* source_obj_ptr, int64_t* block_obj_ptr)
{
    int64_t loc;
    ObjectList objects;
    ObjectNode* node;
    int cnt;
    bool rc;

    ASSERT(source_obj_ptr != NULL); // 16880, "pSourceObj != NULL"
    ASSERT((*source_obj_ptr) != OBJ_HANDLE_NULL); // 16881, "(*pSourceObj) != OBJ_HANDLE_NULL"
    ASSERT(block_obj_ptr != NULL); // 16882, "pBlockObj != NULL"

    if (source_obj_ptr == NULL
        || *source_obj_ptr == OBJ_HANDLE_NULL
        || anim_is_running_non_passive_goal(*source_obj_ptr, NULL)) {
        return false;
    }

    cnt = 0;
    loc = obj_field_int64_get(*source_obj_ptr, OBJ_F_LOCATION);
    object_list_location(loc, OBJ_TM_CRITTER, &objects);
    node = objects.head;
    while (node != NULL) {
        if (!critter_is_dead(node->obj)
            && !anim_is_running_non_passive_goal(node->obj, NULL)) {
            cnt++;
        }
        node = node->next;
    }

    rc = false;
    if (cnt > 1) {
        node = objects.head;
        while (node != NULL) {
            if (!critter_is_dead(node->obj)
                && !anim_is_running_non_passive_goal(node->obj, NULL)
                && obj_field_int32_get(node->obj, OBJ_F_TYPE) == OBJ_TYPE_NPC
                && node->obj != *source_obj_ptr) {
                break;
            }
            node = node->next;
        }

        if (node != NULL) {
            *block_obj_ptr = node->obj;

            if (obj_field_int32_get(*source_obj_ptr, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
                if (obj_field_int32_get(*block_obj_ptr, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
                    if (*source_obj_ptr < *block_obj_ptr) {
                        *block_obj_ptr = *source_obj_ptr;
                        *source_obj_ptr = node->obj;
                    }
                }
            } else {
                if (obj_field_int32_get(*block_obj_ptr, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                    if (*source_obj_ptr < *block_obj_ptr) {
                        *block_obj_ptr = *source_obj_ptr;
                        *source_obj_ptr = node->obj;
                    }
                }
            }

            rc = true;
        }
    }

    object_list_destroy(&objects);
    return rc;
}

// 0x436960
bool anim_goal_attempt_spread_out(int64_t obj, int64_t target_obj)
{
    AnimID anim_id;
    AnimGoalData goal_data;
    AnimRunInfo* run_info;

    if (!anim_can_start_goal(obj, 0)) {
        return false;
    }

    if (!anim_goal_data_init_no_interrupt(&goal_data, obj, AG_ATTEMPT_SPREAD_OUT)) {
        return false;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = target_obj;

    if (anim_is_current_goal_ex(obj, &goal_data, &anim_id)) {
        run_info = &(anim_run_info[anim_id.slot_num]);

        // NOTE: Verified against 0x4369F8 - original calls obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) and discards the result; transcription faithful.
        obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);

        if (run_info->cur_stack_data->type == AG_RUN_NEAR_OBJ) {
            return true;
        }

        if (run_info->cur_stack_data->type != AG_MOVE_NEAR_OBJ
            && run_info->cur_stack_data->type != AG_ATTEMPT_MOVE_NEAR) {
            return true;
        }

        if (location_dist(obj_field_int64_get(obj, OBJ_F_LOCATION), obj_field_int64_get(target_obj, OBJ_F_LOCATION)) <= 7) {
            return true;
        }
    }

    if (!anim_interrupt_all_goals_of_obj(obj, 3, false, false)) {
        return false;
    }

    if (!anim_goal_add(&goal_data, &anim_scratch_goal_id)) {
        return false;
    }

    return true;
}

// NOTE: Passes AnimID by value.
//
// 0x436AB0
void turn_on_running(AnimID anim_id)
{
    AnimRunInfo* run_info;
    char str[ANIM_ID_STR_SIZE];
    int64_t obj;
    Packet8 pkt;

    if (!anim_id_to_run_info(&anim_id, &run_info)) {
        anim_id_to_str(&anim_id, str);
        tig_debug_printf("Anim: turn_on_running: could not turn animID into a AnimRunInfo %s.\n", str);
        return;
    }

    obj = run_info->goals[0].params[AGDATA_SELF_OBJ].obj;
    if (critter_encumbrance_level_get(obj) < ENCUMBRANCE_LEVEL_SIGNIFICANT) {
        if (tig_net_is_active()) {
            if (tig_net_is_host()) {
                run_info->flags |= 0x40;
                sub_44D0C0(run_info);
            }

            anim_modify_data_init(&(pkt.modify_data));
            pkt.modify_data.id = anim_id;
            pkt.type = 8;
            pkt.modify_data.flags = run_info->flags;
            pkt.modify_data.path_flags = run_info->path.flags;
            pkt.modify_data.field_14 = -1;
            pkt.modify_data.path_curr = run_info->path.curr;
            pkt.modify_data.location = obj_field_int64_get(obj, OBJ_F_LOCATION);
            pkt.modify_data.current_aid = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
            pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
            pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
            pkt.field_40 = run_info->extra_target_tile;

            if (tig_net_is_host()) {
                run_info->id.field_8++;
            }

            tig_net_send_app_all(&pkt, sizeof(pkt));
        } else {
            run_info->flags |= 0x40;
        }
    }
}

// 0x436C20
// Scratch-goal wrapper for turn_on_running.
void anim_scratch_goal_turn_on_running(void)
{
    turn_on_running(anim_scratch_goal_id);
}

// NOTE: Passes AnimID by value.
//
// 0x436C50
// Sets the stand-in-place flag documented in the manual as SHIFT-click:
// "Stand in place (don't walk when attacking, etc.)".
void anim_goal_turn_on_stand_in_place(AnimID anim_id)
{
    turn_on_flags(anim_id, 0x100, 0);
}

// 0x436C80
// Scratch-goal wrapper for anim_goal_turn_on_stand_in_place.
void anim_scratch_goal_turn_on_stand_in_place(void)
{
    anim_goal_turn_on_stand_in_place(anim_scratch_goal_id);
}

// NOTE: Passes AnimID by value.
//
// 0x436CB0
// Marks a moving goal to stop at the next blocked tile. Only sent in
// networked games; local single-player movement handles blockers elsewhere.
void anim_goal_turn_on_stop_at_blockers(AnimID anim_id)
{
    if (tig_net_is_active()) {
        turn_on_flags(anim_id, 0x400, 0);
    }
}

// 0x436CF0
// Scratch-goal wrapper for anim_goal_turn_on_stop_at_blockers.
void anim_scratch_goal_turn_on_stop_at_blockers(void)
{
    anim_goal_turn_on_stop_at_blockers(anim_scratch_goal_id);
}

// 0x436D20
// Scratch-goal wrapper for turn_on_flags.
void anim_scratch_goal_turn_on_flags(unsigned int flags1, unsigned int flags2)
{
    turn_on_flags(anim_scratch_goal_id, flags1, flags2);
}

// NOTE: Passes AnimID by value.
//
// 0x436D50
void turn_on_flags(AnimID anim_id, unsigned int flags1, unsigned int flags2)
{
    AnimRunInfo* run_info;
    char str[ANIM_ID_STR_SIZE];
    int64_t obj;
    Packet8 pkt;

    if (!anim_id_to_run_info(&anim_id, &run_info)) {
        anim_id_to_str(&anim_id, str);
        tig_debug_printf("Anim: turn_on_flags: could not turn animID into a AnimRunInfo %s.\n", str);
        return;
    }

    if (tig_net_is_active()) {
        obj = run_info->goals[0].params[AGDATA_SELF_OBJ].obj;

        if (tig_net_is_host()) {
            run_info->flags |= flags1;
            sub_44D0C0(run_info);
        }

        anim_modify_data_init(&(pkt.modify_data));
        pkt.modify_data.id = anim_id;
        pkt.type = 8;
        pkt.modify_data.flags = run_info->flags;
        pkt.modify_data.path_flags = run_info->path.flags;
        pkt.modify_data.field_14 = -1;
        pkt.modify_data.path_curr = run_info->path.curr;
        pkt.modify_data.location = obj_field_int64_get(obj, OBJ_F_LOCATION);
        pkt.modify_data.current_aid = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        pkt.offset_x = obj_field_int32_get(obj, OBJ_F_OFFSET_X);
        pkt.offset_y = obj_field_int32_get(obj, OBJ_F_OFFSET_Y);
        pkt.field_40 = run_info->extra_target_tile;

        if (tig_net_is_host()) {
            run_info->id.field_8++;
        }

        tig_net_send_app_all(&pkt, sizeof(pkt));
    } else {
        run_info->flags |= flags1;
        run_info->path.flags |= flags2;
    }
}

// NOTE: Passes AnimID by value.
//
// 0x436ED0
// Sets the direct-path flag used by object interactions: path creation
// falls back to a straight line instead of route-finding around obstacles.
void anim_goal_turn_on_direct_path(AnimID anim_id)
{
    turn_on_flags(anim_id, 0x4000, 0);
}

// 0x436F30
void notify_speed_recalc(AnimID* anim_id)
{
    AnimRunInfo* run_info;
    char str[ANIM_ID_STR_SIZE];

    if (!anim_id_to_run_info(anim_id, &run_info)) {
        anim_id_to_str(anim_id, str);
        tig_debug_printf("Anim: notify_speed_recalc: could not turn animID into a AnimRunInfo %s.\n", str);
        return;
    }

    // NOTE: Verified at 0x436F6E-0x436F8E - the original also flags the
    // master-table slot entry rather than the obtained run_info.
    anim_run_info[anim_id->slot_num].flags |= 0x80;
}

// 0x436FA0
void anim_speed_recalc(int64_t obj)
{
    AnimID anim_id;

    if (obj != OBJ_HANDLE_NULL
        && anim_is_busy(obj, &anim_id)) {
        notify_speed_recalc(&anim_id);
    }
}

// 0x4372B0
// Rewrites AGDATA_PARENT_OBJ to `a2` in every active goal slot for `a1`.
// Used when a summoned object is attached to its summoner.
bool anim_goal_parent_obj_set_all(int64_t a1, int64_t a2)
{
    int index;
    AnimRunInfo* run_info;
    int goal_index;

    index = anim_find_first(a1);
    if (index == -1) {
        return false;
    }

    while (index != -1) {
        run_info = &(anim_run_info[index]);
        goal_index = run_info->current_goal;
        if (goal_index >= 0) {
            // NOTE: Verified at 0x4372F2-0x4372FA - the original's do/while
            // also always walks goals[0] of each entry.
            goal_index++;
            do {
                run_info->goals[0].params[AGDATA_PARENT_OBJ].obj = a2;
                goal_index--;
            } while (goal_index != 0);
        }

        index = anim_find_next(index, a1);
    }

    return true;
}

// 0x4373A0
int num_goal_subslots_in_use(AnimID* anim_id)
{
    AnimRunInfo* run_info;
    char str[ANIM_ID_STR_SIZE];

    if (!anim_id_to_run_info(anim_id, &run_info)) {
        anim_id_to_str(anim_id, str);
        tig_debug_printf("Anim: num_goal_subslots_in_use: could not turn animID into a AnimRunInfo %s.\n", str);
        return 0;
    }

    return run_info->current_goal;
}

// 0x4373F0
bool is_anim_forever(AnimID* anim_id)
{
    AnimRunInfo* run_info;
    char str[ANIM_ID_STR_SIZE];

    if (!anim_id_to_run_info(anim_id, &run_info)) {
        anim_id_to_str(anim_id, str);
        tig_debug_printf("Anim: is_anim_forever: could not turn animID into a AnimRunInfo %s.\n", str);
        return false;
    }

    return run_info->cur_stack_data->type == 2
        || run_info->cur_stack_data->type == 1;
}

// 0x437460
// Initializes a network AGModifyData packet body to its defaults.
void anim_modify_data_init(AGModifyData* modify_data)
{
    ASSERT(modify_data != NULL); // pAGModifyData != NULL

    anim_id_init(&(modify_data->id));
    modify_data->loc = 0;
    modify_data->location = 0;
    modify_data->flags = 0;
    modify_data->path_flags = 0;
    modify_data->field_14 = -1;
    modify_data->current_aid = TIG_ART_ID_INVALID;
    modify_data->path_curr = -1;
}

// 0x4374C0
void mp_anim_modify(void)
{
    // TODO: Incomplete.
}

// 0x4377C0
bool anim_play_weapon_fx(CombatContext* combat, int64_t source_obj, int64_t target_obj, AnimWeaponEyeCandyType which)
{
    int obj_type;
    int64_t weapon_obj;
    AnimFxNode node;
    tig_art_id_t art_id;
    int fx_id;

    ASSERT(which >= ANIM_WEAPON_EYE_CANDY_TYPE_POWER_GATHER); // 17560, "whichIdx >= ANIM_WEAPON_EYE_CANDY_POWER_GATHER"
    ASSERT(which < ANIM_WEAPON_EYE_CANDY_TYPE_COUNT); // 17561, "whichIdx < ANIMFX_WEAPON_TYPE_COUNT"

    if (source_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    obj_type = obj_field_int32_get(source_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(obj_type)) {
        if (combat != NULL
            && (combat->flags & 0x40000) != 0) {
            if (combat->weapon_obj == OBJ_HANDLE_NULL
                || obj_field_int32_get(combat->weapon_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON) {
                return false;
            }

            weapon_obj = combat->weapon_obj;
        } else {
            weapon_obj = item_wield_get(source_obj, ITEM_INV_LOC_WEAPON);
        }
    } else if (obj_type == OBJ_TYPE_WEAPON) {
        weapon_obj = source_obj;
    } else {
        return false;
    }

    if (weapon_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    art_id = obj_field_int32_get(source_obj, OBJ_F_CURRENT_AID);
    fx_id = 5 * (proto_get_description(weapon_obj) - 6029);
    animfx_node_init(&weapon_eye_candies, &node, target_obj, -1, fx_id + which);
    node.rotation = tig_art_id_rotation_get(art_id);
    node.animate = true;
    node.max_simultaneous_effects = 2;
    if (!animfx_add(&node)) {
        return false;
    }

    if (which == ANIM_WEAPON_EYE_CANDY_TYPE_HIT) {
        // NOTE: Verified at 0x437922/0x437929 (cmp which,3 / add esi,0x5) -
        // the original also passes fx_id + 5 here.
        animfx_node_init(&weapon_eye_candies, &node, target_obj, -1, fx_id + 5);
        node.animate = true;
        node.max_simultaneous_effects = 0;
        node.flags |= ANIMFX_PLAY_STACK;
        animfx_add(&node);
    }

    return true;
}

// 0x437980
// Empty in the original. Called after a PC teleport changes maps; kept as
// the binary's address anchor for that map-transition hook.
void anim_notify_pc_teleported(void)
{
}

// 0x437990
// Frames per second for the critter's current art at the given speed, with
// per-anim/weapon/body-type ramps. Non-critters use the art's own fps.
int anim_get_animation_fps(int64_t obj, tig_art_id_t art_id, int speed)
{
    TigArtAnimData art_anim_data;
    int fps;
    int art_type;
    int v1;
    int body_type;
    int anim;
    int weapon_type;
    int v12;
    int v13;
    int v14;

    if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
        fps = art_anim_data.fps;
    } else {
        fps = 10;
    }

    art_type = tig_art_type(art_id);
    if (art_type != TIG_ART_TYPE_CRITTER
        && art_type != TIG_ART_TYPE_MONSTER
        && art_type != TIG_ART_TYPE_UNIQUE_NPC) {
        return fps;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & (OSF_BODY_OF_WATER | OSF_BODY_OF_FIRE | OSF_BODY_OF_EARTH | OSF_BODY_OF_AIR)) != 0) {
        return fps;
    }

    v1 = 0;
    if (art_type == TIG_ART_TYPE_CRITTER) {
        body_type = tig_art_critter_id_body_type_get(art_id);
        if (body_type == TIG_ART_CRITTER_BODY_TYPE_DWARF
            || body_type == TIG_ART_CRITTER_BODY_TYPE_HALFLING) {
            v1 = 4;
        }
    }

    anim = tig_art_id_anim_get(art_id);
    weapon_type = tig_art_critter_id_weapon_get(art_id);
    switch (anim) {
    case TIG_ART_ANIM_WALK:
        v12 = v1 + 17;
        v13 = v1 + 6;
        v14 = v1 + 30;
        break;
    case TIG_ART_ANIM_STEALTH_WALK:
        v12 = v1 + 10;
        v13 = v1 + 4;
        v14 = v1 + 16;
        break;
    case TIG_ART_ANIM_RUN:
        v12 = v1 + 20;
        v13 = v1 + 8;
        v14 = v1 + 30;
        break;
    case TIG_ART_ANIM_ATTACK:
        switch (weapon_type) {
        case TIG_ART_WEAPON_TYPE_UNARMED:
        case TIG_ART_WEAPON_TYPE_SWORD:
            v12 = 15;
            v13 = 7;
            v14 = 23;
            break;
        case TIG_ART_WEAPON_TYPE_DAGGER:
        case TIG_ART_WEAPON_TYPE_BOW:
            v12 = 10;
            v13 = 6;
            v14 = 14;
            break;
        case TIG_ART_WEAPON_TYPE_AXE:
        case TIG_ART_WEAPON_TYPE_MACE:
        case TIG_ART_WEAPON_TYPE_TWO_HANDED_SWORD:
            v12 = 14;
            v13 = 8;
            v14 = 20;
            break;
        case TIG_ART_WEAPON_TYPE_PISTOL:
            v12 = 8;
            v13 = 3;
            v14 = 12;
            break;
        case TIG_ART_WEAPON_TYPE_RIFLE:
            v12 = 6;
            v13 = 3;
            v14 = 10;
            break;
        case TIG_ART_WEAPON_TYPE_STAFF:
            v12 = 12;
            v13 = 8;
            v14 = 16;
            break;
        default:
            v12 = -1;
            break;
        }
        break;
    case TIG_ART_ANIM_ATTACK_LOW:
        switch (weapon_type) {
        case TIG_ART_WEAPON_TYPE_UNARMED:
            v12 = 13;
            v13 = 8;
            v14 = 18;
            break;
        case TIG_ART_WEAPON_TYPE_DAGGER:
        case TIG_ART_WEAPON_TYPE_BOW:
        case TIG_ART_WEAPON_TYPE_STAFF:
            v12 = 10;
            v13 = 6;
            v14 = 14;
            break;
        case TIG_ART_WEAPON_TYPE_SWORD:
            v12 = 14;
            v13 = 8;
            v14 = 20;
            break;
        case TIG_ART_WEAPON_TYPE_AXE:
        case TIG_ART_WEAPON_TYPE_MACE:
            v12 = 11;
            v13 = 7;
            v14 = 15;
            break;
        case TIG_ART_WEAPON_TYPE_PISTOL:
        case TIG_ART_WEAPON_TYPE_RIFLE:
            v12 = 6;
            v13 = 3;
            v14 = 10;
            break;
        case TIG_ART_WEAPON_TYPE_TWO_HANDED_SWORD:
            v12 = 13;
            v13 = 9;
            v14 = 17;
            break;
        default:
            v12 = -1;
            break;
        }
        break;
    default:
        v12 = -1;
        break;
    }

    if (v12 != -1) {
        if (speed < 8) {
            fps = v13 + speed * (v12 - v13) / 8;
        } else if (speed > 8) {
            if (speed < 30) {
                fps = v12 + speed * (v14 - v12) / 30;
            } else {
                fps = v14;
            }
        }
    }

    return fps;
}

// 0x437C50
// Location reached after following the first `end` rotations from the path's
// starting tile. Debug/desync reporting helper.
bool anim_path_location_after_steps(AnimRunInfo* run_info, int end, int64_t* x, int64_t* y)
{
    int64_t loc;
    int idx;

    if (run_info != NULL
        && run_info->anim_obj != OBJ_HANDLE_NULL
        && run_info->path.field_E8 != 0) {
        loc = run_info->path.field_E8;
        for (idx = 0; idx <= end; idx++) {
            location_in_dir(loc, run_info->path.rotations[idx], &loc);
        }
    }

    if (x != NULL && y != NULL) {
        *x = LOCATION_GET_X(loc);
        *y = LOCATION_GET_Y(loc);
    }

    return true;
}

// 0x437CF0
// Deliberately left as `sub_`: a constant-true stub with no CE caller. See
// the goal-table constant-stub note above.
bool sub_437CF0(int a1, int a2, int a3)
{
    (void)a1;
    (void)a2;
    (void)a3;

    return true;
}

// 0x437D00
// Pops the run's current goal outside the normal interpreter flow. CE has
// no caller; the original invokes it when resetting attempt-move goals.
void anim_pop_current_goal(AnimRunInfo* run_info)
{
    AnimGoalData* goal_data;
    AnimGoalNode* goal_node;

    goal_data = &(run_info->goals[run_info->current_goal]);
    if (run_info->current_goal == 0) {
        run_info->flags |= 0x2;
    }

    if (goal_data->type >= 0 && goal_data->type < 87) {
        goal_node = anim_goal_nodes[goal_data->type];
        if (goal_node->subnodes[14].func != NULL
            && anim_recover_handles(run_info, &(goal_node->subnodes[14]))) {
            goal_node->subnodes[14].func(run_info);
        }

        anim_active_goal_count_decrement(run_info, goal_node);
    }

    run_info->current_goal--;
    if (run_info->current_goal >= 0) {
        run_info->cur_stack_data = &(run_info->goals[run_info->current_goal]);
    }

    run_info->current_state = 0;
}
