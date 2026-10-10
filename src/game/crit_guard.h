#ifndef ARCANUM_GAME_CRIT_GUARD_H_
#define ARCANUM_GAME_CRIT_GUARD_H_

#include <stdbool.h>
#include <stdint.h>

// CE (criticals, milestone CR-1): protection after critical incapacitation.
//
// A character who recovers from a critical's stun, knockdown or knockout -
// the stun ends, they get up, or they wake - cannot be incapacitated by
// another critical until the end of their next turn, so they always get one
// full turn to act. In real time the protection lasts one turn length, `R`,
// from recovery. It covers only those three critical effects: a protected
// character's critical still lands, its damage and other effects apply, and
// the incapacitation is skipped with a floating "Resisted ..." line. Spells
// are unaffected. zsogg's rule, settled 2026-09-25, in arcanum-work's
// COMBAT-REBALANCE-DESIGN-DISCUSSION.md.
//
// Behind `critical protection`, default off: with it off nothing here runs.
// Nothing is saved; a load or a new game forgets every record, which costs at
// most one turn's protection.
//
// One record per character a critical has incapacitated:
//
//   incapacitated  stunned, unconscious or lying down. Not protected:
//                  protection starts on recovery.
//   recovered      protected. In turn-based, until the end of the first turn
//                  that begins with them able to act; in real time, until
//                  `R` has passed since they were last seen incapacitated.
//
// The clock is real milliseconds, advanced from `timeevent_ping` while the
// game is not paused, as the real-time budget's is.

typedef struct CritGuardEntry {
    int64_t obj;
    // Since the last critical incapacitation, a turn has begun with them able
    // to act; it ends their protection when it ends.
    bool free_turn_started;
    // They have been seen incapacitated since the critical. Until then
    // nothing has happened to recover from: the effect is still being
    // applied, or never took.
    bool seen_down;
    // The clock when they were last seen incapacitated.
    unsigned int incapacitated_ms;
    // The last observation, so the log can say when they recovered.
    bool down;
} CritGuardEntry;

// The rules, on one record. Pure: no engine state is read.
void crit_guard_entry_incapacitate(CritGuardEntry* entry, unsigned int clock_ms);
void crit_guard_entry_observe(CritGuardEntry* entry, bool incapacitated, unsigned int clock_ms);
bool crit_guard_entry_protects(const CritGuardEntry* entry, bool incapacitated,
    bool turn_based, unsigned int clock_ms, int window_ms);
void crit_guard_entry_turn_started(CritGuardEntry* entry, bool incapacitated);
bool crit_guard_entry_turn_ended(const CritGuardEntry* entry);
bool crit_guard_entry_lapsed(const CritGuardEntry* entry, bool incapacitated,
    bool turn_based, unsigned int clock_ms, int window_ms);

// The engine side.
void crit_guard_reset(void);
unsigned int crit_guard_filter(int64_t obj, unsigned int dam_flags);
void crit_guard_turn_started(int64_t obj);
void crit_guard_turn_ended(int64_t obj);
void crit_guard_tick(int delta_ms);
bool crit_guard_is_protected(int64_t obj);
// Stunned, unconscious or lying down: what the protection calls
// incapacitated.
bool crit_guard_is_down(int64_t obj);

// The test instrument `debug criticals`, default off: true when an attack by
// `attacker` on `target` is forced critical, with its effects rolled 60
// points more easily. The setting's value says whose attacks: 1 the
// player's, 2 those against the player, 3 both. Never a fumble's hit on
// oneself. Adding 4 (criticals CR-3.0) forces the critical without the 60,
// so weapons' own critical-effect bonuses can be compared.
bool crit_guard_debug_forces(int64_t attacker, int64_t target);
// True when `debug criticals` forces this attack and boosts its effects.
bool crit_guard_debug_boosts(int64_t attacker, int64_t target);

// The opening shot's memory (criticals, a CR-2 repair, 2026-10-10): the
// critters attacked in the current fight. zsogg's rule is "once shot at, it is
// in combat"; the stock combat-mode flag the opening shot first read is set
// only while an NPC attacks and cleared whenever its AI does anything else,
// so in his natural-frequency playtest 20 of 26 revolver hits took the bonus.
// Any attack roll against a critter engages it, under `opening shot`; the
// list clears when the player leaves combat mode, and is not saved. A full
// list treats everyone as engaged.
void crit_guard_engage(int64_t obj);
bool crit_guard_is_engaged(int64_t obj);
void crit_guard_engaged_clear(void);

// The test instrument `debug fumbles` (criticals, milestone CR-4.0), default
// off: true when an attack by `attacker` on `target` is forced to miss
// critically. The setting's value says whose attacks, as `debug criticals`':
// 1 the player's, 2 those against the player, 3 both; and 4 (CR-4.2) every
// attack, so followers' and enemies' fumbles with each other can be measured.
bool crit_guard_debug_fumbles(int64_t attacker, int64_t target);

#endif /* ARCANUM_GAME_CRIT_GUARD_H_ */
