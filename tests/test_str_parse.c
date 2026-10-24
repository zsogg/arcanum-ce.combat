// Tests for `tig/str_parse.c`.
//
// Chosen as the first suite because it is pure logic with one external symbol
// (`tig_debug_printf`), and because almost every data file the game loads is
// read through it: the magictech spell list, prototype rules text, terrain
// names, the world map definition. A defect here is silent and wide.
//
// Two contracts worth knowing before editing these, both surprising:
//
// 1. **The parsers mutate the caller's buffer.** They write '\0' over the
//    separator, parse, then put the original character back. So the input must
//    be writable - a string literal is undefined behaviour, which is why every
//    test below copies into a `char buf[]` first.
//
// 2. **`tig_str_match_str_to_list` calls `exit(EXIT_FAILURE)` when the token
//    matches nothing in its list.** Not a return code, not an assert: a
//    malformed data file terminates the process. That is faithful to the
//    original, so it is documented rather than tested - a test for it would
//    have to fork.

#include <string.h>

#include "tig/str_parse.h"

#include "test_harness.h"

TEST(parse_value_reads_a_plain_number)
{
    char buf[] = "42";
    char* str = buf;
    int value = -1;

    tig_str_parse_value(&str, &value);

    CHECK_INT(value, 42);
    // No separator, so the input is exhausted and the pointer is nulled rather
    // than left dangling at the terminator.
    CHECK_AT_END(str);
}

TEST(parse_value_stops_at_the_separator_and_advances)
{
    char buf[] = "1,2,3";
    char* str = buf;
    int value = -1;

    tig_str_parse_value(&str, &value);
    CHECK_INT(value, 1);
    CHECK_REMAINDER(str, "2,3");

    tig_str_parse_value(&str, &value);
    CHECK_INT(value, 2);
    CHECK_REMAINDER(str, "3");

    tig_str_parse_value(&str, &value);
    CHECK_INT(value, 3);
    CHECK_AT_END(str);
}

TEST(parse_value_skips_leading_whitespace)
{
    char buf[] = "   \t 7 , 8";
    char* str = buf;
    int value = -1;

    tig_str_parse_value(&str, &value);

    CHECK_INT(value, 7);
}

TEST(parse_value_restores_the_buffer_it_mutated)
{
    // The separator is overwritten with '\0' during the parse and must be put
    // back, or the caller's string is truncated from the first comma onward.
    char buf[] = "10,20";
    char* str = buf;
    int value = -1;

    tig_str_parse_value(&str, &value);

    CHECK_INT(value, 10);
    CHECK_STR(buf, "10,20");
}

TEST(parse_value_yields_zero_for_a_non_number)
{
    // atoi semantics, inherited deliberately: no error is reported, so a
    // malformed field reads as 0 rather than failing loudly.
    char buf[] = "abc,5";
    char* str = buf;
    int value = -1;

    tig_str_parse_value(&str, &value);

    CHECK_INT(value, 0);
    CHECK_REMAINDER(str, "5");
}

TEST(parse_range_splits_on_the_hyphen)
{
    char buf[] = "5-10";
    char* str = buf;
    int start = -1;
    int end = -1;

    tig_str_parse_range(&str, &start, &end);

    CHECK_INT(start, 5);
    CHECK_INT(end, 10);
    CHECK_STR(buf, "5-10");
}

TEST(parse_range_without_a_hyphen_is_a_single_value)
{
    char buf[] = "7";
    char* str = buf;
    int start = -1;
    int end = -1;

    tig_str_parse_range(&str, &start, &end);

    CHECK_INT(start, 7);
    CHECK_INT(end, 7);
}

TEST(parse_range_cannot_express_a_negative_start)
{
    // Documents a real trap rather than asserting desirable behaviour. The
    // hyphen search finds the minus sign at offset 0, so the text before it is
    // empty: "-5" parses as the range 0 to 5, not as -5. Any data file that
    // wants a negative lower bound cannot say so through this function.
    char buf[] = "-5";
    char* str = buf;
    int start = -1;
    int end = -1;

    tig_str_parse_range(&str, &start, &end);

    CHECK_INT(start, 0);
    CHECK_INT(end, 5);
}

TEST(parse_range_advances_past_the_separator)
{
    char buf[] = "1-2,9";
    char* str = buf;
    int start = -1;
    int end = -1;

    tig_str_parse_range(&str, &start, &end);

    CHECK_INT(start, 1);
    CHECK_INT(end, 2);
    CHECK_REMAINDER(str, "9");
}

TEST(named_value_matches_its_prefix_and_consumes_it)
{
    char buf[] = "MapKeyedTo: 42";
    char* str = buf;
    int value = -1;

    CHECK(tig_str_parse_named_value(&str, "MapKeyedTo:", &value));
    CHECK_INT(value, 42);
}

TEST(named_value_rejects_a_different_name)
{
    char buf[] = "SomethingElse: 42";
    char* str = buf;
    int value = -1;

    CHECK(!tig_str_parse_named_value(&str, "MapKeyedTo:", &value));
    // On a miss the value is zeroed and the cursor is left where it was, so the
    // caller can try the next candidate name against the same text.
    CHECK_INT(value, 0);
    CHECK_REMAINDER(str, "SomethingElse: 42");
}

TEST(named_value_tolerates_a_null_cursor)
{
    // Callers chain these without re-checking, so a name lookup against an
    // exhausted string has to be safe rather than a crash.
    char* str = NULL;
    int value = -1;

    CHECK(!tig_str_parse_named_value(&str, "Name:", &value));
    CHECK_INT(value, 0);
}

TEST(named_value_requires_the_name_at_the_start)
{
    // strstr would find the name anywhere; the implementation insists the match
    // is at the cursor, so a name appearing later in the line is not a match.
    char buf[] = "Prefix MapKeyedTo: 42";
    char* str = buf;
    int value = -1;

    CHECK(!tig_str_parse_named_value(&str, "MapKeyedTo:", &value));
}

TEST(separator_is_configurable_and_sticks)
{
    tig_str_parse_set_separator('|');

    char buf[] = "3|4";
    char* str = buf;
    int value = -1;

    tig_str_parse_value(&str, &value);
    CHECK_INT(value, 3);
    CHECK_REMAINDER(str, "4");

    // A comma is now just text, so atoi stops at it and the whole remainder is
    // consumed as one field.
    char other[] = "5,6";
    str = other;
    tig_str_parse_value(&str, &value);
    CHECK_INT(value, 5);
    CHECK_AT_END(str);

    tig_str_parse_set_separator(',');
}

TEST(match_str_to_list_is_case_insensitive)
{
    static const char* list[] = { "alpha", "beta", "gamma" };

    char buf[] = "BeTa";
    char* str = buf;
    int value = -1;

    tig_str_match_str_to_list(&str, list, 3, &value);

    CHECK_INT(value, 1);
}

TEST(match_str_to_list_advances_past_the_separator)
{
    static const char* list[] = { "alpha", "beta" };

    char buf[] = "alpha,beta";
    char* str = buf;
    int value = -1;

    tig_str_match_str_to_list(&str, list, 2, &value);
    CHECK_INT(value, 0);
    CHECK_REMAINDER(str, "beta");

    tig_str_match_str_to_list(&str, list, 2, &value);
    CHECK_INT(value, 1);
    CHECK_AT_END(str);
}

TEST(complex_value3_with_two_components_repeats_the_first)
{
    // Porting bug #339: the original stores *value1 as the third (0x531d2b).
    char buf[] = "(10@20)";
    char* str = buf;
    int a = -1, b = -1, c = -1;

    tig_str_parse_complex_value3(&str, '@', &a, &b, &c);

    CHECK_INT(a, 10);
    CHECK_INT(b, 20);
    CHECK_INT(c, 10);
}

TEST(flag_list_tolerates_a_separator_ending_the_field)
{
    // The original reads through a NULL here (0x5320d4-0x532101); CE stops.
    static const char* keys[] = { "alpha", "beta" };
    static const unsigned int values[] = { 0x1, 0x2 };
    char buf[] = "beta |";
    char* str = buf;
    unsigned int value = 0;

    tig_str_parse_flag_list(&str, keys, values, 2, &value);

    CHECK_INT((int)value, 0x2);
}

void test_str_parse_suite(void)
{
    RUN(parse_value_reads_a_plain_number);
    RUN(parse_value_stops_at_the_separator_and_advances);
    RUN(parse_value_skips_leading_whitespace);
    RUN(parse_value_restores_the_buffer_it_mutated);
    RUN(parse_value_yields_zero_for_a_non_number);

    RUN(parse_range_splits_on_the_hyphen);
    RUN(parse_range_without_a_hyphen_is_a_single_value);
    RUN(parse_range_cannot_express_a_negative_start);
    RUN(parse_range_advances_past_the_separator);

    RUN(named_value_matches_its_prefix_and_consumes_it);
    RUN(named_value_rejects_a_different_name);
    RUN(named_value_tolerates_a_null_cursor);
    RUN(named_value_requires_the_name_at_the_start);

    RUN(separator_is_configurable_and_sticks);

    RUN(match_str_to_list_is_case_insensitive);
    RUN(match_str_to_list_advances_past_the_separator);

    RUN(complex_value3_with_two_components_repeats_the_first);
    RUN(flag_list_tolerates_a_separator_ending_the_field);
}
