// The CE half of the emulation differential.
//
// `arcanum-work/harness/emudiff` runs a function out of the original
// Arcanum.exe under emulation and compares its answers against CE's C on the
// same inputs. This binary is how the driver reaches the C: it speaks a line
// protocol on stdin/stdout so thousands of calls cost one process, not one
// process each.
//
//     > list                     -> one "name arity" line per entry, then "."
//     > stat_cost 7              -> "12"
//     > <unknown or bad arity>   -> "!<reason>"
//
// Arguments are signed 64-bit decimal. A wrapper narrows to whatever the
// real signature takes, so the protocol stays uniform while the function
// under test is called exactly as the game calls it. A wrapper answers by
// writing its `out` array; the main loop prints the first `outputs`
// entries space-separated, so a function with several pointer outputs
// (e.g. location_xy) returns one value per pointer, in declaration order.
//
// By-pointer arguments travel as the packed integer the struct (or the
// scratch slot) holds: the wrapper unpacks, calls, and repacks the same
// way. A by-pointer output must be seeded from its drawn argument before
// the call, so an early-return path leaves the same value on both sides.
//
// Only add functions here that are *pure with respect to reachable state*:
// no object handles, no loaded data files, no allocation, no art-database
// access (tig_art_id_frame_set needs tig_art_anim_data, so it is out even
// though it looks like a bit twiddler). The emulator has no Windows under it
// and no game state in it, so anything else diverges for reasons that say
// nothing about the port. Seeded generator state and zero-initialised
// globals are fine when both sides expose the same value - see random_seed
// and the location_* cases.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game/combat.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/item.h"
#include "game/light.h"
#include "game/location.h"
#include "game/magictech.h"
#include "game/mt_item.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/tech.h"
#include "game/terrain.h"
#include "game/timeevent.h"
#include "tig/tig.h"

#define MAX_ARGS 16
#define MAX_OUT 3
#define MAX_LINE 512

typedef void (*OracleFunc)(const int64_t* args, int64_t* out);

typedef struct {
    const char* name;
    int arity;
    int outputs;
    OracleFunc func;
} OracleEntry;

static void o_random_seed(const int64_t* a, int64_t* out)
{
    random_seed((unsigned int)a[0]);
    out[0] = 0;
}

static void o_random_rand(const int64_t* a, int64_t* out)
{
    (void)a;
    out[0] = random_rand();
}

static void o_random_between(const int64_t* a, int64_t* out)
{
    out[0] = random_between((int)a[0], (int)a[1]);
}

static void o_stat_cost(const int64_t* a, int64_t* out)
{
    out[0] = stat_cost((int)a[0]);
}

static void o_basic_skill_money(const int64_t* a, int64_t* out)
{
    out[0] = basic_skill_money((int)a[0], (int)a[1], (int)a[2]);
}

static void o_basic_skill_min_stat_level_required(const int64_t* a, int64_t* out)
{
    out[0] = basic_skill_min_stat_level_required((int)a[0]);
}

static void o_basic_skill_stat(const int64_t* a, int64_t* out)
{
    out[0] = basic_skill_stat((int)a[0]);
}

static void o_tech_skill_stat(const int64_t* a, int64_t* out)
{
    out[0] = tech_skill_stat((int)a[0]);
}

static void o_location_rot(const int64_t* a, int64_t* out)
{
    out[0] = location_rot(a[0], a[1]);
}

static void o_location_dist(const int64_t* a, int64_t* out)
{
    out[0] = location_dist(a[0], a[1]);
}

// A function whose answer is written through a pointer rather than returned.
//
// `DateTime` is a union over a `uint64_t`, so the whole struct travels as one
// integer and the line protocol stays what it is: the packed value goes in and
// the packed value comes back out. On the emulator's side the driver allocates
// the eight bytes, writes them, passes the address and reads them back - see
// `DateTimeArg` in `cases.py`.

static void o_datetime_sub_milliseconds(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    datetime_sub_milliseconds(&datetime, (unsigned int)a[1]);

    out[0] = (int64_t)datetime.value;
}

static void o_datetime_add_milliseconds(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    datetime_add_milliseconds(&datetime, (unsigned int)a[1]);

    out[0] = (int64_t)datetime.value;
}

static void o_datetime_seconds_since_reference_date(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_seconds_since_reference_date(&datetime);
}

static void o_datetime_get_hour(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_get_hour(&datetime);
}

static void o_datetime_get_hour_since_reference_date(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_get_hour_since_reference_date(&datetime);
}

static void o_datetime_get_minute(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_get_minute(&datetime);
}

static void o_datetime_get_day(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_get_day(&datetime);
}

static void o_datetime_get_month(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_get_month(&datetime);
}

static void o_datetime_get_year(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_get_year(&datetime);
}

static void o_datetime_from_milliseconds(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    datetime_from_milliseconds(&datetime, (unsigned int)a[1]);

    out[0] = (int64_t)datetime.value;
}

static void o_datetime_compare(const int64_t* a, int64_t* out)
{
    DateTime datetime1;
    DateTime datetime2;

    datetime1.value = (uint64_t)a[0];
    datetime2.value = (uint64_t)a[1];
    out[0] = datetime_compare(&datetime1, &datetime2);
}

static void o_datetime_is_at_least_milliseconds(const int64_t* a, int64_t* out)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    out[0] = datetime_is_at_least_milliseconds(&datetime, (unsigned int)a[1]);
}

static void o_datetime_add_datetime(const int64_t* a, int64_t* out)
{
    DateTime datetime;
    DateTime other;

    datetime.value = (uint64_t)a[0];
    other.value = (uint64_t)a[1];
    datetime_add_datetime(&datetime, &other);

    // Both pointers are printed: the driver compares every by-pointer
    // argument, and the second (read-only) one must agree that it is
    // untouched.
    out[0] = (int64_t)datetime.value;
    out[1] = (int64_t)other.value;
}

// -- location: coordinates and pointer outputs --------------------------
//
// The location family reads `location_view_options` (type and zoom) and the
// origin/limit globals. CE zero-initialises them, and the original's copies
// are zero in the emulator's image, so the default isometric state matches
// without setup. The limit-dependent cases seed both sides with
// `location_limits_set` first.

static void o_location_limits_set(const int64_t* a, int64_t* out)
{
    location_limits_set(a[0], a[1]);
    out[0] = 0;
}

static void o_location_in_dir(const int64_t* a, int64_t* out)
{
    int64_t new_loc = a[2];

    (void)location_in_dir(a[0], (int)a[1], &new_loc);
    // The driver reads the scratch bytes back as unsigned, so the wrapper
    // prints the same zero-extended value.
    out[0] = (int64_t)(uint64_t)new_loc;
}

static void o_location_in_range(const int64_t* a, int64_t* out)
{
    int64_t new_loc = a[3];

    (void)location_in_range(a[0], (int)a[1], (int)a[2], &new_loc);
    out[0] = (int64_t)(uint64_t)new_loc;
}

static void o_location_xy(const int64_t* a, int64_t* out)
{
    int64_t sx = a[1];
    int64_t sy = a[2];

    location_xy(a[0], &sx, &sy);
    out[0] = (int64_t)(uint64_t)sx;
    out[1] = (int64_t)(uint64_t)sy;
}

static void o_location_at(const int64_t* a, int64_t* out)
{
    int64_t loc = a[2];

    (void)location_at(a[0], a[1], &loc);
    out[0] = (int64_t)(uint64_t)loc;
}

static void o_location_normalize(const int64_t* a, int64_t* out)
{
    int64_t loc = a[0];
    int offset_x = (int)a[1];
    int offset_y = (int)a[2];

    (void)location_normalize(&loc, &offset_x, &offset_y);
    out[0] = (int64_t)(uint64_t)loc;
    out[1] = (int64_t)(uint32_t)offset_x;
    out[2] = (int64_t)(uint32_t)offset_y;
}

static void o_location_limits_get(const int64_t* a, int64_t* out)
{
    int64_t x = a[0];
    int64_t y = a[1];

    location_limits_get(&x, &y);
    out[0] = (int64_t)(uint64_t)x;
    out[1] = (int64_t)(uint64_t)y;
}

static void o_location_center_get(const int64_t* a, int64_t* out)
{
    (void)a;
    out[0] = location_center_get();
}

// 0x4B93F0: screen-space to world-tile linear transform.
static void o_sub_4B93F0(const int64_t* a, int64_t* out)
{
    int v1 = (int)a[2];
    int v2 = (int)a[3];

    location_offset_to_screen((int)a[0], (int)a[1], &v1, &v2);
    out[0] = (int64_t)(uint32_t)v1;
    out[1] = (int64_t)(uint32_t)v2;
}

// -- skill tail ---------------------------------------------------------

static void o_max_skill_for_stat_level(const int64_t* a, int64_t* out)
{
    out[0] = max_skill_for_stat_level((int)a[0]);
}

static void o_training_min_skill_level_required(const int64_t* a, int64_t* out)
{
    out[0] = training_min_skill_level_required((int)a[0]);
}

static void o_tech_skill_money(const int64_t* a, int64_t* out)
{
    out[0] = tech_skill_money((int)a[0], (int)a[1], (int)a[2]);
}

static void o_tech_skill_min_stat_level_required(const int64_t* a, int64_t* out)
{
    out[0] = tech_skill_min_stat_level_required((int)a[0]);
}

// Reads and advances `skill_gambling_next_roll` (zero in both binaries at
// start), so the two sides must stay in lockstep like the random cases.
static void o_skill_gambling_random_roll(const int64_t* a, int64_t* out)
{
    (void)a;
    out[0] = skill_gambling_random_roll();
}

// -- tig art id packing ------------------------------------------------
//
// Pure bit manipulation over the packed 32-bit art id. The setters with a
// range check print and clamp past it, which in the emulator would run the
// original's console code - the cases keep the value argument inside range.

static void o_tig_art_type(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_type((tig_art_id_t)a[0]);
}

static void o_tig_art_num_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_num_get((tig_art_id_t)a[0]);
}

static void o_tig_art_num_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_num_set((tig_art_id_t)a[0], (unsigned int)a[1]);
}

static void o_tig_art_id_rotation_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_rotation_get((tig_art_id_t)a[0]);
}

static void o_tig_art_id_rotation_inc(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_rotation_inc((tig_art_id_t)a[0]);
}

static void o_tig_art_id_rotation_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_rotation_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_id_frame_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_frame_get((tig_art_id_t)a[0]);
}

static void o_tig_art_id_palette_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_palette_get((tig_art_id_t)a[0]);
}

static void o_tig_art_id_palette_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_palette_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_id_flags_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_flags_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_id_anim_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_anim_get((tig_art_id_t)a[0]);
}

static void o_tig_art_id_anim_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_anim_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_critter_id_body_type_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_body_type_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_body_type_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_body_type_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_monster_id_specie_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_monster_id_specie_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_size_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_size_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_gender_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_gender_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_gender_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_gender_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_critter_id_armor_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_armor_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_armor_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_armor_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_critter_id_weapon_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_weapon_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_weapon_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_weapon_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_critter_id_shield_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_shield_get((tig_art_id_t)a[0]);
}

static void o_tig_art_critter_id_shield_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_critter_id_shield_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_tile_id_type_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_tile_id_type_get((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_flippable_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_tile_id_flippable_get((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_flippable1_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_tile_id_flippable1_get((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_flippable2_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_tile_id_flippable2_get((tig_art_id_t)a[0]);
}

static void o_tig_art_wall_id_p_piece_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_wall_id_p_piece_get((tig_art_id_t)a[0]);
}

static void o_tig_art_wall_id_p_piece_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_wall_id_p_piece_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_wall_id_num_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_wall_id_num_get((tig_art_id_t)a[0]);
}

static void o_tig_art_wall_id_variation_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_wall_id_variation_get((tig_art_id_t)a[0]);
}

static void o_tig_art_wall_id_variation_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_wall_id_variation_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_id_damaged_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_damaged_get((tig_art_id_t)a[0]);
}

static void o_tig_art_id_damaged_set(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_id_damaged_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_portal_id_type_get(const int64_t* a, int64_t* out)
{
    out[0] = tig_art_portal_id_type_get((tig_art_id_t)a[0]);
}

// The id constructors: range checks plus bit packing, answering through the
// pointer. The drawn value seeds the output first so an invalid-parameter
// path (which leaves the pointer untouched on both sides) still compares
// equal, and the comparison then catches any side that wrongly writes.

static void o_tig_art_wall_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[6];

    (void)tig_art_wall_id_create((unsigned int)a[0], (int)a[1], (int)a[2],
        (int)a[3], (unsigned int)a[4], (int)a[5], &art_id);
    out[0] = art_id;
}

static void o_tig_art_tile_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[8];

    (void)tig_art_tile_id_create((unsigned int)a[0], (unsigned int)a[1],
        (unsigned int)a[2], (unsigned int)a[3], (unsigned int)a[4],
        (unsigned int)a[5], (unsigned int)a[6], (unsigned int)a[7], &art_id);
    out[0] = art_id;
}

static void o_tig_art_critter_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[9];

    (void)tig_art_critter_id_create((unsigned int)a[0], (int)a[1], (int)a[2],
        (unsigned int)a[3], (unsigned int)a[4], (int)a[5], (int)a[6],
        (int)a[7], (unsigned int)a[8], &art_id);
    out[0] = art_id;
}

static void o_tig_art_monster_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[8];

    (void)tig_art_monster_id_create((int)a[0], (int)a[1], (unsigned int)a[2],
        (unsigned int)a[3], (int)a[4], (int)a[5], (int)a[6],
        (unsigned int)a[7], &art_id);
    out[0] = art_id;
}

static void o_tig_art_unique_npc_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[7];

    (void)tig_art_unique_npc_id_create((int)a[0], (unsigned int)a[1],
        (unsigned int)a[2], (int)a[3], (int)a[4], (int)a[5],
        (unsigned int)a[6], &art_id);
    out[0] = art_id;
}

static void o_tig_art_portal_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[6];

    (void)tig_art_portal_id_create((unsigned int)a[0], (int)a[1], (int)a[2],
        (unsigned int)a[3], (int)a[4], (unsigned int)a[5], &art_id);
    out[0] = art_id;
}

static void o_tig_art_scenery_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[5];

    (void)tig_art_scenery_id_create((unsigned int)a[0], (int)a[1],
        (unsigned int)a[2], (int)a[3], (unsigned int)a[4], &art_id);
    out[0] = art_id;
}

static void o_tig_art_misc_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[2];

    (void)tig_art_misc_id_create((unsigned int)a[0], (unsigned int)a[1], &art_id);
    out[0] = art_id;
}

// --- game-layer additions, 2026-08-20 (roadmap B2) -----------------------
//
// Emudiff's first 79 cases were 44 tig plus the arithmetic leaves; the game
// itself had almost none. These are the game-layer functions that qualify
// under the purity rule at the top of this file: no object handles, no
// loaded data files, no allocation.
//
// `ai_perception_distance` and `calculate_bonus_character_points` were
// candidates and are deliberately absent: both are `static`, and exporting a
// function so a test can reach it changes the port's surface for the test's
// convenience. They stay unreachable until something else needs them.
//
// Several are bare table indexers with no bounds check of their own, so
// their case domains stop at the table's last element. Feeding them an index
// past the end would compare the original's adjacent .data against CE's,
// which says nothing about the port - the emulator would be reading two
// different programs' memory maps and reporting the difference as a defect.

static void o_combat_hit_loc_penalty(const int64_t* a, int64_t* out)
{
    out[0] = combat_hit_loc_penalty((int)a[0]);
}

static void o_critter_encumbrance_level_ratio(const int64_t* a, int64_t* out)
{
    out[0] = critter_encumbrance_level_ratio((int)a[0]);
}

static void o_item_armor_size(const int64_t* a, int64_t* out)
{
    out[0] = (int64_t)item_armor_size((int)a[0]);
}

// --- game-layer additions, 2026-08-21 (roadmap B2, second batch) ---------
//
// B2 reached combat, critter and item. This batch reaches the other half of
// the game layer that qualifies under the purity rule: the hand-transcribed
// static tables (spell icons, college icons, the tech degree costs, the
// default willpower table) and the packed-id decoders in terrain, dialog and
// light.
//
// A table copied out of a disassembly by hand is precisely the defect class
// this harness exists to find, and nothing else in the tree checks one. Every
// table indexer below is called only over the indices its table holds -- see
// the domain notes in cases.py, which say for each case what the bound is and
// why crossing it would compare two programs' memory layouts rather than
// their behaviour.

static void o_terrain_tid_base(const int64_t* a, int64_t* out)
{
    out[0] = terrain_tid_base((uint16_t)a[0]);
}

static void o_terrain_tid_from(const int64_t* a, int64_t* out)
{
    out[0] = terrain_tid_from((uint16_t)a[0]);
}

static void o_terrain_tid_transition(const int64_t* a, int64_t* out)
{
    out[0] = terrain_tid_transition((uint16_t)a[0]);
}

static void o_terrain_tid_tile(const int64_t* a, int64_t* out)
{
    out[0] = terrain_tid_tile((uint16_t)a[0]);
}

static void o_reaction_translate(const int64_t* a, int64_t* out)
{
    out[0] = reaction_translate((int)a[0]);
}

static void o_spell_icon(const int64_t* a, int64_t* out)
{
    out[0] = spell_icon((int)a[0]);
}

static void o_spell_college_small_icon(const int64_t* a, int64_t* out)
{
    out[0] = spell_college_small_icon((int)a[0]);
}

static void o_spell_college_large_icon(const int64_t* a, int64_t* out)
{
    out[0] = spell_college_large_icon((int)a[0]);
}

static void o_spell_min_level(const int64_t* a, int64_t* out)
{
    out[0] = spell_min_level((int)a[0]);
}

static void o_tech_degree_cost_get(const int64_t* a, int64_t* out)
{
    out[0] = tech_degree_cost_get((int)a[0]);
}

static void o_tech_degree_min_intelligence_get(const int64_t* a, int64_t* out)
{
    out[0] = tech_degree_min_intelligence_get((int)a[0]);
}

static void o_tech_schematic_get(const int64_t* a, int64_t* out)
{
    out[0] = tech_schematic_get((int)a[0], (int)a[1]);
}

static void o_mt_item_spell_to_magictech_spell(const int64_t* a, int64_t* out)
{
    out[0] = mt_item_spell_to_magictech_spell((int)a[0]);
}

// The two magictech accessors whose uninitialised path does not touch
// `magictech_spells`, which is a MALLOC'd pointer and therefore NULL in both
// binaries until magictech_init runs. Every other accessor in that family
// either dereferences it unguarded (magictech_is_magic and friends) or falls
// back to a bare constant, so these two are the whole of what is reachable
// here: min_intelligence is the constant itself, and min_willpower carries
// the default table and the MT_80 bound.
static void o_magictech_min_intelligence(const int64_t* a, int64_t* out)
{
    out[0] = magictech_min_intelligence((int)a[0]);
}

static void o_magictech_min_willpower(const int64_t* a, int64_t* out)
{
    out[0] = magictech_min_willpower((int)a[0]);
}

static void o_dialog_speech_id_unpack(const int64_t* a, int64_t* out)
{
    int script_num = (int)a[1];
    int line = (int)a[2];

    dialog_speech_id_unpack((int)a[0], &script_num, &line);
    out[0] = (int64_t)(uint32_t)script_num;
    out[1] = (int64_t)(uint32_t)line;
}

static void o_light_build_color(const int64_t* a, int64_t* out)
{
    unsigned int color = (unsigned int)a[3];

    light_build_color((uint8_t)a[0], (uint8_t)a[1], (uint8_t)a[2], &color);
    out[0] = color;
}

// The original writes a whole dword through each of the three pointers
// (`mov DWORD PTR [ecx],eax` at 0x4D84DC/0x4D84ED/0x4D84F3); CE narrowed the
// out-params to uint8_t*. Every CE caller passes uint8_t locals, so the two
// are internally consistent and the values agree -- but the widths do not,
// and this differential cannot see that, because it compares the value each
// side reports rather than the bytes around it. Recorded here so the next
// reader does not have to rediscover it from the disassembly.
static void o_light_get_color_components(const int64_t* a, int64_t* out)
{
    uint8_t red = (uint8_t)a[1];
    uint8_t green = (uint8_t)a[2];
    uint8_t blue = (uint8_t)a[3];

    light_get_color_components((unsigned int)a[0], &red, &green, &blue);
    out[0] = red;
    out[1] = green;
    out[2] = blue;
}

// --- tig leftovers, 2026-08-21 (later) ------------------------------------
//
// The 2026-08-16 art batch took eight of the twelve id constructors and the
// game-layer sweeps that followed only looked at src/game, so these four sat
// uncovered: container, light, facade and eye candy. They are the same shape
// as the eight already here - range checks then bit packing, answering
// through an out-param - and they carry four more field layouts and one live
// FIXME.
//
// Same seeding rule as the other constructors: the out-param starts from the
// drawn value, so an invalid-parameter path (which writes nothing on either
// side) still compares equal, and a side that wrongly writes is caught.

static void o_tig_art_container_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[5];

    (void)tig_art_container_id_create((unsigned int)a[0], (int)a[1],
        (unsigned int)a[2], (int)a[3], (unsigned int)a[4], &art_id);
    out[0] = art_id;
}

static void o_tig_art_light_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[4];

    (void)tig_art_light_id_create((unsigned int)a[0], (unsigned int)a[1],
        (unsigned int)a[2], (int)a[3], &art_id);
    out[0] = art_id;
}

static void o_tig_art_facade_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[6];

    (void)tig_art_facade_id_create((unsigned int)a[0], (unsigned int)a[1],
        (unsigned int)a[2], (unsigned int)a[3], (unsigned int)a[4],
        (unsigned int)a[5], &art_id);
    out[0] = art_id;
}

static void o_tig_art_eye_candy_id_create(const int64_t* a, int64_t* out)
{
    tig_art_id_t art_id = (tig_art_id_t)a[7];

    (void)tig_art_eye_candy_id_create((unsigned int)a[0], (unsigned int)a[1],
        (int)a[2], (int)a[3], (int)a[4], (unsigned int)a[5], (int)a[6],
        &art_id);
    out[0] = art_id;
}

// dword_5BE900[32], statically initialised in both binaries. No bounds check,
// so the domain is the table.
static void o_sub_503F50(const int64_t* a, int64_t* out)
{
    out[0] = sub_503F50((int)a[0]);
}

// Pure timestamp arithmetic: the unsigned difference, clamped to INT_MAX.
// tig_timer_now and tig_timer_elapsed read the clock and are deliberately
// absent - there is nothing for a differential to compare in a clock.
static void o_tig_timer_between(const int64_t* a, int64_t* out)
{
    out[0] = tig_timer_between((tig_timestamp_t)a[0], (tig_timestamp_t)a[1]);
}

static void o_sub_502830(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)sub_502830((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_num1_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_tile_id_num1_get((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_num2_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_tile_id_num2_get((tig_art_id_t)a[0]);
}

static void o_sub_503700(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)sub_503700((tig_art_id_t)a[0]);
}

static void o_sub_503740(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)sub_503740((tig_art_id_t)a[0], (int)a[1]);
}

static void o_sub_5037B0(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)sub_5037B0((tig_art_id_t)a[0]);
}

static void o_sub_503800(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)sub_503800((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_scenery_id_type_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_scenery_id_type_get((tig_art_id_t)a[0]);
}

static void o_sub_504390(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)sub_504390((tig_art_id_t)a[0]);
}

static void o_tig_art_item_id_disposition_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_disposition_get((tig_art_id_t)a[0]);
}

static void o_tig_art_item_id_disposition_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_disposition_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_item_id_subtype_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_subtype_get((tig_art_id_t)a[0]);
}

static void o_tig_art_item_id_subtype_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_subtype_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_item_id_type_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_type_get((tig_art_id_t)a[0]);
}

static void o_tig_art_item_id_armor_coverage_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_armor_coverage_get((tig_art_id_t)a[0]);
}

static void o_tig_art_item_id_destroyed_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_destroyed_get((tig_art_id_t)a[0]);
}

static void o_tig_art_item_id_destroyed_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_item_id_destroyed_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_container_id_type_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_container_id_type_get((tig_art_id_t)a[0]);
}

static void o_tig_art_light_id_rotation_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_light_id_rotation_get((tig_art_id_t)a[0]);
}

static void o_tig_art_light_id_rotation_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_light_id_rotation_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_light_id_rotation_extended_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_light_id_rotation_extended_get((tig_art_id_t)a[0]);
}

static void o_tig_art_roof_id_piece_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_roof_id_piece_get((tig_art_id_t)a[0]);
}

static void o_tig_art_roof_id_piece_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_roof_id_piece_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_roof_id_fill_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_roof_id_fill_get((tig_art_id_t)a[0]);
}

static void o_tig_art_roof_id_fill_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_roof_id_fill_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_roof_id_fade_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_roof_id_fade_get((tig_art_id_t)a[0]);
}

static void o_tig_art_roof_id_fade_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_roof_id_fade_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_facade_id_num_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_facade_id_num_get((tig_art_id_t)a[0]);
}

static void o_tig_art_facade_id_frame_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_facade_id_frame_get((tig_art_id_t)a[0]);
}

static void o_tig_art_facade_id_walkable_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_facade_id_walkable_get((tig_art_id_t)a[0]);
}

static void o_tig_art_eye_candy_id_type_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_eye_candy_id_type_get((tig_art_id_t)a[0]);
}

static void o_tig_art_eye_candy_id_type_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_eye_candy_id_type_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_eye_candy_id_translucency_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_eye_candy_id_translucency_get((tig_art_id_t)a[0]);
}

static void o_tig_art_eye_candy_id_translucency_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_eye_candy_id_translucency_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_eye_candy_id_scale_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_eye_candy_id_scale_get((tig_art_id_t)a[0]);
}

static void o_tig_art_eye_candy_id_scale_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_eye_candy_id_scale_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_id_flags_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_id_flags_get((tig_art_id_t)a[0]);
}

static const OracleEntry oracle_entries[] = {
    { "random_seed", 1, 1, o_random_seed },
    { "random_rand", 0, 1, o_random_rand },
    { "random_between", 2, 1, o_random_between },
    { "stat_cost", 1, 1, o_stat_cost },
    { "basic_skill_money", 3, 1, o_basic_skill_money },
    { "basic_skill_min_stat_level_required", 1, 1, o_basic_skill_min_stat_level_required },
    { "basic_skill_stat", 1, 1, o_basic_skill_stat },
    { "tech_skill_stat", 1, 1, o_tech_skill_stat },
    { "combat_hit_loc_penalty", 1, 1, o_combat_hit_loc_penalty },
    { "critter_encumbrance_level_ratio", 1, 1, o_critter_encumbrance_level_ratio },
    { "item_armor_size", 1, 1, o_item_armor_size },
    { "max_skill_for_stat_level", 1, 1, o_max_skill_for_stat_level },
    { "training_min_skill_level_required", 1, 1, o_training_min_skill_level_required },
    { "tech_skill_money", 3, 1, o_tech_skill_money },
    { "tech_skill_min_stat_level_required", 1, 1, o_tech_skill_min_stat_level_required },
    { "location_rot", 2, 1, o_location_rot },
    { "location_dist", 2, 1, o_location_dist },
    { "datetime_sub_milliseconds", 2, 1, o_datetime_sub_milliseconds },
    { "datetime_add_milliseconds", 2, 1, o_datetime_add_milliseconds },
    { "datetime_seconds_since_reference_date", 1, 1, o_datetime_seconds_since_reference_date },
    { "datetime_get_hour", 1, 1, o_datetime_get_hour },
    { "datetime_get_hour_since_reference_date", 1, 1, o_datetime_get_hour_since_reference_date },
    { "datetime_get_minute", 1, 1, o_datetime_get_minute },
    { "datetime_get_day", 1, 1, o_datetime_get_day },
    { "datetime_get_month", 1, 1, o_datetime_get_month },
    { "datetime_get_year", 1, 1, o_datetime_get_year },
    { "datetime_from_milliseconds", 2, 1, o_datetime_from_milliseconds },
    { "datetime_compare", 2, 1, o_datetime_compare },
    { "datetime_is_at_least_milliseconds", 2, 1, o_datetime_is_at_least_milliseconds },
    { "datetime_add_datetime", 2, 2, o_datetime_add_datetime },
    { "location_limits_set", 2, 1, o_location_limits_set },
    { "location_in_dir", 3, 1, o_location_in_dir },
    { "location_in_range", 4, 1, o_location_in_range },
    { "location_xy", 3, 2, o_location_xy },
    { "location_at", 3, 1, o_location_at },
    { "location_normalize", 3, 3, o_location_normalize },
    { "location_limits_get", 2, 2, o_location_limits_get },
    { "location_center_get", 0, 1, o_location_center_get },
    { "sub_4B93F0", 4, 2, o_sub_4B93F0 },
    { "skill_gambling_random_roll", 0, 1, o_skill_gambling_random_roll },
    { "sub_502830", 1, 1, o_sub_502830 },
    { "tig_art_tile_id_num1_get", 1, 1, o_tig_art_tile_id_num1_get },
    { "tig_art_tile_id_num2_get", 1, 1, o_tig_art_tile_id_num2_get },
    { "sub_503700", 1, 1, o_sub_503700 },
    { "sub_503740", 2, 1, o_sub_503740 },
    { "sub_5037B0", 1, 1, o_sub_5037B0 },
    { "sub_503800", 2, 1, o_sub_503800 },
    { "tig_art_scenery_id_type_get", 1, 1, o_tig_art_scenery_id_type_get },
    { "sub_504390", 1, 1, o_sub_504390 },
    { "tig_art_item_id_disposition_get", 1, 1, o_tig_art_item_id_disposition_get },
    { "tig_art_item_id_disposition_set", 2, 1, o_tig_art_item_id_disposition_set },
    { "tig_art_item_id_subtype_get", 1, 1, o_tig_art_item_id_subtype_get },
    { "tig_art_item_id_subtype_set", 2, 1, o_tig_art_item_id_subtype_set },
    { "tig_art_item_id_type_get", 1, 1, o_tig_art_item_id_type_get },
    { "tig_art_item_id_armor_coverage_get", 1, 1, o_tig_art_item_id_armor_coverage_get },
    { "tig_art_item_id_destroyed_get", 1, 1, o_tig_art_item_id_destroyed_get },
    { "tig_art_item_id_destroyed_set", 2, 1, o_tig_art_item_id_destroyed_set },
    { "tig_art_container_id_type_get", 1, 1, o_tig_art_container_id_type_get },
    { "tig_art_light_id_rotation_get", 1, 1, o_tig_art_light_id_rotation_get },
    { "tig_art_light_id_rotation_set", 2, 1, o_tig_art_light_id_rotation_set },
    { "tig_art_light_id_rotation_extended_get", 1, 1, o_tig_art_light_id_rotation_extended_get },
    { "tig_art_roof_id_piece_get", 1, 1, o_tig_art_roof_id_piece_get },
    { "tig_art_roof_id_piece_set", 2, 1, o_tig_art_roof_id_piece_set },
    { "tig_art_roof_id_fill_get", 1, 1, o_tig_art_roof_id_fill_get },
    { "tig_art_roof_id_fill_set", 2, 1, o_tig_art_roof_id_fill_set },
    { "tig_art_roof_id_fade_get", 1, 1, o_tig_art_roof_id_fade_get },
    { "tig_art_roof_id_fade_set", 2, 1, o_tig_art_roof_id_fade_set },
    { "tig_art_facade_id_num_get", 1, 1, o_tig_art_facade_id_num_get },
    { "tig_art_facade_id_frame_get", 1, 1, o_tig_art_facade_id_frame_get },
    { "tig_art_facade_id_walkable_get", 1, 1, o_tig_art_facade_id_walkable_get },
    { "tig_art_eye_candy_id_type_get", 1, 1, o_tig_art_eye_candy_id_type_get },
    { "tig_art_eye_candy_id_type_set", 2, 1, o_tig_art_eye_candy_id_type_set },
    { "tig_art_eye_candy_id_translucency_get", 1, 1, o_tig_art_eye_candy_id_translucency_get },
    { "tig_art_eye_candy_id_translucency_set", 2, 1, o_tig_art_eye_candy_id_translucency_set },
    { "tig_art_eye_candy_id_scale_get", 1, 1, o_tig_art_eye_candy_id_scale_get },
    { "tig_art_eye_candy_id_scale_set", 2, 1, o_tig_art_eye_candy_id_scale_set },
    { "tig_art_id_flags_get", 1, 1, o_tig_art_id_flags_get },
    { "tig_art_type", 1, 1, o_tig_art_type },
    { "tig_art_num_get", 1, 1, o_tig_art_num_get },
    { "tig_art_num_set", 2, 1, o_tig_art_num_set },
    { "tig_art_id_rotation_get", 1, 1, o_tig_art_id_rotation_get },
    { "tig_art_id_rotation_inc", 1, 1, o_tig_art_id_rotation_inc },
    { "tig_art_id_rotation_set", 2, 1, o_tig_art_id_rotation_set },
    { "tig_art_id_frame_get", 1, 1, o_tig_art_id_frame_get },
    { "tig_art_id_palette_get", 1, 1, o_tig_art_id_palette_get },
    { "tig_art_id_palette_set", 2, 1, o_tig_art_id_palette_set },
    { "tig_art_id_flags_set", 2, 1, o_tig_art_id_flags_set },
    { "tig_art_id_anim_get", 1, 1, o_tig_art_id_anim_get },
    { "tig_art_id_anim_set", 2, 1, o_tig_art_id_anim_set },
    { "tig_art_critter_id_body_type_get", 1, 1, o_tig_art_critter_id_body_type_get },
    { "tig_art_critter_id_body_type_set", 2, 1, o_tig_art_critter_id_body_type_set },
    { "tig_art_monster_id_specie_get", 1, 1, o_tig_art_monster_id_specie_get },
    { "tig_art_critter_id_size_get", 1, 1, o_tig_art_critter_id_size_get },
    { "tig_art_critter_id_gender_get", 1, 1, o_tig_art_critter_id_gender_get },
    { "tig_art_critter_id_gender_set", 2, 1, o_tig_art_critter_id_gender_set },
    { "tig_art_critter_id_armor_get", 1, 1, o_tig_art_critter_id_armor_get },
    { "tig_art_critter_id_armor_set", 2, 1, o_tig_art_critter_id_armor_set },
    { "tig_art_critter_id_weapon_get", 1, 1, o_tig_art_critter_id_weapon_get },
    { "tig_art_critter_id_weapon_set", 2, 1, o_tig_art_critter_id_weapon_set },
    { "tig_art_critter_id_shield_get", 1, 1, o_tig_art_critter_id_shield_get },
    { "tig_art_critter_id_shield_set", 2, 1, o_tig_art_critter_id_shield_set },
    { "tig_art_tile_id_type_get", 1, 1, o_tig_art_tile_id_type_get },
    { "tig_art_tile_id_flippable_get", 1, 1, o_tig_art_tile_id_flippable_get },
    { "tig_art_tile_id_flippable1_get", 1, 1, o_tig_art_tile_id_flippable1_get },
    { "tig_art_tile_id_flippable2_get", 1, 1, o_tig_art_tile_id_flippable2_get },
    { "tig_art_wall_id_p_piece_get", 1, 1, o_tig_art_wall_id_p_piece_get },
    { "tig_art_wall_id_p_piece_set", 2, 1, o_tig_art_wall_id_p_piece_set },
    { "tig_art_wall_id_num_get", 1, 1, o_tig_art_wall_id_num_get },
    { "tig_art_wall_id_variation_get", 1, 1, o_tig_art_wall_id_variation_get },
    { "tig_art_wall_id_variation_set", 2, 1, o_tig_art_wall_id_variation_set },
    { "tig_art_id_damaged_get", 1, 1, o_tig_art_id_damaged_get },
    { "tig_art_id_damaged_set", 2, 1, o_tig_art_id_damaged_set },
    { "tig_art_portal_id_type_get", 1, 1, o_tig_art_portal_id_type_get },
    { "tig_art_wall_id_create", 7, 1, o_tig_art_wall_id_create },
    { "tig_art_tile_id_create", 9, 1, o_tig_art_tile_id_create },
    { "tig_art_critter_id_create", 10, 1, o_tig_art_critter_id_create },
    { "tig_art_monster_id_create", 9, 1, o_tig_art_monster_id_create },
    { "tig_art_unique_npc_id_create", 8, 1, o_tig_art_unique_npc_id_create },
    { "tig_art_portal_id_create", 7, 1, o_tig_art_portal_id_create },
    { "tig_art_scenery_id_create", 6, 1, o_tig_art_scenery_id_create },
    { "tig_art_misc_id_create", 3, 1, o_tig_art_misc_id_create },
    { "terrain_tid_base", 1, 1, o_terrain_tid_base },
    { "terrain_tid_from", 1, 1, o_terrain_tid_from },
    { "terrain_tid_transition", 1, 1, o_terrain_tid_transition },
    { "terrain_tid_tile", 1, 1, o_terrain_tid_tile },
    { "reaction_translate", 1, 1, o_reaction_translate },
    { "spell_icon", 1, 1, o_spell_icon },
    { "spell_college_small_icon", 1, 1, o_spell_college_small_icon },
    { "spell_college_large_icon", 1, 1, o_spell_college_large_icon },
    { "spell_min_level", 1, 1, o_spell_min_level },
    { "tech_degree_cost_get", 1, 1, o_tech_degree_cost_get },
    { "tech_degree_min_intelligence_get", 1, 1, o_tech_degree_min_intelligence_get },
    { "tech_schematic_get", 2, 1, o_tech_schematic_get },
    { "mt_item_spell_to_magictech_spell", 1, 1, o_mt_item_spell_to_magictech_spell },
    { "magictech_min_intelligence", 1, 1, o_magictech_min_intelligence },
    { "magictech_min_willpower", 1, 1, o_magictech_min_willpower },
    { "dialog_speech_id_unpack", 3, 2, o_dialog_speech_id_unpack },
    { "light_build_color", 4, 1, o_light_build_color },
    { "light_get_color_components", 4, 3, o_light_get_color_components },
    { "tig_art_container_id_create", 6, 1, o_tig_art_container_id_create },
    { "tig_art_light_id_create", 5, 1, o_tig_art_light_id_create },
    { "tig_art_facade_id_create", 7, 1, o_tig_art_facade_id_create },
    { "tig_art_eye_candy_id_create", 8, 1, o_tig_art_eye_candy_id_create },
    { "sub_503F50", 1, 1, o_sub_503F50 },
    { "tig_timer_between", 2, 1, o_tig_timer_between },
};

#define ORACLE_ENTRY_COUNT ((int)(sizeof(oracle_entries) / sizeof(oracle_entries[0])))

static const OracleEntry* oracle_find(const char* name)
{
    int index;

    for (index = 0; index < ORACLE_ENTRY_COUNT; index++) {
        if (strcmp(oracle_entries[index].name, name) == 0) {
            return &oracle_entries[index];
        }
    }

    return NULL;
}

int main(void)
{
    char line[MAX_LINE];

    // The driver alternates writing a request with reading its reply, so
    // anything still sitting in a buffer here is a deadlock.
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        const OracleEntry* entry;
        // Zeroed deliberately. A wrapper that reads past its own arity used to
        // pick up whatever the previous request left on this stack slot, which
        // made tig_art_unique_npc_id_create's seed depend on call order and
        // hid the mistake for months (found 2026-08-21). Zeroing does not make
        // such a wrapper correct - `scripts/tests/test_playtest_tools.py`
        // checks that every wrapper reads exactly a[0..arity-1] - but it makes
        // the symptom reproducible instead of order-dependent.
        int64_t args[MAX_ARGS] = { 0 };
        int64_t out[MAX_OUT] = { 0 };
        char* token;
        char* save;
        int argc = 0;
        int index;

        token = strtok_r(line, " \t\r\n", &save);
        if (token == NULL) {
            continue;
        }

        if (strcmp(token, "list") == 0) {
            for (index = 0; index < ORACLE_ENTRY_COUNT; index++) {
                printf("%s %d\n", oracle_entries[index].name, oracle_entries[index].arity);
            }
            printf(".\n");
            continue;
        }

        entry = oracle_find(token);
        if (entry == NULL) {
            printf("!unknown function %s\n", token);
            continue;
        }

        while ((token = strtok_r(NULL, " \t\r\n", &save)) != NULL) {
            if (argc >= MAX_ARGS) {
                argc = MAX_ARGS + 1;
                break;
            }

            if (sscanf(token, "%" SCNd64, &args[argc]) != 1) {
                argc = -1;
                break;
            }

            argc++;
        }

        if (argc != entry->arity) {
            printf("!%s takes %d args\n", entry->name, entry->arity);
            continue;
        }

        entry->func(args, out);
        for (index = 0; index < entry->outputs; index++) {
            printf("%" PRId64 "%s", out[index], index + 1 < entry->outputs ? " " : "\n");
        }
    }

    return 0;
}
