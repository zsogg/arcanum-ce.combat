#ifndef ARCANUM_GAME_DETERMINISM_H_
#define ARCANUM_GAME_DETERMINISM_H_

#include <stdbool.h>

/**
 * Reproducible runs, off by default.
 *
 * CE reads the wall clock in three places that decide what a run does:
 * `random_init` seeds the game RNG from `time(NULL)`, `tig_guid_init` seeds
 * the GUID generator the same way, and TIG's timer reports real elapsed
 * milliseconds, which is what the game advances its own clock from. Two runs
 * of the same save therefore diverge for reasons that have nothing to do with
 * the code under test.
 *
 * That is not hypothetical. The save-state matrix compared ten named saves
 * against the original and found eight structurally identical; the two that
 * were not are recorded as "live-combat state diverged during the window" and
 * "encounter fired in CE during the window". Neither was a serialization
 * defect. Both were the clock.
 *
 * With `determinism=1` in `arcanum.cfg` (or `-deterministic` on the command
 * line) all three sources are pinned:
 *
 *   - the game RNG is seeded from `determinism seed` rather than the time;
 *   - the GUID generator is seeded from the same value;
 *   - TIG's timer switches to a virtual clock that advances a fixed step per
 *     frame, so a run depends on how many frames ran rather than how long
 *     they took;
 *   - the mouse pointer is snapped to the centre of the window and stops
 *     tracking, so where it physically sits no longer changes the run.
 *
 * The pointer was added on 2026-08-21, after the trace gate reported 7
 * function-membership changes and 311 changed call counts from a run whose
 * only difference was where the pointer had been left. A pointer near the
 * window edge scrolls the map, which draws more terrain and shifts the count
 * of everything on the drawing path. It is *ambient* state rather than input
 * - it is always somewhere, whether or not anybody touches it - which is why
 * seeding the RNG never covered it, and why two runs could agree byte for
 * byte with each other and still disagree with a fixture.
 *
 * **What this does not pin.** Audio mixing, SDL event delivery, and mouse
 * buttons and the wheel, which fire only when somebody presses them and so
 * cannot leak into an unattended run. A run driven by a human is not
 * reproducible no matter what this does. The target is the harness case -
 * autoload a save, run N frames, save again - where the only inputs are
 * scripted. Within that, two runs should agree byte for byte.
 *
 * The switch is a CE addition and has no counterpart in the original binary,
 * so nothing here carries an address annotation.
 */
void determinism_init(void);

/**
 * Whether reproducible mode is on.
 */
bool determinism_enabled(void);

/**
 * The seed in force, or 0 when reproducible mode is off.
 */
unsigned int determinism_seed(void);

/**
 * Turns reproducible mode on for this run, before `determinism_init`.
 *
 * This is how `-deterministic[=SEED]` on the command line works: `main`
 * parses the switch before the game initializes, and the setting file still
 * wins if it also asks for it. Passing 0 for `seed` keeps the configured
 * seed.
 */
void determinism_request(unsigned int seed);

/**
 * CE (card O2): record the run's reproducible identity at its Ready barrier -
 * loading complete, nothing committed. Captures the game RNG state, which is
 * what a replay restores, and names the method used for each pinned source.
 *
 * Prints one parseable line and changes nothing.
 */
void determinism_ready_report(const char* label);

#endif /* ARCANUM_GAME_DETERMINISM_H_ */
