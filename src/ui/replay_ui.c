#include "ui/replay_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/debug.h>
#include <tig/message.h>
#include <tig/timer.h>
#include <tig/video.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/creature_rules.h"
#include "game/crit_effects.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/level.h"
#include "game/light.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/obj_id.h"
#include "game/obj_pool.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/reload.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/spell_rules.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/timeevent.h"
#include "game/weapon_rules.h"
#include "ui/called_shot.h"
#include "ui/combat_ui.h"
#include "ui/intgame.h"
#include "ui/skill_ui.h"
#include "ui/spell_ui.h"
#include "ui/tb_ui.h"
#include "ui/wmap_rnd.h"

#define REPLAY_MAX_COMMANDS 256
#define REPLAY_IDENTITY_SIZE 64

typedef enum ReplayKind {
    REPLAY_KIND_OBJECT,
    REPLAY_KIND_LOCATION,
    // CE (card O2): a named player action with no target. Not a target at all -
    // it carries the action's name and goes through `replay_key_action` rather
    // than the command dispatch. Three so far: the two mode toggles and ending
    // a turn, each one found by a replay that could not reproduce its
    // recording without it.
    REPLAY_KIND_KEY,
    // Real-time session A: what a real-time run needs and a click cannot say.
    //
    //   cast <spell> self | object <id> | location <loc>
    //       the spell button, then the target click - `spell_ui_activate`, then
    //       `intgame_apply_mode_to_target`, the path a click in spell mode takes.
    //   wield <id> | wield <prototype>
    //       the hotkey bar's equip, the one equip route with one function. A
    //       bare number names a prototype in the player's pack (session C), for
    //       a carried item whose identity a script cannot know in advance.
    //   wait <ms>
    //       hold the next row this long, so a script can space its commands.
    REPLAY_KIND_CAST,
    REPLAY_KIND_WIELD,
    REPLAY_KIND_WAIT,
    // `learn <spell>`: the character screen's learn button, `spell_ui_add`, which
    // spends points and checks level and Intelligence as it does for a player.
    // Script-only, like `key character-points`: they build a test character, and
    // nothing records them. `learn <spell> force` (magic MG-1) skips the checks
    // and the points.
    REPLAY_KIND_LEARN,
    // Real-time session C: the two a throw needs.
    //
    //   give <prototype> <count>
    //       that many new items of that prototype into the player's inventory.
    //       Script-only, like `learn`: it stocks a test character, and nothing
    //       records it.
    //   throw <prototype> <location>
    //       one item of that prototype from the inventory to the tile, through
    //       `anim_goal_throw_item` - what dragging it onto the map calls.
    REPLAY_KIND_GIVE,
    REPLAY_KIND_THROW,
    //   skill <skill> <id>
    //       a skill used on an object, through `skill_use` - what the skill
    //       buttons reach for every skill but pickpocketing and repair.
    REPLAY_KIND_SKILL,
    //   raise <stat>
    //       one point into a primary stat (0 Strength ... 4 Intelligence ...
    //       7 Charisma), as the character screen's plus button spends it.
    //       Script-only, like `learn`.
    REPLAY_KIND_RAISE,
    // Called shots in real time and while prowling (2026-09-25):
    //
    //   prowl on | prowl off
    //       the Prowling skill button, `skill_ui_begin(pc, 0)`, pressed only
    //       when the player is not already in the asked state.
    //   panel <limb>
    //       a left click at the centre of the called-shot panel's control for
    //       that limb, enqueued as a mouse message so it meets the same
    //       buttons and window filters a real click does. Logged, and nothing
    //       sent, when the panel is not shown.
    REPLAY_KIND_PROWL,
    REPLAY_KIND_PANEL,
    // Reload prototype session 3 (2026-09-26): where a gun's loaded rounds go.
    //
    //   save <slot>
    //       a save named <slot> (e.g. Slot0009), as the autosave writes one;
    //       `observatory.py launch --slot` then loads it in a later run.
    //   inspect guns
    //       one log line for every weapon the weapon table lists, carried by
    //       the player or a follower or lying on the player's tile: its
    //       loaded shots and loading work, and where it is.
    //   transfer <prototype> <id> / take <prototype> <id>
    //       one carried item of that prototype to that critter, or from it to
    //       the player, through `item_transfer` - where trading and looting end.
    //   drop <prototype> / pickup <prototype>
    //       one carried item dropped at the player's feet (`item_drop`), or
    //       one lying on the player's tile picked up (`item_transfer`).
    //   wear <prototype> [<id>]
    //       armor or a shield of that prototype, carried by the player or by
    //       that critter, put on through `item_wield_set` and its checks
    //       (armor milestone 3, 2026-10-03).
    REPLAY_KIND_SAVE,
    REPLAY_KIND_INSPECT,
    REPLAY_KIND_TRANSFER,
    REPLAY_KIND_TAKE,
    REPLAY_KIND_DROP,
    REPLAY_KIND_PICKUP,
    REPLAY_KIND_WEAR,
    // Criticals milestone CR-2 (2026-10-05): a starting character, for the
    // early-game benchmark.
    //
    //   skill-level <skill> <points> [id]
    //       the player's (or, since 2026-10-22, that critter's) skill set to that many points (0-5; the level is four
    //       times the points), through `basic_skill_points_set` or
    //       `tech_skill_points_set` and the key-stat cap they apply. Training
    //       is unchanged. Script-only, like `raise`.
    //   stat-level <stat> <value> [id]
    //       a primary stat (0 Strength ... 7 Charisma) of the player's (or,
    //       since 2026-10-22, that critter's) set to
    //       that base value (1-20) through `stat_base_set`, so a starting
    //       character's Perception decides the range penalty.
    //   jam gun
    //       the player's wielded gun jammed, as a fumble under `firearm
    //       fumbles` jams it (`reload_jam`), so clearing - in combat or
    //       outside it - can be tested without waiting for a fumble
    //       (criticals CR-4, 2026-10-10). A test row: nothing else jams a gun.
    //   afflict <exposed|unfocused|dazzled|hobbled>
    //       the player given that effect for two of their turns, as a
    //       debuff's spell critical gives it (magic MG-2, 2026-10-11), so an
    //       Unfocused caster's refusal can be seen without an enemy mage.
    //       A test row.
    //   poison <level> [<id>]
    //       the player's poison level - or that critter's - set to <level>
    //       (1-1000) through `stat_base_set`, as a poisoned hit raises it,
    //       so poison's pace can be measured without a poisoned weapon (2026-10-12).
    //       A test row.
    //   use <prototype> [<id>]
    //       a carried item of that prototype used by the player on themselves,
    //       or on that critter, through `item_use_on_obj` as the inventory
    //       does - a restorative's real-time timing (2026-10-13).
    // Release 1, R1 (2026-10-13), UAP's four add-ons - test rows:
    //   xp <amount>
    //       experience given to the player through `critter_give_xp`, as a
    //       quest reward gives it (1-10,000,000), so levelling past 51 can be
    //       reached.
    //   train <basic skill> <training> [id]
    //       the player's (or, since 2026-10-22, that critter's) training in a basic skill (0-11) set to 0-3 through
    //       `basic_skill_training_set`, as a trainer sets it - which runs the
    //       Educator's pass over the followers.
    //   hour <0-23>
    //       the game clock advanced an hour at a time, as resting does, until
    //       it reads that hour - so the lighting scheme darkens or brightens
    //       the map as it would in play (2026-10-19, for lantern screenshots).
    //   encounter <table> [avoid|engage]
    //       a random encounter from that table of WMap_Rnd.mes at the player's
    //       tile, as one fired while travelling would be, awareness included;
    //       the word answers the avoid-or-engage choice if the party spots it
    //       (2026-10-20, card X10).
    //   inspect xp
    //       one log line with the player's level, experience and experience to
    //       the next level, and one per follower with its basic-skill training.
    //   hover <id>
    //       what the pointer resting on that object does in turn-based: the
    //       attack's AP checked through `combat_check_attack`, so the AP bar
    //       previews it, and one log line with the cost, the AP left and the
    //       fatigue exertion would spend (2026-10-21, the Combat Rebalance
    //       displays).
    //   screenshot <label>
    //       a screenshot now, through the shipped F12 path, as `-screenshot=N`
    //       takes one at a frame: of the state the rows before it set up. The
    //       label goes in the log beside it, to say which shot is which.
    //   spawn <prototype> <dx> <dy> <name>
    //       a new critter of that prototype, `dx` and `dy` tiles from the
    //       player, which later rows name `@<name>` wherever they take an
    //       object's identity; the log gives its own (2026-10-21, creature
    //       profiles: a bench meets any creature, not only the map's). A test
    //       row. Since 2026-10-22 it ignores its turn, and joins a running
    //       turn-based encounter.
    //   invulnerable on
    //       the player takes no damage (`OF_INVULNERABLE`, which `combat_dmg`
    //       honours), so a bench can measure what its blows do to a creature
    //       that could kill the fighter (2026-10-21). A test row.
    //   await-death <id>
    //       hold the rows after it until that critter - usually a spawned
    //       one, `@<name>` - is dead or gone, in either mode and whatever its
    //       turn says, as `wait` holds for a time (2026-10-22, creature
    //       profiles: a playtest's next creature comes when the last has
    //       died, not when an encounter ends, which switching turn-based off
    //       and on did early). A test row.
    //   remove <id>
    //       take that object off the map through `object_destroy`, which a
    //       script's destroy calls too and which takes a critter out of its
    //       group first (2026-10-22, creature profiles: the range's Garricks
    //       and officers joined every fight and killed a playtest's creatures
    //       in seconds). Never the player. Before a fight starts: nothing
    //       takes a destroyed critter off a running turn-based encounter's
    //       list. A test row.
    //   examine <id>
    //       what the pointer resting on that object shows in the message
    //       window, as the examine path builds it - the critter panel's
    //       name, portrait and rows - and one log line with its text and,
    //       for a phantom, the line in place of its reaction (2026-10-22,
    //       creature profiles). A `screenshot` row after it shoots the panel.
    REPLAY_KIND_SKILL_LEVEL,
    REPLAY_KIND_STAT_LEVEL,
    REPLAY_KIND_JAM,
    REPLAY_KIND_AFFLICT,
    REPLAY_KIND_POISON,
    REPLAY_KIND_USE,
    REPLAY_KIND_XP,
    REPLAY_KIND_TRAIN,
    REPLAY_KIND_HOUR,
    REPLAY_KIND_ENCOUNTER,
    REPLAY_KIND_HOVER,
    REPLAY_KIND_SCREENSHOT,
    REPLAY_KIND_SPAWN,
    REPLAY_KIND_INVULNERABLE,
    REPLAY_KIND_AWAIT_DEATH,
    REPLAY_KIND_REMOVE,
    REPLAY_KIND_EXAMINE,
} ReplayKind;

// A cast's target, which is a third shape: the caster, an object or a tile.
typedef enum ReplayCastTarget {
    REPLAY_CAST_SELF,
    REPLAY_CAST_OBJECT,
    REPLAY_CAST_LOCATION,
} ReplayCastTarget;

typedef struct ReplayCommand {
    int turn;
    ReplayKind kind;
    int64_t loc;
    char target[REPLAY_IDENTITY_SIZE];
    // CE (card S1): the aim this command was recorded with. A fourth field on
    // the line, absent in every record written before it existed - and absent
    // means `HIT_LOC_TORSO`, an ordinary attack, which is exactly what those
    // records meant when they were written.
    int aim;
    // Real-time session A. `now` submits without waiting for the actor to be
    // free, which is the only way a script can click during a wind-up or a
    // recovery - what cancelling and queueing are about.
    bool now;
    int spell;
    ReplayCastTarget cast_target;
    int wait_ms;
    int proto;
    int count;
    int skill;
    int stat;
    // `prowl`: 1 on, 0 off.
    int prowl;
    // `spawn`: the tiles from the player.
    int dx;
    int dy;
} ReplayCommand;

// `spawn`'s critters, by the name later rows give them after `@`.
#define REPLAY_MAX_SPAWNED 16

static struct {
    char name[REPLAY_IDENTITY_SIZE];
    int64_t obj;
} replay_spawned[REPLAY_MAX_SPAWNED];
static int replay_spawned_count;

static ReplayCommand replay_commands[REPLAY_MAX_COMMANDS];
static int replay_count;
static int replay_next;
static bool replay_loaded;
static bool replay_stall_reported;
static bool replay_waiting;
static tig_timestamp_t replay_wait_started;

// CE (card O2): does the game's mode match the one this command was recorded
// in?
//
// Measured 2026-09-14: the same click is an attack inside combat mode and
// something else outside it, because `intgame.c` gates the whole attack branch
// on `combat_critter_is_combat_mode_active`. A replay that ignores the mode
// therefore does not fail - it quietly performs a *different* action and
// reports it accepted, which is the worst of the available outcomes.
//
// The recorded turn is the proxy: a command recorded at turn 0 was issued
// before any turn started, and one at turn 1 or later was issued inside
// turn-based combat. Holding the command until the mode agrees turns a silent
// substitution into a visible stall.
bool replay_ui_mode_matches(int recorded_turn, bool combat_active)
{
    return recorded_turn >= 1 ? combat_active : !combat_active;
}

static bool replay_mode_matches(const ReplayCommand* command)
{
    return replay_ui_mode_matches(command->turn, combat_turn_based_is_active());
}

// CE (card O2): the named actions a script may press.
//
// One place, so a recorded action that has no replay entry point is a named
// failure rather than a silent no-op. Each goes through the same UI function
// the keyboard calls, so the recorder sees a replayed press exactly as it saw
// the original - which is what makes `replay-script` a fixed point.
static bool replay_key_action(const char* action)
{
    if (strcmp(action, "end-turn") == 0) {
        combat_ui_end_turn_action();
        return true;
    }

    return intgame_mode_action(action);
}

// CE (card O2): a target the click path could not have handed us.
//
// The recorder only ever sees commands that came through `target_pick`, which
// makes dead critters click-through under `TGT_OBJ_NO_ST_CRITTER_DEAD` and
// skips objects that are destroyed or switched off. A replay resolves a GUID
// straight to a handle and is bound by none of that, so it says so here.
//
// Measured 2026-09-15, and this is why it exists rather than being defensive:
// a script that outlived its duel submitted the recorded attack again while
// the guard was mid-`ag_dying`, and the engine asserted in `anim_private.c`
// line 675 -- `goal_type >= 0 && goal_type < ANIM_GOAL_MAX`. The dispatch's
// `OBJ_TYPE_PC`/`OBJ_TYPE_NPC` branch leaves its `anim` local uninitialised for
// a dead critter with no ALT held, and hands it to
// `anim_goal_data_init_no_interrupt`. That defect is the engine's and is
// recorded separately; what belongs here is that no player could have made
// this click, so no recording can contain it and no replay should submit it.
static bool replay_target_is_live(int64_t obj)
{
    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && critter_is_dead(obj)) {
        return false;
    }

    return true;
}

static bool replay_parse_int(const char* token, int* value)
{
    char* end;
    long parsed;

    if (token == NULL) {
        return false;
    }
    parsed = strtol(token, &end, 10);
    if (end == token || *end != '\0') {
        return false;
    }
    *value = (int)parsed;
    return true;
}

// A prototype by the number a modder sees - the `.pro` file's name, and the
// `prototype` in a scenario file - which is its description plus 20
// (`proto_description_to_oid`). Taking the description instead was measured
// wrong on 2026-09-23: `give 15171` made an Explosive Decoy, description 15171,
// not the Smoke Grenade that is prototype 15171.
static int64_t replay_prototype(int prototype)
{
    return prototype > 20 ? proto_get_by_description(prototype - 20) : OBJ_HANDLE_NULL;
}

// The player's first usable item of a prototype, or null - saying what the
// pack does hold when there is none, so a wrong number is visible in the log.
//
// Not `item_find_by_proto`, which returns items the engine has finished with.
// Measured 2026-09-23: a thrown Smoke Grenade stays in the thrower's inventory
// list flagged `OF_DESTROYED` after its spell goes off, the search returned it
// for every later throw, and `anim_throw_item` refused it each time - five of
// six grenades released and never flew.
static int64_t replay_find_carried(int64_t pc_obj, int prototype, bool pack_only)
{
    int64_t proto_obj;
    int64_t item_obj;
    int count;
    int index;

    proto_obj = replay_prototype(prototype);
    count = obj_field_int32_get(pc_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (index = 0; index < count; index++) {
        item_obj = obj_arrayfield_handle_get(pc_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        if (proto_obj == OBJ_HANDLE_NULL
            || obj_field_handle_get(item_obj, OBJ_F_PROTOTYPE_HANDLE) != proto_obj
            || (obj_field_int32_get(item_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0
            || (pack_only && IS_WEAR_INV_LOC(item_inventory_location_get(item_obj)))) {
            continue;
        }
        return item_obj;
    }

    tig_debug_printf("Replay: no usable prototype %d in the pack; it holds:", prototype);
    for (index = 0; index < count; index++) {
        item_obj = obj_arrayfield_handle_get(pc_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        tig_debug_printf(" %d", obj_field_int32_get(item_obj, OBJ_F_DESCRIPTION) + 20);
    }
    tig_debug_printf("\n");
    return OBJ_HANDLE_NULL;
}

// `inspect guns`: one listed weapon's state, and where it is.
static void replay_inspect_one(int64_t item_obj, const char* where)
{
    if (weapon_rule_for(item_obj) == NULL) {
        return;
    }
    // The weight is the engine's (`item_weight`, which the weapon table's
    // Weight overrides): the figure encumbrance sums.
    // Criticals CR-4: `jammed` 1 while a jam holds under `firearm fumbles`.
    tig_debug_printf("Replay: inspect %d loaded %d progress %d jammed %d weight %d %s%s\n",
        obj_field_int32_get(item_obj, OBJ_F_DESCRIPTION) + 20,
        obj_field_int32_get(item_obj, OBJ_F_WEAPON_PAD_I_1),
        reload_progress_get(item_obj),
        reload_jammed(item_obj) ? 1 : 0,
        item_weight(item_obj, OBJ_HANDLE_NULL),
        where,
        IS_WEAR_INV_LOC(item_inventory_location_get(item_obj)) ? " (wielded)" : "");
}

static void replay_inspect_critter(int64_t critter_obj, const char* who)
{
    int count;
    int index;

    count = obj_field_int32_get(critter_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (index = 0; index < count; index++) {
        replay_inspect_one(obj_arrayfield_handle_get(critter_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index), who);
    }
}

// `inspect awareness` (2026-10-12): one log line per critter within 15 tiles
// of the player - its distance, whether it faces the player, and what
// `ai_can_see` and `ai_can_hear` return against the player now (0: perceived;
// otherwise the tiles short; 1000: cannot), with the player's prowling
// effectiveness against it, and whether the critter is prowling itself.
// Read-only: nothing turns or acts.
static void replay_inspect_awareness(int64_t pc_obj)
{
    ObjectList objects;
    ObjectNode* node;
    char id[COMBAT_OBSERVER_IDENTITY_SIZE];
    bool concealed = critter_is_concealed(pc_obj);
    SkillInvocation sight;

    tig_debug_printf("Replay: awareness of the player, %s\n", concealed ? "prowling" : "not prowling");
    ai_objects_in_radius(pc_obj, 15, &objects, OBJ_TM_CRITTER);
    for (node = objects.head; node != NULL; node = node->next) {
        if (node->obj == pc_obj || critter_is_dead(node->obj)) {
            continue;
        }
        combat_observer_object_id(node->obj, id);
        // The difficulty `ai_can_see` subtracts from the prowling
        // effectiveness - built as it builds it.
        skill_invocation_init(&sight);
        object_ref_init(pc_obj, &(sight.source));
        object_ref_init(node->obj, &(sight.target));
        sight.flags |= SKILL_INVOCATION_CHECK_SEEING;
        sight.skill = BASIC_SKILL_PROWLING;
        tig_debug_printf("Replay: awareness %s dist %d facing %d see %d hear %d prowling %d sight_difficulty %d light %d concealed %d\n",
            id,
            (int)object_dist(node->obj, pc_obj),
            critter_is_facing_to(node->obj, pc_obj) ? 1 : 0,
            ai_can_see(node->obj, pc_obj),
            ai_can_hear(node->obj, pc_obj, concealed ? LOUDNESS_SILENT : LOUDNESS_NORMAL),
            basic_skill_effectiveness(pc_obj, BASIC_SKILL_PROWLING, node->obj),
            skill_invocation_difficulty(&sight),
            light_obj_grayscale_get(pc_obj) & 0xFF,
            critter_is_concealed(node->obj) ? 1 : 0);
    }
    object_list_destroy(&objects);
}

// `inspect xp` (release 1, R1): the player's level and experience, and each
// party member's basic skills as training/level - for UAP's level-cap and
// Educator add-ons. Training needs the skill level its tier requires, so the
// level is shown beside it.
static void replay_inspect_skills(int64_t obj, const char* who)
{
    char line[BASIC_SKILL_COUNT * 8 + 1];
    int skill;
    int len = 0;

    line[0] = '\0';
    for (skill = 0; skill < BASIC_SKILL_COUNT && len < (int)sizeof(line) - 8; skill++) {
        len += snprintf(line + len, sizeof(line) - (size_t)len, "%d/%d ",
            basic_skill_training_get(obj, skill), basic_skill_level(obj, skill));
    }
    tig_debug_printf("Replay: xp skills %s %s\n", who, line);
}

static void replay_inspect_xp(int64_t pc_obj)
{
    ObjectList followers;
    ObjectNode* node;
    char name[MAX_STRING];

    tig_debug_printf("Replay: xp level %d experience %d to_next %d\n",
        stat_level_get(pc_obj, STAT_LEVEL),
        stat_level_get(pc_obj, STAT_EXPERIENCE_POINTS),
        level_experience_points_to_next_level(pc_obj));
    replay_inspect_skills(pc_obj, "the-player");
    object_list_followers(pc_obj, &followers);
    for (node = followers.head; node != NULL; node = node->next) {
        object_examine(node->obj, node->obj, name);
        replay_inspect_skills(node->obj, name);
    }
    object_list_destroy(&followers);
}

static void replay_inspect_guns(int64_t pc_obj)
{
    ObjectList objects;
    ObjectNode* node;
    char who[REPLAY_IDENTITY_SIZE + 16];

    replay_inspect_critter(pc_obj, "carried by the player");

    object_list_all_followers(pc_obj, &objects);
    for (node = objects.head; node != NULL; node = node->next) {
        ObjectID oid = obj_get_id(node->obj);
        char id[REPLAY_IDENTITY_SIZE];

        objid_id_to_str(id, oid);
        snprintf(who, sizeof(who), "carried by %s", id);
        replay_inspect_critter(node->obj, who);
    }
    object_list_destroy(&objects);

    object_list_location(obj_field_int64_get(pc_obj, OBJ_F_LOCATION), OBJ_TM_WEAPON, &objects);
    for (node = objects.head; node != NULL; node = node->next) {
        replay_inspect_one(node->obj, "on the player's tile");
    }
    object_list_destroy(&objects);
    tig_debug_printf("Replay: inspect done\n");
}

static void replay_copy_target(ReplayCommand* command, const char* token)
{
    strncpy(command->target, token, sizeof(command->target) - 1);
    command->target[sizeof(command->target) - 1] = '\0';
}

// One line: `<turn> [now] <kind> <arguments>`. The first three kinds keep the
// grammar every existing script was written in, so those scripts load as
// before; the rest are real-time session A's.
static bool replay_parse_line(const char* text, ReplayCommand* command)
{
    char line[512];
    char* tokens[8];
    int count = 0;
    int index = 0;
    char* token;
    const char* kind;

    snprintf(line, sizeof(line), "%s", text);
    for (token = strtok(line, " \t\r\n"); token != NULL && count < 8;
        token = strtok(NULL, " \t\r\n")) {
        tokens[count++] = token;
    }

    memset(command, 0, sizeof(*command));
    command->aim = HIT_LOC_TORSO;

    if (count < 3 || !replay_parse_int(tokens[index++], &(command->turn))) {
        return false;
    }

    if (strcmp(tokens[index], "now") == 0) {
        command->now = true;
        index++;
    }

    if (index + 1 >= count) {
        return false;
    }
    kind = tokens[index++];

    if (strcmp(kind, "object") == 0 || strcmp(kind, "location") == 0) {
        command->kind = strcmp(kind, "location") == 0
            ? REPLAY_KIND_LOCATION
            : REPLAY_KIND_OBJECT;
        if (command->kind == REPLAY_KIND_LOCATION) {
            command->loc = (int64_t)strtoll(tokens[index], NULL, 10);
        } else {
            replay_copy_target(command, tokens[index]);
        }
        index++;
        // `panel` instead of a limb: force no aim, so the command commits
        // whatever the called-shot panel holds - the one way a script can
        // test that a click on the panel reached the attack.
        if (index < count && strcmp(tokens[index], "panel") == 0) {
            command->aim = -1;
            index++;
            return index == count;
        }
        if (index < count && !replay_parse_int(tokens[index++], &(command->aim))) {
            return false;
        }
        if (command->aim < 0 || command->aim >= HIT_LOC_COUNT) {
            // Refused rather than clamped. A record naming a limb this build
            // has no entry for cannot be reproduced, and replaying it as some
            // other limb would be a wrong reproduction that reads as a
            // gameplay difference.
            tig_debug_printf("Replay: aim %d is not a hit location\n", command->aim);
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "key") == 0) {
        command->kind = REPLAY_KIND_KEY;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "wield") == 0) {
        command->kind = REPLAY_KIND_WIELD;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "learn") == 0) {
        command->kind = REPLAY_KIND_LEARN;
        if (!replay_parse_int(tokens[index++], &(command->spell))
            || command->spell < 0) {
            return false;
        }
        // CE (magic, milestone MG-1): `learn <spell> force`, a test row,
        // adds the spell past the level, Intelligence, Willpower and order
        // checks and spends no character points, so a probe can give a
        // level-1 fighter a complete college. `count` carries the flag.
        if (index < count && strcmp(tokens[index], "force") == 0) {
            command->count = 1;
            index++;
        }
        return index == count;
    }

    if (strcmp(kind, "give") == 0) {
        command->kind = REPLAY_KIND_GIVE;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0 || index >= count
            || !replay_parse_int(tokens[index++], &(command->count))
            || command->count <= 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "prowl") == 0) {
        command->kind = REPLAY_KIND_PROWL;
        if (strcmp(tokens[index], "on") == 0) {
            command->prowl = 1;
        } else if (strcmp(tokens[index], "off") == 0) {
            command->prowl = 0;
        } else {
            return false;
        }
        index++;
        return index == count;
    }

    if (strcmp(kind, "panel") == 0) {
        command->kind = REPLAY_KIND_PANEL;
        if (!replay_parse_int(tokens[index++], &(command->aim))
            || command->aim < 0 || command->aim >= HIT_LOC_COUNT) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "save") == 0) {
        command->kind = REPLAY_KIND_SAVE;
        if (strlen(tokens[index]) > 8) {
            return false; // the archive pair is named from eight characters
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "jam") == 0) {
        command->kind = REPLAY_KIND_JAM;
        if (strcmp(tokens[index++], "gun") != 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "afflict") == 0) {
        static const struct {
            const char* name;
            unsigned int effect;
        } afflictions[] = {
            { "exposed", CRIT_EFFECT_EXPOSED },
            { "unfocused", CRIT_EFFECT_UNFOCUSED },
            { "dazzled", CRIT_EFFECT_DAZZLED },
            { "hobbled", CRIT_EFFECT_HOBBLED },
        };
        size_t affliction;

        command->kind = REPLAY_KIND_AFFLICT;
        for (affliction = 0; affliction < sizeof(afflictions) / sizeof(afflictions[0]); affliction++) {
            if (strcmp(tokens[index], afflictions[affliction].name) == 0) {
                command->proto = (int)afflictions[affliction].effect;
                break;
            }
        }
        index++;
        return command->proto != 0 && index == count;
    }

    if (strcmp(kind, "inspect") == 0) {
        command->kind = REPLAY_KIND_INSPECT;
        // `inspect awareness` (2026-10-12): what each critter nearby makes of
        // the player, for prowling. `stat` tells the two apart.
        if (strcmp(tokens[index], "awareness") == 0) {
            command->stat = 1;
        } else if (strcmp(tokens[index], "xp") == 0) {
            command->stat = 2;
        } else if (strcmp(tokens[index], "guns") != 0) {
            return false;
        }
        index++;
        return index == count;
    }

    if (strcmp(kind, "transfer") == 0 || strcmp(kind, "take") == 0) {
        command->kind = strcmp(kind, "take") == 0 ? REPLAY_KIND_TAKE : REPLAY_KIND_TRANSFER;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0 || index >= count) {
            return false;
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "drop") == 0 || strcmp(kind, "pickup") == 0) {
        command->kind = strcmp(kind, "drop") == 0 ? REPLAY_KIND_DROP : REPLAY_KIND_PICKUP;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "poison") == 0) {
        command->kind = REPLAY_KIND_POISON;
        if (!replay_parse_int(tokens[index++], &(command->count))
            || command->count < 1 || command->count > 1000) {
            return false;
        }
        if (index < count) {
            replay_copy_target(command, tokens[index++]);
        }
        return index == count;
    }

    if (strcmp(kind, "xp") == 0) {
        command->kind = REPLAY_KIND_XP;
        if (!replay_parse_int(tokens[index++], &(command->count))
            || command->count < 1 || command->count > 10000000) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "encounter") == 0) {
        command->kind = REPLAY_KIND_ENCOUNTER;
        command->aim = -1;
        if (!replay_parse_int(tokens[index++], &(command->count)) || command->count < 0) {
            return false;
        }
        if (index < count) {
            if (strcmp(tokens[index], "avoid") == 0) {
                command->aim = 0;
            } else if (strcmp(tokens[index], "engage") == 0) {
                command->aim = 1;
            } else {
                return false;
            }
            index++;
        }
        return index == count;
    }

    if (strcmp(kind, "spawn") == 0) {
        command->kind = REPLAY_KIND_SPAWN;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 20 || index + 2 >= count
            || !replay_parse_int(tokens[index++], &(command->dx))
            || !replay_parse_int(tokens[index++], &(command->dy))) {
            return false;
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "await-death") == 0) {
        command->kind = REPLAY_KIND_AWAIT_DEATH;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "remove") == 0) {
        command->kind = REPLAY_KIND_REMOVE;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "examine") == 0) {
        command->kind = REPLAY_KIND_EXAMINE;
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "invulnerable") == 0) {
        command->kind = REPLAY_KIND_INVULNERABLE;
        if (strcmp(tokens[index++], "on") != 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "screenshot") == 0) {
        command->kind = REPLAY_KIND_SCREENSHOT;
        if (index >= count) {
            return false;
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "hover") == 0) {
        command->kind = REPLAY_KIND_HOVER;
        if (index >= count) {
            return false;
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "hour") == 0) {
        command->kind = REPLAY_KIND_HOUR;
        if (!replay_parse_int(tokens[index++], &(command->count))
            || command->count < 0 || command->count > 23) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "train") == 0) {
        command->kind = REPLAY_KIND_TRAIN;
        if (!replay_parse_int(tokens[index++], &(command->skill))
            || command->skill < 0 || command->skill >= BASIC_SKILL_COUNT || index >= count
            || !replay_parse_int(tokens[index++], &(command->count))
            || command->count < TRAINING_NONE || command->count > TRAINING_MASTER) {
            return false;
        }
        // Since 2026-10-22: an optional critter, the player when absent.
        if (index < count) {
            replay_copy_target(command, tokens[index++]);
        }
        return index == count;
    }

    if (strcmp(kind, "use") == 0) {
        command->kind = REPLAY_KIND_USE;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0) {
            return false;
        }
        if (index < count) {
            replay_copy_target(command, tokens[index++]);
        }
        return index == count;
    }

    if (strcmp(kind, "wear") == 0) {
        command->kind = REPLAY_KIND_WEAR;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0) {
            return false;
        }
        if (index < count) {
            replay_copy_target(command, tokens[index++]);
        }
        return index == count;
    }

    if (strcmp(kind, "raise") == 0) {
        command->kind = REPLAY_KIND_RAISE;
        if (!replay_parse_int(tokens[index++], &(command->stat))
            || command->stat < STAT_STRENGTH || command->stat > STAT_CHARISMA) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "stat-level") == 0) {
        command->kind = REPLAY_KIND_STAT_LEVEL;
        if (!replay_parse_int(tokens[index++], &(command->stat))
            || command->stat < STAT_STRENGTH || command->stat > STAT_CHARISMA || index >= count
            || !replay_parse_int(tokens[index++], &(command->count))
            || command->count < 1 || command->count > 20) {
            return false;
        }
        // Since 2026-10-22: an optional critter, the player when absent.
        if (index < count) {
            replay_copy_target(command, tokens[index++]);
        }
        return index == count;
    }

    if (strcmp(kind, "skill-level") == 0) {
        command->kind = REPLAY_KIND_SKILL_LEVEL;
        if (!replay_parse_int(tokens[index++], &(command->skill))
            || command->skill < 0 || command->skill >= SKILL_COUNT || index >= count
            || !replay_parse_int(tokens[index++], &(command->count))
            || command->count < 0 || command->count > 5) {
            return false;
        }
        // Since 2026-10-22: an optional critter, the player when absent.
        if (index < count) {
            replay_copy_target(command, tokens[index++]);
        }
        return index == count;
    }

    if (strcmp(kind, "skill") == 0) {
        command->kind = REPLAY_KIND_SKILL;
        if (!replay_parse_int(tokens[index++], &(command->skill))
            || command->skill < 0 || command->skill >= SKILL_COUNT || index >= count) {
            return false;
        }
        replay_copy_target(command, tokens[index++]);
        return index == count;
    }

    if (strcmp(kind, "throw") == 0) {
        command->kind = REPLAY_KIND_THROW;
        if (!replay_parse_int(tokens[index++], &(command->proto))
            || command->proto <= 0 || index >= count) {
            return false;
        }
        // Creature profiles (2026-10-21): or at a spawned critter's tile.
        if (tokens[index][0] == '@') {
            replay_copy_target(command, tokens[index++]);
            return index == count;
        }
        command->loc = (int64_t)strtoll(tokens[index++], NULL, 10);
        return index == count && command->loc != 0;
    }

    if (strcmp(kind, "wait") == 0) {
        command->kind = REPLAY_KIND_WAIT;
        if (!replay_parse_int(tokens[index++], &(command->wait_ms))
            || command->wait_ms < 0) {
            return false;
        }
        return index == count;
    }

    if (strcmp(kind, "cast") == 0) {
        command->kind = REPLAY_KIND_CAST;
        if (!replay_parse_int(tokens[index++], &(command->spell))
            || command->spell < 0 || index >= count) {
            return false;
        }
        if (strcmp(tokens[index], "self") == 0) {
            command->cast_target = REPLAY_CAST_SELF;
            index++;
        } else if (strcmp(tokens[index], "object") == 0 && index + 1 < count) {
            command->cast_target = REPLAY_CAST_OBJECT;
            replay_copy_target(command, tokens[index + 1]);
            index += 2;
        } else if (strcmp(tokens[index], "location") == 0 && index + 1 < count) {
            command->cast_target = REPLAY_CAST_LOCATION;
            command->loc = (int64_t)strtoll(tokens[index + 1], NULL, 10);
            index += 2;
        } else {
            return false;
        }
        return index == count;
    }

    return false;
}

int replay_ui_count(void)
{
    return replay_count;
}

// For the suite: one loaded row, in a fixed form, so a test can say exactly what
// a line parsed into without reaching into the table.
bool replay_ui_describe(int index, char* buffer, size_t size)
{
    const ReplayCommand* command;
    static const char* kinds[] = { "object", "location", "key", "cast", "wield", "wait", "learn", "give", "throw", "skill", "raise", "prowl", "panel", "save", "inspect", "transfer", "take", "drop", "pickup", "wear", "skill-level", "stat-level", "jam", "afflict", "poison", "use", "xp", "train", "hour", "encounter", "hover", "screenshot", "spawn", "invulnerable", "await-death", "remove", "examine" };
    int written;
    static const char* casts[] = { "self", "object", "location" };

    if (index < 0 || index >= replay_count) {
        return false;
    }
    command = &(replay_commands[index]);
    written = snprintf(buffer, size,
        "turn=%d now=%d kind=%s target=%s loc=%lld aim=%d spell=%d cast=%s wait=%d",
        command->turn, command->now ? 1 : 0, kinds[command->kind],
        command->target, (long long)command->loc, command->aim, command->spell,
        command->kind == REPLAY_KIND_CAST ? casts[command->cast_target] : "-",
        command->wait_ms);
    // Only the kinds that carry them, so every older row reads as it always did.
    if ((command->kind == REPLAY_KIND_GIVE || command->kind == REPLAY_KIND_THROW
            || command->kind == REPLAY_KIND_TRANSFER || command->kind == REPLAY_KIND_TAKE
            || command->kind == REPLAY_KIND_DROP || command->kind == REPLAY_KIND_PICKUP
            || command->kind == REPLAY_KIND_WEAR || command->kind == REPLAY_KIND_AFFLICT
            || command->kind == REPLAY_KIND_POISON || command->kind == REPLAY_KIND_USE
            || command->kind == REPLAY_KIND_XP || command->kind == REPLAY_KIND_HOUR)
        && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " proto=%d count=%d",
            command->proto, command->count);
    }
    if (command->kind == REPLAY_KIND_SKILL && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " skill=%d", command->skill);
    }
    if (command->kind == REPLAY_KIND_SKILL_LEVEL && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " skill=%d points=%d",
            command->skill, command->count);
    }
    if (command->kind == REPLAY_KIND_STAT_LEVEL && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " stat=%d value=%d",
            command->stat, command->count);
    }
    if (command->kind == REPLAY_KIND_RAISE && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " stat=%d", command->stat);
    }
    if (command->kind == REPLAY_KIND_PROWL && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " prowl=%d", command->prowl);
    }
    if (command->kind == REPLAY_KIND_SPAWN && written >= 0 && (size_t)written < size) {
        snprintf(buffer + written, size - (size_t)written, " proto=%d dx=%d dy=%d",
            command->proto, command->dx, command->dy);
    }
    return true;
}

bool replay_ui_load(const char* path)
{
    FILE* stream;
    char line[512];

    replay_count = 0;
    replay_next = 0;
    replay_loaded = false;
    replay_stall_reported = false;
    replay_spawned_count = 0;
    replay_waiting = false;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        tig_debug_printf("Replay: cannot open '%s'; nothing will be replayed\n", path);
        return false;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        if (replay_count >= REPLAY_MAX_COMMANDS) {
            // Refuse rather than truncate: a replay missing its tail would
            // diverge from its recording and look like a gameplay difference.
            tig_debug_printf("Replay: more than %d commands; refusing the file\n",
                REPLAY_MAX_COMMANDS);
            fclose(stream);
            replay_count = 0;
            return false;
        }

        if (!replay_parse_line(line, &(replay_commands[replay_count]))) {
            tig_debug_printf("Replay: cannot parse '%s'; refusing the file\n", line);
            fclose(stream);
            replay_count = 0;
            return false;
        }
        replay_count++;
    }

    fclose(stream);
    replay_loaded = true;
    tig_debug_printf("Replay: loaded %d command(s) from '%s'\n", replay_count, path);
    return true;
}

bool replay_ui_active(void)
{
    return replay_loaded;
}

int replay_ui_remaining(void)
{
    return replay_loaded ? replay_count - replay_next : 0;
}

// An object row's identity, resolved the way the dispatch below does and with
// the same messages: unreadable, or absent from this run.
static bool replay_resolve(const ReplayCommand* command, int64_t* obj_ptr)
{
    ObjectID oid;
    int index;

    // A critter a `spawn` row made, by its name.
    if (command->target[0] == '@') {
        for (index = 0; index < replay_spawned_count; index++) {
            if (strcmp(replay_spawned[index].name, command->target + 1) == 0) {
                *obj_ptr = replay_spawned[index].obj;
                return true;
            }
        }
        tig_debug_printf("Replay: command %d names '%s', which no spawn row made\n",
            replay_next, command->target);
        return false;
    }

    if (!objid_id_from_str(&oid, command->target)) {
        tig_debug_printf("Replay: command %d names an unreadable target '%s'\n",
            replay_next, command->target);
        return false;
    }

    *obj_ptr = obj_pool_perm_lookup(oid);
    if (*obj_ptr == OBJ_HANDLE_NULL) {
        tig_debug_printf("Replay: command %d targets '%s', absent from this run\n",
            replay_next, command->target);
        return false;
    }

    return true;
}

void replay_ui_tick(void)
{
    ReplayCommand* command;
    TargetDescriptor td;
    int64_t pc_obj;
    int64_t target_obj;

    if (!replay_loaded || replay_next >= replay_count) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    command = &replay_commands[replay_next];

    // Real-time session A: a wait holds the queue for its time and nothing
    // else. Measured on the engine's own timer, which the deterministic clock
    // drives, so a scripted wait is the same length in every repeat.
    if (command->kind == REPLAY_KIND_WAIT) {
        if (!replay_waiting) {
            replay_waiting = true;
            tig_timer_now(&replay_wait_started);
            return;
        }
        if (tig_timer_elapsed(replay_wait_started) < command->wait_ms) {
            return;
        }
        replay_waiting = false;
        replay_next++;
        return;
    }

    // Creature profiles (2026-10-22): a hold too, until its critter is dead.
    // A destroyed or freed critter counts as dead - a corpse can decay.
    if (command->kind == REPLAY_KIND_AWAIT_DEATH) {
        if (!replay_resolve(command, &target_obj)) {
            replay_next++;
            return;
        }
        if (obj_handle_is_valid(target_obj)
            && (obj_field_int32_get(target_obj, OBJ_F_FLAGS) & OF_DESTROYED) == 0
            && !critter_is_dead(target_obj)) {
            return;
        }
        replay_stall_reported = false;
        replay_next++;
        tig_debug_printf("Replay: command %d: '%s' is dead\n", replay_next - 1, command->target);
        return;
    }

    // One command at a time, and only while the actor is free. A command
    // submitted over a running goal would be refused or would replace it, and
    // either way the replay would stop matching what it is reproducing.
    //
    // `combat_ui_actor_ready` rather than `anim_is_busy`, which this read until
    // 2026-09-15: a standing critter is busy with its fidget goal, so the
    // stricter test could hold a command forever in a state the engine itself
    // treats as actionable - including the end-turn press, whose own condition
    // is exactly this one. Two notions of "free" in one path is the drift this
    // card has already been bitten by twice.
    // Real-time session A: `now` is the exception - a click during a wind-up
    // or a recovery, which a player can make and cancelling and queueing are
    // about. Everything else below still holds.
    if (!command->now && !combat_ui_actor_ready(pc_obj)) {
        return;
    }

    // The UI drops every message when a turn-based encounter is running and
    // the turn is not the PC's (`intgame_process_event`, first statement). A
    // replay calls the dispatch directly and so is not bound by that gate
    // unless it holds the same line here - and acting on another critter's
    // turn is something no player could do, so no recording can contain it.
    if (combat_turn_based_is_active()
        && combat_turn_based_whos_turn_get() != pc_obj) {
        return;
    }

    // Creature profiles (2026-10-22): a spawn is a test row, not a click, so
    // there is no other action the mode could turn it into, and it ignores
    // its turn as `wait` and `await-death` do. Held to its turn, a
    // playtest's next creature waited for the encounter to end, which the
    // range's own wolves, fleeing the player, never let happen.
    if (command->kind != REPLAY_KIND_SPAWN && !replay_mode_matches(command)) {
        // Say it once - the flag is cleared only when a command is actually
        // submitted, not on the way past this gate. It used to be cleared here,
        // which meant a stall the *next* check caught reported itself every
        // frame: 36 017 identical lines and a 4.3 MB log, measured 2026-09-15.
        //
        // A replay that ends with commands outstanding did not
        // reproduce its recording, and `replay_ui_remaining` is what the run
        // record reads - but a reader of the log deserves to know why it
        // stopped rather than inferring it from a count.
        if (!replay_stall_reported) {
            replay_stall_reported = true;
            tig_debug_printf("Replay: holding command %d (recorded at turn %d);"
                             " turn-based combat is %s\n",
                replay_next, command->turn,
                combat_turn_based_is_active() ? "active" : "not active");
        }
        return;
    }

    if (command->kind == REPLAY_KIND_KEY) {
        // A key action goes through the same UI entry point the keyboard uses,
        // so the recorder sees it exactly as it saw the original press.
        replay_stall_reported = false;
        replay_next++;
        if (!replay_key_action(command->target)) {
            tig_debug_printf("Replay: command %d names an unknown action '%s'\n",
                replay_next - 1, command->target);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_LEARN) {
        replay_stall_reported = false;
        replay_next++;
        if (command->count == 1) {
            tig_debug_printf("Replay: command %d learn %d force: %s\n", replay_next - 1,
                command->spell, spell_add(pc_obj, command->spell, true) ? "known" : "REFUSED");
            return;
        }
        spell_ui_add(pc_obj, command->spell);
        // The character screen refuses on screen - level, Intelligence,
        // Willpower, the college's order - which a script never sees. Said
        // here, because a cast of a spell never learned still plays its
        // animation and reads as a cast that did nothing (measured 2026-09-23).
        if (!spell_is_known(pc_obj, command->spell)) {
            tig_debug_printf("Replay: command %d could not learn spell %d"
                             " (level %d, Intelligence %d, Willpower %d)\n",
                replay_next - 1, command->spell,
                stat_level_get(pc_obj, STAT_LEVEL),
                stat_level_get(pc_obj, STAT_INTELLIGENCE),
                stat_level_get(pc_obj, STAT_WILLPOWER));
        }
        return;
    }

    if (command->kind == REPLAY_KIND_GIVE) {
        int64_t proto_obj;
        int64_t item_obj;
        int given;

        replay_stall_reported = false;
        replay_next++;
        proto_obj = replay_prototype(command->proto);
        if (proto_obj == OBJ_HANDLE_NULL) {
            tig_debug_printf("Replay: command %d names no prototype %d\n",
                replay_next - 1, command->proto);
            return;
        }
        for (given = 0; given < command->count; given++) {
            if (!object_create(proto_obj, obj_field_int64_get(pc_obj, OBJ_F_LOCATION), &item_obj)
                || !item_transfer(item_obj, pc_obj)) {
                tig_debug_printf("Replay: command %d gave %d of %d\n",
                    replay_next - 1, given, command->count);
                return;
            }
        }
        return;
    }

    if (command->kind == REPLAY_KIND_SAVE) {
        replay_stall_reported = false;
        replay_next++;
        tig_debug_printf("Replay: command %d save %s: %s\n", replay_next - 1,
            command->target, gamelib_save(command->target, "replay") ? "saved" : "FAILED");
        return;
    }

    if (command->kind == REPLAY_KIND_INSPECT) {
        replay_stall_reported = false;
        replay_next++;
        if (command->stat == 1) {
            replay_inspect_awareness(pc_obj);
        } else if (command->stat == 2) {
            replay_inspect_xp(pc_obj);
        } else {
            replay_inspect_guns(pc_obj);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_JAM) {
        int64_t weapon_obj;

        replay_stall_reported = false;
        replay_next++;
        weapon_obj = item_wield_get(pc_obj, ITEM_INV_LOC_WEAPON);
        if (!reload_jams_apply(pc_obj, weapon_obj)) {
            tig_debug_printf("Replay: command %d could not jam: no gun under `firearm fumbles` wielded\n",
                replay_next - 1);
            return;
        }
        reload_jam(pc_obj, weapon_obj);
        tig_debug_printf("Replay: command %d jammed the wielded gun\n", replay_next - 1);
        return;
    }

    if (command->kind == REPLAY_KIND_SPAWN) {
        int64_t proto_obj;
        int64_t loc;
        int64_t spawned_obj;
        char identity[COMBAT_OBSERVER_IDENTITY_SIZE];

        replay_stall_reported = false;
        replay_next++;
        proto_obj = proto_get_by_description(command->proto - 20);
        loc = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
        loc = location_make(LOCATION_GET_X(loc) + command->dx, LOCATION_GET_Y(loc) + command->dy);
        if (proto_obj == OBJ_HANDLE_NULL
            || replay_spawned_count >= REPLAY_MAX_SPAWNED
            || !object_create(proto_obj, loc, &spawned_obj)) {
            tig_debug_printf("Replay: command %d could not spawn prototype %d\n",
                replay_next - 1, command->proto);
            return;
        }
        snprintf(replay_spawned[replay_spawned_count].name,
            sizeof(replay_spawned[replay_spawned_count].name), "%s", command->target);
        replay_spawned[replay_spawned_count].obj = spawned_obj;
        replay_spawned_count++;
        // Into a running encounter's turn order, as a critter that reacts to
        // danger joins it (`ai.c`); otherwise it stands idle until something
        // draws it in (measured 2026-10-22).
        if (combat_turn_based_is_active()) {
            combat_turn_based_add_critter(spawned_obj);
        }
        combat_observer_object_id(spawned_obj, identity);
        tig_debug_printf("Replay: command %d spawned '%s' (prototype %d) as %s\n",
            replay_next - 1, command->target, command->proto, identity);
        return;
    }

    if (command->kind == REPLAY_KIND_REMOVE) {
        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &target_obj)) {
            return;
        }
        if (target_obj == pc_obj) {
            tig_debug_printf("Replay: command %d will not remove the player\n", replay_next - 1);
            return;
        }
        object_destroy(target_obj);
        tig_debug_printf("Replay: command %d removed '%s'\n", replay_next - 1, command->target);
        return;
    }

    if (command->kind == REPLAY_KIND_EXAMINE) {
        char text[2000];
        const char* hint;

        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &target_obj)) {
            return;
        }
        if (!tb_ui_build_examine_text(pc_obj, target_obj, text)) {
            tig_debug_printf("Replay: command %d: '%s' refuses to be examined\n",
                replay_next - 1, command->target);
            return;
        }
        intgame_examine_object(pc_obj, target_obj, text);
        hint = creature_profile_examine_hint(creature_rules_profile(target_obj));
        tig_debug_printf("Replay: command %d examines '%s': %s; in place of its reaction: %s\n",
            replay_next - 1, command->target, text, hint != NULL ? hint : "-");
        return;
    }

    if (command->kind == REPLAY_KIND_INVULNERABLE) {
        replay_stall_reported = false;
        replay_next++;
        obj_field_int32_set(pc_obj, OBJ_F_FLAGS,
            obj_field_int32_get(pc_obj, OBJ_F_FLAGS) | OF_INVULNERABLE);
        tig_debug_printf("Replay: command %d made the player invulnerable\n", replay_next - 1);
        return;
    }

    if (command->kind == REPLAY_KIND_AFFLICT) {
        replay_stall_reported = false;
        replay_next++;
        crit_effects_apply_for(pc_obj, (unsigned int)command->proto, SPELL_CRITICAL_EFFECT_TURNS);
        tig_debug_printf("Replay: command %d afflicted the player (effect 0x%x)\n",
            replay_next - 1, (unsigned int)command->proto);
        return;
    }

    if (command->kind == REPLAY_KIND_TRANSFER || command->kind == REPLAY_KIND_TAKE) {
        int64_t item_obj;

        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &target_obj)) {
            return;
        }
        item_obj = replay_find_carried(command->kind == REPLAY_KIND_TAKE ? target_obj : pc_obj,
            command->proto, false);
        if (item_obj == OBJ_HANDLE_NULL
            || !item_transfer(item_obj, command->kind == REPLAY_KIND_TAKE ? pc_obj : target_obj)) {
            tig_debug_printf("Replay: command %d could not move prototype %d\n",
                replay_next - 1, command->proto);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_POISON) {
        int64_t victim_obj = pc_obj;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] != '\0' && !replay_resolve(command, &victim_obj)) {
            return;
        }
        stat_base_set(victim_obj, STAT_POISON_LEVEL, command->count);
        tig_debug_printf("Replay: command %d set poison level %d; it reads %d\n",
            replay_next - 1, command->count, stat_base_get(victim_obj, STAT_POISON_LEVEL));
        return;
    }

    if (command->kind == REPLAY_KIND_XP) {
        replay_stall_reported = false;
        replay_next++;
        critter_give_xp(pc_obj, command->count);
        tig_debug_printf("Replay: command %d gave %d experience; level %d\n",
            replay_next - 1, command->count, stat_level_get(pc_obj, STAT_LEVEL));
        return;
    }

    if (command->kind == REPLAY_KIND_ENCOUNTER) {
        replay_stall_reported = false;
        replay_next++;
        tig_debug_printf("Replay: command %d encounter from table %d: %s\n",
            replay_next - 1, command->count,
            wmap_rnd_force_encounter(command->count, command->aim) ? "spawned" : "nothing spawned");
        return;
    }

    if (command->kind == REPLAY_KIND_HOUR) {
        DateTime datetime;
        int steps;

        replay_stall_reported = false;
        replay_next++;
        datetime_from_milliseconds(&datetime, 3600000);
        for (steps = 0; steps < 24 && datetime_current_hour() != command->count; steps++) {
            timeevent_inc_datetime(&datetime);
        }
        tig_debug_printf("Replay: command %d advanced the clock %d hour(s); it reads hour %d\n",
            replay_next - 1, steps, datetime_current_hour());
        return;
    }

    if (command->kind == REPLAY_KIND_SCREENSHOT) {
        replay_stall_reported = false;
        replay_next++;
        tig_debug_printf("Replay: command %d takes screenshot '%s'\n",
            replay_next - 1, command->target);
        tig_video_screenshot_make();
        return;
    }

    if (command->kind == REPLAY_KIND_HOVER) {
        int64_t hover_obj;
        bool accepted;

        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &hover_obj)) {
            return;
        }
        accepted = combat_check_attack(pc_obj, hover_obj);
        tig_debug_printf("Replay: command %d hovers: cost %d, %d AP left, %s, exertion fatigue %d\n",
            replay_next - 1, combat_required_action_points_get(),
            combat_turn_based_is_active() ? combat_action_points_get() : -1,
            accepted ? "accepted" : "refused", combat_exertion_preview_fatigue());
        return;
    }

    if (command->kind == REPLAY_KIND_TRAIN) {
        int64_t who_obj = pc_obj;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] != '\0' && !replay_resolve(command, &who_obj)) {
            return;
        }
        basic_skill_training_set(who_obj, command->skill, command->count);
        tig_debug_printf("Replay: command %d set basic skill %d training to %d; it reads %d\n",
            replay_next - 1, command->skill, command->count,
            basic_skill_training_get(who_obj, command->skill));
        return;
    }

    if (command->kind == REPLAY_KIND_USE) {
        int64_t item_obj;
        int64_t target_obj = pc_obj;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] != '\0' && !replay_resolve(command, &target_obj)) {
            return;
        }
        item_obj = replay_find_carried(pc_obj, command->proto, false);
        if (item_obj == OBJ_HANDLE_NULL) {
            tig_debug_printf("Replay: command %d carries no prototype %d to use\n",
                replay_next - 1, command->proto);
            return;
        }
        tig_debug_printf("Replay: command %d uses prototype %d\n",
            replay_next - 1, command->proto);
        item_use_on_obj(pc_obj, item_obj, target_obj);
        return;
    }

    if (command->kind == REPLAY_KIND_WEAR) {
        int64_t item_obj;
        int64_t wearer_obj = pc_obj;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] != '\0' && !replay_resolve(command, &wearer_obj)) {
            return;
        }
        item_obj = replay_find_carried(wearer_obj, command->proto, false);
        // Armor, or (2026-10-19) a generic item held in the torch/shield
        // place - a Lantern - which `item_location_get` places there.
        if (item_obj == OBJ_HANDLE_NULL
            || (obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_ARMOR
                && !(obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_GENERIC
                    && item_location_get(item_obj) == ITEM_INV_LOC_SHIELD))
            || !item_wield_set(item_obj, item_location_get(item_obj))) {
            tig_debug_printf("Replay: command %d could not wear prototype %d\n",
                replay_next - 1, command->proto);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_DROP) {
        int64_t item_obj;

        replay_stall_reported = false;
        replay_next++;
        item_obj = replay_find_carried(pc_obj, command->proto, false);
        if (item_obj == OBJ_HANDLE_NULL || !item_drop(item_obj)) {
            tig_debug_printf("Replay: command %d could not drop prototype %d\n",
                replay_next - 1, command->proto);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_PICKUP) {
        ObjectList objects;
        ObjectNode* node;
        int64_t proto_obj;
        bool moved = false;

        replay_stall_reported = false;
        replay_next++;
        proto_obj = replay_prototype(command->proto);
        object_list_location(obj_field_int64_get(pc_obj, OBJ_F_LOCATION), OBJ_TM_WEAPON, &objects);
        for (node = objects.head; node != NULL; node = node->next) {
            if (obj_field_handle_get(node->obj, OBJ_F_PROTOTYPE_HANDLE) == proto_obj) {
                moved = item_transfer(node->obj, pc_obj);
                break;
            }
        }
        object_list_destroy(&objects);
        if (!moved) {
            tig_debug_printf("Replay: command %d found no prototype %d to pick up\n",
                replay_next - 1, command->proto);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_PROWL) {
        bool was_prowling;

        replay_stall_reported = false;
        replay_next++;
        was_prowling = critter_is_concealed(pc_obj);
        if (was_prowling != (command->prowl != 0)) {
            skill_ui_begin(pc_obj, 0);
        }
        tig_debug_printf("Replay: command %d prowl %s; the player was %s and is"
                         " %s, combat mode %s\n",
            replay_next - 1, command->prowl ? "on" : "off",
            was_prowling ? "prowling" : "not prowling",
            critter_is_concealed(pc_obj) ? "prowling" : "not prowling",
            combat_critter_is_combat_mode_active(pc_obj) ? "on" : "off");
        return;
    }

    if (command->kind == REPLAY_KIND_PANEL) {
        TigMessage click;
        int x;
        int y;

        replay_stall_reported = false;
        replay_next++;
        if (!called_shot_control_point(command->aim, &x, &y)) {
            tig_debug_printf("Replay: command %d: the called-shot panel is not"
                             " shown; no click sent\n",
                replay_next - 1);
            return;
        }
        memset(&click, 0, sizeof(click));
        click.type = TIG_MESSAGE_MOUSE;
        click.data.mouse.x = x;
        click.data.mouse.y = y;
        click.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;
        tig_message_enqueue(&click);
        click.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;
        tig_message_enqueue(&click);
        tig_debug_printf("Replay: command %d clicked the called-shot panel at"
                         " %d,%d for limb %d\n",
            replay_next - 1, x, y, command->aim);
        return;
    }

    if (command->kind == REPLAY_KIND_RAISE) {
        int value;
        int cost;
        int unspent;

        replay_stall_reported = false;
        replay_next++;
        // The plus button's rule (`charedit_ui.c`): refused at the stat's
        // maximum or without the points, and charged `stat_cost` of the new
        // value.
        value = stat_base_get(pc_obj, command->stat) + 1;
        cost = stat_cost(value);
        unspent = stat_level_get(pc_obj, STAT_UNSPENT_POINTS);
        if (stat_level_get(pc_obj, command->stat) >= stat_level_max(pc_obj, command->stat)
            || cost > unspent) {
            tig_debug_printf("Replay: command %d could not raise stat %d"
                             " (%d unspent points, cost %d)\n",
                replay_next - 1, command->stat, unspent, cost);
            return;
        }
        stat_base_set(pc_obj, command->stat, value);
        stat_base_set(pc_obj, STAT_UNSPENT_POINTS, unspent - cost);
        return;
    }

    if (command->kind == REPLAY_KIND_STAT_LEVEL) {
        int64_t who_obj = pc_obj;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] != '\0' && !replay_resolve(command, &who_obj)) {
            return;
        }
        // `stat_base_set` refuses to lower a stat below what the character's
        // skills, spells or technologies need: lower those first.
        stat_base_set(who_obj, command->stat, command->count);
        tig_debug_printf("Replay: command %d set stat %d to %d%s; its level is %d\n",
            replay_next - 1, command->stat, stat_base_get(who_obj, command->stat),
            stat_base_get(who_obj, command->stat) == command->count
                ? ""
                : " (refused: a skill, spell or technology needs more)",
            stat_level_get(who_obj, command->stat));
        return;
    }

    if (command->kind == REPLAY_KIND_SKILL_LEVEL) {
        int set;
        int64_t who_obj = pc_obj;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] != '\0' && !replay_resolve(command, &who_obj)) {
            return;
        }
        if (IS_TECH_SKILL(command->skill)) {
            set = tech_skill_points_set(who_obj, GET_TECH_SKILL(command->skill), command->count);
        } else {
            set = basic_skill_points_set(who_obj, GET_BASIC_SKILL(command->skill), command->count);
        }
        tig_debug_printf("Replay: command %d set skill %d to %d point(s)%s; its level is %d\n",
            replay_next - 1, command->skill, set,
            set == command->count ? "" : " (refused by its key stat)",
            IS_TECH_SKILL(command->skill)
                ? tech_skill_level(who_obj, GET_TECH_SKILL(command->skill))
                : basic_skill_level(who_obj, GET_BASIC_SKILL(command->skill)));
        return;
    }

    if (command->kind == REPLAY_KIND_SKILL) {
        replay_stall_reported = false;
        replay_next++;
        if (!replay_resolve(command, &target_obj)) {
            return;
        }
        if (!skill_use(pc_obj, command->skill, target_obj, 0)) {
            tig_debug_printf("Replay: command %d could not use skill %d on '%s'\n",
                replay_next - 1, command->skill, command->target);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_THROW) {
        int64_t item_obj;
        int64_t throw_loc = command->loc;

        replay_stall_reported = false;
        replay_next++;
        if (command->target[0] == '@') {
            if (!replay_resolve(command, &target_obj)) {
                return;
            }
            throw_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
        }
        // A thrown item comes from the pack, not the hands.
        item_obj = replay_find_carried(pc_obj, command->proto, true);
        if (item_obj == OBJ_HANDLE_NULL) {
            return;
        }
        // As the drag it stands for asks first (`intgame.c`).
        if (combat_throw_refused(pc_obj)) {
            tig_debug_printf("Replay: command %d throw refused: not enough action points\n",
                replay_next - 1);
            return;
        }
        if (!anim_goal_throw_item(pc_obj, item_obj, throw_loc)) {
            tig_debug_printf("Replay: command %d could not throw prototype %d\n",
                replay_next - 1, command->proto);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_WIELD) {
        int proto;

        replay_stall_reported = false;
        replay_next++;
        if (replay_parse_int(command->target, &proto) && proto > 0) {
            target_obj = replay_find_carried(pc_obj, proto, false);
            if (target_obj == OBJ_HANDLE_NULL) {
                return;
            }
        } else if (!replay_resolve(command, &target_obj)) {
            return;
        }
        if (!intgame_equip_weapon(target_obj)) {
            tig_debug_printf("Replay: command %d could not wield '%s'\n",
                replay_next - 1, command->target);
        }
        return;
    }

    if (command->kind == REPLAY_KIND_CAST) {
        memset(&td, 0, sizeof(td));
        if (command->cast_target == REPLAY_CAST_LOCATION) {
            td.is_loc = 1;
            td.loc = command->loc;
        } else if (command->cast_target == REPLAY_CAST_SELF) {
            td.obj = pc_obj;
        } else {
            if (!replay_resolve(command, &target_obj)) {
                replay_next++;
                return;
            }
            td.obj = target_obj;
        }

        replay_stall_reported = false;
        replay_next++;

        // Only a spell the character knows: the spell button lists nothing
        // else, and `spell_ui_activate` does not check - measured 2026-09-23,
        // a cast of an unlearned spell played its animation and did nothing.
        if (!spell_is_known(pc_obj, command->spell)) {
            tig_debug_printf("Replay: command %d casts spell %d, which is not known\n",
                replay_next - 1, command->spell);
            return;
        }

        // The spell button, then the click. A spell whose target is the caster
        // is applied by activation itself, as the UI does; anything else is
        // applied through the click path, which checks the target the way a
        // click would. Left in spell mode - a target the click path refused -
        // the replay backs out as a right-click would.
        if (spell_ui_activate(pc_obj, command->spell) == SPELL_UI_ACTIVATE_OK
            && intgame_mode_get() == INTGAME_MODE_SPELL) {
            intgame_apply_mode_to_target(&td);
            if (intgame_mode_get() == INTGAME_MODE_SPELL) {
                spell_ui_cancel();
            }
        }
        return;
    }

    // Real-time session B: a tile is walked to, the way a click on it walks -
    // `anim_goal_move_to_tile`, which is what the UI's own tile click calls. It
    // used to go through the target dispatch below, which a real tile click
    // never reaches: that dispatch reads the target's object type before asking
    // whether it is a tile, and `TargetDescriptor` keeps `obj` and `loc` in one
    // union, so the location was read as an object handle and the game crashed
    // (measured 2026-09-22, SIGSEGV in obj_field_int32_get). No recording ever
    // contained a location row - movement is not recorded - so none replayed.
    if (command->kind == REPLAY_KIND_LOCATION) {
        replay_stall_reported = false;
        replay_next++;
        anim_goal_move_to_tile(pc_obj, command->loc);
        return;
    }

    memset(&td, 0, sizeof(td));
    // An unreadable identity, or a recorded object not in this run, is a real
    // divergence - a replay cannot act on something absent - so
    // `replay_resolve` says so and the replay moves on rather than silently
    // skipping to a different target. A spawned critter's name resolves too.
    if (!replay_resolve(command, &target_obj)) {
        replay_next++;
        return;
    }

    if (!replay_target_is_live(target_obj)) {
        // Held rather than skipped, and held rather than submitted. The
        // recording's command named a living target; this run's is dead, so
        // the replay has already diverged from what it is reproducing.
        // Leaving it outstanding is what makes that visible - the engine
        // reports `replay outstanding=N` on the way out and `compare`
        // refuses a replay that ended holding commands.
        if (!replay_stall_reported) {
            replay_stall_reported = true;
            tig_debug_printf("Replay: holding command %d; its target '%s'"
                             " is not a target the click path would give\n",
                replay_next, command->target);
        }
        return;
    }

    td.is_loc = 0;
    td.obj = target_obj;

    replay_stall_reported = false;
    replay_next++;

    // The recorded aim is in force only across this dispatch. Left set, it
    // would silently aim the player's own next click.
    if (command->aim >= 0) {
        called_shot_replay_aim_set(command->aim);
    }
    intgame_perform_action_on_target(&td);
    called_shot_replay_aim_clear();
}
