// `fresh shop stock`: which shop containers restock when their map loads.

#include "game/fresh_stock.h"
#include "game/obj_flags.h"

#include "test_harness.h"

TEST(a_shipped_shop_restocks)
{
    // The Tarant magick shop's chest as the module ships it: list 17.
    CHECK(fresh_stock_wanted(true, 17, 0, false, false, false));
}

TEST(a_shop_the_save_changed_keeps_its_stock)
{
    // Traded with, restocked or robbed: the save marks it modified.
    CHECK(!fresh_stock_wanted(true, 17, 0, true, false, false));
}

TEST(only_a_listed_container_restocks)
{
    CHECK(!fresh_stock_wanted(true, 0, 0, false, false, false));
    CHECK(!fresh_stock_wanted(false, 17, 0, false, false, false));
    // A spawn-once container is filled once by its own rule.
    CHECK(!fresh_stock_wanted(true, 17, OCOF_INVEN_SPAWN_ONCE, false, false, false));
    CHECK(fresh_stock_wanted(true, 17, OCOF_INVEN_SPAWN_INDEPENDENT, false, false, false));
}

TEST(a_dead_or_recruited_keeper_keeps_the_stock)
{
    CHECK(!fresh_stock_wanted(true, 17, 0, false, true, false));
    CHECK(!fresh_stock_wanted(true, 17, 0, false, false, true));
}

void test_fresh_stock_suite(void)
{
    RUN(a_shipped_shop_restocks);
    RUN(a_shop_the_save_changed_keeps_its_stock);
    RUN(only_a_listed_container_restocks);
    RUN(a_dead_or_recruited_keeper_keeps_the_stock);
}
