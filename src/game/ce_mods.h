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

#endif /* ARCANUM_GAME_CE_MODS_H_ */
