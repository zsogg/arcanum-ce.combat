#ifndef ARCANUM_GAME_SPELL_RULES_H_
#define ARCANUM_GAME_SPELL_RULES_H_

#include <stdbool.h>

// CE (magic, milestone MG-1): per-spell rules, read from `rules/spells.mes`,
// which arcanum-work's `scripts/spell_table.py build` generates from the
// Combat Rebalance's spell sheet. One entry per spell the player can learn,
// keyed by its number (0-79):
//
//     {55}{APCost: 4, Critical: Stun 20}
//
// `APCost`, read under `spell costs` (`combat_cast_cost`): a spell's AP cost,
// 4 or more (zsogg, 2026-10-10: "Magic is generally too strong in Arcanum"). A
// spell not listed costs the stock 4.
//
// `Critical`, read under `spell criticals` (magic, milestone MG-2; zsogg,
// 2026-10-11): what the spell's critical does - `Stun N`, `Knockdown N` or
// `LongStun N` (N% more damage, and that incapacitation), `Refund` (the
// casting fatigue is returned), or a debuff's two-turn effect: `Exposed`,
// `Unfocused`, `Dazzled`, `Hobbled`. A spell without one is never critical.
// `NoStack: 1`, read under `spell no stacking` (magic, milestone MG-3; zsogg,
// 2026-10-11): a second copy of the spell on the same target is refused before
// its caster pays anything. The sheet marks every buff, and Drain Will.
// Like the weapon table, it changes no game data.

// The spells the table may list: the 16 colleges' 80.
#define SPELL_RULES_COUNT 80
#define SPELL_RULE_AP_MIN 4
#define SPELL_RULE_AP_MAX 10
#define SPELL_RULE_DAMAGE_BONUS_MAX 100

typedef enum SpellCritical {
    SPELL_CRITICAL_NONE,
    SPELL_CRITICAL_STUN,
    SPELL_CRITICAL_KNOCKDOWN,
    SPELL_CRITICAL_LONG_STUN,
    SPELL_CRITICAL_REFUND,
    SPELL_CRITICAL_EXPOSED,
    SPELL_CRITICAL_UNFOCUSED,
    SPELL_CRITICAL_DAZZLED,
    SPELL_CRITICAL_HOBBLED,
} SpellCritical;

typedef struct SpellRule {
    // -1 when the entry names no APCost.
    int ap;
    SpellCritical critical;
    // The damage bonus of a Stun, Knockdown or LongStun critical, percent.
    int critical_bonus;
    // MG-3: `NoStack: 1`.
    bool no_stack;
} SpellRule;

// Pure: reads one entry's text - `APCost`, `Critical` and `NoStack`, any of them.
// `NoStack` takes 0 or 1.
// False, with `rule` untouched, for anything else, an AP below 4 or above 10,
// a damage critical without its bonus or another critical with one - so a bad
// entry changes nothing rather than a guessed rule.
bool spell_rule_parse(const char* str, SpellRule* rule);

void spell_rules_init(void);
void spell_rules_exit(void);
// Reads `rules/spells.mes` again. Called at init.
void spell_rules_load(void);

// The spell's listed AP cost, or -1 when the table does not list it.
int spell_rules_ap(int spell);
// MG-2 (zsogg, 2026-10-11): the critical chance - 5% + 2% per spell the
// caster knows in the spell's college - and how many of the victim's turns
// a debuff's critical effect lasts.
#define SPELL_CRITICAL_BASE_CHANCE 5
#define SPELL_CRITICAL_PER_SPELL_KNOWN 2
#define SPELL_CRITICAL_EFFECT_TURNS 2

// MG-2: the spell's critical, and its damage bonus in percent.
SpellCritical spell_rules_critical(int spell, int* bonus);
// MG-3: whether the table refuses a second copy of the spell on a target.
bool spell_rules_no_stack(int spell);
// MG-2: a critical's name for `spell.critical` - "stun", "refund" ...
const char* spell_rules_critical_name(SpellCritical critical);

// CE (magic, zsogg 2026-10-11): under `spell aptitude damage`, the part of a
// `Scaled` spell's damage that the caster's magickal aptitude adds above the
// minimum is 30% smaller - this percent of stock. A full mage's Harm, 3-40,
// deals 28 instead of 40; a caster at aptitude 0 or below keeps the minimum.
#define SPELL_APTITUDE_DAMAGE_PERCENT 70

// Pure: the damage a `Scaled` spell deals, the engine's rule - `min` plus
// aptitude percent of the range, the minimum at aptitude 0 or below - with
// the aptitude part at `percent` of stock (100: stock).
int spell_rules_scaled_damage(int min, int max, int aptitude, int percent);

#endif /* ARCANUM_GAME_SPELL_RULES_H_ */
