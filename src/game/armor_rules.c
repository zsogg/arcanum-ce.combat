#include "game/armor_rules.h"

#include <string.h>

#include <tig/tig.h>

#include "game/ce_parse.h"
#include "game/feature.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/proto.h"

// Pieces the file may list. The shipped game has about 220 helmets, body
// armors, gloves, boots and shields.
#define ARMOR_RULES_MAX 320

static ArmorRule armor_rules[ARMOR_RULES_MAX];
// The prototype object each piece names, found on first use, as the weapon
// table does: 0 is not yet looked up, -1 looked up and not found.
static int64_t armor_rule_protos[ARMOR_RULES_MAX];
static int armor_rule_count;
static int armor_rule_penetration[ARMOR_RULE_KIND_COUNT];

// ---------------------------------------------------------------------------
// The rules, pure.
// ---------------------------------------------------------------------------

static int armor_rule_clamp(int value, int hi)
{
    if (value < 0) {
        return 0;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

bool armor_rule_parse(int id, const char* str, ArmorRule* rule)
{
    ArmorRule parsed;
    char buffer[256];
    char* part;
    char* save;
    char* colon;
    char* key;
    char* end;
    int value;
    bool kind;

    if (str == NULL || strlen(str) >= sizeof(buffer)) {
        return false;
    }

    kind = id >= ARMOR_RULE_KIND_MELEE && id < ARMOR_RULE_KIND_COUNT;
    if (!kind && id <= 20) {
        return false;
    }

    strcpy(buffer, str);
    parsed.id = id;
    parsed.protection = ARMOR_RULE_UNSET;
    parsed.block_chance = ARMOR_RULE_UNSET;
    parsed.penetration = ARMOR_RULE_UNSET;

    for (part = strtok_r(buffer, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
        colon = strchr(part, ':');
        if (colon == NULL) {
            return false;
        }
        *colon = '\0';

        key = part + strspn(part, " \t");
        end = key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }

        if (!ce_parse_int(colon + 1, &value)) {
            return false;
        }

        if (strcmp(key, "Protection") == 0) {
            parsed.protection = armor_rule_clamp(value, ARMOR_RULE_VALUE_MAX);
        } else if (strcmp(key, "BlockChance") == 0) {
            parsed.block_chance = armor_rule_clamp(value, ARMOR_RULE_CHANCE_MAX);
        } else if (strcmp(key, "Penetration") == 0) {
            parsed.penetration = armor_rule_clamp(value, ARMOR_RULE_VALUE_MAX);
        } else {
            return false;
        }
    }

    // A kind takes a penetration and nothing else; a piece takes a protection,
    // a shield also a block chance, and no penetration.
    if (kind
            ? (parsed.penetration == ARMOR_RULE_UNSET
                  || parsed.protection != ARMOR_RULE_UNSET
                  || parsed.block_chance != ARMOR_RULE_UNSET)
            : (parsed.protection == ARMOR_RULE_UNSET
                  || parsed.penetration != ARMOR_RULE_UNSET)) {
        return false;
    }

    *rule = parsed;
    return true;
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

void armor_rules_init(void)
{
    armor_rules_load();
}

void armor_rules_exit(void)
{
    armor_rule_count = 0;
}

void armor_rules_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry entry;
    ArmorRule rule;
    int index;

    armor_rule_count = 0;
    memset(armor_rule_protos, 0, sizeof(armor_rule_protos));
    for (index = 0; index < ARMOR_RULE_KIND_COUNT; index++) {
        armor_rule_penetration[index] = ARMOR_RULE_UNSET;
    }

    if (!mes_load("rules\\armor.mes", &mes_file)) {
        return;
    }

    if (mes_find_first(mes_file, &entry)) {
        do {
            if (!armor_rule_parse(entry.num, entry.str, &rule)) {
                tig_debug_printf("armor rules: refusing entry {%d}{%s}\n", entry.num, entry.str);
                continue;
            }
            if (rule.penetration != ARMOR_RULE_UNSET) {
                armor_rule_penetration[rule.id] = rule.penetration;
                continue;
            }
            if (armor_rule_count >= ARMOR_RULES_MAX) {
                tig_debug_printf("armor rules: more than %d pieces listed; {%d} ignored\n",
                    ARMOR_RULES_MAX, entry.num);
                continue;
            }
            armor_rules[armor_rule_count++] = rule;
        } while (mes_find_next(mes_file, &entry));
    }

    mes_unload(mes_file);
    tig_debug_printf("armor rules: %d piece(s) listed\n", armor_rule_count);
}

static const ArmorRule* armor_rule_for(int64_t armor_obj)
{
    int64_t proto_obj;
    int index;

    if (!feature_enabled(FEATURE_ARMOR_PENETRATION)
        || armor_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(armor_obj, OBJ_F_TYPE) != OBJ_TYPE_ARMOR) {
        return NULL;
    }

    proto_obj = obj_field_handle_get(armor_obj, OBJ_F_PROTOTYPE_HANDLE);
    for (index = 0; index < armor_rule_count; index++) {
        if (armor_rule_protos[index] == 0) {
            // Prototype number = description + 20, as the weapon table.
            armor_rule_protos[index] = proto_get_by_description(armor_rules[index].id - 20);
            if (armor_rule_protos[index] == OBJ_HANDLE_NULL) {
                armor_rule_protos[index] = -1;
            }
        }
        if (armor_rule_protos[index] == proto_obj) {
            return &(armor_rules[index]);
        }
    }

    return NULL;
}

bool armor_rules_protection(int64_t armor_obj, int* protection)
{
    const ArmorRule* rule = armor_rule_for(armor_obj);

    if (rule == NULL) {
        return false;
    }
    *protection = rule->protection;
    return true;
}

bool armor_rules_block_chance(int64_t shield_obj, int* chance)
{
    const ArmorRule* rule = armor_rule_for(shield_obj);

    if (rule == NULL || rule->block_chance == ARMOR_RULE_UNSET) {
        return false;
    }
    *chance = rule->block_chance;
    return true;
}

int armor_rules_default_penetration(ArmorRuleKind kind)
{
    if (!feature_enabled(FEATURE_ARMOR_PENETRATION)
        || kind < ARMOR_RULE_KIND_MELEE
        || kind >= ARMOR_RULE_KIND_COUNT) {
        return ARMOR_RULE_UNSET;
    }
    return armor_rule_penetration[kind];
}
