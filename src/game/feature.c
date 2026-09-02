#include "game/feature.h"

#include <string.h>

#include <tig/tig.h>

#include "game/gamelib.h"
#include "game/settings.h"

typedef struct FeatureEntry {
    const char* key;
    bool default_enabled;
    bool enabled;
    FeatureChangedFunc changed;
} FeatureEntry;

static void feature_changed(FeatureId id);

// One row per declaration, in enum order, so `features[id]` needs no search.
static FeatureEntry features[FEATURE_COUNT] = {
#define FEATURE(id, key, default_enabled) { key, default_enabled, default_enabled, NULL },
#include "game/feature_list.h"
#undef FEATURE
};

// `settings` callbacks take no arguments, so one shared trampoline could not
// tell which key changed. Generating one per feature is what lets the registry
// stay a table instead of a search over every key on every change.
#define FEATURE(id, key, default_enabled)     \
    static void feature_trampoline_##id(void) \
    {                                         \
        feature_changed(FEATURE_##id);        \
    }
#include "game/feature_list.h"
#undef FEATURE

static bool feature_id_valid(FeatureId id)
{
    // Through `int` because an enum with only non-negative enumerators may be
    // unsigned, and `id >= 0` would then be a warning rather than a check.
    int value = (int)id;

    if (value >= 0 && value < FEATURE_COUNT) {
        return true;
    }

    // Unreachable from a call site naming a `FEATURE_*` constant, which is the
    // whole point of the enum. Reachable only through a cast or a value that
    // came from outside, and answering "off" there would be the silent false
    // this module exists to remove.
    tig_debug_printf("feature: id %d is out of range (%d declared)\n",
        value,
        FEATURE_COUNT);
    return false;
}

void feature_init(void)
{
    int index;

    // The default is a string because that is what `settings` stores, and it
    // is only ever used when the key is absent from the config: `settings_load`
    // has already registered whatever the file held, and registering an
    // existing key keeps its value and only replaces the callback.
    //
    // Registering does not fire the callback -- `settings_register` merely
    // stores it -- so installing the trampolines here cannot run a callback
    // against a registry that is still being built.
#define FEATURE(id, key, default_enabled) \
    settings_register(&settings,          \
        key,                              \
        default_enabled ? "1" : "0",      \
        feature_trampoline_##id);
#include "game/feature_list.h"
#undef FEATURE

    for (index = 0; index < FEATURE_COUNT; index++) {
        features[index].enabled = settings_get_value(&settings, features[index].key) != 0;
    }
}

void feature_on_change(FeatureId id, FeatureChangedFunc func)
{
    if (!feature_id_valid(id)) {
        return;
    }

    features[id].changed = func;
}

bool feature_enabled(FeatureId id)
{
    if (!feature_id_valid(id)) {
        return false;
    }

    return features[id].enabled;
}

const char* feature_key(FeatureId id)
{
    if (!feature_id_valid(id)) {
        return NULL;
    }

    return features[id].key;
}

bool feature_default(FeatureId id)
{
    if (!feature_id_valid(id)) {
        return false;
    }

    return features[id].default_enabled;
}

bool feature_find(const char* key, FeatureId* id)
{
    int index;

    if (key == NULL) {
        return false;
    }

    for (index = 0; index < FEATURE_COUNT; index++) {
        if (strcmp(features[index].key, key) == 0) {
            if (id != NULL) {
                *id = (FeatureId)index;
            }
            return true;
        }
    }

    return false;
}

void feature_changed(FeatureId id)
{
    if (!feature_id_valid(id)) {
        return;
    }

    features[id].enabled = settings_get_value(&settings, features[id].key) != 0;

    if (features[id].changed != NULL) {
        features[id].changed();
    }
}
