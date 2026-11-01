// Tests for `tig/qsort.c` (porting bug #374).
//
// The order equal elements come out in is the whole point of the file: the
// game reads it, and the platform's qsort decides it differently. The first
// case is the AI's offensive spell list - the shipped priorities from
// `rules/spelllist.mes`, in the order `mt_ai_action_list_fill` builds it - and
// its expected order is the original's own, measured 2026-11-01 by running
// 0x58A548 under emudiff's PeEmu. glibc's qsort gives Dominate Will (39)
// before Magnetic Inversion (182) and Unseen Force (2) before Drain Will (37);
// the original gives the reverse. emudiff's `tig_qsort` case compares the two
// sorts over thousands of generated arrays; this pins one real one without the
// executable.

#include <stdint.h>

#include "test_harness.h"
#include "tig/qsort.h"

typedef struct SpellPriority {
    int spell;
    int priority;
} SpellPriority;

// `mt_ai_spell_priority_compare`: the highest priority first.
static int spell_priority_compare(const void* va, const void* vb)
{
    const SpellPriority* a = (const SpellPriority*)va;
    const SpellPriority* b = (const SpellPriority*)vb;

    return (a->priority < b->priority) - (a->priority > b->priority);
}

TEST(the_ai_s_offensive_spells_tie_as_in_the_original)
{
    SpellPriority list[] = {
        { 0, 1 },
        { 2, 10 },
        { 11, 14 },
        { 12, 4 },
        { 16, 3 },
        { 22, 13 },
        { 27, 11 },
        { 31, 6 },
        { 33, 15 },
        { 34, 20 },
        { 36, 7 },
        { 37, 10 },
        { 38, 16 },
        { 39, 19 },
        { 46, 1 },
        { 47, 1 },
        { 48, 9 },
        { 49, 18 },
        { 55, 1 },
        { 59, 17 },
        { 66, 1 },
        { 70, 2 },
        { 122, 3 },
        { 127, 9 },
        { 131, 4 },
        { 132, 5 },
        { 182, 19 },
        { 209, 2 },
        { 211, 2 },
        { 233, 10 },
        { 247, 17 },
        { 248, 4 },
    };
    static const int original[] = {
        34,
        182,
        39,
        49,
        59,
        247,
        38,
        33,
        11,
        22,
        27,
        37,
        233,
        2,
        48,
        127,
        36,
        31,
        132,
        131,
        248,
        12,
        16,
        122,
        70,
        209,
        211,
        66,
        0,
        47,
        55,
        46,
    };
    size_t i;

    tig_qsort(list, sizeof(list) / sizeof(list[0]), sizeof(list[0]), spell_priority_compare);
    for (i = 0; i < sizeof(original) / sizeof(original[0]); i++) {
        CHECK_INT(list[i].spell, original[i]);
    }
}

static int int_compare(const void* va, const void* vb)
{
    int a = *(const int*)va;
    int b = *(const int*)vb;

    return (a > b) - (a < b);
}

TEST(every_count_comes_out_sorted)
{
    int values[200];
    uint32_t state = 374;
    int n;
    int i;
    int unsorted = 0;

    // Either side of the cutoff at 8, with many ties and with few.
    for (n = 0; n <= 200; n++) {
        for (i = 0; i < n; i++) {
            state = state * 1103515245u + 12345u;
            values[i] = (int)((state >> 16) % (n % 2 == 0 ? 4u : 1000u));
        }
        tig_qsort(values, (size_t)n, sizeof(values[0]), int_compare);
        for (i = 1; i < n; i++) {
            if (values[i - 1] > values[i]) {
                unsorted++;
            }
        }
    }
    CHECK_INT(unsorted, 0);
}

TEST(nothing_to_sort_is_left_alone)
{
    int one = 7;

    tig_qsort(&one, 1, sizeof(one), int_compare);
    tig_qsort(&one, 0, sizeof(one), int_compare);
    tig_qsort(&one, 5, 0, int_compare);
    CHECK_INT(one, 7);
}

void test_tig_qsort_suite(void)
{
    RUN(the_ai_s_offensive_spells_tie_as_in_the_original);
    RUN(every_count_comes_out_sorted);
    RUN(nothing_to_sort_is_left_alone);
}
