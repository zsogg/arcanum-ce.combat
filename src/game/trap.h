#ifndef ARCANUM_GAME_TRAP_H_
#define ARCANUM_GAME_TRAP_H_

#include "game/context.h"
#include "game/object.h"
#include "game/script.h"
#include "game/timeevent.h"

typedef enum TrapType {
    TRAP_TYPE_INVALID,
    TRAP_TYPE_NONMAGICAL,
    TRAP_TYPE_MAGICAL,
} TrapType;

typedef enum TrapKnownReason {
    TRAP_KNOWN_SPOTTED,
    TRAP_KNOWN_PLACED,
} TrapKnownReason;

bool trap_init(GameInitInfo* init_info);
void trap_exit(void);
bool trap_timeevent_process(TimeEvent* timeevent);
int trap_type(int64_t obj);
bool trap_attempt_spot(int64_t pc_obj, int64_t trap_obj);
bool trap_is_spotted(int64_t pc_obj, int64_t trap_obj);
void trap_mark_known(int64_t pc_obj, int64_t trap_obj, int reason);
bool trap_use_on_obj(int64_t pc_obj, int64_t item_obj, int64_t target_obj);
bool trap_use_at_loc(int64_t pc_obj, int64_t item_obj, int64_t target_loc);
void trap_handle_disarm(int64_t pc_obj, int64_t trap_obj, bool* is_success_ptr, bool* is_critical_ptr);
bool trap_is_trap_device(int64_t obj);
int64_t get_trap_at_location(int64_t loc);
void trap_invoke(int64_t triggerer_obj, int64_t attachee_obj);
bool trap_script_execute(ScriptInvocation* invocation);

// CE (`trap spotting`, default off; zsogg, 2026-10-22; arcanum-work
// `docs/TRAPS.md`).
//
// Pure: whether a spotter of this Spot Trap training sees the trap without a
// roll - an Apprentice a non-magical trap, an Expert a magical one, either
// only at difficulty 0.
bool trap_spot_certain(int training, bool magical, int difficulty);
// A trap object's difficulty; a trapped door or container has none.
int trap_spot_difficulty(int64_t trap_obj);
// Under the switch: whether this mover is the player or one of its
// followers, whose path the party checks.
bool trap_party_spots_path(int64_t mover_obj);
// The party member who spots for the mover: the player or a follower,
// conscious, within 10 tiles of it, with the best chance; the mover itself
// when the switch is off or it is not in the party.
int64_t trap_party_spotter(int64_t mover_obj, int64_t trap_obj);
// Checks each unseen trap at these tiles, nearest first, once each: true
// when one is spotted. A trap missed here is not rolled for again on this map.
bool trap_party_spot_ahead(int64_t mover_obj, const int64_t* locs, int count);

#endif /* ARCANUM_GAME_TRAP_H_ */
