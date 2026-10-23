#ifndef ARCANUM_GAME_ANIM_PRIVATE_H_
#define ARCANUM_GAME_ANIM_PRIVATE_H_

#include "game/context.h"
#include "game/object.h"
#include "game/timeevent.h"

#define ASSERT(x)                                                                      \
    if (!(x)) {                                                                        \
        tig_debug_printf("ASSERTION: File:%s, Line:%d: %s\n", __FILE__, __LINE__, #x); \
        anim_stats();                                                                  \
    }

typedef enum AnimGoal {
    AG_ANIMATE,
    AG_ANIMATE_LOOP,
    AG_ANIM_FIDGET,
    AG_MOVE_TO_TILE,
    AG_RUN_TO_TILE,
    AG_ATTEMPT_MOVE,
    AG_MOVE_TO_PAUSE,
    AG_MOVE_NEAR_TILE,
    AG_MOVE_NEAR_OBJ,
    AG_MOVE_STRAIGHT,
    AG_ATTEMPT_MOVE_STRAIGHT,
    AG_OPEN_DOOR,
    AG_ATTEMPT_OPEN_DOOR,
    AG_UNLOCK_DOOR,
    AG_JUMP_WINDOW,
    AG_PICKUP_ITEM,
    AG_ATTEMPT_PICKUP,
    AG_PICKPOCKET,
    AG_ATTACK,
    AG_ATTEMPT_ATTACK,
    AG_KILL,
    AG_TALK,
    AG_PICK_WEAPON,
    AG_CHASE,
    AG_FOLLOW,
    AG_FLEE,
    AG_THROW_SPELL,
    AG_ATTEMPT_SPELL,
    AG_SHOOT_SPELL,
    AG_HIT_BY_SPELL,
    AG_HIT_BY_WEAPON,
    AG_DYING,
    AG_DESTROY_OBJ,
    AG_USE_SKILL_ON,
    AG_ATTEMPT_USE_SKILL_ON,
    AG_SKILL_CONCEAL,
    AG_PROJECTILE,
    AG_THROW_ITEM,
    AG_USE_OBJECT,
    AG_USE_ITEM_ON_OBJECT,
    AG_USE_ITEM_ON_OBJECT_WITH_SKILL,
    AG_USE_ITEM_ON_TILE,
    AG_USE_ITEM_ON_TILE_WITH_SKILL,
    AG_KNOCKBACK,
    AG_FLOATING,
    AG_EYE_CANDY,
    AG_EYE_CANDY_REVERSE,
    AG_EYE_CANDY_CALLBACK,
    AG_EYE_CANDY_REVERSE_CALLBACK,
    AG_CLOSE_DOOR,
    AG_ATTEMPT_CLOSE_DOOR,
    AG_ANIMATE_REVERSE,
    AG_MOVE_AWAY_FROM_OBJ,
    AG_ROTATE,
    AG_UNCONCEAL,
    AG_RUN_NEAR_TILE,
    AG_RUN_NEAR_OBJ,
    AG_ANIMATE_STUNNED,
    AG_EYE_CANDY_END_CALLBACK,
    AG_EYE_CANDY_REVERSE_END_CALLBACK,
    AG_ANIMATE_KNEEL_MAGIC_HANDS,
    AG_ATTEMPT_MOVE_NEAR,
    AG_KNOCK_DOWN,
    AG_ANIM_GET_UP,
    AG_ATTEMPT_MOVE_STRAIGHT_KNOCKBACK,
    AG_WANDER,
    AG_WANDER_SEEK_DARKNESS,
    AG_USE_PICKLOCK_SKILL_ON,
    AG_PLEASE_MOVE,
    AG_ATTEMPT_SPREAD_OUT,
    AG_ANIMATE_DOOR_OPEN,
    AG_ANIMATE_DOOR_CLOSED,
    AG_PEND_CLOSING_DOOR,
    AG_THROW_SPELL_FRIENDLY,
    AG_ATTEMPT_SPELL_FRIENDLY,
    AG_EYE_CANDY_FIRE_DMG,
    AG_EYE_CANDY_REVERSE_FIRE_DMG,
    AG_ANIMATE_LOOP_FIRE_DMG,
    AG_ATTEMPT_MOVE_STRAIGHT_SPELL,
    AG_MOVE_NEAR_OBJ_COMBAT,
    AG_ATTEMPT_MOVE_NEAR_COMBAT,
    AG_USE_CONTAINER,
    AG_THROW_SPELL_W_CAST_ANIM,
    AG_ATTEMPT_SPELL_W_CAST_ANIM,
    AG_THROW_SPELL_W_CAST_ANIM_2NDARY,
    AG_BACK_OFF_FROM,
    AG_ATTEMPT_USE_PICKPOCKET_SKILL_ON,
    ANIM_GOAL_MAX,
} AnimGoal;

typedef enum AgDataType {
    AGDATATYPE_INT,
    AGDATATYPE_OBJ,
    AGDATATYPE_LOC,
    AGDATATYPE_SOUND,
} AgDataType;

struct AnimRunInfo;

/**
 * Flow codes.
 *
 * A goal's sub-node list is a little program. Each sub-node names a callback
 * plus two `(state, delay)` pairs, and `anim_timeevent_process` picks the pair
 * matching the callback's return value: `success_*` when it returns true,
 * `failure_*` when it returns false.
 *
 * A state word with an empty top byte is a plain 1-based sub-node number - the
 * interpreter jumps there with `current_state = state - 1`. Otherwise the top
 * byte holds these flags, tested in the order they are listed here.
 */

/**
 * Reset to sub-node 1 and stop processing this tick.
 *
 * On its own this restarts the goal after `delay`; the pop and terminate codes
 * fold it in to end the tick once the stack has been unwound. A pop that finds
 * a goal underneath clears the stop again and keeps interpreting the parent.
 */
#define ANIM_FLOW_YIELD 0x10000000

/**
 * Pop one goal off the stack - only when combined with `ANIM_FLOW_YIELD`, the
 * interpreter tests both bits together.
 */
#define ANIM_FLOW_POP 0x20000000

/**
 * Pop two more goals, on top of the one `ANIM_FLOW_POP` removes. Only tested
 * together with the full `ANIM_FLOW_POP_GOAL` pattern; used once, by
 * `attempt_move_near`.
 */
#define ANIM_FLOW_POP_EXTRA 0x08000000

/**
 * Push the goal whose type is in the low 12 bits, and run it from its first
 * sub-node. Combined with `ANIM_FLOW_POP_GOAL` this replaces the current goal
 * rather than nesting under it.
 */
#define ANIM_FLOW_PUSH 0x40000000

/**
 * Unwind the whole stack: run every goal's cleanup callback (sub-node 15) from
 * the top down and end the run. Only when combined with `ANIM_FLOW_YIELD`,
 * again tested as a pair.
 */
#define ANIM_FLOW_KILL 0x80000000

/**
 * Modifiers read by the pop path only (`anim_pop_goal`), so they do nothing on a
 * code that does not also pop.
 *
 * - `KEEP_PATH` leaves the run's path and its in-flight flags alone; without it
 *   the pop zeroes `path.maxPathLength` and clears run flags 0x83C.
 * - `RESET_PATH` additionally forces `path.flags` to 0x01.
 * - `NO_CLEANUP` skips the popped goal's cleanup callback.
 */
#define ANIM_FLOW_KEEP_PATH 0x01000000
#define ANIM_FLOW_RESET_PATH 0x02000000
#define ANIM_FLOW_NO_CLEANUP 0x04000000

/**
 * The patterns the interpreter matches, rather than the raw bits: a code is a
 * pop only if `ANIM_FLOW_POP` and `ANIM_FLOW_YIELD` are both set, and a
 * terminate only if `ANIM_FLOW_KILL` and `ANIM_FLOW_YIELD` are both set.
 */
#define ANIM_FLOW_POP_GOAL (ANIM_FLOW_POP | ANIM_FLOW_YIELD)
#define ANIM_FLOW_POP_3_GOALS (ANIM_FLOW_POP_EXTRA | ANIM_FLOW_POP_GOAL)
#define ANIM_FLOW_REPLACE_GOAL (ANIM_FLOW_PUSH | ANIM_FLOW_POP_GOAL)
#define ANIM_FLOW_TERMINATE (ANIM_FLOW_KILL | ANIM_FLOW_YIELD)

/**
 * A state word is a sub-node number when none of these bits are set; the goal
 * type a push carries lives in the low 12 bits.
 */
#define ANIM_FLOW_MASK 0xFF000000
#define ANIM_FLOW_GOAL_MASK 0xFFF

/**
 * Delays, in milliseconds, or one of these sentinels.
 *
 * `ANIM_DELAY_NONE` continues straight into the next sub-node in the same tick;
 * anything else ends the tick and reschedules the run that far ahead.
 */
#define ANIM_DELAY_NONE 0
#define ANIM_DELAY_PAUSE_TIME (-2) // `run_info->pause_time`
#define ANIM_DELAY_CALLBACK (-3) // whatever the callback left in `anim_callback_delay`
#define ANIM_DELAY_RANDOM (-4) // 0-300ms, and no delay at all on a network client

typedef struct AnimGoalSubNode {
    /* 0000 */ bool (*func)(struct AnimRunInfo* run_info);

    /**
     * Which of the goal's own `AnimGoalData` slots feed `run_info->params[0]`
     * and `[1]` before the callback runs: an `AGDATA_*` index, or -1 for none.
     */
    /* 0004 */ int params[2];

    /**
     * A constant handed to the callback as `run_info->params[2]`, so that one
     * callback can serve several cells - the animation number for
     * `AGbeginAnim*`, the goal type for a `anim_goal_data_init`, a skill or
     * range for the skill checks. -1 where the callback ignores it.
     */
    /* 000C */ int literal_param;

    /* 0010 */ int failure_state;
    /* 0014 */ int failure_delay;
    /* 0018 */ int success_state;
    /* 001C */ int success_delay;
} AnimGoalSubNode;

/**
 * Set on a goal node whose sub-node program never reaches an end on its own -
 * the looping animations and floating text. `anim_goal_is_endless` reports
 * these (and any run whose `AGDATA_FLAGS_DATA` carries 0x80), and they are kept
 * out of `animNumActiveGoals`. See `anim_goal_is_endless`.
 */
#define ANIM_GOAL_NODE_ENDLESS 0x01

typedef struct AnimGoalNode {
    /* 0000 */ int num_subnodes;
    /* 0004 */ int priority_level;
    /**
     * Goal runs alongside the object's other animations, outside the priority
     * system: starting it does not interrupt them (`anim_goal_data_init_ex`
     * sweeps at PRIORITY_NONE), nothing below PRIORITY_HIGHEST interrupts it
     * (`anim_interrupt` reports success without touching it), and it does not
     * make the object busy (`anim_is_busy`) or count toward
     * `animNumActiveGoals`. Set for the eye-candy goals, floating text, door
     * swings, spread-out nudges, and friendly-spell casts.
     */
    /* 0008 */ int concurrent;

    /**
     * The goal only plays something out - an animation, a reaction, a door
     * swinging, floating text - as opposed to running an errand that has a
     * destination or a target (moving, attacking, using, talking, casting).
     *
     * Two consumers, and they agree on the split: `anim_break_nodes_to_map`
     * carries passive goals to the next map for a teleporting object and
     * interrupts the rest, whose paths and targets would not survive the move;
     * and `anim_is_running_non_passive_goal` reports a critter as occupied only
     * when its bottom goal is not passive, which is what keeps
     * `anim_goal_please_move` and the spread-out sweep from nudging a critter
     * that is merely mid-animation.
     */
    /* 000C */ int passive;

    /* 0010 */ int flags;

    /**
     * Goal types that count as this goal when looking for a running one, so
     * that "is this critter already moving there" answers yes for
     * `AG_MOVE_TO_TILE`, `AG_MOVE_NEAR_TILE` and `AG_RUN_TO_TILE` alike.
     * Terminated by -1.
     */
    /* 0014 */ int equivalent_goals[3];

    /**
     * The goal's program, indexed by `run_info->current_state`. Sub-node 15
     * (`subnodes[14]`) is the cleanup callback every unwind path runs; the
     * others are reached only by the flow codes above.
     */
    /* 0020 */ AnimGoalSubNode subnodes[15];
} AnimGoalNode;

typedef struct AnimID {
    int slot_num;
    int unique_id;
    int field_8;
} AnimID;

// Serializeable.
static_assert(sizeof(AnimID) == 0xC, "wrong size");

typedef struct AGModifyData {
    /* 0000 */ AnimID id;
    /* 000C */ int flags;
    /* 0010 */ int path_flags;
    /* 0014 */ int field_14;
    /* 0018 */ int64_t loc;
    /* 0020 */ int64_t location;
    /* 0028 */ tig_art_id_t current_aid;
    /* 002C */ int path_curr;
} AGModifyData;

// Serializeable.
static_assert(sizeof(AGModifyData) == 0x30, "wrong size");

typedef union AnimRunInfoParam {
    int64_t obj;
    int64_t loc;
    int data;
} AnimRunInfoParam;

typedef enum AgData {
    AGDATA_SELF_OBJ,
    AGDATA_TARGET_OBJ,
    AGDATA_BLOCK_OBJ,
    AGDATA_SCRATCH_OBJ,
    AGDATA_PARENT_OBJ,
    AGDATA_TARGET_TILE,
    AGDATA_ORIGINAL_TILE,
    AGDATA_RANGE_DATA,
    AGDATA_ANIM_ID,
    AGDATA_ANIM_ID_PREVIOUS,
    AGDATA_ANIM_DATA,
    AGDATA_SPELL_DATA,
    AGDATA_SKILL_DATA,
    AGDATA_FLAGS_DATA,
    AGDATA_SCRATCH_VAL1,
    AGDATA_SCRATCH_VAL2,
    AGDATA_SCRATCH_VAL3,
    AGDATA_SCRATCH_VAL4,
    AGDATA_SCRATCH_VAL5,
    AGDATA_SCRATCH_VAL6,
    AGDATA_SOUND_HANDLE,
    AGDATA_COUNT,
} AgData;

#define AGDATA_SELF_TILE 31
#define AGDATA_NULL_OBJ 33
#define AGDATA_FORCE_TARGET_TILE 34

typedef struct AnimGoalData {
    /* 0000 */ int type;
    /* 0004 */ int padding_4;
    /* 0008 */ AnimRunInfoParam params[AGDATA_COUNT];
    /* 00B0 */ ObjectLocator field_B0[5];
} AnimGoalData;

// Serializeable.
static_assert(sizeof(AnimGoalData) == 0x178, "wrong size");

typedef struct AnimPath {
    /* 0000 */ unsigned int flags;
    /* 0004 */ int8_t rotations[200];
    /* 00CC */ int field_CC;
    /* 00D0 */ int baseRot; // TODO: Normalize to snake_case.
    /* 00D4 */ int curr;
    /* 00D8 */ int max;
    /* 00DC */ int subsequence;
    /* 00E0 */ int maxPathLength; // TODO: Normalize to snake_case.
    /* 00E4 */ int absMaxPathLength; // TODO: Normalize to snake_case.
    /* 00E8 */ int64_t field_E8;
    /* 00F0 */ int64_t field_F0;
} AnimPath;

// Serializeable.
static_assert(sizeof(AnimPath) == 0xF8, "wrong size");

typedef struct AnimRunInfo {
    /* 0000 */ AnimID id;
    /* 000C */ unsigned int flags;
    /* 0010 */ int current_state;
    /* 0014 */ int path_attached_to_stack_index;
    /* 0018 */ DateTime next_ping_time;
    /* 0020 */ int64_t anim_obj;
    /* 0028 */ int64_t extra_target_tile;
    /* 0030 */ int current_goal;
    /* 0038 */ AnimGoalData goals[8];
    /* 0BF0 */ AnimGoalData* cur_stack_data;
    /* 0C00 */ AnimPath path;
    /* 0CF8 */ DateTime pause_time;
    /* 0D00 */ AnimRunInfoParam params[3];
    /* 0D18 */ int field_D18;
    /* 0D1C */ int field_D1C;
    /* 0D20 */ int field_D20;
    /* 0D24 */ int field_D24;
    /* 0D28 */ int field_D28;
    /* 0D2C */ int field_D2C;
    /* 0D30 */ int field_D30;
    /* 0D34 */ int field_D34;
} AnimRunInfo;

extern const char* anim_goal_names[];
extern int anim_current_run_index;
extern int anim_goal_data_param_types[AGDATA_COUNT];
extern bool anim_out_of_slots;
extern void (*anim_all_goals_finished_callback)(void);
extern bool in_anim_load;
extern int anim_num_allocated_run_indices;
extern int animNumActiveGoals;
extern int anim_next_unique_id;
extern int dword_739E40;
extern int dword_739E44;

#define ANIM_RUN_INFO_COUNT 216
extern AnimRunInfo anim_run_info[ANIM_RUN_INFO_COUNT];

void anim_active_goal_count_increment(AnimRunInfo* run_info, AnimGoalNode* goal_node);
bool anim_goal_is_endless(AnimRunInfo* run_info);
bool anim_private_init(GameInitInfo* init_info);
void anim_private_exit(void);
void anim_private_reset(void);
bool anim_is_processing_run(void);
bool anim_goal_restart(AnimID* anim_id);
bool mp_deallocate_run_index(AnimID* anim_id);
void anim_active_goal_count_decrement(AnimRunInfo* run_info, AnimGoalNode* goal_node);
bool anim_allocate_new_run_index(AnimID* anim_id);
void sub_44D0C0(AnimRunInfo* run_info);
int anim_find_first(int64_t obj);
int anim_find_next(int prev, int64_t obj);
bool anim_goal_data_init(AnimGoalData* goal_data, int64_t obj, int goal_type);
bool anim_goal_data_init_no_interrupt(AnimGoalData* goal_data, int64_t obj, int goal_type);
bool anim_goal_add(AnimGoalData* goal_data, AnimID* anim_id);
bool anim_goal_add_ex(AnimGoalData* goal_data, AnimID* anim_id, unsigned int flags);
bool anim_goal_add_func(AnimGoalData* goal_data, AnimID* anim_id, bool a3, unsigned int flags);
bool anim_goal_type_is_concurrent(int index);
bool anim_subgoal_add(AnimID anim_id, AnimGoalData* goal_data, const char* file, int line);
bool anim_recover_handles(AnimRunInfo* run_info, AnimGoalSubNode* goal_subnode);
void anim_interrupt_attack_on_obj_or_follower(int64_t a1, int64_t a2);
void anim_interrupt_attack_on_obj(int64_t a1, int64_t a2);
bool anim_force_interrupt(AnimID* anim_id);
bool anim_interrupt(AnimID* anim_id, int priority);
bool anim_find_first_of_type(int64_t obj, int type, AnimID* anim_id);
bool anim_find_next_of_type(int64_t obj, int type, AnimID* anim_id);
bool anim_is_current_goal(int64_t obj, AnimGoalData* goal_data);
bool anim_interrupt_all_goals_of_type(int64_t obj, int goal_type, int next_goal_type);
bool anim_is_current_goal_ex(int64_t obj, AnimGoalData* goal_data, AnimID* anim_id);
bool anim_is_current_goal_type(int64_t obj, int goal_type, AnimID* anim_id);
bool anim_is_running_non_passive_goal(int64_t obj, AnimID* anim_id);
bool anim_is_attacking(int64_t attacker_obj, AnimID* anim_id, int64_t target_obj);
bool anim_path_is_possible(int64_t obj, int64_t from, int64_t to);
void anim_path_init(AnimPath* path);
void anim_path_destroy(AnimPath* path);
void anim_path_mark_net_update_pending(AnimRunInfo* run_info);
void anim_run_index_debug(int index);
void anim_stats(void);

#endif /* ARCANUM_GAME_ANIM_PRIVATE_H_ */
