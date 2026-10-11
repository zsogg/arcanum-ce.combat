#include "game/spell_rules.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/ce_parse.h"
#include "game/mes.h"

// Each spell's rule; `ap` -1 and no critical for one not listed.
static SpellRule spell_rules_table[SPELL_RULES_COUNT];

static const struct {
    SpellCritical critical;
    const char* word;
    const char* name;
    bool damage;
} spell_rule_criticals[] = {
    { SPELL_CRITICAL_STUN, "Stun", "stun", true },
    { SPELL_CRITICAL_KNOCKDOWN, "Knockdown", "knockdown", true },
    { SPELL_CRITICAL_LONG_STUN, "LongStun", "long_stun", true },
    { SPELL_CRITICAL_REFUND, "Refund", "refund", false },
    { SPELL_CRITICAL_EXPOSED, "Exposed", "exposed", false },
    { SPELL_CRITICAL_UNFOCUSED, "Unfocused", "unfocused", false },
    { SPELL_CRITICAL_DAZZLED, "Dazzled", "dazzled", false },
    { SPELL_CRITICAL_HOBBLED, "Hobbled", "hobbled", false },
};

#define SPELL_RULE_CRITICALS ((int)(sizeof(spell_rule_criticals) / sizeof(spell_rule_criticals[0])))

static char* spell_rule_trim(char* str)
{
    char* end;

    str += strspn(str, " \t");
    end = str + strlen(str);
    while (end > str && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
    return str;
}

// `Stun 20`, `Refund` ...: the critical and its bonus, or false.
static bool spell_rule_parse_critical(char* value, SpellCritical* critical, int* bonus)
{
    char* space;
    char* number;
    int index;

    value = spell_rule_trim(value);
    space = strchr(value, ' ');
    number = NULL;
    if (space != NULL) {
        *space = '\0';
        number = spell_rule_trim(space + 1);
    }

    for (index = 0; index < SPELL_RULE_CRITICALS; index++) {
        if (strcmp(value, spell_rule_criticals[index].word) == 0) {
            break;
        }
    }
    if (index == SPELL_RULE_CRITICALS) {
        return false;
    }

    if (spell_rule_criticals[index].damage) {
        if (number == NULL || !ce_parse_int(number, bonus)
            || *bonus < 0 || *bonus > SPELL_RULE_DAMAGE_BONUS_MAX) {
            return false;
        }
    } else {
        if (number != NULL && *number != '\0') {
            return false;
        }
        *bonus = 0;
    }

    *critical = spell_rule_criticals[index].critical;
    return true;
}

bool spell_rule_parse(const char* str, SpellRule* rule)
{
    char buffer[160];
    char* part;
    char* save;
    char* colon;
    char* key;
    SpellRule parsed;
    int keys = 0;
    int value;

    if (str == NULL || strlen(str) >= sizeof(buffer)) {
        return false;
    }
    snprintf(buffer, sizeof(buffer), "%s", str);

    parsed.ap = -1;
    parsed.critical = SPELL_CRITICAL_NONE;
    parsed.critical_bonus = 0;
    parsed.no_stack = false;

    for (part = strtok_r(buffer, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
        colon = strchr(part, ':');
        if (colon == NULL) {
            return false;
        }
        *colon = '\0';
        key = spell_rule_trim(part);

        if (strcmp(key, "APCost") == 0) {
            // zsogg, 2026-10-10: no spell below 4 AP - refused, not clamped,
            // so a sheet that tried it is visible in the log.
            if (!ce_parse_int(colon + 1, &value)
                || value < SPELL_RULE_AP_MIN || value > SPELL_RULE_AP_MAX) {
                return false;
            }
            parsed.ap = value;
        } else if (strcmp(key, "Critical") == 0) {
            if (!spell_rule_parse_critical(colon + 1, &parsed.critical, &parsed.critical_bonus)) {
                return false;
            }
        } else if (strcmp(key, "NoStack") == 0) {
            if (!ce_parse_int(colon + 1, &value) || (value != 0 && value != 1)) {
                return false;
            }
            parsed.no_stack = value == 1;
        } else {
            return false;
        }
        keys++;
    }

    if (keys == 0) {
        return false;
    }

    *rule = parsed;
    return true;
}

void spell_rules_init(void)
{
    spell_rules_load();
}

void spell_rules_exit(void)
{
    int index;

    for (index = 0; index < SPELL_RULES_COUNT; index++) {
        spell_rules_table[index].ap = -1;
        spell_rules_table[index].critical = SPELL_CRITICAL_NONE;
        spell_rules_table[index].critical_bonus = 0;
        spell_rules_table[index].no_stack = false;
    }
}

void spell_rules_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry entry;
    int listed = 0;
    SpellRule rule;

    spell_rules_exit();

    if (!mes_load("rules\\spells.mes", &mes_file)) {
        return;
    }

    if (mes_find_first(mes_file, &entry)) {
        do {
            if (entry.num < 0 || entry.num >= SPELL_RULES_COUNT
                || !spell_rule_parse(entry.str, &rule)) {
                tig_debug_printf("spell rules: refusing entry {%d}{%s}\n", entry.num, entry.str);
                continue;
            }
            spell_rules_table[entry.num] = rule;
            listed++;
        } while (mes_find_next(mes_file, &entry));
    }

    mes_unload(mes_file);
    tig_debug_printf("spell rules: %d spell(s) listed\n", listed);
}

int spell_rules_scaled_damage(int min, int max, int aptitude, int percent)
{
    if (aptitude <= 0) {
        return min;
    }

    return min + aptitude * (max - min) / 100 * percent / 100;
}

int spell_rules_ap(int spell)
{
    if (spell < 0 || spell >= SPELL_RULES_COUNT) {
        return -1;
    }

    return spell_rules_table[spell].ap;
}

SpellCritical spell_rules_critical(int spell, int* bonus)
{
    if (spell < 0 || spell >= SPELL_RULES_COUNT) {
        *bonus = 0;
        return SPELL_CRITICAL_NONE;
    }

    *bonus = spell_rules_table[spell].critical_bonus;
    return spell_rules_table[spell].critical;
}

bool spell_rules_no_stack(int spell)
{
    return spell >= 0 && spell < SPELL_RULES_COUNT && spell_rules_table[spell].no_stack;
}

const char* spell_rules_critical_name(SpellCritical critical)
{
    int index;

    for (index = 0; index < SPELL_RULE_CRITICALS; index++) {
        if (spell_rule_criticals[index].critical == critical) {
            return spell_rule_criticals[index].name;
        }
    }
    return "none";
}
