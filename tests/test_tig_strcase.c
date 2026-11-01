// Tests for `tig/strcase.c` (porting bug #375).
//
// The original's _stricmp and _strnicmp fold A-Z only and compare every other
// byte as it is, returning -1, 0 or 1; the values below are theirs, read from
// 0x58ADB0 and 0x595AA0 under emudiff's PeEmu on 2026-11-01. SDL's versions,
// which CE called before, decode UTF-8 and answer 0 for each Windows-1252 pair
// here. emudiff's `tig_strcase` case compares the two over generated strings.

#include "test_harness.h"
#include "tig/strcase.h"

TEST(ascii_folds_and_orders_as_the_original)
{
    CHECK_INT(tig_strcasecmp("abc", "ABC"), 0);
    CHECK_INT(tig_strcasecmp("abc", "ABD"), -1);
    CHECK_INT(tig_strcasecmp("ABD", "abc"), 1);
    // Folding is to lower case, so '_' and '[' sort before letters.
    CHECK_INT(tig_strcasecmp("_x", "ax"), -1);
    CHECK_INT(tig_strcasecmp("[", "a"), -1);
    CHECK_INT(tig_strcasecmp("ab", "abc"), -1);
}

TEST(windows_1252_letters_are_bytes)
{
    CHECK_INT(tig_strcasecmp("\xe9t\xe9", "\xe8t\xe8"), 1); // "été" against "ètè"
    CHECK_INT(tig_strcasecmp("\xc4", "\xe4"), -1); // "Ä" against "ä": not folded
    CHECK_INT(tig_strcasecmp("D\xe9\x62utante", "d\xc9\x42UTANTE"), 1); // \x62 'b', \x42 'B'
    CHECK_INT(tig_strcasecmp("a", "\xe9"), -1);
}

TEST(the_counted_form_stops_at_n)
{
    CHECK_INT(tig_strncasecmp("abcX", "ABCy", 3), 0);
    CHECK_INT(tig_strncasecmp("abcX", "ABCy", 4), -1);
    CHECK_INT(tig_strncasecmp("\xe9t\xe9", "\xe8t\xe8", 3), 1);
    CHECK_INT(tig_strncasecmp("abc", "xyz", 0), 0);
    CHECK_INT(tig_strncasecmp("ab", "AB", 10), 0);
}

void test_tig_strcase_suite(void)
{
    RUN(ascii_folds_and_orders_as_the_original);
    RUN(windows_1252_letters_are_bytes);
    RUN(the_counted_form_stops_at_n);
}
