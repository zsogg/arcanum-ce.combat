// Tests for `tig/rect.c`.
//
// Chosen as the second suite because it is the geometry the entire render path
// stands on and it has no dependencies at all - no SDL, no assets, no display.
// `tig_rect_list_add_disjoint` alone is ~390 lines of hand-transcribed
// splitting logic, and every window, button and blit reaches the screen through
// a dirty rect that it produced.
//
// The interesting tests here are not "does this case give this answer" but
// **property** tests against a pixel-grid oracle: paint the rectangles the
// function returns onto a small grid and check the invariant the caller relies
// on. For the dirty-rect accumulator that invariant is exactly two things:
//
//   1. the resulting list is pairwise disjoint - no pixel is covered twice, or
//      it would be redrawn twice; and
//   2. it covers precisely the union of everything added - no pixel is lost, or
//      it would be left stale on screen.
//
// A case-by-case test would have to guess which of the many overlap
// configurations the splitting logic gets wrong. The grid does not have to
// guess.

#include <string.h>

#include "tig/rect.h"
#include "tig/types.h"

#include "test_harness.h"

// Small enough to brute force, large enough for the overlap shapes that matter.
#define GRID 40

typedef unsigned char Grid[GRID][GRID];

static void grid_clear(Grid g)
{
    memset(g, 0, sizeof(Grid));
}

// Paints a rect onto the grid, counting coverage so double-cover is detectable.
// Anything outside the grid is clamped away; the fixtures stay well inside it.
static void grid_paint(Grid g, const TigRect* r)
{
    int x;
    int y;

    for (y = r->y; y < r->y + r->height; y++) {
        for (x = r->x; x < r->x + r->width; x++) {
            if (x >= 0 && x < GRID && y >= 0 && y < GRID) {
                g[y][x]++;
            }
        }
    }
}

static int grid_max(const Grid g)
{
    int x;
    int y;
    int max = 0;

    for (y = 0; y < GRID; y++) {
        for (x = 0; x < GRID; x++) {
            if (g[y][x] > max) {
                max = g[y][x];
            }
        }
    }

    return max;
}

// Number of cells where the two grids disagree about being covered at all.
static int grid_coverage_diff(const Grid a, const Grid b)
{
    int x;
    int y;
    int diff = 0;

    for (y = 0; y < GRID; y++) {
        for (x = 0; x < GRID; x++) {
            if ((a[y][x] != 0) != (b[y][x] != 0)) {
                diff++;
            }
        }
    }

    return diff;
}

// Adds a rect the way every real caller does.
//
// `tig_rect_list_add_disjoint` must not be handed an empty list: it walks the
// list to find the node to append after, so with no nodes its `prev` stays NULL
// and it dereferences it. All four call sites in the tree (`gamelib.c` twice,
// `object.c`, `light.c`) therefore test the head themselves and seed the first
// node by hand. Writing this helper is how that precondition was found - the
// first version of these tests called the function directly and segfaulted on
// the first case. See the note now in rect.h.
static void list_add(TigRectListNode** head, TigRect* rect)
{
    if (*head != NULL) {
        tig_rect_list_add_disjoint(head, rect);
    } else {
        *head = tig_rect_node_create();
        (*head)->rect = *rect;
    }
}

static void list_free(TigRectListNode* head)
{
    TigRectListNode* next;

    while (head != NULL) {
        next = head->next;
        tig_rect_node_destroy(head);
        head = next;
    }
}

// Runs the accumulator over `rects` and checks both invariants at once.
static void check_disjoint_cover(const TigRect* rects, int count)
{
    Grid expected;
    Grid actual;
    TigRectListNode* head = NULL;
    TigRectListNode* node;
    TigRect scratch;
    int i;

    grid_clear(expected);
    grid_clear(actual);

    for (i = 0; i < count; i++) {
        grid_paint(expected, &(rects[i]));
        scratch = rects[i];
        list_add(&head, &scratch);
    }

    for (node = head; node != NULL; node = node->next) {
        grid_paint(actual, &(node->rect));
    }

    // 1: no pixel is in the list twice.
    CHECK_INT(grid_max(actual), 1);
    // 2: the list covers exactly what was added, no more and no less.
    CHECK_INT(grid_coverage_diff(expected, actual), 0);

    list_free(head);
}

TEST(intersection_of_overlapping_rects)
{
    TigRect a = { 0, 0, 10, 10 };
    TigRect b = { 5, 5, 10, 10 };
    TigRect r;

    CHECK_INT(tig_rect_intersection(&a, &b, &r), TIG_OK);
    CHECK_INT(r.x, 5);
    CHECK_INT(r.y, 5);
    CHECK_INT(r.width, 5);
    CHECK_INT(r.height, 5);
}

TEST(intersection_of_disjoint_rects_reports_no_intersection)
{
    TigRect a = { 0, 0, 10, 10 };
    TigRect b = { 20, 20, 5, 5 };
    TigRect r;

    CHECK_INT(tig_rect_intersection(&a, &b, &r), TIG_ERR_NO_INTERSECTION);
}

TEST(intersection_of_edge_touching_rects_is_empty)
{
    // Rects that share an edge do not overlap: `TigRect` is LTWH and the right
    // and bottom edges are exclusive, so x=10 is the first column *outside*
    // a rect at x=0 width=10. A zero-width overlap is reported as none, which
    // is what stops a shared border being redrawn by both neighbours.
    TigRect a = { 0, 0, 10, 10 };
    TigRect b = { 10, 0, 10, 10 };
    TigRect r;

    CHECK_INT(tig_rect_intersection(&a, &b, &r), TIG_ERR_NO_INTERSECTION);
}

TEST(intersection_with_contained_rect_is_the_inner_rect)
{
    TigRect outer = { 0, 0, 20, 20 };
    TigRect inner = { 5, 5, 4, 4 };
    TigRect r;

    CHECK_INT(tig_rect_intersection(&outer, &inner, &r), TIG_OK);
    CHECK_INT(r.x, 5);
    CHECK_INT(r.y, 5);
    CHECK_INT(r.width, 4);
    CHECK_INT(r.height, 4);
}

TEST(union_is_the_bounding_box)
{
    TigRect a = { 2, 3, 4, 5 };
    TigRect b = { 10, 1, 2, 2 };
    TigRect r;

    CHECK_INT(tig_rect_union(&a, &b, &r), TIG_OK);
    CHECK_INT(r.x, 2);
    CHECK_INT(r.y, 1);
    CHECK_INT(r.width, 10); // 2..12
    CHECK_INT(r.height, 7); // 1..8
}

TEST(union_of_disjoint_rects_spans_the_gap)
{
    // A union is a bounding box, not a set union: the gap between the two is
    // included. Callers that use this to grow a dirty region are relying on
    // that, so it is worth pinning.
    TigRect a = { 0, 0, 2, 2 };
    TigRect b = { 30, 30, 2, 2 };
    TigRect r;

    CHECK_INT(tig_rect_union(&a, &b, &r), TIG_OK);
    CHECK_INT(r.x, 0);
    CHECK_INT(r.y, 0);
    CHECK_INT(r.width, 32);
    CHECK_INT(r.height, 32);
}

TEST(clip_pieces_are_a_minus_b_and_do_not_overlap)
{
    // `tig_rect_clip` returns the parts of `a` left over once `b` is taken out.
    // Painting the pieces plus the intersection must reproduce `a` exactly,
    // with nothing covered twice.
    TigRect a = { 0, 0, 20, 20 };
    TigRect b = { 5, 5, 6, 6 };
    TigRect pieces[4];
    TigRect intersection;
    Grid expected;
    Grid actual;
    int count;
    int i;

    grid_clear(expected);
    grid_clear(actual);
    grid_paint(expected, &a);

    count = tig_rect_clip(&a, &b, pieces);
    CHECK(count >= 0 && count <= 4);

    for (i = 0; i < count; i++) {
        grid_paint(actual, &(pieces[i]));
    }

    CHECK_INT(tig_rect_intersection(&a, &b, &intersection), TIG_OK);
    grid_paint(actual, &intersection);

    CHECK_INT(grid_max(actual), 1);
    CHECK_INT(grid_coverage_diff(expected, actual), 0);
}

TEST(clip_by_a_corner_overlap_leaves_an_l_shape)
{
    TigRect a = { 4, 4, 10, 10 };
    TigRect b = { 0, 0, 8, 8 };
    TigRect pieces[4];
    TigRect intersection;
    Grid expected;
    Grid actual;
    int count;
    int i;

    grid_clear(expected);
    grid_clear(actual);
    grid_paint(expected, &a);

    count = tig_rect_clip(&a, &b, pieces);
    for (i = 0; i < count; i++) {
        grid_paint(actual, &(pieces[i]));
    }
    CHECK_INT(tig_rect_intersection(&a, &b, &intersection), TIG_OK);
    grid_paint(actual, &intersection);

    CHECK_INT(grid_max(actual), 1);
    CHECK_INT(grid_coverage_diff(expected, actual), 0);
}

TEST(disjoint_list_two_overlapping_rects)
{
    static const TigRect rects[] = {
        { 0, 0, 10, 10 },
        { 5, 5, 10, 10 },
    };

    check_disjoint_cover(rects, 2);
}

TEST(disjoint_list_identical_rects_added_twice)
{
    static const TigRect rects[] = {
        { 3, 3, 8, 8 },
        { 3, 3, 8, 8 },
    };

    check_disjoint_cover(rects, 2);
}

TEST(disjoint_list_fully_contained_rect)
{
    static const TigRect rects[] = {
        { 0, 0, 20, 20 },
        { 5, 5, 4, 4 },
    };

    check_disjoint_cover(rects, 2);
}

TEST(disjoint_list_containing_rect_added_second)
{
    // The reverse order matters: the second rect swallows the first, which is
    // the case that has to retire an existing node rather than split the
    // incoming one.
    static const TigRect rects[] = {
        { 5, 5, 4, 4 },
        { 0, 0, 20, 20 },
    };

    check_disjoint_cover(rects, 2);
}

TEST(disjoint_list_cross_overlap)
{
    // Neither rect contains a corner of the other - a vertical bar crossing a
    // horizontal one. This is the configuration a corner-classification scheme
    // is most likely to miss, because every corner flag is zero.
    static const TigRect rects[] = {
        { 10, 0, 6, 30 },
        { 0, 10, 30, 6 },
    };

    check_disjoint_cover(rects, 2);
}

TEST(disjoint_list_edge_touching_rects)
{
    static const TigRect rects[] = {
        { 0, 0, 10, 10 },
        { 10, 0, 10, 10 },
        { 0, 10, 10, 10 },
    };

    check_disjoint_cover(rects, 3);
}

TEST(disjoint_list_many_overlapping_rects)
{
    // A pile with every relation in it at once: nested, crossing, touching and
    // separate. This is the closest thing here to a frame's worth of damage.
    static const TigRect rects[] = {
        { 0, 0, 15, 15 },
        { 10, 10, 15, 15 },
        { 5, 5, 5, 5 },
        { 12, 0, 4, 30 },
        { 0, 12, 30, 4 },
        { 30, 30, 5, 5 },
        { 2, 2, 30, 30 },
        { 20, 1, 3, 3 },
    };

    check_disjoint_cover(rects, 8);
}

TEST(disjoint_list_staircase)
{
    static const TigRect rects[] = {
        { 0, 0, 8, 8 },
        { 4, 4, 8, 8 },
        { 8, 8, 8, 8 },
        { 12, 12, 8, 8 },
        { 16, 16, 8, 8 },
    };

    check_disjoint_cover(rects, 5);
}

TEST(disjoint_list_single_rect_is_returned_unchanged)
{
    TigRect rect = { 3, 4, 5, 6 };
    TigRectListNode* head = NULL;

    list_add(&head, &rect);

    CHECK(head != NULL);
    if (head != NULL) {
        CHECK_INT(head->rect.x, 3);
        CHECK_INT(head->rect.y, 4);
        CHECK_INT(head->rect.width, 5);
        CHECK_INT(head->rect.height, 6);
        CHECK(head->next == NULL);
    }

    list_free(head);
}

TEST(line_bounding_box_normalises_direction)
{
    // The box must be the same whichever way the line is drawn, or a dirty rect
    // for a line would depend on which end the caller passed first.
    TigLine down_right = { 2, 3, 9, 12 };
    TigLine up_left = { 9, 12, 2, 3 };
    TigRect a;
    TigRect b;

    CHECK_INT(tig_line_bounding_box(&down_right, &a), TIG_OK);
    CHECK_INT(tig_line_bounding_box(&up_left, &b), TIG_OK);

    CHECK_INT(a.x, b.x);
    CHECK_INT(a.y, b.y);
    CHECK_INT(a.width, b.width);
    CHECK_INT(a.height, b.height);

    CHECK_INT(a.x, 2);
    CHECK_INT(a.y, 3);
}

TEST(line_intersection_clips_a_line_with_one_end_inside)
{
    // The supported case per rect.h: one endpoint inside the rect. The line is
    // clipped in place to the part within the rect.
    TigRect rect = { 0, 0, 10, 10 };
    TigLine line = { 5, 5, 20, 5 };

    CHECK_INT(tig_line_intersection(&rect, &line), TIG_OK);
    CHECK_INT(line.x1, 5);
    CHECK_INT(line.y1, 5);
    // The far end is pulled back to the rect's edge.
    CHECK(line.x2 <= 10);
}

TEST(line_intersection_misses_a_line_that_crosses_clean_through)
{
    // Documented limitation, tested so it stays documented: a line that passes
    // all the way through with *neither* end inside is reported as no
    // intersection. See the scheme in rect.h. This is faithful, not a defect -
    // the test pins the quirk so a future change to this function has to be
    // deliberate about it.
    TigRect rect = { 5, 5, 10, 10 };
    TigLine line = { 0, 10, 30, 10 };

    CHECK_INT(tig_line_intersection(&rect, &line), TIG_ERR_NO_INTERSECTION);
}

void test_rect_suite(void)
{
    RUN(intersection_of_overlapping_rects);
    RUN(intersection_of_disjoint_rects_reports_no_intersection);
    RUN(intersection_of_edge_touching_rects_is_empty);
    RUN(intersection_with_contained_rect_is_the_inner_rect);

    RUN(union_is_the_bounding_box);
    RUN(union_of_disjoint_rects_spans_the_gap);

    RUN(clip_pieces_are_a_minus_b_and_do_not_overlap);
    RUN(clip_by_a_corner_overlap_leaves_an_l_shape);

    RUN(disjoint_list_two_overlapping_rects);
    RUN(disjoint_list_identical_rects_added_twice);
    RUN(disjoint_list_fully_contained_rect);
    RUN(disjoint_list_containing_rect_added_second);
    RUN(disjoint_list_cross_overlap);
    RUN(disjoint_list_edge_touching_rects);
    RUN(disjoint_list_many_overlapping_rects);
    RUN(disjoint_list_staircase);
    RUN(disjoint_list_single_rect_is_returned_unchanged);

    RUN(line_bounding_box_normalises_direction);
    RUN(line_intersection_clips_a_line_with_one_end_inside);
    RUN(line_intersection_misses_a_line_that_crosses_clean_through);
}
