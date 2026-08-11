// Tests for `game/location.c`.
//
// A location is one packed `int64_t`: x in the low 32 bits, y in the high 32.
// Every position in the game - critters, items, scenery, the scroll origin,
// pathfinding, line of sight - is one of these, so the packing and the tile
// arithmetic are load-bearing for everything that has a place in the world.
//
// **Scope.** The screen-facing half of this file (`location_xy`, `location_at`,
// the origin and scrolling, `location_screen_rect_to_loc_rect`) depends on view
// state that `location_init` builds from a real window, so it is out of reach
// here. What is reachable is the packing, the tile-grid arithmetic and the map
// limits - which is the part everything else is built on:
//
//   - `location_make` / `location_get_x` / `location_get_y`;
//   - `location_in_dir` and `location_in_range`, stepping across the grid;
//   - `location_rot`, the direction from one tile to another;
//   - `location_dist`, and `location_limits_set`/`get`.
//
// The strongest test here is a round trip rather than a table: `location_rot`
// and `location_in_dir` are supposed to be inverses, so stepping one tile in
// direction d and asking which direction that was must give back d, for all
// eight. Neither function mentions the other, so nothing forces them to agree
// except that the game would misbehave if they did not.

#include "game/location.h"

#include "test_harness.h"

// Comfortably larger than anything the tests step to, and small enough that the
// bounds tests can reach an edge.
#define LIMIT_X 200
#define LIMIT_Y 200

// The eight directions, as `location_in_dir` defines them.
static const int DIR_DX[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
static const int DIR_DY[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };

static void use_test_limits(void)
{
    location_limits_set(LIMIT_X, LIMIT_Y);
}

TEST(x_and_y_survive_a_round_trip_through_the_packing)
{
    int64_t loc = location_make(1234, 5678);

    CHECK_INT(location_get_x(loc), 1234);
    CHECK_INT(location_get_y(loc), 5678);

    // Zero in either half must not disturb the other.
    loc = location_make(0, 99);
    CHECK_INT(location_get_x(loc), 0);
    CHECK_INT(location_get_y(loc), 99);

    loc = location_make(99, 0);
    CHECK_INT(location_get_x(loc), 99);
    CHECK_INT(location_get_y(loc), 0);
}

TEST(the_two_halves_do_not_bleed_into_each_other)
{
    // x fills its 32 bits without reaching y, and vice versa. This is the
    // packing's one real failure mode.
    int64_t loc = location_make(0xFFFFFFFFLL, 0);

    CHECK_INT(location_get_x(loc), 0xFFFFFFFFLL);
    CHECK_INT(location_get_y(loc), 0);

    loc = location_make(0, 0xFFFFFFFFLL);
    CHECK_INT(location_get_x(loc), 0);
    CHECK_INT(location_get_y(loc), 0xFFFFFFFFLL);
}

TEST(a_y_in_the_top_bit_is_packed_without_undefined_behaviour)
{
    // `location_make` shifts through `uint64_t` on purpose: a y of 0x80000000
    // or more lands in bit 63, and shifting into the sign bit of a signed type
    // is UB. The header says so; this is the case that proves it, and it is
    // why the suite is worth running under the sanitizer preset.
    //
    // NOTE: The `LOCATION_MAKE` macro in location.h still does the plain signed
    // shift and is *not* equivalent for this input. Both are in use. Only the
    // inline is exercised here, because the macro would abort UBSan.
    int64_t loc = location_make(7, 0x80000000LL);

    CHECK_INT(location_get_x(loc), 7);
    CHECK_INT(location_get_y(loc), 0x80000000LL);
}

TEST(limits_are_stored_and_returned)
{
    int64_t x = 0;
    int64_t y = 0;

    CHECK(location_limits_set(640, 480));
    location_limits_get(&x, &y);
    CHECK_INT(x, 640);
    CHECK_INT(y, 480);

    use_test_limits();
}

TEST(limits_beyond_a_32_bit_grid_are_rejected)
{
    // The guard is `> 0x100000000`, so exactly 2^32 is accepted and anything
    // past it is refused - the packing has no room for a coordinate wider than
    // its half.
    int64_t x = 0;
    int64_t y = 0;

    CHECK(location_limits_set(0x100000000LL, 0x100000000LL));
    CHECK(!location_limits_set(0x100000001LL, 100));
    CHECK(!location_limits_set(100, 0x100000001LL));

    // A rejected call leaves the previous limits in place rather than half
    // applying - `location_limits_set` returns before writing anything.
    location_limits_get(&x, &y);
    CHECK_INT(x, 0x100000000LL);
    CHECK_INT(y, 0x100000000LL);

    use_test_limits();
}

TEST(the_centre_is_half_of_each_limit)
{
    use_test_limits();

    CHECK_INT(location_get_x(location_center_get()), LIMIT_X / 2);
    CHECK_INT(location_get_y(location_center_get()), LIMIT_Y / 2);
}

TEST(stepping_in_each_direction_moves_one_tile)
{
    int64_t from;
    int64_t to;
    int dir;

    use_test_limits();
    from = location_make(100, 100);

    for (dir = 0; dir < 8; dir++) {
        CHECK(location_in_dir(from, dir, &to));
        CHECK_INT(location_get_x(to), 100 + DIR_DX[dir]);
        CHECK_INT(location_get_y(to), 100 + DIR_DY[dir]);
    }
}

TEST(rot_is_the_inverse_of_stepping)
{
    // The round trip. Nothing in either function references the other, so this
    // agreement is a property of the two tables lining up - exactly the sort of
    // thing a transcription can get subtly wrong and nothing else would catch.
    int64_t from;
    int64_t to;
    int dir;
    int wrong = 0;

    use_test_limits();
    from = location_make(100, 100);

    for (dir = 0; dir < 8; dir++) {
        CHECK(location_in_dir(from, dir, &to));
        if (location_rot(from, to) != dir) {
            wrong++;
            CHECK_INT(location_rot(from, to), dir);
        }
    }

    CHECK_INT(wrong, 0);
}

TEST(rot_holds_over_long_distances_not_just_adjacent_tiles)
{
    // Stepping ten tiles must read as the same direction as stepping one,
    // otherwise the tie-breaking in `location_rot` is skewed.
    int64_t from;
    int64_t to;
    int dir;
    int wrong = 0;

    use_test_limits();
    from = location_make(100, 100);

    for (dir = 0; dir < 8; dir++) {
        CHECK(location_in_range(from, dir, 10, &to));
        if (location_rot(from, to) != dir) {
            wrong++;
            CHECK_INT(location_rot(from, to), dir);
        }
    }

    CHECK_INT(wrong, 0);
}

TEST(rot_of_a_location_with_itself_is_east)
{
    // A quirk, pinned rather than fixed. With no displacement every sign is
    // zero and the branch chain lands on 5 - due east - rather than reporting
    // "no direction". Callers asking a critter which way to face while standing
    // on its target get east, and that is what the original does too.
    int64_t loc;

    use_test_limits();
    loc = location_make(50, 50);

    CHECK_INT(location_rot(loc, loc), 5);
}

TEST(stepping_off_the_map_fails_and_leaves_the_edge_intact)
{
    int64_t to;

    use_test_limits();

    // West from x=0.
    CHECK(!location_in_dir(location_make(0, 50), 1, &to));
    // North from y=0.
    CHECK(!location_in_dir(location_make(50, 0), 7, &to));
    // East from the last column, and south from the last row - the limit is
    // exclusive, so LIMIT-1 is the last valid tile.
    CHECK(!location_in_dir(location_make(LIMIT_X - 1, 50), 5, &to));
    CHECK(!location_in_dir(location_make(50, LIMIT_Y - 1), 3, &to));

    // And the tile just inside each edge still steps.
    CHECK(location_in_dir(location_make(1, 50), 1, &to));
    CHECK_INT(location_get_x(to), 0);
    CHECK(location_in_dir(location_make(LIMIT_X - 2, 50), 5, &to));
    CHECK_INT(location_get_x(to), LIMIT_X - 1);
}

TEST(in_range_walks_the_whole_way_or_fails)
{
    int64_t from;
    int64_t to;

    use_test_limits();

    from = location_make(100, 100);
    CHECK(location_in_range(from, 5, 7, &to));
    CHECK_INT(location_get_x(to), 107);
    CHECK_INT(location_get_y(to), 100);

    // A range of zero is the starting tile.
    CHECK(location_in_range(from, 5, 0, &to));
    CHECK_INT(location_get_x(to), 100);
    CHECK_INT(location_get_y(to), 100);

    // Walking off the edge partway through fails rather than clamping. `to` is
    // left holding the last tile reached, which is why the caller must check
    // the return value before using it.
    from = location_make(LIMIT_X - 3, 100);
    CHECK(!location_in_range(from, 5, 10, &to));
}

TEST(distance_is_the_longer_of_the_two_axes)
{
    // Chebyshev, not Euclidean: a diagonal tile is one step away, same as an
    // orthogonal one, which is what makes the eight-way grid consistent.
    use_test_limits();

    CHECK_INT(location_dist(location_make(10, 10), location_make(10, 10)), 0);
    CHECK_INT(location_dist(location_make(10, 10), location_make(13, 10)), 3);
    CHECK_INT(location_dist(location_make(10, 10), location_make(10, 13)), 3);
    // Pure diagonal: three tiles, not six and not 4.24.
    CHECK_INT(location_dist(location_make(10, 10), location_make(13, 13)), 3);
    // Mixed: the longer axis wins.
    CHECK_INT(location_dist(location_make(10, 10), location_make(13, 17)), 7);
}

TEST(distance_is_symmetric)
{
    int64_t a;
    int64_t b;
    int i;
    int wrong = 0;

    use_test_limits();

    for (i = 0; i < 50; i++) {
        a = location_make(10 + i, 100 - i);
        b = location_make(60, 60);
        if (location_dist(a, b) != location_dist(b, a)) {
            wrong++;
        }
    }

    CHECK_INT(wrong, 0);
}

TEST(one_step_in_any_direction_is_always_distance_one)
{
    // Ties the two together: every direction `location_in_dir` offers must be
    // exactly one tile away by `location_dist`. If a direction table entry ever
    // gained a two-tile step, this is what would catch it.
    int64_t from;
    int64_t to;
    int dir;
    int wrong = 0;

    use_test_limits();
    from = location_make(100, 100);

    for (dir = 0; dir < 8; dir++) {
        CHECK(location_in_dir(from, dir, &to));
        if (location_dist(from, to) != 1) {
            wrong++;
        }
    }

    CHECK_INT(wrong, 0);
}

void test_location_suite(void)
{
    RUN(x_and_y_survive_a_round_trip_through_the_packing);
    RUN(the_two_halves_do_not_bleed_into_each_other);
    RUN(a_y_in_the_top_bit_is_packed_without_undefined_behaviour);

    RUN(limits_are_stored_and_returned);
    RUN(limits_beyond_a_32_bit_grid_are_rejected);
    RUN(the_centre_is_half_of_each_limit);

    RUN(stepping_in_each_direction_moves_one_tile);
    RUN(rot_is_the_inverse_of_stepping);
    RUN(rot_holds_over_long_distances_not_just_adjacent_tiles);
    RUN(rot_of_a_location_with_itself_is_east);
    RUN(stepping_off_the_map_fails_and_leaves_the_edge_intact);
    RUN(in_range_walks_the_whole_way_or_fails);

    RUN(distance_is_the_longer_of_the_two_axes);
    RUN(distance_is_symmetric);
    RUN(one_step_in_any_direction_is_always_distance_one);
}
