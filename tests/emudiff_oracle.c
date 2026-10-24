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

#include "game/ai.h"
#include "game/anim_private.h"
#include "game/background.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/fate.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/level.h"
#include "game/light.h"
#include "game/location.h"
#include "game/magictech.h"
#include "game/mt_ai.h"
#include "game/mt_item.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/portal.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/sfx.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/tech.h"
#include "game/terrain.h"
#include "game/timeevent.h"
#include "game/trap.h"
#include "tig/tig.h"

#include "obj_harness.h"

#define MAX_ARGS 16
// 20 since 2026-09-04: `target_context_init` answers by filling a struct, so
// its output is one slot per field rather than a return value. Raising this
// costs 8 bytes of stack per request and nothing else -- `entry->outputs`
// still decides how many are printed.
#define MAX_OUT 20
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

// --- object-handle cases -----------------------------------------------
//
// Most of `src/game` takes an `int64_t obj` -- 46% of `combat.c`, 85% of
// `critter.c`, 89% of `item.c` -- and until now none of it was reachable
// here, because a differential shaped as "pass these words, read EAX" has
// nothing to pass. The scalar seam in those modules is spent; this is how
// the rest opens.
//
// **The two sides are deliberately asymmetric, and that is the design.**
// On the emulator side `obj_field_int32_get` (0x406CA0) is stubbed to answer
// from the drawn values. On this side the object is *real*: `obj_harness`
// builds a prototype through the engine's own `obj_create_proto`, and the
// same `obj_field_int32_get` the game calls reads it.
//
// That is legal under the stub rule in `harness/emudiff/README.md` -- "a stub
// must return something derived from the original's own bytes, never from
// what the C says it should be" -- because this stub *asserts nothing*. It
// does not model a layout; it supplies input, the way `Arg.draw` does. A
// fixture that synthesised an `Object` in guest memory would be making a
// claim about layout and would need sourcing; this one has no layout in it.
//
// **The one claim it does make** is that CE's `OBJ_F_*` ordinals are the same
// numbers the original's code passes. That is checkable independently, and
// was checked before the first case: the original pushes `0x14F` and `0xDA`
// as literals at 0x45F740 and 0x45F768, and CE's enum puts `OBJ_F_TYPE` at
// 335 and `OBJ_F_CRITTER_FLAGS` at 218. Any new field a case reads has to be
// checked the same way, against the literal in the disassembly.
//
// **Why the type domain stops at 18.** The emulator's stub would happily
// return 9999 for OBJ_F_TYPE, but this side cannot build an object of a type
// the engine has no defaults for. `obj_create_proto` handles 0..17;
// OBJ_TYPE_MONSTER (18) and OBJ_TYPE_UNIQUE_NPC (19) fall through
// `obj_type_change_array_len`'s switch to its `default: assert(0)`. The
// original agrees by its own route -- `object_field_valid` at 0x40C2BA
// rejects any type above 0x11 before reaching its jump table -- so the two
// really do have the same eighteen. This is the same rule as "never generate
// past the end of an array": an input only one side can represent compares
// two different things.

// One prototype per object type, built on first use. Creating one per trial
// would allocate thousands from the object pool over a run; a prototype's
// fields are rewritten before each call anyway, so reuse changes nothing.
static int64_t o_obj_by_type[OBJ_TYPE_COUNT];
static bool o_obj_by_type_ready;

static int64_t o_obj_of_type(int type)
{
    int index;

    // Not OBJ_TYPE_COUNT: 18 and 19 abort in `obj_type_change_array_len`, and
    // an abort here takes the whole emudiff run with it rather than failing
    // the one case. Refusing them turns a crash into a `-1` the driver can
    // report, which is how a domain mistake should surface.
    if (type < 0 || type >= OBJ_TYPE_MONSTER) {
        return OBJ_HANDLE_NULL;
    }

    if (!o_obj_by_type_ready) {
        for (index = 0; index < OBJ_TYPE_COUNT; index++) {
            o_obj_by_type[index] = OBJ_HANDLE_NULL;
        }
        o_obj_by_type_ready = true;
    }

    if (o_obj_by_type[type] == OBJ_HANDLE_NULL) {
        o_obj_by_type[type] = obj_harness_create_proto(type);
    }

    return o_obj_by_type[type];
}

// A second prototype per type, for the functions that take two objects.
//
// Added 2026-08-31 with `critter_origin_same`, which compares one critter's
// origin against another's. One object per type is not enough there: the two
// arguments would be the same object, the two origins equal by construction,
// and the only branch reachable would be the one that answers true. The
// emulator side had the mirror-image problem -- a single field map -- and
// learned to key on the handle at the same time.
static int64_t o_obj_by_type_second[OBJ_TYPE_COUNT];
static bool o_obj_by_type_second_ready;

static int64_t o_obj_of_type_second(int type)
{
    int index;

    if (type < 0 || type >= OBJ_TYPE_MONSTER) {
        return OBJ_HANDLE_NULL;
    }

    if (!o_obj_by_type_second_ready) {
        for (index = 0; index < OBJ_TYPE_COUNT; index++) {
            o_obj_by_type_second[index] = OBJ_HANDLE_NULL;
        }
        o_obj_by_type_second_ready = true;
    }

    if (o_obj_by_type_second[type] == OBJ_HANDLE_NULL) {
        o_obj_by_type_second[type] = obj_harness_create_proto(type);
    }

    return o_obj_by_type_second[type];
}

// `critter_is_monstrous` (0x45F730). Three inputs: the object's type, its
// OBJ_F_CRITTER_FLAGS, and whether to pass OBJ_HANDLE_NULL instead of an
// object at all -- the function's first branch, and one no object can
// express.
//
// The flags are only written when the type is a critter, because
// OBJ_F_CRITTER_FLAGS does not exist on a chair: `obj_field_int32_set` would
// reject it through `object_field_not_exists`. The original agrees by a
// different route -- it never reads the field unless the type check passed --
// so the two still see the same thing on every input.
static void o_critter_is_monstrous(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    unsigned int critter_flags = (unsigned int)a[1];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = critter_is_monstrous(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        // Not a silent zero: a type the harness cannot build must not look
        // like a legitimate `false`, or the case would agree with the
        // emulator for the wrong reason on every such input.
        out[0] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)critter_flags);
    }

    out[0] = critter_is_monstrous(obj);
}

// `item_worth` (0x461F20) and `item_is_worthless` (0x461F60), which sit either
// side of the rule the type domain has to obey.
//
// `item_worth` is safe over every buildable type: OBJ_F_ITEM_WORTH is only
// read after `item_is_identified` has checked the type is 5..14, so a chair
// returns 300 on both sides without either ever touching an item field.
//
// `item_is_worthless` is not. It reads OBJ_F_ITEM_WORTH with no guard of any
// kind -- 0x461F60 is a field read and a `neg/sbb/inc` -- so on a chair the
// emulator's stub hands back the drawn worth while CE's real object refuses a
// field a chair does not have and answers 0. That is a difference about the
// fixture, not about the port, and its domain is restricted to item types for
// that reason. It was run unrestricted first, and it differs; see
// `harness/emudiff/README.md`.
static void o_item_worth(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[4] != 0) {
        out[0] = item_worth(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_FLAGS, (int)a[1]);
        obj_field_int32_set(obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY, (int)a[2]);
        obj_field_int32_set(obj, OBJ_F_ITEM_WORTH, (int)a[3]);
    }

    out[0] = item_worth(obj);
}

// No NULL branch, and the omission is the finding. `item_is_worthless` has no
// NULL guard either, so on OBJ_HANDLE_NULL the emulator's stub answers with the
// drawn worth while CE's field read on a null handle answers something else
// entirely -- measured 2026-08-31 as 9 mismatches in 1200, every one of them
// `worth == 0`. The NULL path of this function is outside what a field-read
// fixture can express, and saying so is better than a case that quietly
// excludes it.
static void o_item_is_worthless(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_WORTH, (int)a[1]);
    }

    out[0] = item_is_worthless(obj);
}

// The combat.c three, reached through the object fixture. `combat.c` had one
// case for 89 definitions because 41 of them take an object handle; these are
// every function in the file whose call closure reaches nothing but the field
// accessors, per `scripts/obj_case_candidates.py`.
//
// Two are `static` and are linked through `combat_globalized.o` -- see
// tests/CMakeLists.txt. Declared here rather than included, because they are
// deliberately absent from `combat.h`: globalizing changes the binding, not
// the header, and adding them to the header would be the engine source change
// the whole trick exists to avoid.
int combat_weapon_loudness(int64_t weapon_obj);
bool combat_turn_based_critter_is_dontdraw(int64_t obj);

// `combat_critter_is_combat_mode_active` (0x4B3D50). No NULL branch: the
// function reads OBJ_F_TYPE with no guard in front of it, so OBJ_HANDLE_NULL
// is outside what the fixture can represent and outside the case's domain.
// The original agrees -- 0x4B3D5A pushes the field straight away.
static void o_combat_critter_is_combat_mode_active(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)a[1]);
    }

    out[0] = combat_critter_is_combat_mode_active(obj);
}

// `combat_weapon_loudness` (0x4B2810). This one *does* guard NULL -- 0x4B281D
// ORs the handle halves -- so the NULL branch is in the domain, and it is the
// LOUDNESS_NORMAL default rather than a separate answer.
//
// Worth knowing about the original: it tests the flags with `test %al,%bl`
// and `test $0x2,%al`, i.e. on the low byte only. OWF_LOUD (0x1) and
// OWF_SILENT (0x2) both live there, so CE's full-width `&` agrees; a flag
// above bit 7 would not, and there is none.
static void o_combat_weapon_loudness(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = combat_weapon_loudness(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_WEAPON) {
        obj_field_int32_set(obj, OBJ_F_WEAPON_FLAGS, (int)a[1]);
    }

    out[0] = combat_weapon_loudness(obj);
}

// `combat_turn_based_critter_is_dontdraw` (0x4B7DC0). Reads OBJ_F_FLAGS with
// no guard of any kind, then defers to `critter_is_sleeping`, which has both.
//
// The type domain is unrestricted anyway, and that is a fact about the field
// rather than luck: OBJ_F_FLAGS is ordinal 19, inside the common block that
// precedes every per-type block in `ObjectField`, so every object owns it.
// OBJ_F_CRITTER_FLAGS is not, hence the guard on writing it.
//
// NULL is excluded: the unguarded OBJ_F_FLAGS read is exactly the shape the
// fixture cannot represent.
static void o_combat_turn_based_critter_is_dontdraw(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);
    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)a[2]);
    }

    out[0] = combat_turn_based_critter_is_dontdraw(obj);
}

// --- critter.c through the object fixture (2026-08-31, card V1) ----------
//
// `critter.c` is the module with the highest object-handle ratio in the tree
// -- 69 of its 81 definitions take one -- and it had two cases. These are the
// candidates `scripts/obj_case_candidates.py --module critter` prints whose
// closure reaches nothing but the field accessors and functions this suite
// has already differentiated (`tig_art_type`, `tig_art_id_anim_get`,
// `tig_art_monster_id_specie_get`, `light_build_color`,
// `tig_art_light_id_create`).
//
// Every ordinal below was read off the literal the original pushes, not
// inferred from CE's name for the field:
//
//     OBJ_F_CURRENT_AID              1 = 0x1     pushed at 0x45D763, 0x45F579
//     OBJ_F_FLAGS                   19 = 0x13    pushed at 0x45D73A
//     OBJ_F_CRITTER_FLAGS          218 = 0xDA    pushed at 0x45D8A8, 0x45FA78
//     OBJ_F_CRITTER_FLAGS2         219 = 0xDB    pushed at 0x45FBD0
//     OBJ_F_CRITTER_FATIGUE_PTS    224 = 0xE0    pushed at 0x45D3C7
//     OBJ_F_CRITTER_FATIGUE_ADJ    225 = 0xE1    pushed at 0x45D4D7
//     OBJ_F_CRITTER_FATIGUE_DAMAGE 226 = 0xE2    pushed at 0x45D587
//     OBJ_F_PC_BACKGROUND          257 = 0x101   pushed at 0x4C26B4
//     OBJ_F_NPC_ORIGIN             291 = 0x123   pushed at 0x45D2AE
//     OBJ_F_NPC_FACTION            292 = 0x124   pushed at 0x45D09E
//     OBJ_F_NPC_SOCIAL_CLASS       296 = 0x128   pushed at 0x45D018
//
// The type guard the original inlines everywhere here is `cmp eax,0xf` /
// `cmp eax,0x10`, i.e. OBJ_TYPE_PC and OBJ_TYPE_NPC, which is exactly CE's
// `obj_type_is_critter`.

// `critter_social_class_get` (0x45D010). A bare field read with no guard of
// any kind, so its domain is the one type that owns OBJ_F_NPC_SOCIAL_CLASS.
// That leaves nothing to branch on, and the case is still worth its cost:
// what it holds still is that `critter.c` reads *this* field and not a
// neighbour, because the wrapper writes 0x128 by name and the emulator
// answers 0x128 by number.
static void o_critter_social_class_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_NPC) {
        obj_field_int32_set(obj, OBJ_F_NPC_SOCIAL_CLASS, (int)a[1]);
    }

    out[0] = critter_social_class_get(obj);
}

// `critter_faction_get` (0x45D080) and `critter_origin_get` (0x45D290), the
// same shape: an `== OBJ_TYPE_NPC` guard in front of an NPC-block field, and
// 0 for everything else. The type domain is unrestricted because the guard
// is what keeps the field read away from an object that has no such field --
// on both sides, by different routes. NULL is out of both domains: the
// OBJ_F_TYPE read at 0x45D08A and 0x45D29A has nothing in front of it.
static void o_critter_faction_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_NPC) {
        obj_field_int32_set(obj, OBJ_F_NPC_FACTION, (int)a[1]);
    }

    out[0] = critter_faction_get(obj);
}

static void o_critter_origin_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_NPC) {
        obj_field_int32_set(obj, OBJ_F_NPC_ORIGIN, (int)a[1]);
    }

    out[0] = critter_origin_get(obj);
}

// `critter_is_pc` (0x45D360). One field, one comparison, every buildable type
// in the domain -- the cheapest possible check that CE and the original agree
// on which number OBJ_TYPE_PC is. NULL is out: the read is unguarded.
// `critter_origin_same` (0x45D320). Two objects, and the only case here that
// needs them: `origin != 0 && origin == critter_origin_get(b)`, where both
// origins come out of the NPC block of two different critters.
//
// NULL is out of both domains for the reason `critter_origin_get` gives -- its
// OBJ_F_TYPE read at 0x45D29A has nothing in front of it.
static void o_critter_origin_same(const int64_t* a, int64_t* out)
{
    int type_a = (int)a[0];
    int type_b = (int)a[2];
    int64_t obj_a = o_obj_of_type(type_a);
    int64_t obj_b = o_obj_of_type_second(type_b);

    if (obj_a == OBJ_HANDLE_NULL || obj_b == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type_a == OBJ_TYPE_NPC) {
        obj_field_int32_set(obj_a, OBJ_F_NPC_ORIGIN, (int)a[1]);
    }

    if (type_b == OBJ_TYPE_NPC) {
        obj_field_int32_set(obj_b, OBJ_F_NPC_ORIGIN, (int)a[3]);
    }

    out[0] = critter_origin_same(obj_a, obj_b);
}

static void o_critter_is_pc(const int64_t* a, int64_t* out)
{
    int64_t obj = o_obj_of_type((int)a[0]);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    out[0] = critter_is_pc(obj);
}

// The three fatigue getters (0x45D390, 0x45D4A0, 0x45D550). Identical shape:
// `obj_type_is_critter` in front, then one field of the critter block. What
// separates them is only which ordinal they read, and that is what a case per
// function checks -- CE's `critter_fatigue_adj_get` reading 0xE0 instead of
// 0xE1 would be invisible to a single combined case and is exactly the slip
// the original's own *setter* made (see the FIX note at 0x45D4F0).
static void o_critter_fatigue_field_get(const int64_t* a,
    int64_t* out,
    int field,
    int (*fn)(int64_t))
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, field, (int)a[1]);
    }

    out[0] = fn(obj);
}

static void o_critter_fatigue_pts_get(const int64_t* a, int64_t* out)
{
    o_critter_fatigue_field_get(a, out, OBJ_F_CRITTER_FATIGUE_PTS,
        critter_fatigue_pts_get);
}

static void o_critter_fatigue_adj_get(const int64_t* a, int64_t* out)
{
    o_critter_fatigue_field_get(a, out, OBJ_F_CRITTER_FATIGUE_ADJ,
        critter_fatigue_adj_get);
}

static void o_critter_fatigue_damage_get(const int64_t* a, int64_t* out)
{
    o_critter_fatigue_field_get(a, out, OBJ_F_CRITTER_FATIGUE_DAMAGE,
        critter_fatigue_damage_get);
}

// `critter_is_sleeping` (0x45D870) and `critter_is_concealed` (0x45EDE0).
// Both guard NULL first (`or esi,edi` at 0x45D87C and 0x45EDEA), so NULL is
// in the domain, and both end in a single-bit test of OBJ_F_CRITTER_FLAGS:
// `shr eax,0xc; and eax,1` -- bit 12, OCF_SLEEPING -- and `movsx eax,al;
// and eax,1` -- bit 0, OCF_IS_CONCEALED.
static void o_critter_is_sleeping(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = critter_is_sleeping(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)a[1]);
    }

    out[0] = critter_is_sleeping(obj);
}

static void o_critter_is_concealed(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = critter_is_concealed(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)a[1]);
    }

    out[0] = critter_is_concealed(obj);
}

// `critter_has_dark_sight` (0x45FB90), the widest of these: a NULL guard, a
// critter guard, an inlined-in-CE call to `background_get` (0x4C2690, which
// carries its own NULL and `== OBJ_TYPE_PC` guards before reading
// OBJ_F_PC_BACKGROUND), and finally bit 11 of OBJ_F_CRITTER_FLAGS2.
//
// Two fields belonging to two different blocks are in play, and the type
// domain is still unrestricted, because each read sits behind the guard that
// makes it legal: OBJ_F_PC_BACKGROUND is only reached when the type is PC and
// OBJ_F_CRITTER_FLAGS2 only when it is a critter. The wrapper writes each one
// under the same condition, which is why a chair never reaches either.
//
// The background constant is 78: `cmp eax,0x4e` at 0x45FBC3.
static void o_critter_has_dark_sight(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[3] != 0) {
        out[0] = critter_has_dark_sight(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_PC) {
        obj_field_int32_set(obj, OBJ_F_PC_BACKGROUND, (int)a[1]);
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, (int)a[2]);
    }

    out[0] = critter_has_dark_sight(obj);
}

// `critter_is_prone` (0x45D730). Three fields and a call into tig art:
// OBJ_F_FLAGS tested against OF_DESTROYED|OF_OFF (`test al,0x3` at 0x45D746),
// the critter guard, then OBJ_F_CURRENT_AID through `tig_art_id_anim_get`
// (0x503E20) compared against 7, TIG_ART_ANIM_FALL_DOWN.
//
// The art call is inside the differential's reach because
// `tig_art_id_anim_get` is itself a case here and is pure bit extraction on
// both sides -- the emulator runs the original's copy of it rather than a
// stub, so this stays a symmetric boundary.
//
// NULL is out of the domain: 0x45D73A pushes OBJ_F_FLAGS with nothing in
// front of it.
// `portal_is_open` (0x4F08C0). V4 batch 2. One field read and one tig call:
// `tig_art_id_frame_get(OBJ_F_CURRENT_AID) != 0`.
//
// **No NULL guard and no type guard**, so NULL is out of the domain, but the
// type domain stays open for the same reason `critter_can_jump_window`'s
// does - OBJ_F_CURRENT_AID is ordinal 1, inside the common block every object
// owns. Leaving all types in is the better choice besides: PORTAL falls to
// `tig_art_id_frame_get`'s *default* arm, and the wrong-type draws walk the
// rest of that switch, where TILE/WALL/ITEM answer 0 unconditionally.
// Both are `static` in CE and come in through `reaction_globalized.o` and
// `skill_ui_globalized.o`; declared here rather than in their headers, for the
// reason given above `combat_weapon_loudness` - the shipped build must not
// gain a symbol because a test wanted one.
int reaction_get_base(int64_t obj);
bool skill_ui_is_pickpocket_immune(int64_t obj);

// `reaction_get_base` (0x4C12D0). V4 batch 2. A bare read of
// OBJ_F_NPC_REACTION_BASE - ordinal 0x127, read off the literal at 0x4C12D8 in
// batch 1 - with no guard of any kind. So NULL is out of the domain, and so is
// every type but NPC: CE's object refuses a field the type does not own and
// answers 0, while the emulator's stub answers the drawn value.
//
// `static` in CE; reaction_globalized.o is what makes it reachable.
// --- the targeting batch (card V4, batch 2) ---------------------------------
//
// These answer by WRITING A LIST, not by returning, so the case compares the
// list afterwards. Layout read out of the original at 0x4F3F7A-0x4F3FC1, not
// out of target.h: `mov (%eax),%ecx` puts the count at +0x00, `lea 0x8(%eax)`
// puts the entries at +0x08, `add $0x38,%edx` makes the stride 0x38, and the
// append writes the handle at entry+0x00 and zeroes entry+0x08.
//
// **The appended handle itself is not comparable** and this is the reason a
// case here needs thought: CE appends its own object's handle and the emulator
// appends the fixture's, so the raw values differ by construction - the
// `incomparable` shape that refused `item_ui_target_flags_get`. It is
// normalised instead: 1 if the slot holds the object that was offered, -1 if
// nothing was written at all, 0 if something else landed there. That keeps
// "appended", "refused" and "wrote the wrong thing" three distinct answers.
// Both are `static` in CE and arrive through `target_globalized.o`; declared
// here rather than in target.h, so the shipped build does not gain a symbol
// because a test wanted one.
void target_list_add_obj_unless_off(TargetList* list, int64_t obj);
void target_list_add_obj_unless_destroyed(TargetList* list, int64_t obj);

#define O_TL_POISON ((int64_t)0xDEADBEEFDEADBEEFLL)

static int64_t o_target_list_seed(TargetList* list, int cnt, int dup, int64_t obj)
{
    memset(list, 0, sizeof(*list));
    list->cnt = cnt;
    for (int i = 0; i < cnt && i < 4; i++) {
        // Distinct, non-null, and not the object under test - so the dedup
        // scan has entries to walk past and reject.
        list->entries[i].obj = (int64_t)(0x5000 + i);
        list->entries[i].loc = (int64_t)(0x9000 + i);
    }
    if (dup && cnt > 0) {
        list->entries[0].obj = obj; // the scan must find this and refuse
    }
    // Poison the slot the append would use, so "wrote the sentinel" and "did
    // not write at all" are different observations.
    if (cnt >= 0 && cnt < 256) {
        list->entries[cnt].obj = O_TL_POISON;
        list->entries[cnt].loc = O_TL_POISON;
    }
    return obj;
}

static void o_target_list_report(const TargetList* list, int cnt_before, int64_t obj, int64_t* out)
{
    out[0] = list->cnt;
    if (cnt_before < 0 || cnt_before >= 256) {
        out[1] = -2;
        out[2] = -2;
        return;
    }
    int64_t landed = list->entries[cnt_before].obj;
    out[1] = (landed == O_TL_POISON) ? -1 : (landed == obj ? 1 : 0);
    // `loc` only when something was appended. Reporting the poison itself
    // compares 0xDEADBEEFDEADBEEF as a signed int64 here against the same
    // bits read back unsigned on the emulator side - a difference in how the
    // two harnesses widen a number, not in what either engine did.
    out[2] = (landed == O_TL_POISON) ? -1 : list->entries[cnt_before].loc;
}

static void o_target_list_add_obj_unless_off(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int cnt_before = (int)a[4];
    static TargetList list;
    int64_t obj;

    if (a[3] != 0) {
        obj = OBJ_HANDLE_NULL;
    } else {
        obj = o_obj_of_type(type);
        if (obj == OBJ_HANDLE_NULL) {
            out[0] = -1;
            out[1] = -1;
            out[2] = -1;
            return;
        }
        obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);
        // Only on a critter. The field is read behind an is-critter guard, so
        // setting it elsewhere changes nothing - but CE refuses a field the
        // type does not own and says so on stderr, and a fixture that fills
        // the log with refusals it does not need is a fixture that would hide
        // the one that mattered.
        if (obj_type_is_critter(type)) {
            obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, (int)a[2]);
        }
    }

    o_target_list_seed(&list, cnt_before, (int)a[5], obj);
    target_list_add_obj_unless_off(&list, obj);
    o_target_list_report(&list, cnt_before, obj, out);
}

static void o_target_list_add_obj_unless_destroyed(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int cnt_before = (int)a[3];
    static TargetList list;
    int64_t obj;

    if (a[2] != 0) {
        obj = OBJ_HANDLE_NULL;
    } else {
        obj = o_obj_of_type(type);
        if (obj == OBJ_HANDLE_NULL) {
            out[0] = -1;
            out[1] = -1;
            out[2] = -1;
            return;
        }
        obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);
    }

    o_target_list_seed(&list, cnt_before, (int)a[4], obj);
    target_list_add_obj_unless_destroyed(&list, obj);
    o_target_list_report(&list, cnt_before, obj, out);
}

static void o_reaction_get_base(const int64_t* a, int64_t* out)
{
    int64_t obj = o_obj_of_type(OBJ_TYPE_NPC);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_NPC_REACTION_BASE, (int)a[0]);

    out[0] = reaction_get_base(obj);
}

// `skill_ui_is_pickpocket_immune` (0x57A5E0). V4 batch 2. Two field reads and
// two mask tests OR'd together, so the domain has to reach all four
// combinations - `FlagBoundaryArg` on each mask does that, and the OR means a
// swapped pair of masks still answers correctly whenever both bits agree.
// That is why the two are drawn independently rather than together.
//
// Both fields are critter-owned, so the type domain is PC and NPC. `static` in
// CE; skill_ui_globalized.o is what makes it reachable.
static void o_skill_ui_is_pickpocket_immune(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, (int)a[1]);
    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)a[2]);

    out[0] = skill_ui_is_pickpocket_immune(obj);
}

static void o_portal_is_open(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_CURRENT_AID, (int)a[1]);

    out[0] = portal_is_open(obj);
}

static void o_critter_is_prone(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);
    obj_field_int32_set(obj, OBJ_F_CURRENT_AID, (int)a[2]);

    out[0] = critter_is_prone(obj);
}

// `critter_can_jump_window` (0x45F570). No guard at all -- not on the handle
// and not on the type -- so NULL is out and the type domain stays open only
// because OBJ_F_CURRENT_AID is ordinal 1, inside the common block every
// object owns. The whole function is a switch on `tig_art_type` of that
// field: CRITTER answers true, MONSTER defers to
// `tig_art_monster_id_specie_get == TIG_ART_MONSTER_SPECIE_ORC`, everything
// else false. The original's `sub eax,2` / `sub eax,0xa` chain at 0x45F58D is
// the same two type numbers, 2 and 12.
static void o_critter_can_jump_window(const int64_t* a, int64_t* out)
{
    int64_t obj = o_obj_of_type((int)a[0]);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_CURRENT_AID, (int)a[1]);

    out[0] = critter_can_jump_window(obj);
}

// `critter_light_get` (0x45FA70), the first object-handle case with two
// outputs: it returns an art id and writes a colour through its second
// parameter. Both are compared.
//
// It reads OBJ_F_CRITTER_FLAGS with no guard of any kind (0x45FA78 pushes it
// immediately), so the domain is restricted to the two critter types and NULL
// is out. The four light bits and their four colours all come off the
// original: `test eax,0x400000` / `0x200000` / `0x100000` / `0x80000` at
// 0x45FA87, 0x45FAC1, 0x45FAFB and 0x45FB32, and the colour triples pushed at
// 0x45FAAF (244,255,255), 0x45FADF (255,214,172) and 0x45FB1B (236,138,85).
//
// The priority order is the point: XLARGE beats LARGE beats MEDIUM beats
// SMALL, so an input with several bits set has exactly one right answer and a
// reordered chain in CE would show up here.
static void o_critter_light_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    unsigned int color = 0;
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        out[1] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, (int)a[1]);
    }

    out[0] = (uint32_t)critter_light_get(obj, &color);
    out[1] = color;
}

// --- item.c through the object fixture (2026-08-31, card V2) ------------
//
// `item.c` has the highest object-handle *share* in the tree -- 121 of 135
// definitions, 89% -- and had two cases, `item_worth` and `item_is_worthless`,
// which are the pair the fixture was proved on above. Of the twelve candidates
// `scripts/obj_case_candidates.py --module item` prints, eight are added here
// and two are refused; the refusals are written up in
// `harness/emudiff/README.md` rather than left as silence.
//
// Field ordinals, each read off the literal the original pushes:
//
//     OBJ_F_CONTAINER_INVENTORY_SOURCE  61 = 0x3D    pushed at 0x4641DF
//     OBJ_F_ITEM_INV_AID                93 = 0x5D    pushed at 0x465C66
//     OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY  96 = 0x60    pushed at 0x461528
//     OBJ_F_WEAPON_FLAGS               112 = 0x70    pushed at 0x465DB7
//     OBJ_F_WEAPON_RANGE               121 = 0x79    pushed at 0x465DEF
//     OBJ_F_WEAPON_AMMO_TYPE           125 = 0x7D    pushed at 0x465CD4
//     OBJ_F_GENERIC_FLAGS              211 = 0xD3    pushed at 0x465664
//     OBJ_F_CRITTER_INVENTORY_SOURCE   239 = 0xEF    pushed at 0x4641CC
//
// `obj_type_is_item` is inlined throughout as `cmp eax,5` / `cmp eax,0xe`,
// which is OBJ_TYPE_WEAPON..OBJ_TYPE_GENERIC and matches CE's helper.

// `item_is_item` (0x461310). A NULL guard and a range check on OBJ_F_TYPE,
// nothing else -- so NULL is in the domain and the type domain is open.
static void o_item_is_item(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[1] != 0) {
        out[0] = item_is_item(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    out[0] = item_is_item(obj);
}

// `item_magic_tech_complexity` (0x461520). A bare field read: 0x461528 pushes
// 0x60 with no guard of any kind in front of it. That puts it in the same
// class as `item_is_worthless` above -- ITEM TYPES ONLY, and no NULL -- and
// for the same reason, which is that the fixture cannot represent a field CE's
// object does not have.
static void o_item_magic_tech_complexity(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY, (int)a[1]);
    }

    out[0] = item_magic_tech_complexity(obj);
}

// `item_inventory_source` (0x4641A0). A NULL guard, then a two-arm switch on
// OBJ_F_TYPE reading a *different* field on each arm -- the container's source
// on OBJ_TYPE_CONTAINER, the critter's on OBJ_TYPE_NPC.
//
// The two arms are drawn separately rather than sharing one value, and that is
// the point of the case: with one drawn value a CE that swapped the two arms
// would answer correctly on every input.
static void o_item_inventory_source(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[3] != 0) {
        out[0] = item_inventory_source(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_CONTAINER) {
        obj_field_int32_set(obj, OBJ_F_CONTAINER_INVENTORY_SOURCE, (int)a[1]);
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_INVENTORY_SOURCE, (int)a[2]);
    }

    out[0] = item_inventory_source(obj);
}

// `item_is_identified` (0x464370). NULL guard, item-type guard, then two ways
// to answer true: a complexity at or below zero, or bit 0 of OBJ_F_ITEM_FLAGS.
// The short-circuit is the interesting part -- a negative complexity answers
// true whatever the flag says.
static void o_item_is_identified(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[3] != 0) {
        out[0] = item_is_identified(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY, (int)a[1]);
        obj_field_int32_set(obj, OBJ_F_ITEM_FLAGS, (int)a[2]);
    }

    out[0] = item_is_identified(obj);
}

// `item_weapon_ammo_type` (0x465CA0). NULL guard, `== OBJ_TYPE_WEAPON`, then
// the field. Both refusals answer 10000 (0x2710 at 0x465CB1 and 0x465CCD),
// which is the sentinel `item_weapon_skill` below then compares against
// nothing -- it falls through to the ARROW test, and 10000 is not ARROW.
static void o_item_weapon_ammo_type(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = item_weapon_ammo_type(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_WEAPON) {
        obj_field_int32_set(obj, OBJ_F_WEAPON_AMMO_TYPE, (int)a[1]);
    }

    out[0] = item_weapon_ammo_type(obj);
}

// `item_armor_coverage` (0x465C30). Three guards -- NULL, `item_is_item`, and
// `== OBJ_TYPE_ARMOR` -- and then OBJ_F_ITEM_INV_AID through
// `tig_art_item_id_armor_coverage_get` (0x504570), which is itself a case here
// and is pure bit arithmetic on both sides. The emulator runs the original's
// own copy of it rather than a stub, so the boundary stays symmetric.
static void o_item_armor_coverage(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = item_armor_coverage(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_INV_AID, (int)a[1]);
    }

    out[0] = item_armor_coverage(obj);
}

// `item_location_get` (0x4655D0). The widest of this batch: a seven-way switch
// on the armour coverage, each arm answering a different ITEM_INV_LOC_*
// constant, plus a WEAPON arm, a GENERIC-with-torch-flag arm and a -1 default.
// Ten outcomes from three fields.
//
// NO NULL in the domain: 0x4655DA pushes OBJ_F_TYPE straight away. That the
// *inner* `item_armor_coverage` guards NULL does not help -- the outer read
// happens first.
static void o_item_location_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_INV_AID, (int)a[1]);
    }

    if (type == OBJ_TYPE_GENERIC) {
        obj_field_int32_set(obj, OBJ_F_GENERIC_FLAGS, (int)a[2]);
    }

    out[0] = item_location_get(obj);
}

// `item_weapon_skill` (0x465D80). Five fields, five outcomes, and the deepest
// chain in `item.c` that stays inside the fixture: NULL answers SKILL_MELEE,
// a non-weapon SKILL_THROWING, OWF_BOOMERANGS (bit 6, `test al,0x40`)
// SKILL_THROWING again, then a three-way conjunction -- a *negated* complexity
// above zero, a range of at least 3, and an ammo type in {BULLET, CHARGE,
// FUEL} -- for SKILL_FIREARMS, and finally ARROW for SKILL_BOW or
// SKILL_MELEE.
//
// Worth knowing about the original's tail: at 0x465E18 it computes the last
// two answers as `neg eax; sbb eax,eax; and eax,2`, which is 0 when the ammo
// type is 0 and 2 otherwise. That is SKILL_BOW = 0 and SKILL_MELEE = 2, and it
// is why those two constants have to be what CE says they are for the case to
// pass at all.
static void o_item_weapon_skill(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[5] != 0) {
        out[0] = item_weapon_skill(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_WEAPON) {
        obj_field_int32_set(obj, OBJ_F_WEAPON_FLAGS, (int)a[1]);
        obj_field_int32_set(obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY, (int)a[2]);
        obj_field_int32_set(obj, OBJ_F_WEAPON_AMMO_TYPE, (int)a[3]);
        obj_field_int32_set(obj, OBJ_F_WEAPON_RANGE, (int)a[4]);
    }

    out[0] = item_weapon_skill(obj);
}

// --- object.c through the object fixture (card V3) ---------------------------
//
// Six candidates, and the whole file's arithmetic that a field fixture can
// reach. Every ordinal below is the literal the original pushes, read out of
// `Arcanum.exe` with objdump:
//
//     OBJ_F_HP_PTS      27 = 0x1B    0x43D418
//     OBJ_F_HP_ADJ      28 = 0x1C    0x43D4C8
//     OBJ_F_HP_DAMAGE   29 = 0x1D    0x43D518
//     OBJ_F_FLAGS       19 = 0x13    0x43D9CC
//     OBJ_F_TYPE       335 = 0x14F   0x43D948, 0x43D99A, 0x43D69A
//
// and `OF_DYNAMIC` is bit 13, from `not eax; shr eax,0xd; and eax,1` at
// 0x43D9D8 -- 0x2000, which is what CE's obj_flags.h says.
//
// **These five ordinals are all below OBJ_F_END (37), so they are in the
// common block every object type owns.** That is what makes the type domain
// unrestricted here for the first time in this fixture: `object_hp_pts_get`
// reads 0x1B off a wall as legitimately as off a critter, and CE's
// `object_field_valid` agrees for all eighteen buildable types. The case
// *checks* that rather than assuming it -- a type the fixture could not
// represent would show up as a mismatch on exactly those inputs.
//
// `object_hp_from_pts` is `static` in CE and comes in through
// `object_globalized.o`; declared here rather than in object.h, for the reason
// given above `combat_weapon_loudness`.
int object_hp_from_pts(int64_t obj);

// A getter shaped `obj_field_int32_get(obj, <ordinal>)` and nothing else --
// no NULL guard, no type guard, no arithmetic. Three of them, and three cases
// rather than one shared case, for the same reason the fatigue trio is three:
// they differ only in the ordinal, and a neighbouring ordinal is the slip this
// engine has already been caught making (0x45D4F0 writes 0xE0 where it means
// 0xE1).
static void o_object_hp_field_get(const int64_t* a, int64_t* out, int field,
    int (*fn)(int64_t))
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    // Unconditional, unlike every earlier case in this file: the field is in
    // the common block, so there is no type for which the set would be
    // refused. If one ever were, the emulator would answer the drawn value
    // and this side a 0, and the case would say so.
    obj_field_int32_set(obj, field, (int)a[1]);

    out[0] = fn(obj);
}

static void o_object_hp_pts_get(const int64_t* a, int64_t* out)
{
    o_object_hp_field_get(a, out, OBJ_F_HP_PTS, object_hp_pts_get);
}

static void o_object_hp_adj_get(const int64_t* a, int64_t* out)
{
    o_object_hp_field_get(a, out, OBJ_F_HP_ADJ, object_hp_adj_get);
}

static void o_object_hp_damage_get(const int64_t* a, int64_t* out)
{
    o_object_hp_field_get(a, out, OBJ_F_HP_DAMAGE, object_hp_damage_get);
}

// `object_hp_from_pts` (0x43D690). Tier 2 over `object_hp_pts_get`: a critter
// gets four hit points per stored point and everything else exactly one, so
// the case is the multiply and the type test that selects it. The inner
// getter is cased separately and first, deliberately -- a delegating function
// hides its callee's branches from `variety.py`, and with the callee already
// held still a difference here can only be about this function's own shape.
//
// NO NULL: 0x43D69A pushes OBJ_F_TYPE with nothing in front of it.
static void o_object_hp_from_pts(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_HP_PTS, (int)a[1]);

    out[0] = object_hp_from_pts(obj);
}

// `object_is_static_type` (0x43D940) and `object_is_static` (0x43D990). They
// share a five-way type test -- PROJECTILE, CONTAINER, critter, item -- and
// differ only in the tail: the first answers true for everything else, the
// second additionally clears OF_DYNAMIC. Two cases rather than one, for the
// same reason as the trio above: a tail is exactly what a transcription drops.
//
// Neither guards NULL (both push OBJ_F_TYPE first), so NULL is out of both
// domains.
//
// Worth knowing about the original's codegen: 0x43D975 reaches `xor eax,eax`
// then `test eax,eax; sete dl`, which is a `!` applied to a constant -- the
// static-type arm loads 1 and returns 0, the other loads 0 and returns 1. The
// answer is the negation of what the register holds, and reading the register
// rather than the tail would get both arms backwards.
static void o_object_is_static_type(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    out[0] = object_is_static_type(obj);
}

static void o_object_is_static(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);

    out[0] = object_is_static(obj);
}

// --- magictech.c through the object fixture (card V3) ------------------------
//
// Ordinals, from the literal the original pushes:
//
//     OBJ_F_SPELL_FLAGS   20 = 0x14    0x456445, 0x459062, 0x459192, 0x45A58A
//     OBJ_F_FLAGS         19 = 0x13    0x450A8A, 0x45A5B3
//     OBJ_F_ITEM_PARENT   88 = 0x58    0x450A9B
//
// and the flag masks, each read off the literal the original tests:
//
//     OF_INVENTORY             0x1000  `test ah,0x10`  0x450A96
//     OSF_INVISIBLE            0x1     `test al,0x1`   0x45A596
//     OSF_DETECTING_INVISIBLE  0x200   `test ah,0x2`   0x45A5AE
//     OF_INVISIBLE             0x200   `test ah,0x2`   0x45A5BF
//
// `magictech_disallowed_flags_check` is `static` in CE and comes in through
// `magictech_globalized.o`; `magictech_invocation_parent_get` is external but
// absent from magictech.h. Both are declared here rather than in the header,
// for the reason given above `combat_weapon_loudness`.
bool magictech_disallowed_flags_check(int64_t a1, int64_t a2, MagicTechInfo* magictech);
int64_t magictech_invocation_parent_get(int64_t obj);

// A handle cannot be compared across the two sides -- CE's comes out of its
// own object pool and the emulator's is the fixture's made-up constant -- so
// every case here that answers with one reports *which* handle it answered
// with instead. The emulator driver applies the same three-way mapping.
#define O_HANDLE_TOKEN_NULL 0
#define O_HANDLE_TOKEN_SELF 1
#define O_HANDLE_TOKEN_PARENT 2
#define O_HANDLE_TOKEN_OTHER 3

static int64_t o_handle_token(int64_t result, int64_t self, int64_t parent)
{
    if (result == OBJ_HANDLE_NULL) {
        return O_HANDLE_TOKEN_NULL;
    }
    if (result == self) {
        return O_HANDLE_TOKEN_SELF;
    }
    if (result == parent) {
        return O_HANDLE_TOKEN_PARENT;
    }
    return O_HANDLE_TOKEN_OTHER;
}

// `target_obj_resolve` (0x4F2C60). Reads the type through its `int64_t*`, and
// on SCENERY substitutes whoever is standing in it: `cmp $0x3` at 0x4F2C7C is
// OBJ_TYPE_SCENERY, `push $0x46` at 0x4F2C86 is OBJ_F_SCENERY_WHOS_IN_ME, the
// `or %edx,%ecx` at 0x4F2C94 is the null test on the 64-bit handle, and the
// substituting arm returns the literal 0x10 - OBJ_TYPE_NPC.
//
// **No NULL guard on the handle**, so NULL is out of the domain: the function
// reads a field off whatever it is given.
//
// Two answers: the returned type, and *which* handle the pointer holds
// afterwards - the handle itself cannot cross the boundary, so `o_handle_token`
// is used, as for `magictech_invocation_parent_get`.
static void o_target_obj_resolve(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);
    int64_t whos_in_me = OBJ_HANDLE_NULL;

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        out[1] = -1;
        return;
    }

    if (a[1] != 0) {
        // A second real object, so the substitution has something to find and
        // the token can tell "who it answered with" apart from "itself".
        whos_in_me = o_obj_of_type_second(OBJ_TYPE_NPC);
    }
    if (type == OBJ_TYPE_SCENERY) {
        obj_field_handle_set(obj, OBJ_F_SCENERY_WHOS_IN_ME, whos_in_me);
    }

    int64_t subject = obj;
    out[0] = target_obj_resolve(&subject);
    out[1] = o_handle_token(subject, obj, whos_in_me);
}

// --- the candidate tail, batch 3 (card V4) ----------------------------------

// `anim_is_portal` (0x4284A0). Three guards over one handle taken out of a
// run-info block: null, then `OF_DESTROYED|OF_OFF`, then the type.
//
// **The handle's offset is the original's, and it is the only thing this
// fixture claims.** `mov 0xd00(%eax),%esi` / `mov 0xd04(%eax),%edi` at
// 0x4284A6 puts `params[0]` at +0xD00; CE's `anim_private.h` says the same in
// its `/* 0D00 */` comment, but CE is 64-bit and its own offset is larger, so
// this wrapper writes the named field and the emulator side writes 0xD00.
//
// Ordinals and constants off the pushed literals: `push $0x13` at 0x4284B8 is
// OBJ_F_FLAGS, `test $0x3,%al` at 0x4284C4 is `OF_DESTROYED|OF_OFF`, and
// `push $0x14F` at 0x4284C8 with the `dec/neg/sbb/inc` at 0x4284D7 is
// `type == 1`, which pins OBJ_TYPE_PORTAL to 1.
//
// `static` in CE, and reached through `anim_globalized.o` - the same object
// `arcanum-tests` links, not a second copy. Declared here rather than in
// `anim_private.h` so the shipped build gains no symbol because a test wanted
// one.
bool anim_is_portal(AnimRunInfo* run_info);

static void o_anim_is_portal(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    static AnimRunInfo run_info;
    int64_t obj;

    if (a[2] != 0) {
        obj = OBJ_HANDLE_NULL;
    } else {
        obj = o_obj_of_type(type);
        if (obj == OBJ_HANDLE_NULL) {
            out[0] = -1;
            return;
        }
        obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);
    }

    memset(&run_info, 0, sizeof(run_info));
    run_info.params[0].obj = obj;

    out[0] = anim_is_portal(&run_info);
}

// `mt_ai_item_allowed_for_ai` (0x4CCA30) - the producer calls it `sub_4CCA30`
// because the symbol ledger has no name for it; CE does.
//
// "May the AI use this item": an identified item always may, and an
// unidentified one only if there is no leader to disapprove, or if it is food
// or a scroll.
//
// `test $0x1,%al` at 0x4CCA46 is OIF_IDENTIFIED; `add $0x8,%eax` then the
// 64-bit read at 0x4CCA4E-0x4CCA53 is `leader_obj` at +0x08 - the ONLY member
// the function touches, so the struct needs nothing else; `push $0x1` at
// 0x4CCA5A is OBJ_F_CURRENT_AID, and `cmp $0x4` / `cmp $0x5` at
// 0x4CCA6C/0x4CCA71 pin TIG_ART_ITEM_TYPE_FOOD to 4 and _SCROLL to 5.
//
// `tig_art_item_id_type_get` (0x504550) runs for real on both sides - it is
// pure art-id arithmetic and is already a case of its own.
//
// `static` in CE, reached through `mt_ai_globalized.o`.
bool mt_ai_item_allowed_for_ai(int64_t item_obj, MagicTechAi* magictech_ai);

static void o_mt_ai_item_allowed_for_ai(const int64_t* a, int64_t* out)
{
    MagicTechAi magictech_ai;
    // Any of the item family carries OBJ_F_ITEM_FLAGS; GENERIC is the plain
    // one. The function never reads the type, so this is a carrier for the two
    // fields it does read, not an input.
    int64_t obj = o_obj_of_type(OBJ_TYPE_GENERIC);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_ITEM_FLAGS, (int)a[0]);
    obj_field_int32_set(obj, OBJ_F_CURRENT_AID, (int)a[1]);

    // Zeroed and then given a leader, so the one member the function reads is
    // the only one that differs between trials.
    memset(&magictech_ai, 0, sizeof(magictech_ai));
    magictech_ai.leader_obj = a[2] != 0
        ? OBJ_HANDLE_NULL
        : o_obj_of_type_second(OBJ_TYPE_NPC);

    out[0] = mt_ai_item_allowed_for_ai(obj, &magictech_ai);
}

// `target_context_init` (0x4F2600) -- the first case whose whole answer is a
// struct the function INITIALISES, and the first that needs the third field
// accessor. Nothing is returned: the function's entire effect is 15 stores
// into a `TargetContext`, one of them conditional, plus a tail call to
// `target_params_init` (0x4F25E0) that this case therefore covers too.
//
// **The two sides do not share a struct layout and must not pretend to.** CE
// is 64-bit, so `TargetContext`'s pointers are 8 bytes and every offset in
// `target.h`'s comments is the ORIGINAL's, not CE's. This wrapper reads named
// fields; the emulator side reads the original's offsets, each sourced from a
// store in the disassembly. The comparison is therefore field-by-field, never
// byte-by-byte.
//
// **The struct is poisoned before the call**, so "wrote zero" and "did not
// write" are different observations. `O_TC_POISON` is 0x5A repeated: positive
// as a signed int64, which matters because the emulator reads its side back
// unsigned and CE prints signed -- the widening trap that made the target-list
// batch report the sentinel as a difference.
//
// **Three values cannot cross the boundary raw and are tokenised.**
//  * `params` is a POINTER. CE's is a host address, the emulator's a guest
//    one; only its identity is comparable.
//  * `self_obj` / `source_obj` hold an object handle, which CE takes from its
//    own pool -- `o_handle_token` again.
//  * `source_loc` is whatever `obj_field_int64_get(obj, OBJ_F_LOCATION)`
//    answered, and each side compares against *its own* reading of that field
//    rather than against a shared constant. So the observable is "the function
//    stored the field it read", which is what the branch is for, and no claim
//    is made that the two engines' locations are equal.
//
// **`OBJ_F_LOCATION` is read, never written.** Writing it calls `obj_find_move`
// and there are no sectors here (`obj_harness.h`). That is also why the
// location is not a drawn input: the domain is exactly the function's two
// branches, and it is exhaustive at four inputs.
//
// The ordinal is checked against the literal the original pushes, not inferred
// from the name: `push $0x2` at 0x4F2647, and OBJ_F_LOCATION is 2.
#define O_TC_POISON ((int64_t)0x5A5A5A5A5A5A5A5ALL)
#define O_TC_POISON32 ((int64_t)0x5A5A5A5AL)

// Untouched / NULL / the params block that was passed / something else.
#define O_PTR_POISON 3
#define O_PTR_NULL 0
#define O_PTR_EXPECTED 1
#define O_PTR_OTHER 2

static int64_t o_ptr_token(const void* value, const void* expected)
{
    if (value == NULL) {
        return O_PTR_NULL;
    }
    if (value == expected) {
        return O_PTR_EXPECTED;
    }
    // The poison pattern the memset wrote, read back as a pointer. The
    // emulator's poison is four bytes of the same byte and CE's is eight, so
    // the token is what makes them comparable.
    if ((int64_t)(intptr_t)value == O_TC_POISON) {
        return O_PTR_POISON;
    }
    return O_PTR_OTHER;
}

// -1 untouched, 1 the value the field accessor answered, 0 anything else.
static int64_t o_loc_token(int64_t value, int64_t expected)
{
    if (value == O_TC_POISON) {
        return -1;
    }
    return value == expected ? 1 : 0;
}

static void o_target_context_init(const int64_t* a, int64_t* out)
{
    TargetContext ctx;
    TargetParams params;
    int64_t obj = OBJ_HANDLE_NULL;
    int64_t loc = 0;

    if (a[0] == 0) {
        // Any real object will do -- the function reads exactly one field off
        // it. OBJ_TYPE_NPC is the one every other case in this batch uses.
        obj = o_obj_of_type(OBJ_TYPE_NPC);
        if (obj == OBJ_HANDLE_NULL) {
            for (int i = 0; i < 20; i++) {
                out[i] = -1;
            }
            return;
        }
        loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    }

    memset(&ctx, 0x5A, sizeof(ctx));
    memset(&params, 0x5A, sizeof(params));

    target_context_init(&ctx, a[1] != 0 ? NULL : &params, obj);

    out[0] = o_ptr_token(ctx.params, &params);
    out[1] = o_handle_token(ctx.self_obj, obj, OBJ_HANDLE_NULL);
    out[2] = o_handle_token(ctx.source_obj, obj, OBJ_HANDLE_NULL);
    out[3] = o_loc_token(ctx.source_loc, loc);
    out[4] = ctx.target_obj;
    out[5] = ctx.target_loc;
    out[6] = ctx.orig_target_obj;
    out[7] = ctx.orig_target_loc;
    out[8] = ctx.attacker_obj;
    out[9] = ctx.summoned_obj;
    out[10] = o_ptr_token(ctx.targets, NULL);
    out[11] = o_ptr_token(ctx.obj_list, NULL);
    out[12] = o_ptr_token(ctx.summoned_obj_list, NULL);
    out[13] = ctx.source_spell_flags;
    out[14] = ctx.target_spell_flags;

    // What `target_params_init` left behind, or the poison if it was never
    // called. All six of its stores are covered: 0x4F25E6-0x4F25F4.
    out[15] = (int64_t)params.tgt;
    out[16] = params.spell_flags;
    out[17] = params.no_spell_flags;
    out[18] = params.radius;
    out[19] = params.count;
}

// `magictech_invocation_parent_get` (0x450A50). Answers "who is really casting
// this" -- an item held in someone's inventory answers with its owner, and
// everything else answers with itself. Four ways out: NULL for NULL, itself
// for a critter, itself for an item that is not in an inventory, and the
// OBJ_F_ITEM_PARENT handle for one that is.
//
// The second `obj_field_handle_get` (0x406E80) is stubbed on the emulator side
// exactly as `obj_field_int32_get` is, and for the same reason: it is a door
// into the object system, not a claim about it. Doing that rather than cutting
// the domain short of the branch is what keeps the answer worth having -- the
// inventory arm is the only interesting thing this function does.
static void o_magictech_invocation_parent_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;
    int64_t parent;

    if (a[3] != 0) {
        out[0] = o_handle_token(magictech_invocation_parent_get(OBJ_HANDLE_NULL),
            OBJ_HANDLE_NULL, OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    // A second object, of a type that is neither the subject nor an item, so
    // the three tokens can never collide.
    parent = o_obj_of_type_second(OBJ_TYPE_CONTAINER);

    obj_field_int32_set(obj, OBJ_F_FLAGS, (int)a[1]);

    // OBJ_F_ITEM_PARENT (88) is above OBJ_F_END, so only an item owns it --
    // and the original never reads it unless the type test already passed.
    if (obj_type_is_item(type)) {
        obj_field_handle_set(obj, OBJ_F_ITEM_PARENT,
            a[2] != 0 ? OBJ_HANDLE_NULL : parent);
    }

    out[0] = o_handle_token(magictech_invocation_parent_get(obj), obj, parent);
}

// `magictech_disallowed_flags_check` (0x456430). Three masks off the spell's
// own rules row, tested against two different objects: the caster's spell
// flags, the target's spell flags, and -- only if the target is a critter --
// the target's critter flags.
//
// The MagicTechInfo the third argument points at is built here from CE's
// named fields, and on the emulator side from the three offsets the original
// itself uses: `[ebx+0x11c]` at 0x456455, `[ebx+0x120]` at 0x45647A and
// `[ebx+0x124]` at 0x4564C1. Two forced-independent derivations of the same
// three numbers, which is the blit fixture's rule and not the object stub's --
// this one *is* a claim about a layout, so it has to be sourced.
//
// **The target handle may be OBJ_HANDLE_NULL, and that input is why this
// case exists.** Writing it is what turned up the missing guard: the original
// guards the target -- `or eax,edi; je` at 0x45646F, answering true -- and CE
// did not, so a call with a null target read its three fields off whatever
// object `obj_lock(OBJ_HANDLE_NULL)` lands on, which is index 0 of the object
// pool. It is in the domain now that CE has the guard.
//
// **What that input can and cannot do is worth being exact about, because it
// looks like a regression test and is not one.** An unfixed CE reads pool
// slot 0, whose fields in this oracle are all zero, so it would answer `true`
// as well and agree by accident. Nothing here would catch the guard being
// removed again.
//
// The divergence is real and the guard is right, but it changes no reachable
// behaviour and is NOT numbered: on shipped data the three masks are non-zero
// only for spells that require a critter target, so a null target never
// arrives while one of them is set. See PORTING-BUGS.md, *Deliberately not
// numbered*, for the intersection test that settled it.
static void o_magictech_disallowed_flags_check(const int64_t* a, int64_t* out)
{
    int type_a = (int)a[0];
    int type_b = (int)a[2];
    int64_t obj_a;
    int64_t obj_b;
    MagicTechInfo info;

    memset(&info, 0, sizeof(info));
    info.disallowed_sf = (unsigned int)a[5];
    info.disallowed_tsf = (unsigned int)a[6];
    info.disallowed_tcf = (unsigned int)a[7];

    if (a[9] != 0) {
        obj_b = OBJ_HANDLE_NULL;
    } else {
        obj_b = o_obj_of_type_second(type_b);
        if (obj_b == OBJ_HANDLE_NULL) {
            out[0] = -1;
            return;
        }

        // OBJ_F_SPELL_FLAGS is ordinal 20, below OBJ_F_END, so every type
        // owns it.
        obj_field_int32_set(obj_b, OBJ_F_SPELL_FLAGS, (int)a[3]);
        if (obj_type_is_critter(type_b)) {
            obj_field_int32_set(obj_b, OBJ_F_CRITTER_FLAGS, (int)a[4]);
        }
    }

    if (a[8] != 0) {
        out[0] = magictech_disallowed_flags_check(OBJ_HANDLE_NULL, obj_b, &info);
        return;
    }

    obj_a = o_obj_of_type(type_a);
    if (obj_a == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj_a, OBJ_F_SPELL_FLAGS, (int)a[1]);

    out[0] = magictech_disallowed_flags_check(obj_a, obj_b, &info);
}

// `magictech_find_effect_run` (0x459170). Asks whether an object is under a
// magical effect of a given kind, answering with the run's index.
//
// **Its sibling `magictech_find_effect_caster` (0x459040) is NOT a case.** It
// was written, it passed, and `variety.py` reported `distinct=1` - every
// input it can legally be given answers `(false, 0)`. The difference between
// the two is one branch target: `_run` may be handed a NULL out-pointer,
// because 0x45917A jumps to a plain `xor eax,eax; ret` that writes nothing,
// and "wrote -1" against "did not write" is the only pair of distinguishable
// answers either function has left once the loop is out of reach. The caster
// has no such input, because the ORIGINAL faults on one: 0x45905C tests the
// pointer and branches to 0x45911E, which is the block that *stores* through
// it - `mov DWORD PTR [ebp+0x0],0x0`. CE checks the pointer first and returns
// without writing, so CE is the more defensive of the two and every CE caller
// passes `&local`; the divergence is recorded in PORTING-BUGS.md under the
// unnumbered defects.
//
// **This is a PARTIAL case and the domain says so: the 512-entry search loop
// is not covered, and cannot be.** `magictech_run_info` is a malloc'd pointer
// on both sides -- 0x6876E0 in the shipped image reads 0, and CE's is NULL
// until `magictech_init` runs, which nothing here does. Reaching the loop
// therefore dereferences NULL in *both* binaries rather than searching an
// empty pool, so the flag mask is drawn to make the guard in front of it
// always true: the mask lives in the high half of the word and the object's
// spell flags in the low half, so `spell_flags & flags` is zero by
// construction while both halves still vary. What is covered is the three
// guards and the two sentinels, and a mutation to either sentinel is caught.
//
// The card that scheduled this predicted the loop would "find nothing either
// way"; it would fault either way, which is a stronger reason for the same
// restriction and is why the domain is built rather than merely narrowed.
static void o_magictech_find_effect_run(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;
    int index = (int)0xDEADBEEF;
    bool null_ptr = a[4] != 0;

    if (a[3] != 0) {
        out[0] = magictech_find_effect_run(OBJ_HANDLE_NULL, (unsigned int)a[2],
            null_ptr ? NULL : &index);
        out[1] = (int64_t)(uint32_t)index;
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        out[1] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, (int)a[1]);

    out[0] = magictech_find_effect_run(obj, (unsigned int)a[2],
        null_ptr ? NULL : &index);
    out[1] = (int64_t)(uint32_t)index;
}

// `magictech_invisibility_target_check` (0x45A580). Whether `a1` may target
// `a2` given who is invisible: the target's OSF_INVISIBLE, the caster's
// OSF_DETECTING_INVISIBLE, the target's own OF_INVISIBLE, and then -- for a PC
// target only -- whether it is doing something that gives it away.
//
// Tier 2, and the only candidate in the file with inner calls. Two of them,
// the `anim_is_current_goal_type` pair, are dead under the fixture and
// *symmetrically* so, which is the thing that had to be checked rather than
// assumed: `anim_run_info` is a static array in both binaries -- 0x687700 in
// the original, from `mov ecx,0x68770C` at 0x44D2FD -- not a malloc'd pointer
// like `magictech_run_info` above. It is zeroed, `flags & 1` is false in every
// one of its 216 slots, and `anim_find_first` therefore answers -1 on both
// sides for every input. So the PC arm reduces to its third call,
// `combat_critter_is_combat_mode_active`, which is a case of its own.
//
// No NULL on either side: 0x45A58A pushes the target's field first.
static void o_magictech_invisibility_target_check(const int64_t* a, int64_t* out)
{
    int type_a = (int)a[0];
    int type_b = (int)a[2];
    int64_t obj_a = o_obj_of_type(type_a);
    int64_t obj_b = o_obj_of_type_second(type_b);

    if (obj_a == OBJ_HANDLE_NULL || obj_b == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj_a, OBJ_F_SPELL_FLAGS, (int)a[1]);
    obj_field_int32_set(obj_b, OBJ_F_SPELL_FLAGS, (int)a[3]);
    obj_field_int32_set(obj_b, OBJ_F_FLAGS, (int)a[4]);
    if (obj_type_is_critter(type_b)) {
        obj_field_int32_set(obj_b, OBJ_F_CRITTER_FLAGS, (int)a[5]);
    }

    out[0] = magictech_invisibility_target_check(obj_a, obj_b);
}

// --- the candidate tail, batch 1 (card V4) -----------------------------------
//
// No cluster and no shared file: twelve one-sitting functions from eleven
// modules, which is what the whole of V4 looks like. Every ordinal and every
// mask below was read off the literal the original uses, as always:
//
//     OBJ_F_SOUND_EFFECT              33 = 0x21    0x4F0FF4, 0x4F1034
//     OBJ_F_SCENERY_FLAGS             69 = 0x45    0x41B9A4
//     OBJ_F_ITEM_FLAGS                87 = 0x57    0x4CC1BD
//     OBJ_F_ITEM_SPELL_1             100 = 0x64    0x4CC186
//     OBJ_F_ITEM_SPELL_MANA_STORE    105 = 0x69    0x4CC176
//     OBJ_F_GENERIC_FLAGS            211 = 0xD3    0x4BCA84
//     OBJ_F_CRITTER_AUTO_LEVEL_SCHEME 245 = 0xF5   0x4A6D69
//     OBJ_F_PC_FLAGS_FATE            254 = 0xFE    0x4F5098
//     OBJ_F_PC_BACKGROUND_TEXT       258 = 0x102   0x4C26D8
//
//     OGF_IS_TRAP_DEVICE      0x0004     `test al,0x4`   0x4BCA93
//     OSCF_SOUND_SMALL        0x0040     `test al,0x40`  0x41B9B0
//     OSCF_SOUND_MEDIUM       0x0080     `test al,0x80`  0x41B9BA
//     OSCF_SOUND_EXTRA_LARGE  0x0100     `test ah,0x1`   0x41B9C7
//
// The type constants each function compares against were checked the same
// way: SCENERY 3 at 0x41B99F, CONTAINER 2 at 0x4F0FEF, PORTAL 1 at 0x4F102F,
// GENERIC 0xE at 0x4BCA7F, PC 0xF at 0x40DA3F and 0x4C26AF.

static void o_player_is_pc_obj(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[1] != 0) {
        out[0] = player_is_pc_obj(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    out[0] = player_is_pc_obj(obj);
}

// `ai_max_dialog_distance` (0x4AFBB0). Tier 2, and the thinnest delegation in
// the tree: `player_is_pc_obj(obj) ? 5 : 10`. It is a case anyway, and for the
// reason the `object_is_static` pair is two cases - the constants are the
// whole function, and they are exactly what a transcription drops.
static void o_ai_max_dialog_distance(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[1] != 0) {
        out[0] = ai_max_dialog_distance(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    out[0] = ai_max_dialog_distance(obj);
}

// V4. Scalar in, scalar out - no object fixture, because it takes a goal-type
// index rather than a handle. It reads `anim_goal_nodes[index]->concurrent`
// with no bounds check and no NULL check, and the table is statically
// initialised on BOTH sides (87 entries; the original's is at 0x5B03D0 and is
// image data, not BSS), so the case compares one transcribed flag per goal
// type across all 85 non-NULL types at once.
//
// The domain excludes 13 and 22, which are NULL in both tables (AG_UNLOCK_DOOR
// and AG_PICK_WEAPON) - both engines would dereference NULL, which is a shared
// crash rather than a comparison.
static void o_anim_goal_type_is_concurrent(const int64_t* a, int64_t* out)
{
    out[0] = anim_goal_type_is_concurrent((int)a[0]);
}

static void o_trap_is_trap_device(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = trap_is_trap_device(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_GENERIC) {
        obj_field_int32_set(obj, OBJ_F_GENERIC_FLAGS, (int)a[1]);
    }

    out[0] = trap_is_trap_device(obj);
}

static void o_background_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = background_get(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_PC) {
        obj_field_int32_set(obj, OBJ_F_PC_BACKGROUND, (int)a[1]);
    }

    out[0] = background_get(obj);
}

// `background_text_get` (0x4C26D0) and `fate_is_activated` (0x4F5090) read a
// PC-only field with NO guard of any kind in front of it -- 0x4C26D8 and
// 0x4F5098 push it straight away. So the type domain is PC alone and NULL is
// out of both, which is `item_is_worthless`'s rule met for the fourth time.
static void o_background_text_get(const int64_t* a, int64_t* out)
{
    int64_t obj = o_obj_of_type(OBJ_TYPE_PC);

    obj_field_int32_set(obj, OBJ_F_PC_BACKGROUND_TEXT, (int)a[0]);
    out[0] = background_text_get(obj);
}

static void o_fate_is_activated(const int64_t* a, int64_t* out)
{
    int64_t obj = o_obj_of_type(OBJ_TYPE_PC);

    obj_field_int32_set(obj, OBJ_F_PC_FLAGS_FATE, (int)a[0]);
    // The fate index is bounded by the case's domain, not here: it indexes
    // `fate_flags[FATE_COUNT]` (0x5BC48C in the original, `and eax,[edx*4+
    // 0x5BC48C]` at 0x4F50AB) with no check, so anything past the end
    // compares whatever sits beside a table in two different binaries.
    out[0] = fate_is_activated(obj, (int)a[1]);
}

static void o_auto_level_scheme_get(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj;

    if (a[2] != 0) {
        out[0] = auto_level_scheme_get(OBJ_HANDLE_NULL);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_critter(type)) {
        obj_field_int32_set(obj, OBJ_F_CRITTER_AUTO_LEVEL_SCHEME, (int)a[1]);
    }

    out[0] = auto_level_scheme_get(obj);
}

// `gsound_size` (0x41B980). Three flag tests in priority order behind a
// SCENERY type check, answering a `TigSoundPositionalSize`. The order is the
// point: SMALL wins over MEDIUM wins over EXTRA_LARGE, and everything else --
// including a non-scenery -- is LARGE. A case that only ever set one bit at a
// time could not see a reordering, so the domain sets combinations.
//
// NO NULL: 0x41B98B pushes OBJ_F_TYPE with nothing in front of it.
static void o_gsound_size(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (type == OBJ_TYPE_SCENERY) {
        obj_field_int32_set(obj, OBJ_F_SCENERY_FLAGS, (int)a[1]);
    }

    out[0] = gsound_size(obj);
}

// `sfx_container_sound` (0x4F0FD0) and `sfx_portal_sound` (0x4F1010). The same
// shape against a different type constant -- CONTAINER 2 and PORTAL 1 -- and
// two cases for it, because a transcription that used the wrong one of the
// two would still answer -1 for every refusal and the stored sum otherwise.
// OBJ_F_SOUND_EFFECT is ordinal 33, below OBJ_F_END, so every type owns it and
// the set below is unconditional.
static void o_sfx_sound(const int64_t* a, int64_t* out, int (*fn)(int64_t, int))
{
    int type = (int)a[0];
    int64_t obj;

    if (a[3] != 0) {
        out[0] = fn(OBJ_HANDLE_NULL, (int)a[2]);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    obj_field_int32_set(obj, OBJ_F_SOUND_EFFECT, (int)a[1]);

    out[0] = fn(obj, (int)a[2]);
}

static void o_sfx_container_sound(const int64_t* a, int64_t* out)
{
    o_sfx_sound(a, out, (int (*)(int64_t, int))sfx_container_sound);
}

static void o_sfx_portal_sound(const int64_t* a, int64_t* out)
{
    o_sfx_sound(a, out, (int (*)(int64_t, int))sfx_portal_sound);
}

// `mt_item_spell` (0x4CB7A0). Reads `OBJ_F_ITEM_SPELL_1 + slot`, so the slot
// is an unchecked index into five consecutive fields (100..104, with
// MANA_STORE at 105 immediately after). The domain stops at 4 for the reason
// `basic_skill_stat`'s stops at 11: one past the end reads a different field,
// which is noise rather than a comparison.
static void o_mt_item_spell(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int slot = (int)a[1];
    int64_t obj;

    if (a[3] != 0) {
        out[0] = mt_item_spell(OBJ_HANDLE_NULL, slot);
        return;
    }

    obj = o_obj_of_type(type);
    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_SPELL_1 + slot, (int)a[2]);
    }

    out[0] = mt_item_spell(obj, slot);
}

// `mt_item_valid_ai_action` (0x4CC160) is a PARTIAL case, and the domain says
// so: its third branch reads `magictech_spells[spell].item_triggers`, and
// `magictech_spells` is a malloc'd pointer that is NULL in both binaries until
// `magictech_init` runs -- which nothing here does. So the spell field is
// pinned to the 10000 sentinel, which is the value that answers `true` before
// the dereference; what is covered is the mana-store refusal and the sentinel
// arm, two distinguishable answers.
//
// Worth keeping: the original reads OBJ_F_TYPE at 0x4CC16A and throws the
// result away. CE carries that as a NOTE and does the same, so the case
// exercises a read whose only purpose is to be faithful.
//
// NO NULL: that discarded read is unguarded.
static void o_mt_item_valid_ai_action(const int64_t* a, int64_t* out)
{
    int type = (int)a[0];
    int64_t obj = o_obj_of_type(type);

    if (obj == OBJ_HANDLE_NULL) {
        out[0] = -1;
        return;
    }

    if (obj_type_is_item(type)) {
        obj_field_int32_set(obj, OBJ_F_ITEM_SPELL_MANA_STORE, (int)a[1]);
        obj_field_int32_set(obj, OBJ_F_ITEM_SPELL_1, 10000);
        obj_field_int32_set(obj, OBJ_F_ITEM_FLAGS, (int)a[2]);
    }

    out[0] = mt_item_valid_ai_action(obj);
}

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

/* Declared `static` in art.c and reached via the globalized object built in
 * tests/CMakeLists.txt - see the comment there. Prototypes here rather than a
 * header, because art.c deliberately does not export them. */
/* `TigArtHeader` is private to art.c, so this file cannot name the type.
 * Declaring the parameter as `void*` is ABI-identical for a pointer and
 * avoids re-declaring a layout here that would silently drift from the real
 * one. The buffer's first four bytes are `flags`, which is not an assumption:
 * `scripts/art_decode.py` derived flags-at-offset-0 independently from 15,638
 * shipped `.art` files before this case existed. */
int tig_art_header_num_rotations(void* hdr);

static void o_tig_art_header_num_rotations(const int64_t* a, int64_t* out)
{
    unsigned char hdr[0x84];

    memset(hdr, 0, sizeof(hdr));
    {
        unsigned int flags = (unsigned int)a[0];
        memcpy(hdr, &flags, sizeof(flags));
    }
    out[0] = (uint32_t)tig_art_header_num_rotations(hdr);
}

int tig_art_blt_flags_convert(unsigned int art_blt_flags, unsigned int* vb_blt_flags_ptr);
int tig_art_build_path(unsigned int art_id, char* path, size_t maxlen);

static void o_tig_art_blt_flags_convert(const int64_t* a, int64_t* out)
{
    unsigned int vb_flags = (unsigned int)a[1];

    out[0] = (uint32_t)tig_art_blt_flags_convert((unsigned int)a[0], &vb_flags);
    out[1] = (int64_t)(uint64_t)vb_flags;
}

static void o_tig_art_build_path(const int64_t* a, int64_t* out)
{
    char path[260];
    int rc;

    /* a[1] is the harness's scratch buffer, which it fills with the drawn
     * value before the call. Mirror that here rather than assuming zero, so
     * both sides start from the same buffer state - otherwise a function that
     * writes nothing would be compared as "CE's zeros vs the emulator's
     * fill". The case draws 0, so in practice this is a memset to 0. */
    memset(path, (int)(a[1] & 0xFF), sizeof(path));
    rc = tig_art_build_path((unsigned int)a[0], path, sizeof(path));
    out[0] = (uint32_t)rc;

    /* The path itself as a checksum: the emulator side cannot hand back a
     * string, and a 32-bit digest distinguishes every path this can build.
     * FNV-1a, chosen because the emulator half can compute it identically
     * over the bytes it reads out of guest memory. */
    {
        uint32_t h = 2166136261u;
        size_t i;
        for (i = 0; i < sizeof(path) && path[i] != '\0'; i++) {
            h ^= (unsigned char)path[i];
            h *= 16777619u;
        }
        out[1] = (int64_t)(uint64_t)h;
    }
}

/* --- the art blit worker (roadmap P5.1 step 2) --------------------------
 *
 * `tig_art_cache_entry_blit` is the first case here that is NOT pure: it
 * reads `tig_art_cache_entries` and writes into a locked video buffer. The
 * emulator half reaches it by stubbing the three `tig_video_buffer_*` calls
 * (`peemu.stub`); this half builds the same fixture in CE's own types.
 *
 * WHY THE STRUCTS ARE REPLICATED HERE. They live in `art.c` and `video.c`
 * with no header, and moving them would be a source change to ported code.
 * Copying them is safe *in this direction* for a reason worth stating: the
 * emulator side takes its offsets from `Arcanum.exe` (0x505A6E, 0x505A75,
 * 0x505AA3), so the two derivations are independent. If this copy were wrong
 * the two blits would DISAGREE - a false positive that gets investigated -
 * rather than agreeing on a shared misunderstanding, which is the failure
 * mode the project has a rule about. The offsets cannot be shared anyway: CE
 * is 64-bit, so its cache entry is 0x378 bytes where the original's is 0x26C.
 *
 * WHAT IS COMPARED, AND WHAT IS NOT. The original branches on
 * `tig_art_bits_per_pixel` (0x60474C) into 8-, 16-, 24- and 32-bpp loops; CE
 * implements only the 32-bpp one, because its surfaces are always 32-bit. The
 * emulator side sets that global to 32 so the comparable path runs. The other
 * three paths have no CE counterpart and are NOT covered by this case.
 *
 * The original's 32-bpp loop also runs each palette entry through
 * `tig_color`'s per-channel conversion tables, where CE writes
 * `plt->colors[i]` straight. Those tables are built by the ORIGINAL's own
 * init (0x52C070 / 0x52C160) under emulation rather than synthesised, and for
 * an 8/8/8 format the original computes them as the identity map - which is
 * why the two agree. The constant it ORs in (0x739EE0) is the alpha mask
 * passed to 0x52C160.
 */
#define ART_MAX_PALETTES 4
#define ART_MAX_ROTATIONS 8
#define ART_MAX_FRAMES 32
#define ART_TIG_MAX_PATH 260
#define BLIT_MAX_DIM 16
#define BLIT_MAX_PAD 8

typedef struct OracleArtFrameData {
    int width;
    int height;
    int data_size;
    int hot_x;
    int hot_y;
    int offset_x;
    int offset_y;
} OracleArtFrameData;

typedef struct OracleArtHeader {
    unsigned int flags;
    int fps;
    int bpp;
    TigPalette* palette_tbl[ART_MAX_PALETTES];
    int action_frame;
    int num_frames;
    OracleArtFrameData* frames_tbl[ART_MAX_ROTATIONS];
    int data_size[ART_MAX_ROTATIONS];
    uint8_t* pixels_tbl[ART_MAX_ROTATIONS];
} OracleArtHeader;

typedef struct OracleArtCacheEntry {
    unsigned int flags;
    char path[ART_TIG_MAX_PATH];
    tig_timestamp_t time;
    unsigned int art_id;
    OracleArtHeader hdr;
    uint8_t dirty[ART_MAX_PALETTES][ART_MAX_ROTATIONS];
    TigVideoBuffer** video_buffers[ART_MAX_PALETTES][ART_MAX_ROTATIONS];
    uint8_t** pixels_tbl[ART_MAX_ROTATIONS];
    TigPalette* palette_tbl[ART_MAX_PALETTES];
    int system_memory_usage;
    int video_memory_usage;
} OracleArtCacheEntry;

/* Replicated from video.c for the same reason, and only as far as the blit
 * path reaches: lock() touches surface and lock_count, data() reads
 * frame.width/height, flags, surface->pitch and surface->pixels. */
typedef struct OracleVideoBuffer {
    unsigned int flags;
    TigRect frame;
    int texture_width;
    int texture_height;
    unsigned int background_color;
    unsigned int color_key;
    SDL_Surface* surface;
    int lock_count;
} OracleVideoBuffer;

/* Globalized out of art.c by tests/CMakeLists.txt - no source change. */
extern int tig_art_cache_entry_blit(int cache_entry_index, TigArtBlitInfo* blit_info);
extern void* tig_art_cache_entries;

/* The fixture content. Deliberately closed-form rather than a PRNG: the
 * emulator half must reproduce these exactly in Python, and a one-line
 * formula is far harder to get subtly out of step than a generator. Index 0
 * recurs on purpose - it is the transparent case. */
static unsigned char blit_pixel_at(int seed, int rot, int frame, int i)
{
    return (unsigned char)((seed + rot * 37 + frame * 11 + i * 7) % 256);
}

static uint32_t blit_palette_at(int p, int i)
{
    return (uint32_t)((((i * 3 + p) & 0xFF) << 16)
        | (((i * 5 + p) & 0xFF) << 8)
        | ((i * 7 + p) & 0xFF));
}

static void o_tig_art_cache_entry_blit(const int64_t* a, int64_t* out)
{
    static OracleArtCacheEntry entry;
    static OracleArtFrameData frames[ART_MAX_ROTATIONS][ART_MAX_FRAMES];
    static uint8_t* pixel_ptrs[ART_MAX_ROTATIONS][ART_MAX_FRAMES];
    static uint8_t pixels[ART_MAX_ROTATIONS][ART_MAX_FRAMES][BLIT_MAX_DIM * BLIT_MAX_DIM];
    static TigPalette palettes[ART_MAX_PALETTES];
    static uint8_t dst[(BLIT_MAX_DIM + BLIT_MAX_PAD) * BLIT_MAX_DIM * 4];

    tig_art_id_t art_id = (tig_art_id_t)a[0];
    int width = (int)a[1];
    int height = (int)a[2];
    int pitch = (int)(a[1] + a[3]) * 4;
    int seed = (int)a[4];
    size_t dst_size = (size_t)pitch * (size_t)height;

    TigArtBlitInfo blit_info;
    OracleVideoBuffer video_buffer;
    SDL_Surface* surface;
    uint32_t h;
    size_t i;
    int rot;
    int frame;
    int p;
    int n;

    memset(&entry, 0, sizeof(entry));
    for (rot = 0; rot < ART_MAX_ROTATIONS; rot++) {
        for (frame = 0; frame < ART_MAX_FRAMES; frame++) {
            frames[rot][frame].width = width;
            frames[rot][frame].height = height;
            frames[rot][frame].data_size = width * height;
            for (n = 0; n < width * height; n++) {
                pixels[rot][frame][n] = blit_pixel_at(seed, rot, frame, n);
            }
            pixel_ptrs[rot][frame] = pixels[rot][frame];
        }
        entry.hdr.frames_tbl[rot] = frames[rot];
        entry.pixels_tbl[rot] = pixel_ptrs[rot];
    }
    for (p = 0; p < ART_MAX_PALETTES; p++) {
        for (n = 0; n < 256; n++) {
            palettes[p].colors[n] = blit_palette_at(p, n);
        }
        /* Slot 0 is always present; the rest follow the seed, so the
         * NULL-falls-back-to-palette-0 branch is exercised on both sides. */
        entry.hdr.palette_tbl[p] = (p == 0 || ((seed >> p) & 1)) ? &palettes[p] : NULL;
    }
    tig_art_cache_entries = &entry;

    /* SDL_CreateSurfaceFrom needs no video subsystem, and it lets the pitch be
     * chosen here - which is the point, since the loop's row stepping is
     * `pitch / 4 - width` and a pitch SDL picked for itself would not vary.
     * 0xA5 fill so that a blit writing outside its rows is caught by the hash
     * rather than landing on zeros that look intentional. */
    memset(dst, 0xA5, sizeof(dst));
    surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_XRGB8888, dst, pitch);
    if (surface == NULL) {
        out[0] = -1;
        out[1] = 0;
        return;
    }

    memset(&video_buffer, 0, sizeof(video_buffer));
    video_buffer.frame.width = width;
    video_buffer.frame.height = height;
    video_buffer.surface = surface;

    memset(&blit_info, 0, sizeof(blit_info));
    blit_info.art_id = art_id;
    blit_info.dst_video_buffer = (TigVideoBuffer*)&video_buffer;

    out[0] = (uint32_t)tig_art_cache_entry_blit(0, &blit_info);

    /* FNV-1a over the destination, same as tig_art_build_path's digest and for
     * the same reason: the emulator half cannot hand back a buffer, and it can
     * compute this identically over the bytes it reads out of guest memory.
     * The whole buffer is hashed, not just the blitted rows, so an overrun
     * shows up too. */
    h = 2166136261u;
    for (i = 0; i < dst_size; i++) {
        h ^= dst[i];
        h *= 16777619u;
    }
    out[1] = (int64_t)(uint64_t)h;

    SDL_DestroySurface(surface);
}

/* --- the sprite blitter (FC1 phase 5c) -----------------------------------
 *
 * `art_blit` (0x505EB0), CE's renderer for every sprite. The fixture is the
 * worker's above, plus what the blitter takes that the worker does not: the
 * flags, a source and a destination rect, the colour, the alpha bytes and
 * the per-column colour array, into a fixed 20x20 destination whose X byte is
 * zero. The emulator half's comment lists the offsets it read out of
 * Arcanum.exe for these. */
#define ARTBLIT_BUF_W 20
#define ARTBLIT_BUF_H 20
#define ARTBLIT_PAD 2
#define ARTBLIT_COLORS 32
#define ARTBLIT_GUARD 1024

extern int art_blit(int cache_entry_index, TigArtBlitInfo* blit_info);

static uint32_t artblit_dst[(ARTBLIT_BUF_W + ARTBLIT_PAD) * ARTBLIT_BUF_H];
extern int tig_art_bits_per_pixel;

static uint32_t artblit_dst_at(int seed, int i)
{
    return (uint32_t)((((uint64_t)(uint32_t)seed * 2654435761u + (uint64_t)i * 40503u) >> 3) & 0x00FFFFFF);
}

static uint32_t artblit_color_at(int seed, int i)
{
    return (uint32_t)((((uint64_t)(uint32_t)seed * 7919u + (uint64_t)i * 104729u) >> 2) & 0x00FFFFFF);
}

static void o_art_blit(const int64_t* a, int64_t* out)
{
    static OracleArtCacheEntry entry;
    static OracleArtFrameData frames[ART_MAX_ROTATIONS][ART_MAX_FRAMES];
    static uint8_t* pixel_ptrs[ART_MAX_ROTATIONS][ART_MAX_FRAMES];
    /* Each frame inside ARTBLIT_GUARD zero bytes, as the emulator half. */
    static uint8_t pixels[ART_MAX_ROTATIONS][ART_MAX_FRAMES][2 * ARTBLIT_GUARD + BLIT_MAX_DIM * BLIT_MAX_DIM];
    static TigPalette palettes[ART_MAX_PALETTES];
    uint32_t* dst = artblit_dst;
    static uint32_t colors[ARTBLIT_COLORS];
    static bool color_ready = false;

    tig_art_id_t art_id = (tig_art_id_t)a[0];
    int width = (int)a[1];
    int height = (int)a[2];
    int seed = (int)a[3];
    TigRect src_rect = { (int)a[5], (int)a[6], (int)a[7], (int)a[8] };
    TigRect dst_rect = { (int)a[9], (int)a[10], (int)a[11], (int)a[12] };
    TigRect lerp_rect;
    int pitch = (ARTBLIT_BUF_W + ARTBLIT_PAD) * 4;
    uint32_t alphas = (uint32_t)a[14];

    TigArtBlitInfo blit_info;
    OracleVideoBuffer video_buffer;
    SDL_Surface* surface;
    uint32_t h;
    const uint8_t* bytes;
    size_t i;
    int rot;
    int frame;
    int p;
    int n;

    /* Stretched COLOR_LERP is a recorded difference; see the emulator half. */
    if ((a[4] & 0x20000) != 0) {
        dst_rect.width = src_rect.width;
        dst_rect.height = src_rect.height;
    }
    lerp_rect = dst_rect;

    if (!color_ready) {
        tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0);
        color_ready = true;
    }
    tig_art_bits_per_pixel = 32;

    memset(&entry, 0, sizeof(entry));
    for (rot = 0; rot < ART_MAX_ROTATIONS; rot++) {
        for (frame = 0; frame < ART_MAX_FRAMES; frame++) {
            frames[rot][frame].width = width;
            frames[rot][frame].height = height;
            frames[rot][frame].data_size = width * height;
            memset(pixels[rot][frame], 0, sizeof(pixels[rot][frame]));
            for (n = 0; n < width * height; n++) {
                pixels[rot][frame][ARTBLIT_GUARD + n] = blit_pixel_at(seed, rot, frame, n);
            }
            pixel_ptrs[rot][frame] = &(pixels[rot][frame][ARTBLIT_GUARD]);
        }
        entry.hdr.frames_tbl[rot] = frames[rot];
        entry.pixels_tbl[rot] = pixel_ptrs[rot];
    }
    for (p = 0; p < ART_MAX_PALETTES; p++) {
        for (n = 0; n < 256; n++) {
            palettes[p].colors[n] = blit_palette_at(p, n);
        }
        entry.hdr.palette_tbl[p] = (p == 0 || ((seed >> p) & 1)) ? &palettes[p] : NULL;
    }
    tig_art_cache_entries = &entry;

    for (n = 0; n < (ARTBLIT_BUF_W + ARTBLIT_PAD) * ARTBLIT_BUF_H; n++) {
        dst[n] = artblit_dst_at(seed, n);
    }
    for (n = 0; n < ARTBLIT_COLORS; n++) {
        colors[n] = artblit_color_at(seed, n);
    }

    surface = SDL_CreateSurfaceFrom(ARTBLIT_BUF_W, ARTBLIT_BUF_H, SDL_PIXELFORMAT_XRGB8888, dst, pitch);
    if (surface == NULL) {
        out[0] = -1;
        out[1] = 0;
        return;
    }

    memset(&video_buffer, 0, sizeof(video_buffer));
    video_buffer.frame.width = ARTBLIT_BUF_W;
    video_buffer.frame.height = ARTBLIT_BUF_H;
    video_buffer.surface = surface;

    memset(&blit_info, 0, sizeof(blit_info));
    blit_info.flags = (TigArtBlitFlags)a[4];
    blit_info.art_id = art_id;
    blit_info.src_rect = &src_rect;
    blit_info.color = (tig_color_t)a[13];
    blit_info.field_14 = colors;
    blit_info.lerp_rect = &lerp_rect;
    memcpy(blit_info.alpha, &alphas, sizeof(blit_info.alpha));
    blit_info.dst_video_buffer = (TigVideoBuffer*)&video_buffer;
    blit_info.dst_rect = &dst_rect;

    out[0] = (uint32_t)art_blit(0, &blit_info);

    h = 2166136261u;
    bytes = (const uint8_t*)dst;
    for (i = 0; i < sizeof(artblit_dst); i++) {
        h ^= bytes[i];
        h *= 16777619u;
    }
    out[1] = (int64_t)(uint64_t)h;

    SDL_DestroySurface(surface);
}

static void o_tig_color_rgb_to_grayscale(const int64_t* a, int64_t* out)
{
    tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    out[0] = (uint32_t)tig_color_rgb_to_grayscale((tig_color_t)a[0]);
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

static void o_tig_art_tile_id_corners_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_tile_id_corners_get((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_corners_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_tile_id_corners_set((tig_art_id_t)a[0], (int)a[1]);
}

static void o_tig_art_tile_id_variant_get(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_tile_id_variant_get((tig_art_id_t)a[0]);
}

static void o_tig_art_tile_id_variant_set(const int64_t* a, int64_t* out)
{
    out[0] = (uint32_t)tig_art_tile_id_variant_set((tig_art_id_t)a[0], (int)a[1]);
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
    { "critter_is_monstrous", 3, 1, o_critter_is_monstrous },
    { "combat_critter_is_combat_mode_active", 2, 1, o_combat_critter_is_combat_mode_active },
    { "combat_weapon_loudness", 3, 1, o_combat_weapon_loudness },
    { "combat_turn_based_critter_is_dontdraw", 3, 1, o_combat_turn_based_critter_is_dontdraw },
    { "item_worth", 5, 1, o_item_worth },
    { "item_is_worthless", 2, 1, o_item_is_worthless },
    { "item_is_item", 2, 1, o_item_is_item },
    { "item_magic_tech_complexity", 2, 1, o_item_magic_tech_complexity },
    { "item_inventory_source", 4, 1, o_item_inventory_source },
    { "item_is_identified", 4, 1, o_item_is_identified },
    { "item_weapon_ammo_type", 3, 1, o_item_weapon_ammo_type },
    { "item_armor_coverage", 3, 1, o_item_armor_coverage },
    { "item_location_get", 3, 1, o_item_location_get },
    { "item_weapon_skill", 6, 1, o_item_weapon_skill },
    { "object_hp_pts_get", 2, 1, o_object_hp_pts_get },
    { "object_hp_adj_get", 2, 1, o_object_hp_adj_get },
    { "object_hp_damage_get", 2, 1, o_object_hp_damage_get },
    { "object_hp_from_pts", 2, 1, o_object_hp_from_pts },
    { "object_is_static_type", 1, 1, o_object_is_static_type },
    { "object_is_static", 2, 1, o_object_is_static },
    { "magictech_invocation_parent_get", 4, 1, o_magictech_invocation_parent_get },
    { "magictech_disallowed_flags_check", 10, 1, o_magictech_disallowed_flags_check },
    { "magictech_find_effect_run", 5, 2, o_magictech_find_effect_run },
    { "magictech_invisibility_target_check", 6, 1, o_magictech_invisibility_target_check },
    { "player_is_pc_obj", 2, 1, o_player_is_pc_obj },
    { "ai_max_dialog_distance", 2, 1, o_ai_max_dialog_distance },
    { "anim_goal_type_is_concurrent", 1, 1, o_anim_goal_type_is_concurrent },
    { "trap_is_trap_device", 3, 1, o_trap_is_trap_device },
    { "background_get", 3, 1, o_background_get },
    { "background_text_get", 1, 1, o_background_text_get },
    { "fate_is_activated", 2, 1, o_fate_is_activated },
    { "auto_level_scheme_get", 3, 1, o_auto_level_scheme_get },
    { "gsound_size", 2, 1, o_gsound_size },
    { "sfx_container_sound", 4, 1, o_sfx_container_sound },
    { "sfx_portal_sound", 4, 1, o_sfx_portal_sound },
    { "mt_item_spell", 4, 1, o_mt_item_spell },
    { "mt_item_valid_ai_action", 3, 1, o_mt_item_valid_ai_action },
    { "critter_social_class_get", 2, 1, o_critter_social_class_get },
    { "critter_faction_get", 2, 1, o_critter_faction_get },
    { "critter_origin_get", 2, 1, o_critter_origin_get },
    { "critter_is_pc", 1, 1, o_critter_is_pc },
    { "critter_origin_same", 4, 1, o_critter_origin_same },
    { "critter_fatigue_pts_get", 2, 1, o_critter_fatigue_pts_get },
    { "critter_fatigue_adj_get", 2, 1, o_critter_fatigue_adj_get },
    { "critter_fatigue_damage_get", 2, 1, o_critter_fatigue_damage_get },
    { "critter_is_sleeping", 3, 1, o_critter_is_sleeping },
    { "critter_is_concealed", 3, 1, o_critter_is_concealed },
    { "critter_has_dark_sight", 4, 1, o_critter_has_dark_sight },
    { "target_obj_resolve", 2, 2, o_target_obj_resolve },
    { "target_context_init", 2, 20, o_target_context_init },
    { "anim_is_portal", 3, 1, o_anim_is_portal },
    { "mt_ai_item_allowed_for_ai", 3, 1, o_mt_ai_item_allowed_for_ai },
    { "target_list_add_obj_unless_off", 6, 3, o_target_list_add_obj_unless_off },
    { "target_list_add_obj_unless_destroyed", 5, 3, o_target_list_add_obj_unless_destroyed },
    { "reaction_get_base", 1, 1, o_reaction_get_base },
    { "skill_ui_is_pickpocket_immune", 3, 1, o_skill_ui_is_pickpocket_immune },
    { "portal_is_open", 2, 1, o_portal_is_open },
    { "critter_is_prone", 3, 1, o_critter_is_prone },
    { "critter_can_jump_window", 2, 1, o_critter_can_jump_window },
    { "critter_light_get", 2, 2, o_critter_light_get },
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
    { "tig_art_header_num_rotations", 1, 1, o_tig_art_header_num_rotations },
    { "tig_art_blt_flags_convert", 2, 2, o_tig_art_blt_flags_convert },
    { "tig_art_build_path", 2, 2, o_tig_art_build_path },
    { "tig_art_cache_entry_blit", 5, 2, o_tig_art_cache_entry_blit },
    { "tig_color_rgb_to_grayscale", 1, 1, o_tig_color_rgb_to_grayscale },
    { "art_blit", 15, 2, o_art_blit },
    { "sub_502830", 1, 1, o_sub_502830 },
    { "tig_art_tile_id_num1_get", 1, 1, o_tig_art_tile_id_num1_get },
    { "tig_art_tile_id_num2_get", 1, 1, o_tig_art_tile_id_num2_get },
    { "tig_art_tile_id_corners_get", 1, 1, o_tig_art_tile_id_corners_get },
    { "tig_art_tile_id_corners_set", 2, 1, o_tig_art_tile_id_corners_set },
    { "tig_art_tile_id_variant_get", 1, 1, o_tig_art_tile_id_variant_get },
    { "tig_art_tile_id_variant_set", 2, 1, o_tig_art_tile_id_variant_set },
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
