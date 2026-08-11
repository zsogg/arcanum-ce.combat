// Tests for `tig/bsearch.c`.
//
// Twenty-six lines, one caller, and worth a suite anyway: it is not a plain
// binary search. On a miss it returns the **insertion point** rather than NULL,
// and `mes.c` depends on exactly that - `mes_find_next` subtracts the returned
// pointer from the base to get an index, then steps forward only when the key
// was actually found. Get the miss contract wrong and message iteration walks
// off by one, silently, in every .mes file the game reads.
//
// The file also carries "NOTE: Original code is slightly different", so this is
// a reimplementation rather than a transcription - which is the category that
// produced both of this session's defects.

#include <stdlib.h>

#include "tig/bsearch.h"

#include "test_harness.h"

static int compare_int(const void* a, const void* b)
{
    int lhs = *(const int*)a;
    int rhs = *(const int*)b;

    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }

    return 0;
}

// Returns the index the search landed on, which is what `mes.c` computes.
static int search_index(const int* key, const int* base, size_t cnt, int* exists)
{
    int* found = (int*)tig_bsearch(key, base, cnt, sizeof(int), compare_int, exists);

    return (int)(found - base);
}

TEST(finds_an_element_that_is_present)
{
    static const int data[] = { 1, 3, 5, 7, 9, 11 };
    int key;
    int exists = -1;
    int index;
    size_t i;

    // Every element, so no single lucky midpoint can carry the test.
    for (i = 0; i < 6; i++) {
        key = data[i];
        index = search_index(&key, data, 6, &exists);
        CHECK_INT(exists, 1);
        CHECK_INT(index, (int)i);
    }
}

TEST(reports_the_insertion_point_for_a_missing_key)
{
    // The contract `mes.c` leans on: a miss returns where the key *would* go.
    static const int data[] = { 10, 20, 30, 40 };
    int key;
    int exists = -1;

    key = 25;
    CHECK_INT(search_index(&key, data, 4, &exists), 2);
    CHECK_INT(exists, 0);

    key = 15;
    CHECK_INT(search_index(&key, data, 4, &exists), 1);
    CHECK_INT(exists, 0);
}

TEST(a_key_below_the_range_lands_at_the_start)
{
    static const int data[] = { 10, 20, 30 };
    int key = 1;
    int exists = -1;

    CHECK_INT(search_index(&key, data, 3, &exists), 0);
    CHECK_INT(exists, 0);
}

TEST(a_key_above_the_range_lands_one_past_the_end)
{
    // This returns a pointer one element past the array. It is not
    // dereferenceable, and the only caller range-checks the derived index
    // before use - `mes_find_next` returns false when `index >= num_entries`.
    static const int data[] = { 10, 20, 30 };
    int key = 99;
    int exists = -1;

    CHECK_INT(search_index(&key, data, 3, &exists), 3);
    CHECK_INT(exists, 0);
}

TEST(an_empty_array_reports_a_miss_at_index_zero)
{
    static const int data[] = { 0 }; // never read; cnt is 0
    int key = 5;
    int exists = -1;

    CHECK_INT(search_index(&key, data, 0, &exists), 0);
    CHECK_INT(exists, 0);
}

TEST(a_single_element_array_hits_and_misses_correctly)
{
    static const int data[] = { 42 };
    int key;
    int exists = -1;

    key = 42;
    CHECK_INT(search_index(&key, data, 1, &exists), 0);
    CHECK_INT(exists, 1);

    key = 41;
    CHECK_INT(search_index(&key, data, 1, &exists), 0);
    CHECK_INT(exists, 0);

    key = 43;
    CHECK_INT(search_index(&key, data, 1, &exists), 1);
    CHECK_INT(exists, 0);
}

TEST(finds_every_element_of_an_even_and_an_odd_sized_array)
{
    // Midpoint arithmetic is where a hand-written search goes wrong, and it
    // rounds differently for even and odd counts. Sweep both, and sweep the
    // gaps between elements too.
    static const int even[] = { 0, 2, 4, 6, 8, 10, 12, 14 };
    static const int odd[] = { 0, 2, 4, 6, 8, 10, 12 };
    int key;
    int exists;
    int i;

    for (i = 0; i < 8; i++) {
        key = even[i];
        CHECK_INT(search_index(&key, even, 8, &exists), i);
        CHECK_INT(exists, 1);

        // The odd value just below sits at the same insertion point.
        key = even[i] - 1;
        CHECK_INT(search_index(&key, even, 8, &exists), i);
        CHECK_INT(exists, 0);
    }

    for (i = 0; i < 7; i++) {
        key = odd[i];
        CHECK_INT(search_index(&key, odd, 7, &exists), i);
        CHECK_INT(exists, 1);

        key = odd[i] - 1;
        CHECK_INT(search_index(&key, odd, 7, &exists), i);
        CHECK_INT(exists, 0);
    }
}

TEST(finds_a_match_among_duplicates)
{
    // With equal keys the search returns as soon as a midpoint compares equal,
    // so *which* duplicate comes back is an implementation detail. Assert only
    // what callers can rely on: it reports a hit, and points at a matching
    // element. Pinning a particular index here would be inventing a contract.
    static const int data[] = { 1, 5, 5, 5, 9 };
    int key = 5;
    int exists = -1;
    int index;

    index = search_index(&key, data, 5, &exists);

    CHECK_INT(exists, 1);
    CHECK(index >= 1 && index <= 3);
    CHECK_INT(data[index], 5);
}

TEST(works_on_elements_wider_than_a_pointer)
{
    // `size` is a byte stride, and `mes.c` passes `sizeof(MesFileEntry)`. A
    // stride mistake survives an int-sized test and fails on a real struct.
    typedef struct Wide {
        int key;
        char padding[24];
    } Wide;

    static const Wide data[] = {
        { 10, { 0 } },
        { 20, { 0 } },
        { 30, { 0 } },
    };

    Wide key;
    int exists = -1;
    Wide* found;

    key.key = 30;
    found = (Wide*)tig_bsearch(&key, data, 3, sizeof(Wide), compare_int, &exists);

    CHECK_INT(exists, 1);
    CHECK_INT((int)(found - data), 2);
    CHECK_INT(found->key, 30);

    key.key = 25;
    found = (Wide*)tig_bsearch(&key, data, 3, sizeof(Wide), compare_int, &exists);
    CHECK_INT(exists, 0);
    CHECK_INT((int)(found - data), 2);
}

void test_bsearch_suite(void)
{
    RUN(finds_an_element_that_is_present);
    RUN(reports_the_insertion_point_for_a_missing_key);
    RUN(a_key_below_the_range_lands_at_the_start);
    RUN(a_key_above_the_range_lands_one_past_the_end);
    RUN(an_empty_array_reports_a_miss_at_index_zero);
    RUN(a_single_element_array_hits_and_misses_correctly);
    RUN(finds_every_element_of_an_even_and_an_odd_sized_array);
    RUN(finds_a_match_among_duplicates);
    RUN(works_on_elements_wider_than_a_pointer);
}
