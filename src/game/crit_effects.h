#ifndef ARCANUM_GAME_CRIT_EFFECTS_H_
#define ARCANUM_GAME_CRIT_EFFECTS_H_

#include <stdbool.h>
#include <stdint.h>

// CE (criticals, milestone CR-2): temporary critical effects.
//
// Under `critical effects`, every critical hit leaves its victim a temporary
// effect chosen by where it struck, besides the stock rolls (zsogg,
// 2026-10-05):
//
//   torso  Winded      extra fatigue, half the hit's rolled damage, at once
//   arm    Numbed arm  -20 to hit on their attacks
//   leg    Hobbled     moving costs double (turn-based)
//   head   Staggered   half the AP on their next turn (turn-based)
//
// A stock crippling or blinding critical leaves the matching temporary effect
// instead (blinding, Dazzled: -20 to hit); the permanent injury stays only on
// a second roll of 25%, and a stock scar likewise. A fumble's hit on its
// wielder rolls its effects once rather than twice (MODDING-NOTES quirk 58)
// and never scars.
//
// An effect lasts until the end of the victim's next turn, or `R` in real
// time, and a new one refreshes rather than stacks. Hobbled and Staggered are
// turn-based only. Undead and amorphous creatures are spared, as from the
// stock bodily effects. Nothing is saved.

#define CRIT_EFFECT_WINDED 0x01u
#define CRIT_EFFECT_NUMBED_ARM 0x02u
#define CRIT_EFFECT_HOBBLED 0x04u
#define CRIT_EFFECT_STAGGERED 0x08u
#define CRIT_EFFECT_DAZZLED 0x10u

// The to-hit penalty of Numbed arm and Dazzled, in difficulty points.
#define CRIT_EFFECT_TO_HIT_PENALTY 20

// A permanent injury's second roll, in percent.
#define CRIT_EFFECT_PERMANENT_CHANCE 25

typedef struct CritEffectEntry {
    int64_t obj;
    // The lasting effects (all but Winded, which is spent at once).
    unsigned int effects;
    // A turn of theirs has begun since the last effect; its end ends them.
    bool turn_started;
    // The clock when the last effect was applied.
    unsigned int applied_ms;
} CritEffectEntry;

// The rules, on one record. Pure: no engine state is read.
unsigned int crit_effects_for_location(int hit_loc);
void crit_effects_entry_apply(CritEffectEntry* entry, unsigned int effects, unsigned int clock_ms);
int crit_effects_entry_to_hit_penalty(const CritEffectEntry* entry);
int crit_effects_entry_move_multiplier(const CritEffectEntry* entry, bool turn_based);
int crit_effects_entry_turn_action_points(const CritEffectEntry* entry, int action_points);
void crit_effects_entry_turn_started(CritEffectEntry* entry);
bool crit_effects_entry_turn_ended(const CritEffectEntry* entry);
bool crit_effects_entry_lapsed(const CritEffectEntry* entry, bool turn_based,
    unsigned int clock_ms, int window_ms);

// The engine side.
void crit_effects_reset(void);
void crit_effects_apply(int64_t obj, unsigned int effects);
int crit_effects_to_hit_penalty(int64_t obj);
int crit_effects_move_multiplier(int64_t obj);
int crit_effects_turn_action_points(int64_t obj, int action_points);
void crit_effects_turn_started(int64_t obj);
void crit_effects_turn_ended(int64_t obj);
void crit_effects_tick(int delta_ms);
int crit_effects_names(unsigned int effects, char* buffer, int size);

#endif /* ARCANUM_GAME_CRIT_EFFECTS_H_ */
