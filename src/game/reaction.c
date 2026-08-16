#include "game/reaction.h"

#include "game/ai.h"
#include "game/critter.h"
#include "game/effect.h"
#include "game/fate.h"
#include "game/magictech.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/reputation.h"
#include "game/stat.h"
#include "game/ui.h"

static int reaction_get_ex(int64_t a1, int64_t a2, unsigned int flags);
static int reaction_stored_get(int64_t a1, int64_t a2);
static int reaction_get_base(int64_t obj);
static bool reaction_slot_find(int64_t a1, int64_t a2, bool a3, int* a4);
static void reaction_set(int64_t npc_obj, int64_t pc_obj, int value);
static void reaction_slot_set(int64_t npc_obj, int64_t pc_obj, int level, int index);
static int reaction_modifiers_get(int64_t npc_obj, int64_t pc_obj, unsigned int flags);
static int reaction_neutral_distance(int a1);

// 0x5FC88C
static mes_file_handle_t reaction_mes_file;

// 0x5FC890
static char* reaction_names[REACTION_COUNT];

// 0x5B684C
static int reaction_race_modifier_tbl[RACE_COUNT][8] = {
    {
        0,
        0,
        5,
        5,
        0,
        10,
        -5,
        -5,
    },
    {
        -5,
        10,
        -10,
        -5,
        0,
        5,
        -10,
        -15,
    },
    {
        -10,
        -20,
        0,
        0,
        -10,
        10,
        -15,
        -10,
    },
    {
        0,
        0,
        0,
        0,
        0,
        5,
        -5,
        -5,
    },
    {
        0,
        0,
        0,
        0,
        5,
        5,
        -10,
        5,
    },
    {
        0,
        0,
        20,
        15,
        0,
        10,
        -10,
        -10,
    },
    {
        0,
        -20,
        -15,
        -10,
        -10,
        0,
        10,
        0,
    },
    {
        0,
        -20,
        -15,
        -5,
        20,
        10,
        0,
        5,
    },
    {
        -30,
        -30,
        5,
        -20,
        -30,
        -5,
        -30,
        -30,
    },
    {
        -10,
        -30,
        -20,
        -15,
        -20,
        -20,
        0,
        -10,
    },
    {
        -20,
        -30,
        -40,
        -30,
        -20,
        -20,
        10,
        0,
    },
};

// 0x4C0BD0
bool reaction_init(GameInitInfo* init_info)
{
    MesFileEntry mes_file_entry;
    int index;

    (void)init_info;

    if (!mes_load("mes\\reaction.mes", &reaction_mes_file)) {
        return false;
    }

    for (index = 0; index < REACTION_COUNT; index++) {
        mes_file_entry.num = index;
        mes_get_msg(reaction_mes_file, &mes_file_entry);
        reaction_names[index] = mes_file_entry.str;
    }

    return true;
}

// 0x4C0C30
void reaction_exit(void)
{
    mes_unload(reaction_mes_file);
}

// 0x4C0C40
bool reaction_met_before(int64_t npc_obj, int64_t pc_obj)
{
    int v1;

    if (npc_obj == pc_obj) {
        return true;
    }

    // Make sure `pc_obj` is actually PC.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return false;
    }

    // Make sure `npc_obj` is actually NPC.
    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return false;
    }

    return reaction_slot_find(npc_obj, pc_obj, false, &v1);
}

// 0x4C0CC0
int reaction_get(int64_t npc_obj, int64_t pc_obj)
{
    return reaction_get_ex(npc_obj, pc_obj, 0);
}

// The value ai.c uses for hostility decisions (attack/peace thresholds).
// Only differs from `reaction_get` by flags bit 0x1, which floors the
// race/stat-modifier term at -49 inside `reaction_modifiers_get`.
//
// 0x4C0CE0
int reaction_get_for_ai(int64_t npc_obj, int64_t pc_obj)
{
    return reaction_get_ex(npc_obj, pc_obj, 1);
}

// 0x4C0D00
int reaction_get_ex(int64_t npc_obj, int64_t pc_obj, unsigned int flags)
{
    int value;
    int64_t mind_controlled_by_obj;

    if (ai_shitlist_has(npc_obj, pc_obj)) {
        return 0;
    }

    // Make sure `pc_obj` is actually PC.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return 50;
    }

    // Make sure `npc_obj` is actually NPC.
    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return 50;
    }

    value = reaction_modifiers_get(npc_obj, pc_obj, flags) + reaction_stored_get(npc_obj, pc_obj);
    value = effect_adjust_reaction(npc_obj, value);

    if (value < 50
        && magictech_find_effect_caster(npc_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)
        && mind_controlled_by_obj == pc_obj) {
        value = 50;
    }

    return value;
}

// 0x4C0DE0
void reaction_adj(int64_t npc_obj, int64_t pc_obj, int value)
{
    reaction_adj_ex(npc_obj, pc_obj, value, true);
}

// CE: There is a bug when temporary reaction adjustments must be reversible,
// notably with the Charm spell. The bug stems from applying separate modifiers
// for negative and positive adjustments that are not symmetric. For example, a
// PC gnome has `badreactionadj +10`. When Charm is applied it grants +30 bonus
// to reaction. When it wears off the net reduction becomes -20 (-30 from Charm
// and +10 from being a gnome), thus granting permanent increase in reaction.
// The fix is to add a parameter that controls whether a reaction adjustment
// should be affected by effects or not.
void reaction_adj_ex(int64_t npc_obj, int64_t pc_obj, int value, bool apply_effects)
{
    int64_t mind_controlled_by_obj;
    int base;
    int adjusted_value;
    unsigned int flags;

    if (value == 0) {
        return;
    }

    // Make sure `pc_obj` is actually PC.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    // Make sure `npc_obj` is actually NPC.
    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    if (magictech_find_effect_caster(npc_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)
        && mind_controlled_by_obj == pc_obj) {
        return;
    }

    if (value < 0
        && stat_is_extraordinary(pc_obj, STAT_CHARISMA)
        && critter_pc_leader_get(npc_obj) == pc_obj) {
        return;
    }

    base = reaction_stored_get(npc_obj, pc_obj);
    if (apply_effects) {
        adjusted_value = effect_adjust_good_bad_reaction(pc_obj, value);
    } else {
        adjusted_value = value;
    }
    reaction_set(npc_obj, pc_obj, base + adjusted_value);

    if (value < 0 && critter_leader_get(npc_obj) == pc_obj) {
        flags = obj_field_int32_get(npc_obj, OBJ_F_NPC_FLAGS);
        flags |= ONF_CHECK_LEADER;
        obj_field_int32_set(npc_obj, OBJ_F_NPC_FLAGS, flags);

        flags = obj_field_int32_get(npc_obj, OBJ_F_CRITTER_FLAGS2);
        flags |= OCF2_CHECK_REACTION_BAD;
        obj_field_int32_set(npc_obj, OBJ_F_CRITTER_FLAGS2, flags);
    }
}

// 0x4C0F50
void reaction_forget(int64_t npc_obj, int64_t pc_obj)
{
    int index;

    for (index = 0; index < 10; index++) {
        if (index != 0
            && obj_arrayfield_handle_get(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index) == pc_obj) {
            obj_arrayfield_obj_set(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index, OBJ_HANDLE_NULL);
            break;
        }
    }

    combat_recalc_reaction(npc_obj);
}

// 0x4C0FC0
int reaction_translate(int value)
{
    if (value <= 0) return REACTION_HATRED;
    if (value <= 20) return REACTION_DISLIKE;
    if (value <= 40) return REACTION_SUSPICIOUS;
    if (value <= 60) return REACTION_NEUTRAL;
    if (value <= 80) return REACTION_COURTEOUS;
    if (value <= 100) return REACTION_AMIABLE;
    return REACTION_LOVE;
}

// 0x4C1010
const char* reaction_get_name(int reaction)
{
    return reaction_names[reaction];
}

// Called when a conversation starts (dialog.c): slot 0 becomes this PC's
// current reaction, bumped to 100 by the FORCE_GOOD_REACTION fate when it is
// below that. `reaction_current_reset` is the matching end-of-conversation
// reset.
//
// 0x4C1020
void reaction_current_set(int64_t npc_obj, int64_t pc_obj)
{
    int v1;

    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    v1 = reaction_stored_get(npc_obj, pc_obj);
    if (v1 < 100 && fate_resolve(pc_obj, FATE_FORCE_GOOD_REACTION)) {
        v1 = 100;
    }

    reaction_slot_set(npc_obj, pc_obj, v1, 0);
}

// End of a conversation (dialog.c): slot 0 goes back to (nobody, base) and
// the value it held is remembered in this PC's reaction slot. The matching
// start-of-conversation setter is `reaction_current_set`.
//
// 0x4C10A0
void reaction_current_reset(int64_t npc_obj, int64_t pc_obj)
{
    int v1;

    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    v1 = reaction_stored_get(npc_obj, pc_obj);
    reaction_slot_set(npc_obj, 0, v1, 0);
    reaction_set(npc_obj, pc_obj, v1);
}

// 0x4C1110
int64_t reaction_current_pc_get(int64_t npc)
{
    if (obj_field_int32_get(npc, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return 0;
    }

    return obj_arrayfield_handle_get(npc, OBJ_F_NPC_REACTION_PC_IDX, 0);
}

// Reaction-to-price mapping, settled against 0x4C1150 (the transcription is
// exact: `cmp 0x64` clamps to 100, and the 0x66666667 /5 and 0x51EB851F /100
// magic divisions are the two `v2` formulas). Percent of `a3` by reaction:
// <= 0 (hatred) 200%, climbing to 102% just below 50, 100% at neutral,
// then down to 80% at 100 and above. Hating NPCs charge double; loving ones
// give 20% off.
//
// 0x4C1150
int reaction_price_scale(int64_t a1, int64_t a2, int a3)
{
    int v1;
    int v2;

    v1 = reaction_get(a1, a2);
    if (v1 > 100) {
        v1 = 100;
    } else {
        if (v1 < 0) {
            v2 = 200;
            return a3 * v2 / 100;
        }

        if (v1 < 50) {
            v2 = 2 * (100 - v1);
            return a3 * v2 / 100;
        }
    }

    v2 = 120 - 2 * v1 / 5;
    return a3 * v2 / 100;
}

// 0x4C11D0
void reaction_bargain_adj(int64_t a1, int64_t a2, int a3)
{
    int v1;

    if (a3 < 0) {
        v1 = a3 / 100;
        if (v1 == 0) {
            v1 = -2;
        } else if (v1 < -30) {
            v1 = -30;
        }
    } else if (a3 > 50
        && reaction_translate(reaction_get(a1, a2)) < REACTION_AMIABLE) {
        if (a3 < 200) {
            v1 = a3 / 40;
        } else if (a3 < 1000) {
            v1 = (a3 - 200) / 100 + 5;
        } else {
            v1 = 15;
        }
    } else {
        v1 = 0;
    }

    if (v1 != 0) {
        reaction_adj(a1, a2, v1);
    }
}

// 0x4C1290
int reaction_stored_get(int64_t npc_obj, int64_t pc_obj)
{
    int v1;

    if (reaction_slot_find(npc_obj, pc_obj, 1, &v1)) {
        return v1;
    }

    return reaction_get_base(npc_obj);
}

// 0x4C12D0
int reaction_get_base(int64_t obj)
{
    return obj_field_int32_get(obj, OBJ_F_NPC_REACTION_BASE);
}

// 0x4C12F0
bool reaction_slot_find(int64_t npc_obj, int64_t pc_obj, bool a3, int* a4)
{
    int index;

    for (index = 0; index < 10; index++) {
        if ((index != 0 || a3)
            && pc_obj == obj_arrayfield_handle_get(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index)) {
            *a4 = obj_arrayfield_uint32_get(npc_obj, OBJ_F_NPC_REACTION_LEVEL_IDX, index);
            return true;
        }
    }

    return false;
}

// Store a reaction for a PC: update its existing slot, take the first free
// one, or evict the slot whose level is closest to neutral (least worth
// remembering) when all ten are full. Slot 0 is the current reaction and is
// only written through this path when the PC already owns it.
//
// 0x4C1360
void reaction_set(int64_t npc_obj, int64_t pc_obj, int value)
{
    int index;
    int candidate;
    int64_t obj;
    int v1;
    int reaction_level;
    int v2;
    int v3;

    // NOTE: Silence compiler warning.
    v3 = -1;

    candidate = -1;
    for (index = 0; index < 10; index++) {
        obj = obj_arrayfield_handle_get(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index);
        if (obj == pc_obj) {
            reaction_slot_set(npc_obj, pc_obj, value, index);
            return;
        }

        if (obj == OBJ_HANDLE_NULL
            && index != 0
            && candidate == -1) {
            candidate = index;
        }
    }

    if (candidate != -1) {
        reaction_slot_set(npc_obj, pc_obj, value, candidate);
        return;
    }

    // FIXME: Unused.
    datetime_current_second();

    v1 = reaction_neutral_distance(value);
    for (index = 0; index < 10; index++) {
        if (index != 0) {
            reaction_level = obj_arrayfield_uint32_get(npc_obj, OBJ_F_NPC_REACTION_LEVEL_IDX, index);

            // FIXME: Unused.
            obj_arrayfield_uint32_get(npc_obj, OBJ_F_NPC_REACTION_TIME_IDX, index);

            v2 = reaction_neutral_distance(reaction_level);
            if (v2 < v1 && (candidate == -1 || v2 < v3)) {
                candidate = index;
                v3 = v2;
            }
        }
    }

    if (candidate != -1) {
        reaction_slot_set(npc_obj, pc_obj, value, candidate);
        return;
    }
}

// 0x4C1490
void reaction_slot_set(int64_t npc_obj, int64_t pc_obj, int level, int index)
{
    obj_arrayfield_obj_set(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index, pc_obj);
    obj_arrayfield_uint32_set(npc_obj, OBJ_F_NPC_REACTION_LEVEL_IDX, index, level);
    obj_arrayfield_uint32_set(npc_obj, OBJ_F_NPC_REACTION_TIME_IDX, index, datetime_current_second());
    ui_refresh_health_bar(npc_obj);
    combat_recalc_reaction(npc_obj);
}

// 0x4C1500
int reaction_modifiers_get(int64_t npc_obj, int64_t pc_obj, unsigned int flags)
{
    int v1 = 0;
    int modifier;
    int npc_race;
    int pc_race;

    if ((obj_field_int32_get(npc_obj, OBJ_F_NPC_FLAGS) & ONF_ALOOF) == 0
        && !critter_is_monstrous(npc_obj)) {
        modifier = stat_level_get(pc_obj, STAT_REACTION_MODIFIER);
        npc_race = stat_level_get(npc_obj, STAT_RACE);
        pc_race = stat_level_get(pc_obj, STAT_RACE);

        v1 = reaction_race_modifier_tbl[npc_race][pc_race] + modifier;
        if ((flags & 0x1) != 0) {
            if (v1 < -49) {
                v1 = -49;
            }
        }
    }

    v1 += reputation_reaction_adj(pc_obj, npc_obj);

    return v1;
}

// 0x4C15A0
int reaction_neutral_distance(int a1)
{
    // FIXME: Unused.
    datetime_current_second();

    if (a1 > 50) {
        return a1 - 50;
    } else {
        return 50 - a1;
    }
}
