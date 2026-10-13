#ifndef ARCANUM_GAME_FEATURE_H_
#define ARCANUM_GAME_FEATURE_H_

#include <stdbool.h>

/**
 * Named, opt-in engine features.
 *
 * A thin layer over `settings` rather than a second configuration system: each
 * feature is a settings key with a default and an optional change callback,
 * and `arcanum.cfg` remains the only place a value lives. What this adds is a
 * *name* the rest of the tree can agree on -- the engine, the mod toolkit and
 * a mod manifest all mean the same thing by "uap fixes".
 *
 * `feature_list.h` is the registry. Because the enum is generated from it,
 * `feature_enabled(FEATURE_TYPO)` does not compile; there is no way to ask
 * about a feature that was never declared. That matters because the failure
 * mode this replaces is a silent one: `settings_get_value` answers 0 for a key
 * nobody registered, which reads exactly like "the feature is off".
 *
 * The by-name path exists for callers outside the engine's own code -- a mod
 * manifest, a console -- and it refuses rather than guessing: see
 * `feature_find`.
 */

typedef enum FeatureId {
#define FEATURE(id, key, default_enabled) FEATURE_##id,
#include "game/feature_list.h"
#undef FEATURE
    FEATURE_COUNT,
} FeatureId;

typedef void (*FeatureChangedFunc)(void);

/**
 * Registers every declared feature with `settings` and reads its current
 * value.
 *
 * Call once, after `settings_load` and **before** the modules that attach
 * callbacks. Registration deliberately does not fire callbacks: none can be
 * attached yet, and a module that needs its callback run at init calls it
 * itself, which is what `uap_init` has always done.
 */
void feature_init(void);

/**
 * Attaches a change callback, or clears it with `NULL`.
 *
 * The callback fires when the value changes, not when it is attached -- the
 * same contract `settings_register` has, and the reason `uap_init` calls its
 * own callback once by hand.
 */
void feature_on_change(FeatureId id, FeatureChangedFunc func);

/** Whether a feature is switched on. */
bool feature_enabled(FeatureId id);

/** The `arcanum.cfg` key a feature reads, for diagnostics and tooling. */
const char* feature_key(FeatureId id);

/** What a feature reads when its key is absent from the config. */
bool feature_default(FeatureId id);

/**
 * Looks a feature up by its settings key.
 *
 * Returns false and leaves `*id` untouched when no such feature is declared.
 * **The false is the refusal, and callers must treat it as one** -- reporting
 * an unknown flag rather than proceeding as though it were off. A mod that
 * asks for a feature this build does not have is a mod that will not behave as
 * its author intended, and saying so beats running it quietly.
 */
bool feature_find(const char* key, FeatureId* id);

// CE (release 1, R4): set a feature for this session only, as an enabled mod
// bundle does (`ce_mods.c`). The config is not written, so turning the bundle
// off restores what the config says; while overridden, a change to the config
// value does not move the feature. Call after `feature_init`, before modules
// attach their callbacks.
void feature_override(FeatureId id, bool enabled);
bool feature_overridden(FeatureId id);
// Every override dropped; each feature reads its config value again.
void feature_clear_overrides(void);

#endif /* ARCANUM_GAME_FEATURE_H_ */
