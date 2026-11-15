#ifndef ARCANUM_GAME_CE_MODS_H_
#define ARCANUM_GAME_CE_MODS_H_

#include <stdbool.h>
#include <stddef.h>

// CE (release 1, R4, 2026-10-13): optional mod bundles a release ships, each
// toggled by one line of the config.
//
// A bundle is a folder `ce-mods/<id>/` in the game folder: the mod's files,
// laid out as the game reads them (`rules/weapons.mes`), and `features.cfg`,
// the switches the mod sets (`weapon rules=1`, one per line). For every bundle
// present, CE registers the setting `mod <id>` (default 0). When it is 1, CE
// sets the bundle's switches for the session - never writing them into the
// config - and mounts the folder above `ce-data`, so turning the mod off is
// one line and leaves nothing behind. The original exe knows neither folder.
//
// A file the module's own archives carry (`modules/Arcanum.dat` and its
// patches, mounted after the bundles) would hide the bundle's copy, so a
// bundle puts such files under `module/` (`module/rules/wmap_rnd.mes`). That
// folder is mounted with the module, above its archives and below its folder:
// the module folder stays on top, where the game writes saves.

#define CE_MODS_DIR "ce-mods"
#define CE_MODS_MAX 16
#define CE_MODS_ID_MAX 64

// Pure: a bundle id is lower-case letters, digits and '-', 1-63 long.
bool ce_mods_id_valid(const char* id);

// Pure: one line of `features.cfg`. True, with `key` (of `key_size`) and
// `*value` set, for a `key=value` line; false for a blank line, a comment
// (`#` or `;`) or anything malformed. Spaces around the key and value are
// trimmed; the value is a whole number.
bool ce_mods_parse_line(const char* line, char* key, size_t key_size, int* value);

// After `feature_init`: find the bundles, register their settings, and apply
// the switches of those enabled.
void ce_mods_init(void);

// From `gamelib_load_data`, after `ce-data`: mount the enabled bundles.
void ce_mods_mount(void);

// From `gamelib_load_module_data`, after the module's archives and before its
// folder: mount each enabled bundle's `module/` folder, if it has one.
void ce_mods_mount_module(void);

// With the module's data: unmount what `ce_mods_mount_module` mounted.
void ce_mods_unmount_module(void);

// CE (2026-11-15): a map's placed objects are read from a cache the engine
// builds from the map's `.mob` files and does not rebuild while its module
// GUID matches (map_preprocess_mobile, quirk 33), so a bundle's `.mob` files
// would be unseen wherever the player already has the cache, and would stay
// in it after the bundle is turned off. The signature names the enabled
// bundles that carry a folder for the map (`module\maps\<map>` or
// `maps\<map>`), comma-separated in mount order, "" for none; the cache is
// rebuilt whenever it differs from the one recorded beside it.
#define CE_MODS_SIGNATURE_MAX (CE_MODS_MAX * CE_MODS_ID_MAX)
// Bundles found at start, on or off: 0 means a game folder with no `ce-mods`,
// where map loading takes the original's path call for call.
extern int ce_mods_found_count;
void ce_mods_map_signature(const char* map_name, char* buffer, size_t size);

// Pure: append `id` to a signature, comma-separated. False, and the buffer
// unchanged, when it does not fit.
bool ce_mods_signature_append(char* buffer, size_t size, const char* id);

#endif /* ARCANUM_GAME_CE_MODS_H_ */
