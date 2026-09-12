#ifndef ARCANUM_GAME_COMBAT_OBSERVER_H_
#define ARCANUM_GAME_COMBAT_OBSERVER_H_

#include "game/context.h"

#define COMBAT_OBSERVER_IDENTITY_SIZE 40

bool combat_observer_open(const char* path);
void combat_observer_close(void);
bool combat_observer_is_enabled(void);
bool combat_observer_is_complete(void);

void combat_observer_object_id(int64_t obj,
    char identity[COMBAT_OBSERVER_IDENTITY_SIZE]);
void combat_observer_turn_started(const char* actor, int action_points);
void combat_observer_turn_ended(const char* actor, int action_points);
void combat_observer_actor_snapshot(const char* actor, int level,
    int strength, int dexterity, int speed, int melee, int dodge,
    int hp_current, int hp_max, int fatigue_current, int fatigue_max,
    int weapon_description, int armor_description);
void combat_observer_ap_checked(const char* actor, int requested,
    int before, bool accepted);
void combat_observer_ap_committed(const char* actor, int requested,
    int before, int after, bool accepted, int fatigue_before,
    int fatigue_after);
unsigned long long combat_observer_attack_started(const char* attacker,
    const char* target, int skill, int hit_location);
void combat_observer_attack_resolved(unsigned long long action_id,
    const char* attacker, const char* target, int skill, int hit_location,
    unsigned int flags, int result);
void combat_observer_skill_roll(const char* source, const char* target,
    int skill, int effectiveness, int difficulty, int roll,
    int critical_roll, bool success, bool critical);
void combat_observer_damage_committed(const char* attacker,
    const char* target, const int* components, unsigned int damage_flags,
    int hp_amount, int hp_before, int hp_after, int fatigue_before,
    int fatigue_after, bool dead, bool unconscious);

/* Pure writer fault seams used by the unit suite. */
void combat_observer_test_set_event_limit(unsigned int limit);
void combat_observer_test_fail_after(int lines);
void combat_observer_test_reset(void);

#endif /* ARCANUM_GAME_COMBAT_OBSERVER_H_ */
