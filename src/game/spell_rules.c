#include "game/spell_rules.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/ce_parse.h"
#include "game/mes.h"

// The listed AP cost of each spell, -1 for none.
static int spell_rule_ap[SPELL_RULES_COUNT];

bool spell_rule_parse(const char* str, int* ap)
{
    char buffer[128];
    char* colon;
    char* key;
    char* end;
    int value;

    if (str == NULL || strlen(str) >= sizeof(buffer)) {
        return false;
    }
    snprintf(buffer, sizeof(buffer), "%s", str);

    // One key only: a comma means more than the table has to say.
    if (strchr(buffer, ',') != NULL) {
        return false;
    }

    colon = strchr(buffer, ':');
    if (colon == NULL) {
        return false;
    }
    *colon = '\0';

    key = buffer + strspn(buffer, " \t");
    end = key + strlen(key);
    while (end > key && (end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }

    if (strcmp(key, "APCost") != 0 || !ce_parse_int(colon + 1, &value)) {
        return false;
    }

    // zsogg, 2026-10-10: no spell below 4 AP - refused, not clamped, so a
    // sheet that tried it is visible in the log.
    if (value < SPELL_RULE_AP_MIN || value > SPELL_RULE_AP_MAX) {
        return false;
    }

    *ap = value;
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
        spell_rule_ap[index] = -1;
    }
}

void spell_rules_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry entry;
    int listed = 0;
    int ap;

    spell_rules_exit();

    if (!mes_load("rules\\spells.mes", &mes_file)) {
        return;
    }

    if (mes_find_first(mes_file, &entry)) {
        do {
            if (entry.num < 0 || entry.num >= SPELL_RULES_COUNT
                || !spell_rule_parse(entry.str, &ap)) {
                tig_debug_printf("spell rules: refusing entry {%d}{%s}\n", entry.num, entry.str);
                continue;
            }
            spell_rule_ap[entry.num] = ap;
            listed++;
        } while (mes_find_next(mes_file, &entry));
    }

    mes_unload(mes_file);
    tig_debug_printf("spell rules: %d spell(s) listed\n", listed);
}

int spell_rules_ap(int spell)
{
    if (spell < 0 || spell >= SPELL_RULES_COUNT) {
        return -1;
    }

    return spell_rule_ap[spell];
}
