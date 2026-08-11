// Tests for `tig/idxtable.c`.
//
// A `std::unordered_map<int, T>` written by hand in 1999, holding UI state,
// script names and multiplayer bookkeeping (`ui.c`, `script_name.c`,
// `multiplayer.c`). A defect in a hash table is silent and wide, which is the
// same argument that put `str_parse` first.
//
// Two contracts here are surprising enough to be worth pinning in tests rather
// than trusting anyone to re-read the header:
//
// 1. **Negative keys alias positive ones.** `set` does `if (key < 0) key =
//    -key;` and so do `get`, `contains` and `remove`, so `table[-100]` *is*
//    `table[100]`. The module header says so; the tests below make it fail
//    loudly if that ever changes.
//
// 2. **The bucket count is fixed at 63 and there is no rehashing.** Keys 63
//    apart collide forever, so the chain-walking paths are reached by ordinary
//    data rather than by luck, and the tests exercise them deliberately.
//
// NOT tested, deliberately: `INT_MIN`. Negating it overflows, the value stays
// negative, and `key % 63` is then negative too - so the table indexes its
// bucket array out of bounds. It is faithful to the original, it is unreachable
// from any caller in the tree (keys are object and string ids), and a test for
// it would be a test that corrupts memory. Documented instead, in the same
// spirit as `tig_str_match_str_to_list` calling `exit()`.

#include <string.h>

#include "tig/idxtable.h"

#include "test_harness.h"

// Keys 63 apart share a bucket - see the header note. Used wherever a test
// needs to force chaining.
#define BUCKETS 63

typedef struct EnumContext {
    int visited;
    int key_sum;
    int stop_after;
} EnumContext;

static bool enum_count_all(void* value, int key, void* context)
{
    EnumContext* ctx = (EnumContext*)context;

    (void)value;

    ctx->visited++;
    ctx->key_sum += key;

    return true;
}

static bool enum_stop_early(void* value, int key, void* context)
{
    EnumContext* ctx = (EnumContext*)context;

    (void)value;
    (void)key;

    ctx->visited++;

    return ctx->visited < ctx->stop_after;
}

TEST(set_then_get_round_trips_a_value)
{
    TigIdxTable table;
    int value = 1234;
    int out = 0;

    tig_idxtable_init(&table, sizeof(int));

    tig_idxtable_set(&table, 7, &value);
    CHECK(tig_idxtable_get(&table, 7, &out));
    CHECK_INT(out, 1234);
    CHECK_INT(tig_idxtable_count(&table), 1);

    tig_idxtable_exit(&table);
}

TEST(get_of_a_missing_key_reports_false)
{
    TigIdxTable table;
    int out = -1;

    tig_idxtable_init(&table, sizeof(int));

    CHECK(!tig_idxtable_get(&table, 42, &out));
    // The out parameter is left alone on a miss, so a caller that ignores the
    // return value sees whatever it initialised.
    CHECK_INT(out, -1);
    CHECK_INT(tig_idxtable_count(&table), 0);

    tig_idxtable_exit(&table);
}

TEST(setting_an_existing_key_overwrites_without_growing)
{
    TigIdxTable table;
    int first = 10;
    int second = 20;
    int out = 0;

    tig_idxtable_init(&table, sizeof(int));

    tig_idxtable_set(&table, 5, &first);
    tig_idxtable_set(&table, 5, &second);

    CHECK(tig_idxtable_get(&table, 5, &out));
    CHECK_INT(out, 20);
    CHECK_INT(tig_idxtable_count(&table), 1);

    tig_idxtable_exit(&table);
}

TEST(contains_agrees_with_get)
{
    TigIdxTable table;
    int value = 1;

    tig_idxtable_init(&table, sizeof(int));
    tig_idxtable_set(&table, 3, &value);

    CHECK(tig_idxtable_contains(&table, 3));
    CHECK(!tig_idxtable_contains(&table, 4));

    tig_idxtable_exit(&table);
}

TEST(negative_keys_alias_positive_ones)
{
    // Documented in the module header and relied upon by callers that hand it
    // signed ids. Setting -100 and reading 100 must give the same slot.
    TigIdxTable table;
    int value = 99;
    int out = 0;

    tig_idxtable_init(&table, sizeof(int));

    tig_idxtable_set(&table, -100, &value);

    CHECK(tig_idxtable_contains(&table, 100));
    CHECK(tig_idxtable_get(&table, 100, &out));
    CHECK_INT(out, 99);
    // One entry, not two - the alias is the same slot, so the count proves it.
    CHECK_INT(tig_idxtable_count(&table), 1);

    tig_idxtable_exit(&table);
}

TEST(colliding_keys_are_kept_apart)
{
    // 5, 68 and 131 all land in bucket 5. This is the chain-walking path, and
    // with a fixed 63 buckets and no rehashing it is reached by ordinary data.
    TigIdxTable table;
    int a = 100;
    int b = 200;
    int c = 300;
    int out = 0;

    tig_idxtable_init(&table, sizeof(int));

    tig_idxtable_set(&table, 5, &a);
    tig_idxtable_set(&table, 5 + BUCKETS, &b);
    tig_idxtable_set(&table, 5 + 2 * BUCKETS, &c);

    CHECK_INT(tig_idxtable_count(&table), 3);

    CHECK(tig_idxtable_get(&table, 5, &out));
    CHECK_INT(out, 100);
    CHECK(tig_idxtable_get(&table, 5 + BUCKETS, &out));
    CHECK_INT(out, 200);
    CHECK(tig_idxtable_get(&table, 5 + 2 * BUCKETS, &out));
    CHECK_INT(out, 300);

    tig_idxtable_exit(&table);
}

TEST(remove_unlinks_from_the_middle_of_a_chain)
{
    // Removing the head, the middle or the tail of a bucket chain are three
    // different pointer updates; the middle is the one that goes wrong.
    TigIdxTable table;
    int a = 1;
    int b = 2;
    int c = 3;
    int out = 0;

    tig_idxtable_init(&table, sizeof(int));

    tig_idxtable_set(&table, 9, &a);
    tig_idxtable_set(&table, 9 + BUCKETS, &b);
    tig_idxtable_set(&table, 9 + 2 * BUCKETS, &c);

    tig_idxtable_remove(&table, 9 + BUCKETS);

    CHECK_INT(tig_idxtable_count(&table), 2);
    CHECK(!tig_idxtable_contains(&table, 9 + BUCKETS));
    CHECK(tig_idxtable_get(&table, 9, &out));
    CHECK_INT(out, 1);
    CHECK(tig_idxtable_get(&table, 9 + 2 * BUCKETS, &out));
    CHECK_INT(out, 3);

    tig_idxtable_exit(&table);
}

TEST(remove_of_a_missing_key_is_a_no_op)
{
    TigIdxTable table;
    int value = 1;

    tig_idxtable_init(&table, sizeof(int));
    tig_idxtable_set(&table, 11, &value);

    tig_idxtable_remove(&table, 12);

    CHECK_INT(tig_idxtable_count(&table), 1);
    CHECK(tig_idxtable_contains(&table, 11));

    tig_idxtable_exit(&table);
}

TEST(reinsert_after_remove_restores_the_entry)
{
    TigIdxTable table;
    int value = 77;
    int out = 0;

    tig_idxtable_init(&table, sizeof(int));

    tig_idxtable_set(&table, 21, &value);
    tig_idxtable_remove(&table, 21);
    CHECK_INT(tig_idxtable_count(&table), 0);

    value = 88;
    tig_idxtable_set(&table, 21, &value);

    CHECK_INT(tig_idxtable_count(&table), 1);
    CHECK(tig_idxtable_get(&table, 21, &out));
    CHECK_INT(out, 88);

    tig_idxtable_exit(&table);
}

TEST(stores_values_larger_than_a_pointer)
{
    // `value_size` is arbitrary and the entry copies by `memcpy`, so a struct
    // wider than a pointer is the case that catches a size mix-up.
    typedef struct Wide {
        int a;
        int b;
        int c;
        int d;
        int e;
    } Wide;

    TigIdxTable table;
    Wide in = { 1, 2, 3, 4, 5 };
    Wide out;

    memset(&out, 0, sizeof(out));

    tig_idxtable_init(&table, sizeof(Wide));
    tig_idxtable_set(&table, 4, &in);

    CHECK(tig_idxtable_get(&table, 4, &out));
    CHECK_INT(out.a, 1);
    CHECK_INT(out.e, 5);

    tig_idxtable_exit(&table);
}

TEST(enumerate_visits_every_entry)
{
    TigIdxTable table;
    EnumContext ctx;
    int value = 0;
    int i;

    tig_idxtable_init(&table, sizeof(int));

    for (i = 1; i <= 10; i++) {
        value = i * 10;
        tig_idxtable_set(&table, i, &value);
    }

    memset(&ctx, 0, sizeof(ctx));
    CHECK(tig_idxtable_enumerate(&table, enum_count_all, &ctx));
    CHECK_INT(ctx.visited, 10);
    // 1 + 2 + ... + 10, so every key was seen exactly once and none twice.
    CHECK_INT(ctx.key_sum, 55);

    tig_idxtable_exit(&table);
}

TEST(enumerate_stops_when_the_callback_returns_false)
{
    TigIdxTable table;
    EnumContext ctx;
    int value = 0;
    int i;

    tig_idxtable_init(&table, sizeof(int));

    for (i = 1; i <= 10; i++) {
        value = i;
        tig_idxtable_set(&table, i, &value);
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.stop_after = 3;

    // Returns false to tell the caller it stopped early.
    CHECK(!tig_idxtable_enumerate(&table, enum_stop_early, &ctx));
    CHECK_INT(ctx.visited, 3);

    tig_idxtable_exit(&table);
}

TEST(copy_is_independent_of_the_source)
{
    // `tig_idxtable_copy` deep-copies the chains. If it shared entries instead,
    // writing through one table would show up in the other - and freeing both
    // would double-free.
    TigIdxTable src;
    TigIdxTable dst;
    int value = 5;
    int out = 0;

    tig_idxtable_init(&src, sizeof(int));

    value = 5;
    tig_idxtable_set(&src, 1, &value);
    value = 6;
    tig_idxtable_set(&src, 1 + BUCKETS, &value); // same bucket, forces a chain copy

    tig_idxtable_copy(&dst, &src);

    CHECK_INT(tig_idxtable_count(&dst), 2);
    CHECK(tig_idxtable_get(&dst, 1, &out));
    CHECK_INT(out, 5);
    CHECK(tig_idxtable_get(&dst, 1 + BUCKETS, &out));
    CHECK_INT(out, 6);

    // Mutate the copy; the source must not move.
    value = 999;
    tig_idxtable_set(&dst, 1, &value);

    CHECK(tig_idxtable_get(&src, 1, &out));
    CHECK_INT(out, 5);

    // And removing from the copy must not touch the source.
    tig_idxtable_remove(&dst, 1 + BUCKETS);
    CHECK_INT(tig_idxtable_count(&src), 2);
    CHECK(tig_idxtable_contains(&src, 1 + BUCKETS));

    tig_idxtable_exit(&dst);
    tig_idxtable_exit(&src);
}

TEST(count_tracks_inserts_and_removals)
{
    TigIdxTable table;
    int value = 0;
    int i;

    tig_idxtable_init(&table, sizeof(int));
    CHECK_INT(tig_idxtable_count(&table), 0);

    for (i = 0; i < 100; i++) {
        value = i;
        tig_idxtable_set(&table, i, &value);
    }
    CHECK_INT(tig_idxtable_count(&table), 100);

    // Re-setting existing keys must not inflate the count.
    for (i = 0; i < 100; i++) {
        value = i * 2;
        tig_idxtable_set(&table, i, &value);
    }
    CHECK_INT(tig_idxtable_count(&table), 100);

    for (i = 0; i < 50; i++) {
        tig_idxtable_remove(&table, i);
    }
    CHECK_INT(tig_idxtable_count(&table), 50);

    CHECK(tig_idxtable_get(&table, 99, &value));
    CHECK_INT(value, 198);

    tig_idxtable_exit(&table);
}

void test_idxtable_suite(void)
{
    RUN(set_then_get_round_trips_a_value);
    RUN(get_of_a_missing_key_reports_false);
    RUN(setting_an_existing_key_overwrites_without_growing);
    RUN(contains_agrees_with_get);

    RUN(negative_keys_alias_positive_ones);
    RUN(colliding_keys_are_kept_apart);

    RUN(remove_unlinks_from_the_middle_of_a_chain);
    RUN(remove_of_a_missing_key_is_a_no_op);
    RUN(reinsert_after_remove_restores_the_entry);

    RUN(stores_values_larger_than_a_pointer);

    RUN(enumerate_visits_every_entry);
    RUN(enumerate_stops_when_the_callback_returns_false);

    RUN(copy_is_independent_of_the_source);
    RUN(count_tracks_inserts_and_removals);
}
