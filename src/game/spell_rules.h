#ifndef ARCANUM_GAME_SPELL_RULES_H_
#define ARCANUM_GAME_SPELL_RULES_H_

#include <stdbool.h>

// CE (magic, milestone MG-1): per-spell rules, read from `rules/spells.mes`,
// which arcanum-work's `scripts/spell_table.py build` generates from the
// Combat Rebalance's spell sheet. One entry per spell the player can learn,
// keyed by its number (0-79):
//
//     {55}{APCost: 4}
//
// Read under `spell costs` (`combat_cast_cost`): a spell's AP cost, 4 or more
// (zsogg, 2026-10-10: "Magic is generally too strong in Arcanum"). A spell not
// listed costs the stock 4. Like the weapon table, it changes no game data.

// The spells the table may list: the 16 colleges' 80.
#define SPELL_RULES_COUNT 80
#define SPELL_RULE_AP_MIN 4
#define SPELL_RULE_AP_MAX 10

// Pure: reads one entry's text. False, with `ap` untouched, for anything but
// a single `APCost` within its range - so a bad entry changes nothing rather
// than a guessed cost.
bool spell_rule_parse(const char* str, int* ap);

void spell_rules_init(void);
void spell_rules_exit(void);
// Reads `rules/spells.mes` again. Called at init.
void spell_rules_load(void);

// The spell's listed AP cost, or -1 when the table does not list it.
int spell_rules_ap(int spell);

#endif /* ARCANUM_GAME_SPELL_RULES_H_ */
