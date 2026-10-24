#include "game/area.h"

#include <string.h>

#include "game/location.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/sector.h"
#include "game/townmap.h"
#include "game/ui.h"

/**
 * Default area detection radius when it's not specified.
 */
#define DEFAULT_RADIUS 5

typedef uint8_t AreaFlags;

#define AREA_KNOWN 0x1u

static int get_nearest_area(int64_t obj);
static int area_from_loc(int64_t loc);

/**
 * The number of areas.
 *
 * 0x5FF5A8
 */
static int num_areas;

/**
 * Array of area locations.
 *
 * 0x5FF5AC
 */
static int64_t* area_locations;

/**
 * Array of area names.
 *
 * 0x5FF5B0
 */
static char** area_names;

/**
 * "gamearea.mes"
 *
 * 0x5FF5B4
 */
static mes_file_handle_t area_gamearea_mes_file;

/**
 * Per-player area flags (in multiplayer mode), tracking known areas.
 *
 * 0x5FF5B8
 */
static AreaFlags* area_flags_per_player[8];

/**
 * Array of area short descriptions.
 *
 * 0x5FF5D8
 */
static char** area_descriptions;

/**
 * Array of x offsets to area worldmap labels.
 *
 * 0x5FF5DC
 */
static int* area_wm_x_offsets;

/**
 * Array of y offsets to area worldmap labels.
 *
 * 0x5FF5E0
 */
static int* area_wm_y_offsets;

/**
 * Array of area detection radiuses.
 *
 * 0x5FF5E4
 */
static int* area_radiuses;

/**
 * Array of area flags, tracking known areas in single-player mode.
 *
 * 0x5FF5E8
 */
static AreaFlags* area_flags;

/**
 * Last area marked known in single-player mode.
 *
 * 0x5FF5EC
 */
static int area_last_known_area;

/**
 * Per-player last area marked known in multiplayer mode.
 *
 * 0x5FF5F0
 */
static int area_last_known_area_per_player[8];

/**
 * Called when the game is initialized.
 *
 * 0x4CA940
 */
bool area_init(GameInitInfo* init_info)
{
    (void)init_info;

    num_areas = 0;

    return true;
}

/**
 * Called when the game is being reset.
 *
 * 0x4CA950
 */
void area_reset(void)
{
    int area;
    int player;

    // Clear local player area flags.
    for (area = 0; area < num_areas; area++) {
        area_flags[area] = 0;
    }

    // Clear per-player area flags for all players.
    for (player = 0; player < 8; player++) {
        for (area = 0; area < num_areas; area++) {
            area_flags_per_player[player][area] = 0;
        }
    }
}

/**
 * Called when the game shuts down.
 *
 * 0x4CA9A0
 */
void area_exit(void)
{
}

/**
 * Called when a module is being loaded.
 *
 * 0x4CA9B0
 */
bool area_mod_load(void)
{
    MesFileEntry mes_file_entry;
    int index;
    char* str;
    int64_t x;
    int64_t y;
    char* pch;
    int radius;

    num_areas = 0;

    // Load module-specific areas message file (optional).
    if (!mes_load("mes\\gamearea.mes", &area_gamearea_mes_file)) {
        return true;
    }

    // Count the number of areas.
    mes_file_entry.num = 0;
    mes_get_msg(area_gamearea_mes_file, &mes_file_entry);
    do {
        num_areas++;
    } while (mes_find_next(area_gamearea_mes_file, &mes_file_entry));

    // Allocate arrays for area data.
    area_names = (char**)MALLOC(sizeof(*area_names) * num_areas);
    area_descriptions = (char**)MALLOC(sizeof(*area_descriptions) * num_areas);
    area_locations = (int64_t*)MALLOC(sizeof(*area_locations) * num_areas);
    area_flags = (uint8_t*)MALLOC(sizeof(*area_flags) * num_areas);

    for (index = 0; index < 8; index++) {
        area_flags_per_player[index] = (uint8_t*)MALLOC(sizeof(*area_flags_per_player[index]) * num_areas);
    }

    area_radiuses = (int*)MALLOC(sizeof(*area_radiuses) * num_areas);
    area_wm_x_offsets = (int*)MALLOC(sizeof(*area_wm_x_offsets) * num_areas);
    area_wm_y_offsets = (int*)MALLOC(sizeof(*area_wm_y_offsets) * num_areas);

    // Parse each area entry.
    index = 0;
    mes_file_entry.num = 0;
    mes_get_msg(area_gamearea_mes_file, &mes_file_entry);
    tig_str_parse_set_separator(',');

    do {
        str = mes_file_entry.str;

        // The first pair are coordinates of the placement of the area on the
        // world map.
        tig_str_parse_value_64(&str, &x);
        tig_str_parse_value_64(&str, &y);
        area_locations[index] = location_make(x, y);

        // The next pair are offsets (in pixels) of the area label on the
        // world map.
        area_wm_x_offsets[index] = 0;
        area_wm_y_offsets[index] = 0;
        tig_str_parse_value(&str, &(area_wm_x_offsets[index]));
        tig_str_parse_value(&str, &(area_wm_y_offsets[index]));

        // Parse the name and short description, separated by '/'.
        pch = strchr(mes_file_entry.str, '/');
        if (pch != NULL) {
            area_names[index] = pch + 1;

            pch = strchr(pch + 1, '/');
            if (pch != NULL) {
                // Clamp area name.
                *pch++ = '\0';
                area_descriptions[index] = pch;

                area_flags[index] = 0;
                area_radiuses[index] = DEFAULT_RADIUS * 64;

                // Parse radius if present.
                pch = strchr(pch, '/');
                if (pch != NULL) {
                    // Clamp area short description.
                    *pch++ = '\0';

                    if (tig_str_parse_named_value(&pch, "Radius:", &radius)) {
                        if (index > 0) {
                            // Convert radius in sectors to radius in tiles.
                            area_radiuses[index] = radius * 64;
                        } else if (index == 0) {
                            // Special case - radius of unknown area is always
                            // 0.
                            area_radiuses[AREA_UNKNOWN] = 0;
                        } else {
                            // NOTE: Verified at 0x4CAC50-0x4CAC6D - the
                            // original also has the unreachable
                            // index<0 -> -1 store.
                            area_radiuses[index] = -1;
                        }
                    }
                }
            } else {
                tig_debug_printf("Area: area_init: ERROR: Line %d has invalid description data!\n", index);
            }
        } else {
            tig_debug_printf("Area: area_init: ERROR: Line %d has invalid location data!\n", index);
        }

        index++;
    } while (mes_find_next(area_gamearea_mes_file, &mes_file_entry));

    return true;
}

/**
 * Called when a module is being unloaded.
 *
 * 0x4CACB0
 */
void area_mod_unload(void)
{
    int player;

    if (area_gamearea_mes_file != MES_FILE_HANDLE_INVALID) {
        FREE(area_names);
        FREE(area_descriptions);
        FREE(area_locations);
        FREE(area_flags);

        for (player = 0; player < 8; player++) {
            FREE(area_flags_per_player[player]);
        }

        FREE(area_radiuses);
        FREE(area_wm_x_offsets);
        FREE(area_wm_y_offsets);

        mes_unload(area_gamearea_mes_file);
        area_gamearea_mes_file = MES_FILE_HANDLE_INVALID;
    }
}

// CE: The original writes `area_count()` bytes nine times with nothing in the
// stream recording how many areas were in force, so adding one area shifts
// every later module by nine bytes and the load dies at the next sentinel
// (MODDING-NOTES.md quirk 17). That made every content mod save-incompatible:
// a location mod required a new game, and uninstalling one invalidated every
// save written while it was installed.
//
// CE writes a `magic + count` prefix and reads both layouts. Old saves keep
// loading; new ones survive an area being added or removed.
//
// **Detection is determined, not guessed.** `AreaFlags` is a uint8_t with the
// single flag AREA_KNOWN = 0x1, so every byte of `area_flags` is 0 or 1, and
// the first four bytes of an original-layout block are all in {0, 1}. No byte
// of 'AREA' is 0 or 1, so an old save cannot be mistaken for a new one.
//
// What this costs is byte-identity with Troika's own output for this module,
// which makes the savegame round-trip one-way. That is the Compatible regime
// in FIDELITY-CONTRACT.md, and nothing outside this project reads a save CE
// wrote.
#define AREA_SAVE_MAGIC 0x41455241u /* 'AREA', little-endian */

// A count read out of a file is not trusted just because we wrote it: a
// truncated or corrupted save must be refused rather than used to size an
// allocation.
#define AREA_SAVE_MAX_COUNT 65536

// How many areas an original-layout block was written with, from the distance
// to gamelib's sentinel. Returns 0 when it cannot be determined, which the
// caller treats as "behave as the original did".
static int area_count_from_block_length(TigFile* stream, int start)
{
    // 9 flag arrays + 9 int32s, so the largest block worth considering is
    // bounded by the same limit as the explicit count.
    enum { SCAN_MAX = 9 * AREA_SAVE_MAX_COUNT + 9 * (int)sizeof(int) + 8 };
    unsigned char* buffer;
    int read_bytes;
    int index;
    int length = 0;
    int payload;

    buffer = (unsigned char*)MALLOC(SCAN_MAX);
    read_bytes = (int)tig_file_fread(buffer, 1, SCAN_MAX, stream);

    for (index = 0; index + 4 <= read_bytes; index++) {
        if (buffer[index] == 0xFE
            && buffer[index + 1] == 0xCA
            && buffer[index + 2] == 0xEF
            && buffer[index + 3] == 0xBE) {
            length = index;
            break;
        }
    }
    FREE(buffer);

    if (length == 0) return 0;

    payload = length - 9 * (int)sizeof(int);
    if (payload <= 0 || payload % 9 != 0) return 0;

    payload /= 9;
    if (payload > AREA_SAVE_MAX_COUNT) return 0;

    (void)start;
    return payload;
}

static bool area_read_flags(TigFile* stream, AreaFlags* dst, int saved_count)
{
    AreaFlags* scratch;
    int copy;

    if (saved_count == num_areas) {
        return tig_file_fread(dst, num_areas, 1, stream) == 1;
    }

    // Heap, not stack. `saved_count` is bounded above but still comes from a
    // file, and a multi-kilobyte stack array sized from file content is the
    // shape of porting bug #11.
    scratch = (AreaFlags*)MALLOC(sizeof(*scratch) * saved_count);
    if (tig_file_fread(scratch, saved_count, 1, stream) != 1) {
        FREE(scratch);
        return false;
    }

    // Areas the save never knew about stay unknown, which is what an area a
    // mod added after the save was written should be.
    copy = saved_count < num_areas ? saved_count : num_areas;
    memset(dst, 0, num_areas);
    memcpy(dst, scratch, copy);
    FREE(scratch);
    return true;
}

/**
 * Called when the game is being loaded.
 *
 * 0x4CAD50
 */
bool area_load(GameLoadInfo* load_info)
{
    int player;
    unsigned int magic;
    int saved_count;
    int pos;

    if (tig_file_fgetpos(load_info->stream, &pos) != 0) return false;
    if (tig_file_fread(&magic, sizeof(magic), 1, load_info->stream) != 1) return false;

    if (magic == AREA_SAVE_MAGIC) {
        if (tig_file_fread(&saved_count, sizeof(saved_count), 1, load_info->stream) != 1) return false;
        if (saved_count <= 0 || saved_count > AREA_SAVE_MAX_COUNT) {
            tig_debug_printf("area_load: refusing implausible area count %d\n", saved_count);
            return false;
        }
    } else {
        // An original-layout save has no count, so recover it from the block
        // length. The module is followed by gamelib's 0xBEEFCAFE sentinel,
        // and the block is exactly `9 * count + 9 * sizeof(int)` bytes, so
        // the count is arithmetic once the end is known.
        //
        // **The sentinel cannot occur inside an Area block**, by the same
        // argument that makes the magic safe: the block holds only flag bytes
        // (0 or 1) and small area indices, and none of 0xFE, 0xCA, 0xEF, 0xBE
        // can be produced by either. So the first match after the block start
        // is this module's own sentinel and not a coincidence in the data.
        //
        // Without this, the count prefix would rescue only saves written
        // after it existed -- every save a player already had would still die
        // at the sentinel the moment a mod changed the area list, which is the
        // whole of quirk 17 and the reason for the change.
        if (tig_file_fsetpos(load_info->stream, &pos) != 0) return false;
        saved_count = area_count_from_block_length(load_info->stream, pos);
        if (saved_count <= 0) {
            // Could not be determined: behave exactly as the original did,
            // so a save this cannot read is no worse off than before.
            saved_count = num_areas;
        }
        if (tig_file_fsetpos(load_info->stream, &pos) != 0) return false;
    }

    if (!area_read_flags(load_info->stream, area_flags, saved_count)) return false;
    if (tig_file_fread(&area_last_known_area, sizeof(area_last_known_area), 1, load_info->stream) != 1) return false;

    for (player = 0; player < 8; player++) {
        if (!area_read_flags(load_info->stream, area_flags_per_player[player], saved_count)) return false;
        if (tig_file_fread(&(area_last_known_area_per_player[player]), sizeof(area_last_known_area_per_player[0]), 1, load_info->stream) != 1) return false;
    }

    return true;
}

/**
 * Called when the game is being saved.
 *
 * 0x4CADF0
 */
bool area_save(TigFile* stream)
{
    int player;
    unsigned int magic = AREA_SAVE_MAGIC;
    int count = num_areas;

    // CE: see area_load. The count is what makes a save survive an area being
    // added or removed.
    if (tig_file_fwrite(&magic, sizeof(magic), 1, stream) != 1) return false;
    if (tig_file_fwrite(&count, sizeof(count), 1, stream) != 1) return false;

    if (tig_file_fwrite(area_flags, num_areas, 1, stream) != 1) return false;
    if (tig_file_fwrite(&area_last_known_area, sizeof(area_last_known_area), 1, stream) != 1) return false;

    for (player = 0; player < 8; player++) {
        if (tig_file_fwrite(area_flags_per_player[player], num_areas, 1, stream) != 1) return false;
        if (tig_file_fwrite(&(area_last_known_area_per_player[player]), sizeof(area_last_known_area_per_player[0]), 1, stream) != 1) return false;
    }

    return true;
}

/**
 * Returns the total number of areas.
 *
 * 0x4CAE80
 */
int area_count(void)
{
    return num_areas;
}

/**
 * Retrieves the name of a specific area.
 *
 * 0x4CAE90
 */
char* area_get_name(int area)
{
    return area >= 0 && area < num_areas
        ? area_names[area]
        : NULL;
}

/**
 * Retrieves the description of a specific area.
 *
 * 0x4CAEB0
 */
char* area_get_description(int area)
{
    return area >= 0 && area < num_areas
        ? area_descriptions[area]
        : NULL;
}

/**
 * Retrieves the location of a specific area.
 *
 * 0x4CAED0
 */
int64_t area_get_location(int area)
{
    return area >= 0 && area < num_areas
        ? area_locations[area]
        : 0;
}

/**
 * Retrieves the world map label offsets for a specific area.
 *
 * 0x4CAF00
 */
void area_get_wm_offset(int area, int* x, int* y)
{
    // NOTE: Verified at 0x4CAF0A/0x4CAF1D - the original performs the same
    // unguarded indexed reads with no bounds check.
    *x = area_wm_x_offsets[area];
    *y = area_wm_y_offsets[area];
}

/**
 * Checks if an area is known to a player character.
 *
 * 0x4CAF50
 */
bool area_is_known(int64_t pc_obj, int area)
{
    // Validate the object and ensure it's a player character.
    if (pc_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return false;
    }

    if (pc_obj == player_get_local_pc_obj()) {
        return (area_flags[area] & AREA_KNOWN) != 0;
    }

    return false;
}

/**
 * Marks an area as known to a player character.
 *
 * 0x4CAFD0
 */
bool area_set_known(int64_t pc_obj, int area)
{
    // If the area is already known, no action is needed.
    if (area_is_known(pc_obj, area)) {
        return true;
    }

    // Validate the object and ensure it's a player character.
    if (pc_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return false;
    }

    if (pc_obj == player_get_local_pc_obj()) {
        area_flags[area] |= AREA_KNOWN;
        area_last_known_area = area;
        ui_toggle_primary_button(UI_PRIMARY_BUTTON_WORLDMAP, true);
        return true;
    }

    return false;
}

/**
 * Retrieves the last known area for a player character.
 *
 * 0x4CB100
 */
int area_get_last_known_area(int64_t pc_obj)
{
    // Validate the object and ensure it's a player character.
    if (pc_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return AREA_UNKNOWN;
    }

    if (pc_obj == player_get_local_pc_obj()) {
        return area_last_known_area;
    }

    return AREA_UNKNOWN;
}

/**
 * Resets the last known area for a player character.
 *
 * 0x4CB160
 */
void area_reset_last_known_area(int64_t pc_obj)
{
    // Validate the object and ensure it's a player character.
    if (pc_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    if (pc_obj == player_get_local_pc_obj()) {
        area_last_known_area = AREA_UNKNOWN;
    }
}

/**
 * Finds the nearest area to a given location.
 *
 * 0x4CB220
 */
int get_nearest_area(int64_t loc)
{
    int area;
    int nearest_area;
    int64_t nearest_dist;
    int64_t distance;

    // Initialize with the unknown area as a base.
    nearest_area = AREA_UNKNOWN;
    nearest_dist = location_dist(area_get_location(nearest_area), loc);

    // Compare distances to all other areas.
    for (area = 1; area < num_areas; area++) {
        distance = location_dist(area_get_location(area), loc);
        if (distance < nearest_dist) {
            nearest_area = area;
            nearest_dist = distance;
        }
    }

    return nearest_area;
}

/**
 * Finds the nearest known area to a location within a range for a player
 * character.
 *
 * 0x4CB2A0
 */
int area_get_nearest_known_area(int64_t loc, int64_t pc_obj, int64_t range)
{
    int area;
    int nearest_area;
    int64_t nearest_dist;
    int64_t dist;

    // Validate the object and ensure it's a player character.
    if (pc_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return AREA_UNKNOWN;
    }

    // FIX (porting bug #319): only an area nearer than the range counts
    // (0x4cb329-0x4cb335, 0x4cb37d-0x4cb38b and 0x4cb3cc-0x4cb3d8 all test
    // dist < range); CE started from range + 1 and accepted an area exactly
    // at it (320 tiles from the world map's callers).
    nearest_area = AREA_UNKNOWN;
    nearest_dist = range;

    // Check all areas for known status and distance.
    for (area = 1; area < num_areas; area++) {
        dist = location_dist(area_get_location(area), loc);
        if (dist < nearest_dist) {
            if (area_is_known(pc_obj, area)) {
                nearest_area = area;
                nearest_dist = dist;
            }
        }
    }

    return nearest_area;
}

// FIX: CE takes the nearest area whose own radius holds `loc`. The original
// takes the first such area in list order (0x4cb4ee-0x4cb552), then any later
// area nearer than it and inside the current choice's radius, whether or not
// its own radius holds `loc` (0x4cb574-0x4cb5f4), so the Teleport Exit spell
// (its only caller, on the start map) could pick an area the caster is not in.
//
// 0x4CB4D0
int area_get_nearest_area_in_range(int64_t loc, bool safe)
{
    int area;
    int nearest_area;
    int64_t nearest_dist;
    int64_t dist;
    int64_t radius;

    // Initialize with an invalid distance to ensure at least one area is
    // considered.
    nearest_area = AREA_UNKNOWN;
    nearest_dist = 99999;

    // Check all areas for distance.
    for (area = 1; area < num_areas; area++) {
        radius = area_radiuses[area];
        if (safe && radius <= 0) {
            radius = 64;
        }

        dist = location_dist(area_get_location(area), loc);
        if (dist < radius
            && dist < nearest_dist) {
            nearest_area = area;
            nearest_dist = dist;
        }
    }

    return nearest_area;
}

/**
 * Determines the area from a specified location.
 *
 * 0x4CB630
 */
int area_from_loc(int64_t loc)
{
    int loc_townmap;
    int area;
    int area_townmap;

    // Retrieve the townmap for the sector containing the specified location.
    loc_townmap = townmap_get(sector_id_from_loc(loc));
    if (loc_townmap == TOWNMAP_NONE) {
        return AREA_UNKNOWN;
    }

    // Find the nearest area to the specified location.
    area = get_nearest_area(loc);
    if (area == AREA_UNKNOWN) {
        return AREA_UNKNOWN;
    }

    // Retrieve the townmap from the sector containing area's origin location.
    area_townmap = townmap_get(sector_id_from_loc(area_get_location(area)));
    if (area_townmap == TOWNMAP_NONE) {
        return AREA_UNKNOWN;
    }

    // Ensure both townmap IDs match.
    if (loc_townmap != area_townmap) {
        return AREA_UNKNOWN;
    }

    return area;
}

/**
 * Retrieves the area associated with an object's current map.
 *
 * The `obj` is implied to be a player character, but this is not enforced.
 *
 * 0x4CB6A0
 */
int area_of_object(int64_t obj)
{
    int map;
    int area;

    // Validate the object.
    if (obj == OBJ_HANDLE_NULL) {
        return AREA_UNKNOWN;
    }

    map = map_current_map();

    // Retrieve area from the current map.
    if (!map_get_area(map, &area)) {
        return AREA_UNKNOWN;
    }

    // Special case - the primary map usually does not have associated area.
    // In this case attempt to obtain area from PC's location.
    if (area == AREA_UNKNOWN
        && map == map_by_type(MAP_TYPE_START_MAP)) {
        area = area_from_loc(obj_field_int64_get(obj, OBJ_F_LOCATION));
    }

    return area;
}
