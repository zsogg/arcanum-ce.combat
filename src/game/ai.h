#ifndef ARCANUM_GAME_AI_H_
#define ARCANUM_GAME_AI_H_

#include "game/context.h"
#include "game/object.h"
#include "game/timeevent.h"

typedef void (*AiFloatLineFunc)(int64_t npc_obj, int64_t pc_obj, const char* str, int speech_id);
typedef void (*AiEndDialogFunc)(int64_t, int);

/**
 * Optional fatigue policy for combat AI spell casting.
 *
 * Stock behaviour is reproduced exactly when `rules/ai_fatigue.mes` is absent
 * or when every entry carries the stock value. The file is parsed once at
 * `ai_init`; loose `data/rules/ai_fatigue.mes` therefore overrides base
 * archives through the normal repository precedence.
 */
typedef struct AiFatigueConfig {
    /* Below this fatigue percentage a leaderless NPC refuses memorized
       spells. Stock 20. */
    int min_cast_pct_leaderless;

    /* Below this fatigue percentage an NPC with a leader refuses memorized
       spells. Stock 0, i.e. followers may cast until the cost gate stops
       them. */
    int min_cast_pct_follower;

    /* Minimum fatigue that must remain after a memorized cast, expressed
       as `cost + reserve > current_fatigue`. Stock 1. */
    int reserve_after_cast;

    /* At or below this fatigue percentage in combat mode, inventory item
       spells are considered before memorized spells. Stock 50. */
    int item_first_pct;

    /* Below this fatigue percentage an AI in danger tries its
       AI_FatigueRecover spell list. Stock 20. */
    int recover_below_pct;

    /* At or above this fatigue percentage, white-necromancy item spells
       skip the failure-chance gate. Stock 80. */
    int white_necro_item_skip_pct;

    /* When true, `ai_check_use_spell` uses the engine's real charging
       check (`magictech_can_charge_spell_fatigue`) instead of the old
       `spell_cast_cost` estimate. Stock false. */
    bool use_real_charge_cost;
} AiFatigueConfig;

const AiFatigueConfig* ai_fatigue_config_get(void);

/**
 * What `ai_fatigue_config_load` decided about one entry.
 *
 * `BAD_VALUE` is the case card X7 exists for: the key is one this build
 * knows, so the entry is not unknown - the value is unreadable, and the
 * documented default is kept.
 */
typedef enum {
    AI_FATIGUE_ENTRY_UNKNOWN,
    AI_FATIGUE_ENTRY_APPLIED,
    AI_FATIGUE_ENTRY_BAD_VALUE,
} AiFatigueEntryResult;

/**
 * Applies one `Key: value` entry exactly as `ai_fatigue_config_load` would.
 *
 * Exposed so the suite can drive the judgement without standing up the file
 * layers a `.mes` read needs - the same reason `asan-gate.sh` and
 * `mod-matrix.sh` factor their verdicts out for their own tests.
 */
AiFatigueEntryResult ai_fatigue_config_apply_entry(const char* str);

/** Restores the documented defaults. For tests, and for a reload. */
void ai_fatigue_config_reset(void);

// (Re)loads `rules/ai_fatigue.mes` into the AI fatigue config. Called by
// `ai_init`; exposed for the unit suite.
void ai_fatigue_config_load(void);

typedef enum AiDangerSourceType {
    AI_DANGER_SOURCE_TYPE_NONE,
    AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
    AI_DANGER_SOURCE_TYPE_FLEE,
    AI_DANGER_SOURCE_TYPE_SURRENDER,
} AiDangerSourceType;

typedef enum AiSpeak {
    AI_SPEAK_OK,
    AI_SPEAK_DEAD,
    AI_SPEAK_COMBAT,
    AI_SPEAK_ANIM,
    AI_SPEAK_REACTION,
    AI_SPEAK_CANNOT,
    AI_SPEAK_SLEEP_UNCONSCIOUS,
} AiSpeak;

typedef enum AiFollow {
    AI_FOLLOW_OK,

    // The NPC will not follow the PC because the PC is too good.
    AI_FOLLOW_TOO_GOOD,

    // The NPC will not follow the PC because the PC is too evil.
    AI_FOLLOW_TOO_BAD,

    // The NPC will not follow the PC because the NPC dislikes the PC.
    AI_FOLLOW_DISLIKE,

    // The NPC will not follow the PC because the NPC is already grouped with
    // another PC who is at least as charismatic.
    AI_FOLLOW_ALREADY_IN_GROUP,

    // The NPC will not follow the PC because the PC is at his charisma limit
    // (which is greater zero).
    AI_FOLLOW_LIMIT_REACHED,

    // The NPC will not follow the PC because the PC's charisma does not allow
    // for followers.
    AI_FOLLOW_NOT_ALLOWED,

    // The NPC is too high level for the PC to be a follower.
    AI_FOLLOW_LOW_LEVEL,
} AiFollow;

typedef enum AiKos {
    AI_KOS_NO,
    AI_KOS_FACTION,
    AI_KOS_REACTION,
    AI_KOS_ALIGNMENT,
    AI_KOS_GUARD,
    AI_KOS_DECOY,
} AiKos;

typedef enum AiProtect {
    AI_PROTECT_NO,
    AI_PROTECT_FACTION,
    AI_PROTECT_ORIGIN,
    AI_PROTECT_GROUP,
    AI_PROTECT_SELF,
} AiProtect;

typedef enum AiUpsetAttacking {
    AI_UPSET_ATTACKING_NONE,
    AI_UPSET_ATTACKING_GOOD,
    AI_UPSET_ATTACKING_ORIGIN,
    AI_UPSET_ATTACKING_FACTION,
    AI_UPSET_ATTACKING_PARTY,
} AiUpsetAttacking;

typedef enum AiAttemptOpenPortal {
    AI_ATTEMPT_OPEN_PORTAL_OK,
    AI_ATTEMPT_OPEN_PORTAL_LOCKED,
    AI_ATTEMPT_OPEN_PORTAL_JAMMED,
    AI_ATTEMPT_OPEN_PORTAL_MAGICALLY_HELD,
    AI_ATTEMPT_OPEN_PORTAL_NOT_ALLOWED,
    AI_ATTEMPT_OPEN_PORTAL_NOT_CRITTER,
} AiAttemptOpenPortal;

typedef enum AiAttemptOpenContainer {
    AI_ATTEMPT_OPEN_CONTAINER_OK,
    AI_ATTEMPT_OPEN_CONTAINER_LOCKED,
    AI_ATTEMPT_OPEN_CONTAINER_JAMMED,
    AI_ATTEMPT_OPEN_CONTAINER_MAGICALLY_HELD,
    AI_ATTEMPT_OPEN_CONTAINER_NOT_ALLOWED,
} AiAttemptOpenContainer;

typedef struct AiRedirect {
    /* 0000 */ int64_t source_obj;
    /* 0008 */ int64_t target_obj;
    /* 0010 */ int min_iq;
    /* 0014 */ unsigned int critter_flags;
} AiRedirect;

typedef enum AiNpcWitnessPcCritical {
    AI_NPC_WITNESS_PC_CRITICAL_HIT,
    AI_NPC_WITNESS_PC_CRITICAL_MISS,
} AiNpcWitnessPcCritical;

typedef enum AiUseSkill {
    AI_USE_SKILL_OK,
    AI_USE_SKILL_BAD_SOURCE,
    AI_USE_SKILL_BAD_TARGET,
} AiUseSkill;

typedef enum AiActionType {
    AI_ACTION_TYPE_UNKNOWN,
    AI_ACTION_TYPE_CAST_SPELL,
    AI_ACTION_TYPE_USE_ITEM,
    AI_ACTION_TYPE_USE_SKILL,
} AiActionType;

bool ai_init(GameInitInfo* init_info);
void ai_exit(void);
bool ai_mod_load(void);
void ai_mod_unload(void);
void ai_set_callbacks(AiEndDialogFunc a1, AiFloatLineFunc float_line_func);
void ai_process(int64_t obj);
void ai_redirect_init(AiRedirect* ai_redirect, int64_t source_obj, int64_t target_obj);
void ai_redirect_perform(AiRedirect* ai_redirect);
void ai_attack(int64_t source_obj, int64_t target_obj, int loudness, unsigned int flags);
void ai_start_attacking(int64_t attacker_obj, int64_t target_obj);
void ai_notify_explosion_dynamite(int64_t pc_obj);
void ai_notify_killed(int64_t victim_obj, int64_t killer_obj);
void ai_make_peace(int64_t obj, int64_t other_obj);
void ai_stop_attacking(int64_t obj);
void ai_allow_attacking(int64_t obj);
void ai_npc_wait(int64_t obj);
void ai_npc_unwait(int64_t obj, bool force);
bool ai_npc_wait_here_timeevent_process(TimeEvent* timeevent);
bool ai_is_fighting(int64_t obj);
int ai_object_hp_ratio(int64_t obj);
int ai_critter_fatigue_ratio(int64_t obj);
int64_t ai_keep_or_find_target(int64_t obj);
bool ai_timeevent_process(TimeEvent* timeevent);
void ai_timeevent_schedule(int64_t obj);
void ai_timeevent_clear(int64_t obj);
int ai_can_speak(int64_t npc_obj, int64_t pc_obj, bool a3);
int ai_check_follow(int64_t npc_obj, int64_t pc_obj, bool ignore_charisma_limits);
// CE (`follower reaction joins`): called as an NPC joins a leader unforced; an
// NPC the switch let join at low reaction is raised to the reaction it needs
// to stay.
void ai_follow_joined(int64_t npc_obj, int64_t pc_obj);
// CE (`ai targeting`): forgets the round-keyed memos (unreachable targets,
// who cast) as a turn-based fight starts.
void ai_fight_memos_forget(void);
// CE (`ai targeting`): whether a memo made at turn key `recorded` is within
// `turns` turns of `key` (`ai_desperation_key`: a round, or in real time a
// negative key counting down). Pure.
bool ai_memo_age_within(int key, int recorded, int turns);
void ai_timeevent_add_immediate(int64_t obj, int first_heartbeat);
int ai_line_of_fire_cost(int64_t source_obj, int64_t target_loc, int64_t* block_obj_ptr);
void ai_switch_weapon(int64_t obj);
void ai_calc_party_size_and_level(int64_t obj, int* cnt_ptr, int* lvl_ptr);
int ai_check_kos(int64_t source_obj, int64_t target_obj);
void ai_objects_in_radius(int64_t obj, int radius, ObjectList* objects, unsigned int flags);
int ai_check_use_skill(int64_t source_obj, int64_t target_obj, int64_t item_obj, int skill);
void ai_npc_witness_pc_critical(int64_t pc_obj, int type);
void ai_npc_near_death(int64_t npc_obj, int64_t pc_obj);
bool ai_critter_can_open_portals(int64_t obj);
int ai_attempt_open_portal(int64_t obj, int64_t portal_obj, int dir);
int ai_attempt_open_container(int64_t obj, int64_t container_obj);
void ai_notify_portal_container_guards(int64_t critter_obj, int64_t target_obj, bool a3, int loudness);
void ai_flee(int64_t obj, int64_t danger_obj);
void ai_stop_fleeing(int64_t obj);
void ai_set_no_flee(int64_t obj);
bool ai_surrendered(int64_t obj, int64_t* danger_source_ptr);
int ai_can_see(int64_t source_obj, int64_t target_obj);
bool ai_is_moving(int64_t obj);
int ai_can_see_stock(int64_t source_obj, int64_t target_obj);
int ai_can_hear_stock(int64_t source_obj, int64_t target_obj, int loudness);
int ai_presence_loudness(int64_t obj);
int64_t ai_sight_block(int64_t source_obj, int64_t target_loc);
int ai_can_hear(int64_t a1, int64_t a2, int loudness);
void ai_npc_fighting_toggle(void);
bool ai_shitlist_has(int64_t npc_obj, int64_t shit_obj);
// CE (`grazing hits`; zsogg, 2026-11-09): an `ai_attack` flag beside the
// stock 0x01 (accidental), 0x02 (quiet) and 0x04 (an order). With 0x01, the
// victim makes no shitlist entry either way, keeps who hit it last and does
// not turn to the attacker (a player victim's party does not either).
#define AI_ATTACK_GRAZE 0x08
// The same: whether a hit of `attacker_obj`'s on `victim_obj`, aimed at
// `aimed_obj` (none for a burning area; anything not a critter counts as
// none), only grazed it: not the one aimed at, neither on the other's
// shitlist, and kin to the attacker or fighting someone not on its side. A
// player victim counts as busy when the attacker is fighting someone not on
// the player's side. False with the switch off.
bool ai_attack_is_graze(int64_t attacker_obj, int64_t aimed_obj, int64_t victim_obj);
// The same rule's logic. Pure.
bool ai_graze_rule(bool aimed, bool hostile, bool kin, bool busy_elsewhere);
int ai_max_dialog_distance(int64_t obj);
void ai_target_lock(int64_t obj, int64_t tgt);
void ai_target_unlock(int64_t obj);

// CE: combat AI milestone AI-3 (`ai targeting`). Whether an NPC fighting a
// target `current_dist` tiles away turns to a new attacker `threat_dist`
// tiles away: only for an adjacent attacker while the present target is not
// adjacent, or one at least AI_RETARGET_NEARER_BY tiles nearer and at most
// half the distance. Pure, so the rule is tested without a game.
#define AI_RETARGET_NEARER_BY 3

// CE: under `ai follower leash` (combat AI milestone AI-3), how far from its
// PC leader a follower's target may be, in tiles; stock is 20.
#define AI_FOLLOWER_LEASH 8

// CE: how far from `leader_obj` the follower `obj` may take a target. 20, the
// stock distance, except under `ai follower leash` for a follower of a PC
// that has not been told to Spread Out (combat AI milestones AI-3 and AI-4).
int ai_follower_reach(int64_t obj, int64_t leader_obj);
// CE: the leader `ai_follower_reach` is asked about for `obj`: under `ai
// follower leash` the player heading its chain of leaders (a companion's
// summon is the player's too; review 2026-11-08, followers-3), otherwise its
// direct leader. The distance is still counted from the direct leader.
int64_t ai_follower_leash_leader(int64_t obj);
bool ai_targeting_prefers_new_attacker(int current_dist, int threat_dist);

// CE: combat AI milestone AI-5, `follower casting`. The chance in 100 that an
// NPC reaches for its spells on an action, before AI-2's preference: the
// stock rule (`ai_magictech_chance`, 0x4ABE20) with AI-5's waiver. Stock gives
// 0 to a critter with a leader while both are above half health and its foe
// is not stronger; otherwise 50 when its AI set keeps a distance in combat,
// else 25. Under `follower casting` a caster-type follower of the player is
// not held back: it gets the 50 or 25, and `*waived` says so. A follower of
// the player is one whose chain of leaders reaches the player (followers-3);
// the hp test reads its direct leader. Pure, so the
// rule is tested without a game.
int ai_spell_chance_rule(bool has_leader, bool player_leader, int own_hp_pct,
    int leader_hp_pct, bool foe_stronger, bool caster_type, bool keeps_distance,
    bool follower_casting, bool* waived);
// CE: AI-7, `caster tactics`. Whether an area spell taking `allies` of the
// caster's side (the caster aside) and `enemies` of the target's is refused:
// any ally, unless two or more enemies go with it (zsogg, 2026-11-02). Pure.
bool ai_tactics_area_refuses(int allies, int enemies);
// CE: AI-7/AI-8 (review 2026-11-08, cross-1). Which side a critter is on in
// a caster's fight: the caster's or the target's by kin (`with_caster`,
// `with_target`, caster first); otherwise, if its AI is fighting, the side
// opposite the one its combat focus is on (the caster's side first) - so a
// faction-0 pack is sided by what it does. Pure.
typedef enum AiSide {
    AI_SIDE_NONE,
    AI_SIDE_CASTER,
    AI_SIDE_TARGET,
} AiSide;
AiSide ai_side_rule(bool with_caster, bool with_target, bool fighting,
    bool focus_with_caster, bool focus_with_target);
// CE: AI-8 rule 3, one critter in an area attack's blast (zsogg 2026-11-09):
// an enemy by the damage it would take (`worth`; a `downed` one nothing), an
// ally the spell does not spare at 1.1 times that (rounded up) against it,
// anyone else nothing. Pure.
int ai_judgement_area_worth(AiSide side, bool downed, bool spared, int worth);
// CE: the same - whether an area spell's target rules spare `obj` (its player
// leader `obj_leader`) from a caster whose player leader is `caster_leader`:
// the player and every follower under it (`Tgt_Non_Party_Critters`,
// 0x4F2D20), unless the spell is a `blast`, which under `grenade blasts`
// takes the party too. Pure.
bool ai_judgement_area_spares(int64_t caster_leader, int64_t obj, int64_t obj_leader, bool blast);
// CE: AI-7. Whether a target last seen disabled at turn key `disabled_key`
// has only just recovered at `now_key`: within one turn, either way the key
// runs (rounds count up; real-time turns down). Pure.
bool ai_tactics_just_recovered(int disabled_key, int now_key);
// CE: AI-8, `caster judgement` (zsogg, 2026-11-08). Whether it binds `obj`:
// any NPC, enemy or follower.
bool ai_judgement_applies(int64_t obj);
// CE: AI-8 rule 1's worths, in hit points. Pure.
// - a damage spell on one target: the expected damage, never more than the
//   target has left, plus 20 for a kill, at the chance it lands;
int ai_judgement_damage_value(int expected, int hp_left, int land_pct);
// - a spell that disables or weakens but the sheet marks neither `Control` nor
//   `Debuff` (a dispel, a tech spell): half the target's hit points left, x
//   the spell data's AI value / 20 (at most 1), at the chance it lands;
int ai_judgement_control_value(int hp_left, int ai_value, int land_pct);
// - a disable or debuff (review 2026-11-08, casting-2; zsogg 2026-11-09): the
//   damage it denies - `attack_damage` per attack, x the attacks
//   `ap_per_turn` allows at `attack_ap` each (at least one), x `turns` held,
//   x `share_pct` (100 a disable, 50 a debuff) - never more than killing the
//   target is worth, at the chance it lands;
int ai_judgement_disable_value(int attack_damage, int ap_per_turn, int attack_ap, int turns, int share_pct, int hp_left, int land_pct);
// - the turns a disable holds its target: a stun the turns its `stun_ticks`
//   skip (three a turn-based turn), a spell that `lasts` (a duration or
//   upkeep) AI_JUDGEMENT_HOLD_TURNS, anything else one; never more than
//   AI_JUDGEMENT_HOLD_TURNS, a horizon rather than a conversion.
#define AI_JUDGEMENT_HOLD_TURNS 3
int ai_judgement_hold_turns(bool stuns, int stun_ticks, bool lasts);
// CE: AI-8 rule 1's floor (review 2026-11-08, cross-7; zsogg 2026-11-09): a
// spell worth less is not cast. 10, or the caster's own attack for the
// spell's AP (`weapon_avg` a blow at `weapon_chance`, `weapon_ap` a blow),
// valued as a damage spell is. Pure.
int ai_judgement_floor(int weapon_avg, int weapon_chance, int weapon_ap, int spell_ap, int hp_left);
// - a summon: 12 x its AI value, in full with an enemy within 8 tiles, a fifth
//   from 20, in between by distance;
int ai_judgement_summon_value(int ai_value, int nearest_enemy);
// - a buff: 10 for each threatening enemy, at most 5, x (1 + AI value / 5).
int ai_judgement_buff_value(int threats, int ai_value);
// CE: AI-8. A worth per fatigue: hundredths of a hit point for each point of
// the cost plus two upkeep periods. Pure.
int ai_judgement_score(int value, int cost, int upkeep);
// CE: AI-8 rule 2 (review 2026-11-08, casting-1; zsogg 2026-11-09). Whether a
// spell that is not an attack, costing fatigue, is refused for spending into
// the reserve kept for the best attack worth casting (three of its casts),
// whatever it scores. Pure.
bool ai_judgement_budget_refuses(int fatigue_left, int cost, int reserve);
// CE: AI-8 rule 2 for heals (healing-3). Whether a heal from memory is
// refused for spending into the reserve: only for a patient above 30% health.
// Pure.
bool ai_judgement_heal_refuses(int fatigue_left, int cost, int reserve, int patient_hp_pct);
// CE: AI-8 rule 4. Whether a turn-based buff or summon leaving too few action
// points for the best attack is passed over for that attack, worth more. Pure.
bool ai_judgement_turn_refuses(int ap_left, int ap_cost, int attack_ap_cost, int value, int attack_value);
// CE: AI-8. One allowed spell as the judged pass weighs it.
typedef struct AiJudgedSpell {
    int spell;
    int64_t item_obj;
    int64_t target_obj;
    // `ai.decided`'s code; 0 while allowed.
    int code;
    int value;
    int score;
    // Fatigue; 0 for an item's cast.
    int cost;
    int ap_cost;
    // Rule 1's floor for it.
    int floor;
    bool attack;
} AiJudgedSpell;
// CE: AI-8 rules 1, 2 and 4 over the weighed spells, in place: under its
// floor 23; not an attack and into the reserve 21; in a turn-based turn
// (`ap_left` >= 0, -1 otherwise) leaving the best attack unaffordable 22;
// the best score chosen and the rest 20. The reserve is three casts of the
// best attack that clears its floor, into `*reserve_ptr`. Returns the chosen
// index, -1 for none. Pure.
int ai_judgement_pick(AiJudgedSpell* cands, int cnt, int fatigue_left, int ap_left, int* reserve_ptr);
// CE: AI-5. Whether `ONF_CAST_HIGHEST` holds a critter to each college's
// highest spell: always in stock; under `follower casting`, not for a follower
// of the player.
bool ai_cast_highest_applies(unsigned int npc_flags, bool player_follower, bool follower_casting);
// CE: AI-6, `follower healing`. The order a follower of the player tries its
// patients in: by health percentage, worst first, ties in stock order (itself,
// its leader, then the other followers). Pure; `order` gets `count` indices.
void ai_heal_patient_order(const int hp_pct[], int count, int order[]);
// CE: AI-6. The failure chance at or above which `ai_check_use_spell` refuses a
// spell or item (code 8): 38, or -1 for no check when stock waives it (White
// Necromancy on a badly hurt patient). Under `follower healing` a follower of
// the player is refused even a waived heal from 75.
int ai_failure_refusal_threshold(bool white_heal_waived, bool futile_cut);
// CE: AI-6 F2. Whether the leash refuses a follower's target: the distance is
// counted from its leader (stock and `ai follower leash`) or, under the leash
// for a follower on Wait Here, from itself (`dist_from_self`); under the leash
// a target the player ordered it to attack is never refused. Pure.
bool ai_follower_leash_refuses(bool leash_on, bool waiting, bool ordered,
    int dist_from_leader, int dist_from_self, int reach);
// CE: AI-6 F2, `follower conduct`. Whether `follower_obj` has not yet said it
// will not defend the player against `attacker_obj` this fight, noting that it
// now has; `ai_conduct_refusals_forget` starts the next fight afresh.
bool ai_conduct_refusal_first(int64_t follower_obj, int64_t attacker_obj);
void ai_conduct_refusals_forget(void);
// CE: AI-5. Whether a spell centred on its caster (`centred_radius`, -1 for any
// other spell) cannot reach a target `dist` tiles away. Jolt is one: taken by
// the AI against a distant foe, its cast never happens and the turn is lost.
bool ai_caster_centred_out_of_reach(int centred_radius, int dist);

// CE: combat AI milestone AI-3. The animation system reports an NPC whose
// approach to `target_obj` found no path, so its AI can turn to another target.
void ai_note_unreachable(int64_t obj, int64_t target_obj);

// CE: combat AI milestone AI-3. Magictech reports a spell a critter has begun
// casting, so a hunter can see who casts.
void ai_note_cast(int64_t obj);

// CE: combat AI milestone AI-3, `rules/ai_roles.mes`. Each entry is
// `Hunter: <description number>`; the result codes are the fatigue file's.
void ai_roles_reset(void);
AiFatigueEntryResult ai_roles_apply_entry(const char* str);
bool ai_roles_is_hunter_description(int description);

#endif /* ARCANUM_GAME_AI_H_ */
