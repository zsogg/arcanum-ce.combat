#ifndef TIG_TIMER_H_
#define TIG_TIMER_H_

#include <stdbool.h>

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Represents point in time as in integer number of milliseconds elapsed since
// arbitrary reference point.
//
// NOTE: The reference point is an implementation detail. On Windows this value
// is a number of milliseconds since the system was started. On other platforms
// the reference point might be something else. In general the game should not
// rely on it's meaning. Instead it should only be used to calculate duration
// with `tig_timer_elapsed` or `tig_timer_between`.
typedef unsigned int tig_timestamp_t;

// Represents a difference between two time points.
//
// NOTE: There is no consensus about signness of this type. The majority of
// call sites use signed compares, however there are several places with
// unsigned checks. One of the arguments for it to be signed is that
// `tig_timer_between` caps duration at 0x7FFFFFFF (that is max 32 bit signed
// int).
typedef int tig_duration_t;

// Virtual clock, for reproducible runs.
//
// TIMER normally reads `SDL_GetTicks()`, so every timestamp depends on how
// fast the machine ran the frame. The game advances its own clock from these
// values, which means two runs of the same save diverge purely because one
// took longer - the save-state matrix recorded exactly that twice
// (`Slot0003 turnbased-mid`, `Slot0006 random encounter`), where the only
// finding was that combat progressed and an encounter fired "during the
// window".
//
// With the virtual clock on, `tig_timer_now` and `tig_timer_elapsed` read a
// counter that advances by a fixed step once per `tig_ping` instead of by
// however many milliseconds really passed. A run then depends on how many
// frames were executed, not on how long they took.
//
// Off by default. The game turns it on through `determinism_init` when the
// `determinism` setting is set; nothing else should call it.
void tig_timer_virtual_set(bool enabled, unsigned int step_ms);

// Whether the virtual clock is running.
bool tig_timer_virtual_enabled(void);

// Advances the virtual clock by one step. No-op when it is off.
//
// Called from `tig_ping`, which is the engine's per-frame heartbeat.
void tig_timer_virtual_advance(void);

// Initializes TIMER subsystem.
int tig_timer_init(TigInitInfo* init_info);

// Shutdowns TIMER subsystem.
void tig_timer_exit(void);

// Creates a new time point representing current date/time.
//
// Returns `TIG_OK`.
int tig_timer_now(tig_timestamp_t* timestamp_ptr);

// Returns number of milliseconds elapsed since `start`.
tig_duration_t tig_timer_elapsed(tig_timestamp_t start);

// Returns number of milliseconds elapsed between `end` and `start`.
tig_duration_t tig_timer_between(tig_timestamp_t start, tig_timestamp_t end);

#ifdef __cplusplus
}
#endif

#endif /* TIG_TIMER_H_ */
