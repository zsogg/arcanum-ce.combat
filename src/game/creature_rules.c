#include "game/creature_rules.h"

#include <string.h>

#include <tig/tig.h>

#include "game/ce_parse.h"
#include "game/combat.h"
#include "game/crit_effects.h"
#include "game/criticals.h"
#include "game/feature.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/proto.h"

// Creatures the file may list. The shipped game has about 250 creature
// prototypes; the sheet lists those whose profile, chart or protection it
// sets.
#define CREATURE_RULES_MAX 400

static CreatureRule creature_rules[CREATURE_RULES_MAX];
// The prototype object each entry names, found on first use, as the armor
// table does: 0 is not yet looked up, -1 looked up and not found.
static int64_t creature_rule_protos[CREATURE_RULES_MAX];
static int creature_rule_count;

static const char* creature_profile_names[CREATURE_PROFILE_COUNT] = {
    "living",
    "skeletal",
    "fleshy",
    "mechanical",
    "spirit",
    "phantom",
};

static const char* creature_chart_names[CRIT_BODY_TYPE_COUNT] = {
    "biped",
    "quadruped",
    "amorphous",
    "insect",
    "snake",
    "avian",
};

// ---------------------------------------------------------------------------
// The rules, pure.
// ---------------------------------------------------------------------------

static int creature_rule_clamp(int value, int hi)
{
    if (value < 0) {
        return 0;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

// Trims spaces and tabs from both ends, in place.
static char* creature_rule_trim(char* str)
{
    char* end;

    str += strspn(str, " \t");
    end = str + strlen(str);
    while (end > str && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
    return str;
}

static int creature_rule_name_index(const char* value, const char** names, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        if (strcmp(value, names[index]) == 0) {
            return index;
        }
    }
    return CREATURE_RULE_UNSET;
}

bool creature_rule_parse(int id, const char* str, CreatureRule* rule)
{
    CreatureRule parsed;
    char buffer[256];
    char* part;
    char* save;
    char* colon;
    char* key;
    char* text;
    int value;
    bool any = false;

    if (id <= 0 || str == NULL || strlen(str) >= sizeof(buffer)) {
        return false;
    }

    strcpy(buffer, str);
    parsed.id = id;
    parsed.profile = CREATURE_RULE_UNSET;
    parsed.chart = CREATURE_RULE_UNSET;
    parsed.natural = CREATURE_RULE_UNSET;
    parsed.firearm_scale = CREATURE_RULE_UNSET;

    for (part = strtok_r(buffer, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
        colon = strchr(part, ':');
        if (colon == NULL) {
            return false;
        }
        *colon = '\0';
        key = creature_rule_trim(part);
        text = creature_rule_trim(colon + 1);

        if (strcmp(key, "Profile") == 0) {
            parsed.profile = creature_rule_name_index(text, creature_profile_names, CREATURE_PROFILE_COUNT);
            if (parsed.profile == CREATURE_RULE_UNSET) {
                return false;
            }
        } else if (strcmp(key, "Chart") == 0) {
            parsed.chart = creature_rule_name_index(text, creature_chart_names, CRIT_BODY_TYPE_COUNT);
            if (parsed.chart == CREATURE_RULE_UNSET) {
                return false;
            }
        } else if (strcmp(key, "Natural") == 0) {
            if (!ce_parse_int(text, &value)) {
                return false;
            }
            parsed.natural = creature_rule_clamp(value, CREATURE_RULE_VALUE_MAX);
        } else if (strcmp(key, "FirearmScale") == 0) {
            if (!ce_parse_int(text, &value)) {
                return false;
            }
            parsed.firearm_scale = creature_rule_clamp(value, CREATURE_RULE_SCALE_MAX);
        } else {
            return false;
        }
        any = true;
    }

    if (!any) {
        return false;
    }

    *rule = parsed;
    return true;
}

CreatureProfile creature_profile_default(unsigned int critter_flags,
    unsigned int critter_flags2, int chart)
{
    if ((critter_flags & OCF_MECHANICAL) != 0) {
        return CREATURE_PROFILE_MECHANICAL;
    }
    if (chart == CRIT_BODY_TYPE_AMORPHOUS || (critter_flags2 & OCF2_ELEMENTAL) != 0) {
        return CREATURE_PROFILE_SPIRIT;
    }
    if ((critter_flags & OCF_UNDEAD) != 0) {
        return CREATURE_PROFILE_FLESHY;
    }
    return CREATURE_PROFILE_LIVING;
}

bool creature_profile_bodily(CreatureProfile profile)
{
    return profile == CREATURE_PROFILE_LIVING;
}

bool creature_profile_knockdown(CreatureProfile profile)
{
    return profile == CREATURE_PROFILE_LIVING
        || profile == CREATURE_PROFILE_SKELETAL
        || profile == CREATURE_PROFILE_FLESHY
        || profile == CREATURE_PROFILE_MECHANICAL;
}

int creature_profile_knockdown_bonus(CreatureProfile profile, int crit_hit_type)
{
    return profile == CREATURE_PROFILE_SKELETAL && crit_hit_type == CRIT_HIT_TYPE_CRUSHING ? 10 : 0;
}

unsigned int creature_profile_temporary(CreatureProfile profile, int hit_loc,
    int crit_hit_type)
{
    switch (profile) {
    case CREATURE_PROFILE_LIVING:
        return crit_effects_for_location(hit_loc);
    case CREATURE_PROFILE_SKELETAL:
        // Only a crushing blow shatters bone; a blade or a bullet finds little
        // to hit.
        if (crit_hit_type != CRIT_HIT_TYPE_CRUSHING) {
            return 0;
        }
        // Then as flesh.
        // fall through
    case CREATURE_PROFILE_FLESHY:
        // A limb loosens, the head staggers; the torso has no breath to lose.
        switch (hit_loc) {
        case HIT_LOC_HEAD:
            return CRIT_EFFECT_STAGGERED;
        case HIT_LOC_ARM:
            return CRIT_EFFECT_NUMBED_ARM;
        case HIT_LOC_LEG:
            return CRIT_EFFECT_HOBBLED;
        default:
            return 0;
        }
    case CREATURE_PROFILE_MECHANICAL:
        if (crit_hit_type == CRIT_HIT_TYPE_ELECTRICAL) {
            return CRIT_EFFECT_MALFUNCTION;
        }
        switch (hit_loc) {
        case HIT_LOC_ARM:
            return CRIT_EFFECT_JAMMED_ARM;
        case HIT_LOC_LEG:
            return CRIT_EFFECT_HOBBLED;
        default:
            return CRIT_EFFECT_MALFUNCTION;
        }
    case CREATURE_PROFILE_SPIRIT:
    case CREATURE_PROFILE_PHANTOM:
        return CRIT_EFFECT_DISRUPTED;
    default:
        return 0;
    }
}

const char* creature_profile_name(CreatureProfile profile)
{
    if (profile < 0 || profile >= CREATURE_PROFILE_COUNT) {
        return NULL;
    }
    return creature_profile_names[profile];
}

const char* creature_profile_examine_hint(CreatureProfile profile)
{
    if (profile == CREATURE_PROFILE_PHANTOM) {
        return "Only criticals, magic or explosives hurt it";
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

void creature_rules_init(void)
{
    creature_rules_load();
}

void creature_rules_exit(void)
{
    creature_rule_count = 0;
}

void creature_rules_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry entry;
    CreatureRule rule;

    creature_rule_count = 0;
    memset(creature_rule_protos, 0, sizeof(creature_rule_protos));

    if (!mes_load("rules\\creatures.mes", &mes_file)) {
        return;
    }

    if (mes_find_first(mes_file, &entry)) {
        do {
            if (!creature_rule_parse(entry.num, entry.str, &rule)) {
                tig_debug_printf("creature rules: refusing entry {%d}{%s}\n", entry.num, entry.str);
                continue;
            }
            if (creature_rule_count >= CREATURE_RULES_MAX) {
                tig_debug_printf("creature rules: more than %d creatures listed; {%d} ignored\n",
                    CREATURE_RULES_MAX, entry.num);
                continue;
            }
            creature_rules[creature_rule_count++] = rule;
        } while (mes_find_next(mes_file, &entry));
    }

    mes_unload(mes_file);
    tig_debug_printf("creature rules: %d creature(s) listed\n", creature_rule_count);
}

static bool creature_rules_applies(int64_t obj)
{
    return feature_enabled(FEATURE_CREATURE_PROFILES)
        && obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE));
}

static const CreatureRule* creature_rule_for(int64_t obj)
{
    int64_t proto_obj;
    int index;

    if (!creature_rules_applies(obj)) {
        return NULL;
    }

    proto_obj = obj_field_handle_get(obj, OBJ_F_PROTOTYPE_HANDLE);
    for (index = 0; index < creature_rule_count; index++) {
        if (creature_rule_protos[index] == 0) {
            // Prototype number = description + 20, as the armor table.
            creature_rule_protos[index] = proto_get_by_description(creature_rules[index].id - 20);
            if (creature_rule_protos[index] == OBJ_HANDLE_NULL) {
                creature_rule_protos[index] = -1;
            }
        }
        if (creature_rule_protos[index] == proto_obj) {
            return &(creature_rules[index]);
        }
    }

    return NULL;
}

CreatureProfile creature_rules_profile(int64_t obj)
{
    const CreatureRule* rule;

    if (!creature_rules_applies(obj)) {
        return CREATURE_PROFILE_NONE;
    }

    rule = creature_rule_for(obj);
    if (rule != NULL && rule->profile != CREATURE_RULE_UNSET) {
        return (CreatureProfile)rule->profile;
    }

    return creature_profile_default(obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS),
        obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2),
        creature_rules_body_chart(obj));
}

int creature_rules_body_chart(int64_t obj)
{
    const CreatureRule* rule = creature_rule_for(obj);

    if (rule != NULL && rule->chart != CREATURE_RULE_UNSET) {
        return rule->chart;
    }
    return obj_field_int32_get(obj, OBJ_F_CRITTER_CRIT_HIT_CHART);
}

bool creature_rules_natural(int64_t obj, int* natural)
{
    const CreatureRule* rule = creature_rule_for(obj);

    if (rule == NULL || rule->natural == CREATURE_RULE_UNSET) {
        return false;
    }
    *natural = rule->natural;
    return true;
}

int creature_rules_firearm_scale(int64_t obj)
{
    const CreatureRule* rule = creature_rule_for(obj);

    if (rule == NULL || rule->firearm_scale == CREATURE_RULE_UNSET) {
        return 100;
    }
    return rule->firearm_scale;
}
