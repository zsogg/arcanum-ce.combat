// Tests for `tig/debug.c` - the log line assembler.
//
// 62 lines, and almost all of the interest is in one function. Written
// 2026-08-29.
//
// **`tig_debug_printf` is CE's own code, not a transcription.** The original at
// 0x4FEB70 gates on a flag at 0x604614, formats straight into a 0x400 buffer at
// 0x604214, and dispatches to a table of up to eight output callbacks
// (0x604618..0x604638). It does no buffering. CE buffers because `SDL_Log`
// always appends a newline, which would break the engine's progress-message
// idiom - "Loading..." then "done, took %d ms\n" on one line. So the question
// here is not "does it match 0x4FEB70" but "does the accumulator behave".
//
// **The output is captured, not inspected on stderr.** `SDL_SetLogOutputFunction`
// installs a sink for the duration and it is restored afterwards, so these cases
// assert on the exact lines emitted rather than on a human reading a terminal.
//
// **SDL strips the trailing newline before the sink sees it**, measured
// 2026-08-29 - the module hands `SDL_Log` a string ending in `\n` and the sink
// receives it without. That is the same newline handling the module's NOTE is
// working around, seen from the other side, and it is why the expectations
// below carry no trailing newline while interior ones survive.

#include <stdio.h>
#include <string.h>

#include "tig/debug.h"
#include "tig/memory.h"

#include "test_harness.h"

#define DBG_MAX_LINES 8
#define DBG_LINE 2048

static char dbg_lines[DBG_MAX_LINES][DBG_LINE];
static int dbg_count;
static SDL_LogOutputFunction dbg_prev_func;
static void* dbg_prev_data;

static void SDLCALL dbg_sink(void* userdata, int category,
    SDL_LogPriority priority, const char* message)
{
    (void)userdata;
    (void)category;
    (void)priority;

    if (dbg_count < DBG_MAX_LINES) {
        SDL_strlcpy(dbg_lines[dbg_count], message, DBG_LINE);
    }
    dbg_count++;
}

static void dbg_begin(void)
{
    // Drain whatever a previous case left part-assembled, so a leaked partial
    // message cannot show up at the front of this case's first line.
    tig_debug_printf("\n");

    SDL_GetLogOutputFunction(&dbg_prev_func, &dbg_prev_data);
    SDL_SetLogOutputFunction(dbg_sink, NULL);
    dbg_count = 0;
    dbg_lines[0][0] = '\0';
}

static void dbg_end(void)
{
    SDL_SetLogOutputFunction(dbg_prev_func, dbg_prev_data);
}

// ---------------------------------------------------------------------------

// A message that ends in a newline is emitted at once. The buffer is handed to
// `SDL_Log` whole, newline included; SDL is what removes it before the sink.
TEST(a_message_ending_in_a_newline_is_emitted_immediately)
{
    dbg_begin();

    tig_debug_printf("hello\n");

    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "hello");

    dbg_end();
}

// The progress idiom the buffering exists for.
TEST(a_message_without_a_newline_waits_for_one)
{
    dbg_begin();

    tig_debug_printf("Loading...");
    CHECK_INT(dbg_count, 0);

    tig_debug_printf("done, took 42 ms\n");
    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "Loading...done, took 42 ms");

    dbg_end();
}

TEST(the_buffer_is_emptied_after_a_flush)
{
    dbg_begin();

    tig_debug_printf("first\n");
    tig_debug_printf("second\n");

    CHECK_INT(dbg_count, 2);
    CHECK_STR(dbg_lines[0], "first");
    CHECK_STR(dbg_lines[1], "second");

    dbg_end();
}

TEST(println_appends_the_newline_so_it_always_flushes)
{
    dbg_begin();

    tig_debug_println("a line");

    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "a line");

    dbg_end();
}

TEST(formatting_arguments_reach_the_output)
{
    dbg_begin();

    tig_debug_printf("%s %d %c\n", "value", 17, 'x');

    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "value 17 x");

    dbg_end();
}

// A newline in the MIDDLE does not flush - only a trailing one does, because
// the test is on the last character of the accumulated buffer.
TEST(only_a_trailing_newline_flushes)
{
    dbg_begin();

    tig_debug_printf("one\ntwo");
    CHECK_INT(dbg_count, 0);

    tig_debug_printf("\n");
    CHECK_INT(dbg_count, 1);
    // The interior newline survives; only the trailing one is stripped.
    CHECK_STR(dbg_lines[0], "one\ntwo");

    dbg_end();
}

// **The defect this suite was written around.** `SDL_strlcat` returns the
// *would-be* length - "the length of the string in `dst` plus the length of
// `src`" - so once the accumulation passes the 1024-byte buffer the old code
// tested `len == 1023` (false), then indexed `buffer[len - 1]` with `len - 1`
// past the end of the static. Under the ASan preset this case is the regression
// test; in a plain build it catches the other half, which is that the log wedged
// permanently once the buffer filled.
TEST(a_flood_without_newlines_still_flushes_and_stays_usable)
{
    char chunk[201];
    int index;

    dbg_begin();

    memset(chunk, 'x', sizeof(chunk) - 1);
    chunk[sizeof(chunk) - 1] = '\0';

    // 8 x 200 = 1600 characters, none of them a newline, so the 1024-byte
    // buffer is overrun well before the end.
    for (index = 0; index < 8; index++) {
        tig_debug_printf("%s", chunk);
    }

    // Whatever it did with the overflow, the module must still be able to emit.
    tig_debug_printf("end\n");
    CHECK(dbg_count >= 1);

    // And it must still work afterwards - the wedge was permanent.
    dbg_count = 0;
    tig_debug_printf("after the flood\n");
    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "after the flood");

    dbg_end();
}

// A single message longer than the buffer is truncated by `SDL_vsnprintf` into
// `tmp` first, so it can never carry more than 1023 characters in - but it must
// not wedge the accumulator either.
TEST(a_single_oversized_message_does_not_wedge_the_log)
{
    char huge[3000];

    dbg_begin();

    memset(huge, 'y', sizeof(huge) - 2);
    huge[sizeof(huge) - 2] = '\n';
    huge[sizeof(huge) - 1] = '\0';

    tig_debug_printf("%s", huge);

    dbg_count = 0;
    tig_debug_printf("still alive\n");
    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "still alive");

    dbg_end();
}

// An empty format contributes nothing and must not flush an empty buffer.
// `len != 0` is what stops a bare `tig_debug_printf("")` emitting a blank line -
// and it is also what stops `buffer[len - 1]` being `buffer[-1]`, which is why
// removing it is caught only under the ASan preset. Behaviourally the read
// returns whatever precedes the static and almost never looks like a newline, so
// a plain build sees nothing wrong.
TEST(an_empty_message_emits_nothing)
{
    dbg_begin();

    tig_debug_printf("%s", "");
    CHECK_INT(dbg_count, 0);

    tig_debug_printf("real\n");
    CHECK_INT(dbg_count, 1);
    CHECK_STR(dbg_lines[0], "real");

    dbg_end();
}

// `tig_debug_init` installs the debug printer as the MEMORY subsystem's output
// function - that is how a memory error reaches the log at all. Restored after,
// because `test_memory.c` installs its own catcher per case.
TEST(init_installs_the_memory_output_func_and_logs_a_banner)
{
    dbg_begin();

    CHECK_INT(tig_debug_init(NULL), 0);
    // A blank line then the date; the exact date is not assertable, its
    // presence is.
    CHECK(dbg_count >= 2);

    // And the installation is checked by USING it, not by trusting the call.
    // Asserting only on the banner passed even when init installed NULL -
    // measured 2026-08-29 as a MISSED mutation. `tig_memory_print_stats` goes
    // through whatever output function is installed, so if that is
    // `tig_debug_println` the text arrives in this suite's sink.
    dbg_count = 0;
    tig_memory_print_stats(0);
    CHECK(dbg_count > 0);
    CHECK(strstr(dbg_lines[0], "Dynamic memory usage") != NULL);

    tig_debug_exit();
    tig_memory_set_output_func(NULL);
    dbg_end();
}

// Release 1, R9: the log file a player attaches to a bug report.
#define DBG_LOG "./arcanum_test_debug.log"

// Read back in text mode: the log is written in text mode, so on Windows each
// line ends "\r\n" on disk, as a Windows player's editor expects, and reading
// it the same way turns that back into "\n" (R8, 2026-10-18).
static long dbg_file_text(char* out, size_t size)
{
    FILE* stream = fopen(DBG_LOG, "r");
    long length;

    out[0] = '\0';
    if (stream == NULL) {
        return -1;
    }
    length = (long)fread(out, 1, size - 1, stream);
    out[length] = '\0';
    fclose(stream);
    return length;
}

TEST(the_log_file_starts_at_the_first_line_and_keeps_every_later_one)
{
    static char text[4096];

    dbg_begin();
    CHECK_INT(tig_debug_init(NULL), 0);
    tig_debug_printf("before settings\n");
    CHECK(tig_debug_log_file_open(DBG_LOG));
    tig_debug_printf("after %d\n", 2);
    // Flushed per line: the text is on disk before the file is closed.
    CHECK(dbg_file_text(text, sizeof(text)) > 0);
    CHECK(strstr(text, "before settings\n") != NULL);
    CHECK(strstr(text, "after 2\n") != NULL);
    tig_debug_exit();
    tig_memory_set_output_func(NULL);
    dbg_end();
    remove(DBG_LOG);
}

TEST(lines_discarded_before_the_file_opens_are_not_in_it)
{
    static char text[4096];

    dbg_begin();
    CHECK_INT(tig_debug_init(NULL), 0);
    tig_debug_printf("kept only in memory\n");
    tig_debug_log_file_discard_early();
    tig_debug_printf("not kept either\n");
    CHECK(tig_debug_log_file_open(DBG_LOG));
    tig_debug_printf("written\n");
    dbg_file_text(text, sizeof(text));
    CHECK(strstr(text, "kept only in memory") == NULL);
    CHECK(strstr(text, "not kept either") == NULL);
    CHECK(strstr(text, "written\n") != NULL);
    tig_debug_exit();
    tig_memory_set_output_func(NULL);
    dbg_end();
    remove(DBG_LOG);
}

TEST(the_early_lines_are_capped_rather_than_grown_without_end)
{
    static char line[1000];
    FILE* stream;
    long size;
    int index;

    dbg_begin();
    CHECK_INT(tig_debug_init(NULL), 0);
    memset(line, 'x', sizeof(line) - 2);
    line[sizeof(line) - 2] = '\n';
    line[sizeof(line) - 1] = '\0';
    for (index = 0; index < 400; index++) { // ~400 KB, past the 256 KB cap
        tig_debug_printf("%s", line);
    }
    CHECK(tig_debug_log_file_open(DBG_LOG));
    stream = fopen(DBG_LOG, "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        fseek(stream, 0, SEEK_END);
        size = ftell(stream);
        fclose(stream);
        CHECK(size > 200 * 1024);
        CHECK(size <= 256 * 1024);
    }
    tig_debug_exit();
    tig_memory_set_output_func(NULL);
    dbg_end();
    remove(DBG_LOG);
}

void test_debug_suite(void)
{
    RUN(a_message_ending_in_a_newline_is_emitted_immediately);
    RUN(a_message_without_a_newline_waits_for_one);
    RUN(the_buffer_is_emptied_after_a_flush);
    RUN(println_appends_the_newline_so_it_always_flushes);
    RUN(formatting_arguments_reach_the_output);
    RUN(only_a_trailing_newline_flushes);
    RUN(a_flood_without_newlines_still_flushes_and_stays_usable);
    RUN(a_single_oversized_message_does_not_wedge_the_log);
    RUN(an_empty_message_emits_nothing);
    RUN(init_installs_the_memory_output_func_and_logs_a_banner);
    RUN(the_log_file_starts_at_the_first_line_and_keeps_every_later_one);
    RUN(lines_discarded_before_the_file_opens_are_not_in_it);
    RUN(the_early_lines_are_capped_rather_than_grown_without_end);
}
