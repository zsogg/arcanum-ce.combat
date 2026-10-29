#ifndef ARCANUM_GAME_STAT_H_
#define ARCANUM_GAME_STAT_H_

#include "game/context.h"
#include "game/timeevent.h"

#define LOW_INTELLIGENCE 4

typedef enum Stat {
    STAT_STRENGTH,
    STAT_DEXTERITY,
    STAT_CONSTITUTION,
    STAT_BEAUTY,
    STAT_INTELLIGENCE,
    STAT_PERCEPTION,
    STAT_WILLPOWER,
    STAT_CHARISMA,
    STAT_CARRY_WEIGHT,
    STAT_DAMAGE_BONUS,
    STAT_AC_ADJUSTMENT,
    STAT_SPEED,
    STAT_HEAL_RATE,
    STAT_POISON_RECOVERY,
    STAT_REACTION_MODIFIER,
    STAT_MAX_FOLLOWERS,
    STAT_MAGICK_TECH_APTITUDE,
    STAT_LEVEL,
    STAT_EXPERIENCE_POINTS,
    STAT_ALIGNMENT,
    STAT_FATE_POINTS,
    STAT_UNSPENT_POINTS,
    STAT_MAGICK_POINTS,
    STAT_TECH_POINTS,
    STAT_POISON_LEVEL,
    STAT_AGE,
    STAT_GENDER,
    STAT_RACE,
    STAT_COUNT,
} Stat;

typedef enum Gender {
    GENDER_FEMALE,
    GENDER_MALE,
    GENDER_COUNT,
} Gender;

typedef enum Race {
    RACE_HUMAN,
    RACE_DWARF,
    RACE_ELF,
    RACE_HALF_ELF,
    RACE_GNOME,
    RACE_HALFLING,
    RACE_HALF_ORC,
    RACE_HALF_OGRE,
    RACE_DARK_ELF,
    RACE_OGRE,
    RACE_ORC,
    RACE_COUNT,
} Race;

extern const char* stat_lookup_keys_tbl[STAT_COUNT];

bool stat_init(GameInitInfo* init_info);
void stat_exit(void);
void stat_set_defaults(int64_t obj);
int stat_level_get(int64_t obj, int stat);
int stat_base_get(int64_t obj, int stat);
int stat_base_set(int64_t obj, int stat, int value);
bool stat_is_extraordinary(int64_t obj, int stat);
int stat_cost(int value);
const char* stat_name(int stat);
const char* stat_short_name(int stat);
const char* gender_name(int gender);
const char* race_name(int race);
int stat_level_min(int64_t obj, int stat);
int stat_level_max(int64_t obj, int stat);
bool stat_level_set(int64_t obj, int stat, int value);
bool stat_poison_timeevent_process(TimeEvent* timeevent);

// CE (poison per turn; zsogg, 2026-09-24): under `poison per turn`, poison
// ticks on the poisoned critter's turns in combat instead of every 15 game
// seconds - in turn-based on every second turn of theirs, taken or skipped;
// in real-time combat every two turn lengths (4 s at the stock 2000 ms).
// Outside combat it keeps the stock clock.
#define STAT_POISON_TURNS_PER_TICK 2
#define STAT_POISON_TURN_LENGTHS_PER_TICK 2
// Pure: a turn of a poisoned critter has come round; true when it ticks, every
// `per_tick` turns.
bool stat_poison_turn_counts(int* turns, int per_tick);
// Pure: `delta_ms` of real-time combat passed; how many ticks fall due.
int stat_poison_time_counts(unsigned int* ms, int delta_ms, int period_ms);
// The engine side: `obj`'s turn-based turn has come round.
void stat_poison_turn_reached(int64_t obj);
// Real milliseconds while the game is not paused.
void stat_poison_tick(int delta_ms);
// CE (poison retune; zsogg, 2026-10-28). Pure: a tick's damage at `level` -
// stock 1, 2 from 200 and 3 from 550; under `poison every turn` in combat,
// `scaled`, 1 + level / 100.
int stat_poison_tick_damage(int level, bool scaled);
// Pure: `poison dose cap` - the level after a hit of `dose` lands on `level`:
// at most four times the dose, and never lower than before.
int stat_poison_dose_capped(int level, int dose);

#endif /* ARCANUM_GAME_STAT_H_ */
