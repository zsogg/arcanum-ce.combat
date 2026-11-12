// Tests for the draw rectangle's sector grid (`sector_rect_from_loc_rect`,
// card HW1). The original holds 3x3 sectors with no bound on the boundary
// writes, so a view about 5K wide wrote past the arrays; CE holds
// `SECTOR_RECT_MAX` each way and refuses a rectangle wider than that.

#include "game/context.h"
#include "game/location.h"
#include "game/sector.h"

#include "test_harness.h"

static LocRect rect_of_sectors(int64_t start, int sectors)
{
    LocRect rect;

    // From `start` across `sectors` whole sectors of 64 tiles, then one tile
    // into the next, so the span crosses `sectors` + 1 sectors.
    rect.x1 = start;
    rect.y1 = start;
    rect.x2 = start + 64 * sectors;
    rect.y2 = start + 64 * sectors;
    return rect;
}

TEST(a_rectangle_of_five_sectors_each_way_is_laid_out)
{
    SectorRect sector_rect;
    LocRect rect = rect_of_sectors(6400, SECTOR_RECT_MAX - 1);
    int row;
    int col;

    CHECK(sector_rect_from_loc_rect(&rect, &sector_rect));
    CHECK_INT(sector_rect.num_rows, SECTOR_RECT_MAX);
    for (row = 0; row < sector_rect.num_rows; row++) {
        CHECK_INT(sector_rect.rows[row].num_cols, SECTOR_RECT_MAX);
        for (col = 0; col < sector_rect.rows[row].num_cols; col++) {
            CHECK_INT((int)sector_rect.rows[row].num_hor_tiles[col], col == SECTOR_RECT_MAX - 1 ? 1 : 64);
        }
    }
}

// One sector more than the grid holds: refused, and nothing written past it
// (the sanitizer presets would report a write).
TEST(a_rectangle_wider_than_the_grid_is_refused)
{
    struct {
        SectorRect rect;
        int guard;
    } probe;
    LocRect wide = rect_of_sectors(6400, SECTOR_RECT_MAX);
    LocRect huge = rect_of_sectors(6400, 40);

    probe.guard = 0x5EC7;
    CHECK(!sector_rect_from_loc_rect(&wide, &probe.rect));
    CHECK(!sector_rect_from_loc_rect(&huge, &probe.rect));
    CHECK_INT(probe.guard, 0x5EC7);
}

void test_sector_rect_suite(void)
{
    RUN(a_rectangle_of_five_sectors_each_way_is_laid_out);
    RUN(a_rectangle_wider_than_the_grid_is_refused);
}
