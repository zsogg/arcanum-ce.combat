// Tests for the sleep text filter (porting bug #322).
//
// A sleeping PC's broadcasts go through `text_filter_sleeping_process`
// (0x4F6BB0): each letter becomes the next character of the replacement line
// from text_filter_sleep.mes, except where a protected string matches, which
// is kept as written. The filter's state is set here directly, shaped like the
// shipped file (replacement line, then "i love sheep" and "troika").

#include <stdlib.h>
#include <string.h>

#include "test_harness.h"

void text_filter_sleeping_process(const char* src, char** dst);
extern char** text_filter_sleeping_strings;
extern char* text_filter_sleeping_replacement_str;
extern size_t text_filter_sleeping_replacement_length;
extern size_t text_filter_sleeping_replacement_index;

static char replacement[] = "rram";
static char protected_love[] = "i love sheep";
static char protected_troika[] = "troika";
static char* protected_strings[] = { protected_love, protected_troika, NULL };

static char* filter(const char* src)
{
    char* dst = NULL;

    text_filter_sleeping_process(src, &dst);
    return dst;
}

static void set_up(void)
{
    text_filter_sleeping_strings = protected_strings;
    text_filter_sleeping_replacement_str = replacement;
    text_filter_sleeping_replacement_length = strlen(replacement);
    text_filter_sleeping_replacement_index = 0;
}

TEST(letters_are_mumbled_and_the_rest_is_kept)
{
    char* out;

    set_up();
    out = filter("Go, now!");
    CHECK_STR(out, "rr, amr!");
    free(out);
}

TEST(the_replacement_index_carries_over_between_lines)
{
    char* out;

    set_up();
    free(filter("abc"));
    out = filter("ab");
    CHECK_STR(out, "mr");
    free(out);
}

TEST(protected_strings_survive_in_any_case)
{
    char* out;

    set_up();
    out = filter("I LOVE SHEEP, Troika");
    CHECK_STR(out, "I LOVE SHEEP, Troika");
    free(out);
}

TEST(a_protected_string_ending_the_line_stops_at_its_end)
{
    char* out;

    // The original reads past the input here (0x4f6c95 skips the end test).
    set_up();
    out = filter("go troika");
    CHECK_STR(out, "rr troika");
    free(out);
}

void test_text_filter_suite(void)
{
    RUN(letters_are_mumbled_and_the_rest_is_kept);
    RUN(the_replacement_index_carries_over_between_lines);
    RUN(protected_strings_survive_in_any_case);
    RUN(a_protected_string_ending_the_line_stops_at_its_end);
}
