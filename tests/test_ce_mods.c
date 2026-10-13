// Release 1, R4 (2026-10-13): the optional mod bundles a release ships - the
// pure parts. Mounting and switching are exercised in scripted runs.

#include <string.h>

#include "game/ce_mods.h"

#include "test_harness.h"

TEST(a_bundle_id_is_lower_case_letters_digits_and_dashes)
{
    CHECK(ce_mods_id_valid("combat-rebalance"));
    CHECK(ce_mods_id_valid("real-time-pacing"));
    CHECK(ce_mods_id_valid("a1"));
    CHECK(!ce_mods_id_valid(""));
    CHECK(!ce_mods_id_valid(NULL));
    CHECK(!ce_mods_id_valid("Combat"));
    CHECK(!ce_mods_id_valid("a b"));
    CHECK(!ce_mods_id_valid("../x"));
    CHECK(!ce_mods_id_valid("a_b"));
}

TEST(a_features_line_reads_a_key_and_a_whole_number)
{
    char key[64];
    int value = -1;

    CHECK(ce_mods_parse_line("weapon rules=1\n", key, sizeof(key), &value));
    CHECK_STR(key, "weapon rules");
    CHECK_INT(value, 1);
    CHECK(ce_mods_parse_line("  real time pace ms = 2000 \r\n", key, sizeof(key), &value));
    CHECK_STR(key, "real time pace ms");
    CHECK_INT(value, 2000);
    CHECK(ce_mods_parse_line("on-hit spells free=0", key, sizeof(key), &value));
    CHECK_INT(value, 0);

    CHECK(!ce_mods_parse_line("", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("   \n", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("# a comment=1", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("; also a comment", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("no equals sign", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("=1", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("weapon rules=", key, sizeof(key), &value));
    CHECK(!ce_mods_parse_line("weapon rules=yes", key, sizeof(key), &value));

    // A key too long for the buffer is refused, not truncated.
    char small[4];
    CHECK(!ce_mods_parse_line("weapon rules=1", small, sizeof(small), &value));
}

void test_ce_mods_suite(void)
{
    RUN(a_bundle_id_is_lower_case_letters_digits_and_dashes);
    RUN(a_features_line_reads_a_key_and_a_whole_number);
}
