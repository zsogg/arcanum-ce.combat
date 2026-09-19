// Tests for `tig/memory.c` - the debug allocator and its guard bytes.
//
// 529 lines, the largest module in P5.6's non-display group, and until now
// with no suite. Written 2026-08-29.
//
// **Read this before adding a case: almost none of this module is reachable
// in a shipped build.** `memory.h` routes `MALLOC`/`FREE`/`CALLOC`/`REALLOC`/
// `STRDUP` to `tig_memory_*` only when `TIG_DEBUG_MEMORY` is defined, and
// nothing in the tree defines it - so every allocation in the engine goes to
// the CRT and the block list is permanently empty. Upstream's own header note
// says as much ("allocation functions of this module are never used"). What
// *is* live is the reporting half: `tig_debug_init` installs the output
// function, `main.c` and `mainmenu_ui.c` print stats at shutdown, and
// `tig_art_init` reads `tig_memory_get_system_status` to size the art cache.
//
// That reachability fact is why the defect this suite found is not in the
// numbered ledger, and it is also why the suite is worth having anyway: the
// switch is a deliberate, documented one, and whoever flips it should inherit
// a guard checker that works.
//
// **The block list is exclusively ours.** Nothing else in the test binary
// calls `tig_memory_alloc`, so counts and the list order are deterministic and
// a case may assert on them. Each case brackets itself with `mem_begin` /
// `mem_end`, which reset the statistics and assert the list came back empty.
//
// **How the fatal paths are tested at all.** `tig_memory_fatal_error` is
// `SDL_NORETURN` and ends in `exit(EXIT_FAILURE)`, but it hands the formatted
// message to `tig_memory_output_func` *first*. Installing an output function
// that records the message and `longjmp`s escapes before the `exit`, which
// makes the error contract - the module's actual product - observable without
// forking. See `EXPECT_FATAL`.
//
// **What that leaves behind, stated rather than hoped.** Every fatal fires
// with the module mutex held once (`free`, `realloc` and `validate_all` all
// take it on entry), so a caught fatal leaks one level of lock depth. SDL
// mutexes are recursive by contract - "it is legal for the owning thread to
// lock an already-locked mutex" - and this suite is single-threaded, so every
// later lock still succeeds. It is an artifact of the test, not of the module,
// and it cannot deadlock anything here.
//
// **Requires a debug build.** `tig_memory_stats`/`_reset_stats`/
// `_validate_memory_leaks` are upstream's own test hooks behind `#ifndef
// NDEBUG`. Tests are only ever built with `linux-x64-debug` and
// `linux-x64-asan`, both of which are `CMAKE_BUILD_TYPE=Debug`.

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "tig/memory.h"

#include "test_harness.h"

// ---------------------------------------------------------------------------
// Capturing what the module prints.

#define MEM_MAX_LINES 32
#define MEM_LINE 1024

static char mem_lines[MEM_MAX_LINES][MEM_LINE];
static int mem_line_count;

// Shared by both output callbacks below, so it is declared before either.
static jmp_buf mem_fatal_jmp;
static volatile int mem_fatal_armed;
static volatile int mem_fatal_fired;
static char mem_fatal_message[MEM_LINE];

static void mem_fatal_escaped(void);

static void mem_capture_output(const char* str)
{
    if (mem_line_count < MEM_MAX_LINES) {
        strncpy(mem_lines[mem_line_count], str, MEM_LINE - 1);
        mem_lines[mem_line_count][MEM_LINE - 1] = '\0';
        mem_line_count++;
    }

    // No reporting case expects an error, so one arriving here is a fatal on
    // its way to `exit`. Recorded, then escaped through the case guard.
    if (strstr(str, "TIG Memory: Error") != NULL) {
        strncpy(mem_fatal_message, str, MEM_LINE - 1);
        mem_fatal_message[MEM_LINE - 1] = '\0';
        mem_fatal_fired = 1;
        mem_fatal_escaped();
    }
}

static void mem_capture_begin(void)
{
    mem_line_count = 0;
    tig_memory_set_output_func(mem_capture_output);
}

// Index of the first captured line containing `needle`, or -1.
static int mem_line_with(const char* needle)
{
    int index;

    for (index = 0; index < mem_line_count; index++) {
        if (strstr(mem_lines[index], needle) != NULL) {
            return index;
        }
    }

    return -1;
}

static int mem_lines_with(const char* needle)
{
    int index;
    int found = 0;

    for (index = 0; index < mem_line_count; index++) {
        if (strstr(mem_lines[index], needle) != NULL) {
            found++;
        }
    }

    return found;
}

// Pull the first `%zu` out of a captured line, so the printed accounting can be
// compared with what `tig_memory_stats` reports. Returns -1 when absent.
static long long mem_number_after(const char* needle)
{
    int index = mem_line_with(needle);
    const char* pch;

    if (index < 0) {
        return -1;
    }

    for (pch = mem_lines[index]; *pch != '\0'; pch++) {
        if (*pch >= '0' && *pch <= '9') {
            return strtoll(pch, NULL, 10);
        }
    }

    return -1;
}

// ---------------------------------------------------------------------------
// Catching a fatal.

// The outer net. A fatal that no `EXPECT_*` armed for would otherwise reach
// `exit(EXIT_FAILURE)` and take the whole binary down mid-case - and because
// `test_run` prints its `FAIL` line only after the body returns, the run would
// end with no summary and no named case, looking exactly like a run that found
// nothing. That is the 2026-08-25 `test_check` buffering defect wearing a
// different hat, and it was measured here: reverting the guard walk produced
// two correct failing checks and then a dead process, which the first pass of
// the mutation harness scored as MISSED.
static jmp_buf mem_case_jmp;
static volatile int mem_case_armed;

static void mem_fatal_escaped(void)
{
    if (mem_case_armed) {
        mem_case_armed = 0;
        longjmp(mem_case_jmp, 1);
    }
}

static void mem_fatal_output(const char* str)
{
    strncpy(mem_fatal_message, str, MEM_LINE - 1);
    mem_fatal_message[MEM_LINE - 1] = '\0';
    mem_fatal_fired = 1;

    if (mem_fatal_armed) {
        mem_fatal_armed = 0;
        longjmp(mem_fatal_jmp, 1);
    }

    mem_fatal_escaped();
}

// Run `stmt` expecting it to reach `tig_memory_fatal_error`. Leaves the message
// in `mem_fatal_message` and sets `mem_fatal_fired`. A `stmt` that returns
// normally fails the check rather than passing quietly, which is the mutation
// that would otherwise make every fatal case vacuous.
#define EXPECT_FATAL(stmt)                                               \
    do {                                                                 \
        mem_fatal_fired = 0;                                             \
        mem_fatal_message[0] = '\0';                                     \
        tig_memory_set_output_func(mem_fatal_output);                    \
        mem_fatal_armed = 1;                                             \
        if (setjmp(mem_fatal_jmp) == 0) {                                \
            stmt;                                                        \
            mem_fatal_armed = 0;                                         \
        }                                                                \
        mem_fatal_armed = 0;                                             \
        test_check(mem_fatal_fired != 0, #stmt " reaches a fatal error", \
            __FILE__, __LINE__);                                         \
    } while (0)

// Run `stmt` expecting it NOT to fatal.
#define EXPECT_NO_FATAL(stmt)                                                \
    do {                                                                     \
        mem_fatal_fired = 0;                                                 \
        mem_fatal_message[0] = '\0';                                         \
        tig_memory_set_output_func(mem_fatal_output);                        \
        mem_fatal_armed = 1;                                                 \
        if (setjmp(mem_fatal_jmp) == 0) {                                    \
            stmt;                                                            \
        }                                                                    \
        mem_fatal_armed = 0;                                                 \
        test_check(mem_fatal_fired == 0, #stmt " completes without a fatal", \
            __FILE__, __LINE__);                                             \
    } while (0)

// ---------------------------------------------------------------------------
// Per-case bracketing.

static void mem_begin(void)
{
    // The catcher, not NULL. `tig_memory_fatal_error` calls the output
    // function only if one is set and then exits either way, so a fatal raised
    // with none installed kills the binary having printed NOTHING - no
    // message, no failing check, no case name. Measured: two mutations
    // (`guard-bytes-swapped-at-alloc`, `realloc-does-not-rehead-the-list`)
    // scored MISSED for exactly this reason, dying in the first case of the
    // suite before any diagnosis could be written.
    tig_memory_set_output_func(mem_fatal_output);
    tig_memory_reset_stats();
}

static void mem_end(void)
{
    TigMemoryStats stats;

    tig_memory_stats(&stats);
    // Anything still held here would follow the case into the next one, since
    // the block list is a file static.
    CHECK_INT(stats.current_blocks, 0);
    CHECK_INT(stats.current_allocated, 0);

    tig_memory_reset_stats();
    tig_memory_set_output_func(NULL);
}

#define AT __FILE__, __LINE__

// Every case runs through this, so a fatal nothing armed for ends the case
// rather than the process. Without it the first mutation of the guard walk
// killed the binary two checks into its diagnosis and the run reported no
// failing case at all.
//
// After an escape the block list may still hold whatever the case had
// outstanding, and there is no API to drain it - so later cases can report
// non-empty `mem_end` counts. Under a mutation that is expected: the FIRST
// escaped fatal is the diagnosis and what follows is wreckage.
static void (*mem_guard_target)(void);

static void mem_guarded_case(void)
{
    mem_case_armed = 1;
    if (setjmp(mem_case_jmp) == 0) {
        mem_guard_target();
    } else {
        test_check(0, "a fatal error escaped this case", __FILE__, __LINE__);
        printf("      message: %s\n", mem_fatal_message);
        fflush(stdout);
    }
    mem_case_armed = 0;
    tig_memory_set_output_func(NULL);
}

#define MEM_RUN(name)                      \
    do {                                   \
        mem_guard_target = name;           \
        test_run(#name, mem_guarded_case); \
    } while (0)

// ---------------------------------------------------------------------------
// Allocation and the accounting behind it.

TEST(alloc_hands_back_a_block_and_counts_it)
{
    TigMemoryStats stats;
    void* ptr;

    mem_begin();

    ptr = tig_memory_alloc(64, AT);
    CHECK(ptr != NULL);

    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 64);

    tig_memory_free(ptr, AT);

    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 0);
    CHECK_INT(stats.current_allocated, 0);

    mem_end();
}

// The guards bracket the payload exactly, which is what makes `data[size]` and
// `data[-1]` writable without leaving the malloc'd region - the property the
// corruption cases below rely on, and the reason ASan does not object to them.
TEST(guards_bracket_the_payload_with_the_documented_bytes)
{
    unsigned char* ptr;
    int index;

    mem_begin();

    ptr = (unsigned char*)tig_memory_alloc(24, AT);

    // START_GUARD_SIZE and END_GUARD_SIZE are both `sizeof(void*)`.
    for (index = 1; index <= (int)sizeof(void*); index++) {
        CHECK_INT(ptr[-index], 0xAA);
    }
    for (index = 0; index < (int)sizeof(void*); index++) {
        CHECK_INT(ptr[24 + index], 0xBB);
    }

    tig_memory_free(ptr, AT);
    mem_end();
}

TEST(the_maxima_are_high_water_marks)
{
    TigMemoryStats stats;
    void* a;
    void* b;
    void* c;

    mem_begin();

    a = tig_memory_alloc(10, AT);
    b = tig_memory_alloc(20, AT);
    c = tig_memory_alloc(30, AT);

    tig_memory_stats(&stats);
    CHECK_INT(stats.max_blocks, 3);
    CHECK_INT(stats.max_allocated, 60);

    tig_memory_free(b, AT);
    tig_memory_free(c, AT);

    // Freeing moves the current figures and must leave the peaks alone.
    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 10);
    CHECK_INT(stats.max_blocks, 3);
    CHECK_INT(stats.max_allocated, 60);

    tig_memory_free(a, AT);
    mem_end();
}

// `calloc` multiplies without checking for overflow and zeroes the product -
// faithful: the original computes it with a plain `imul` at 0x4FE3E8 and clears
// exactly that many bytes with `rep stos`.
TEST(calloc_zeroes_the_product_of_count_and_size)
{
    TigMemoryStats stats;
    unsigned char* ptr;
    int index;
    int nonzero = 0;

    mem_begin();

    ptr = (unsigned char*)tig_memory_calloc(8, 16, AT);
    for (index = 0; index < 8 * 16; index++) {
        if (ptr[index] != 0) {
            nonzero++;
        }
    }
    CHECK_INT(nonzero, 0);

    // One block of 128 bytes, not eight blocks of 16.
    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 128);

    tig_memory_free(ptr, AT);
    mem_end();
}

TEST(strdup_copies_the_string_and_accounts_the_terminator)
{
    TigMemoryStats stats;
    char* copy;

    mem_begin();

    copy = tig_memory_strdup("Tarant", AT);
    CHECK_STR(copy, "Tarant");

    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 7);

    tig_memory_free(copy, AT);
    mem_end();
}

// A zero-byte block is a real block: it occupies a list entry and its two
// guards sit adjacent. Worth pinning because it is the input that makes
// `tig_memory_validate_memory_leaks` answer wrongly - see below.
TEST(a_zero_byte_allocation_is_still_a_block)
{
    TigMemoryStats stats;
    unsigned char* ptr;

    mem_begin();

    ptr = (unsigned char*)tig_memory_alloc(0, AT);
    CHECK(ptr != NULL);
    CHECK_INT(ptr[-1], 0xAA);
    CHECK_INT(ptr[0], 0xBB);

    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 0);

    tig_memory_free(ptr, AT);
    mem_end();
}

// ---------------------------------------------------------------------------
// Reallocation.

TEST(realloc_of_null_allocates)
{
    TigMemoryStats stats;
    void* ptr;

    mem_begin();

    ptr = tig_memory_realloc(NULL, 48, AT);
    CHECK(ptr != NULL);

    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 48);

    tig_memory_free(ptr, AT);
    mem_end();
}

TEST(realloc_grows_preserving_contents_and_re_guards_the_end)
{
    TigMemoryStats stats;
    unsigned char* ptr;
    int index;
    int kept = 1;

    mem_begin();

    ptr = (unsigned char*)tig_memory_alloc(16, AT);
    for (index = 0; index < 16; index++) {
        ptr[index] = (unsigned char)(index + 1);
    }

    ptr = (unsigned char*)tig_memory_realloc(ptr, 64, AT);
    for (index = 0; index < 16; index++) {
        if (ptr[index] != (unsigned char)(index + 1)) {
            kept = 0;
        }
    }
    CHECK_INT(kept, 1);

    // The end guard follows the payload to its new place; the start guard is
    // carried by `realloc` rather than rewritten (the original does the same -
    // it writes only 0xBBBBBBBB, at 0x4FE6D1).
    CHECK_INT(ptr[-1], 0xAA);
    for (index = 0; index < (int)sizeof(void*); index++) {
        CHECK_INT(ptr[64 + index], 0xBB);
    }

    // Still one block; the allocated figure moves by the delta.
    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 64);

    tig_memory_free(ptr, AT);
    mem_end();
}

// Shrinking subtracts. `current_allocated += size - old_size` is `size_t`
// arithmetic that wraps on the subtraction and unwraps on the addition, which
// is correct modulo 2^N and matches the original's `sub edi,ebx; add eax,edi`
// at 0x4FE6E2.
TEST(realloc_shrinks_and_the_accounting_goes_down)
{
    TigMemoryStats stats;
    void* ptr;

    mem_begin();

    ptr = tig_memory_alloc(256, AT);
    ptr = tig_memory_realloc(ptr, 32, AT);

    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 32);
    // The peak is unchanged by a shrink.
    CHECK_INT(stats.max_allocated, 256);

    tig_memory_free(ptr, AT);
    mem_end();
}

// `realloc` unlinks the block and pushes it back on the front, so it becomes
// the most recently listed. Faithful: 0x4FE6C8/0x4FE6CB re-head the list the
// same way `alloc` does. Observable through the ALL_BLOCKS listing, which walks
// from the head.
TEST(realloc_moves_the_block_to_the_head_of_the_list)
{
    void* first;
    void* second;
    int first_index;
    int second_index;

    mem_begin();

    first = tig_memory_alloc(11, AT);
    second = tig_memory_alloc(22, AT);

    // Freshly allocated, `second` is listed before `first`.
    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);
    first_index = mem_line_with("11 bytes at");
    second_index = mem_line_with("22 bytes at");
    CHECK(first_index >= 0);
    CHECK(second_index >= 0);
    CHECK(second_index < first_index);

    // Reallocating `first` puts it back on the front.
    first = tig_memory_realloc(first, 33, AT);

    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);
    first_index = mem_line_with("33 bytes at");
    second_index = mem_line_with("22 bytes at");
    CHECK(first_index >= 0);
    CHECK(second_index >= 0);
    CHECK(first_index < second_index);

    tig_memory_set_output_func(NULL);
    tig_memory_free(first, AT);
    tig_memory_free(second, AT);
    mem_end();
}

// ---------------------------------------------------------------------------
// The error contract.

// Not a no-op. libc's `free(NULL)` is defined and does nothing; this one walks
// the list for a block whose `data` is NULL, finds none, and kills the process.
// Faithful - the original has no null check either, going straight from the
// init test to the list walk at 0x4FE452.
TEST(freeing_null_is_fatal_rather_than_a_no_op)
{
    mem_begin();

    EXPECT_FATAL(tig_memory_free(NULL, AT));
    CHECK(strstr(mem_fatal_message, "unable to locate block to free") != NULL);

    mem_end();
}

TEST(freeing_a_pointer_the_module_never_handed_out_is_fatal)
{
    static int foreign;

    mem_begin();

    EXPECT_FATAL(tig_memory_free(&foreign, AT));
    CHECK(strstr(mem_fatal_message, "unable to locate block to free") != NULL);

    mem_end();
}

TEST(reallocating_an_unknown_pointer_is_fatal)
{
    static int foreign;

    mem_begin();

    EXPECT_FATAL(tig_memory_realloc(&foreign, 8, AT));
    CHECK(strstr(mem_fatal_message, "unable to locate block to reallocate") != NULL);

    mem_end();
}

// Every byte of the start guard is checked, not just one. Before 2026-08-29 the
// loop never advanced its pointer, so only `data[-sizeof(void*)]` - the byte
// furthest from the block - was ever read, and the likeliest underrun of all,
// one byte written just below the payload, went undetected. The original walks
// the bytes (`inc edi` at 0x4FEAD4).
TEST(every_start_guard_byte_is_checked)
{
    volatile int offset;

    mem_begin();

    for (offset = 1; offset <= (int)sizeof(void*); offset++) {
        unsigned char* ptr = (unsigned char*)tig_memory_alloc(16, AT);
        unsigned char keep = ptr[-offset];

        ptr[-offset] = 0x00;
        EXPECT_FATAL(tig_memory_free(ptr, AT));
        CHECK(strstr(mem_fatal_message, "starting guard byte") != NULL);

        // The fatal fires before any accounting moves, so the block is still
        // live and still ours to dispose of.
        ptr[-offset] = keep;
        tig_memory_free(ptr, AT);
    }

    mem_end();
}

// The same for the end guard: `inc edi` at 0x4FEB03.
TEST(every_end_guard_byte_is_checked)
{
    volatile int offset;

    mem_begin();

    for (offset = 0; offset < (int)sizeof(void*); offset++) {
        unsigned char* ptr = (unsigned char*)tig_memory_alloc(16, AT);
        unsigned char keep = ptr[16 + offset];

        ptr[16 + offset] = 0x00;
        EXPECT_FATAL(tig_memory_free(ptr, AT));
        CHECK(strstr(mem_fatal_message, "ending guard byte") != NULL);

        ptr[16 + offset] = keep;
        tig_memory_free(ptr, AT);
    }

    mem_end();
}

// The message names which byte went. It was wrong for every byte but the first
// while the pointer was not advancing, since `index` counted while `*grd`
// did not move.
TEST(the_fatal_message_names_the_guard_byte_that_went)
{
    unsigned char* ptr;

    mem_begin();

    ptr = (unsigned char*)tig_memory_alloc(16, AT);
    // Byte 0 of the end guard is `data[size]`; byte 1 is the next one along.
    ptr[16 + 1] = 0x00;
    EXPECT_FATAL(tig_memory_free(ptr, AT));
    CHECK(strstr(mem_fatal_message, "ending guard byte 1 ") != NULL);
    ptr[16 + 1] = 0xBB;
    tig_memory_free(ptr, AT);

    mem_end();
}

// The asymmetry is Troika's: `free` validates before touching anything
// (0x4FE48F calls 0x4FEAA0), `realloc` never validates at all - 0x4FE5F0 walks
// the list, unlinks, and goes straight to the CRT. A corrupt block can
// therefore be reallocated without complaint and only fails when freed.
TEST(realloc_does_not_validate_guards_but_free_does)
{
    unsigned char* ptr;

    mem_begin();

    ptr = (unsigned char*)tig_memory_alloc(16, AT);
    ptr[-1] = 0x00;

    EXPECT_NO_FATAL(ptr = (unsigned char*)tig_memory_realloc(ptr, 32, AT));
    CHECK(ptr != NULL);
    // The corruption travels with the payload.
    CHECK_INT(ptr[-1], 0x00);

    EXPECT_FATAL(tig_memory_free(ptr, AT));
    CHECK(strstr(mem_fatal_message, "starting guard byte") != NULL);

    ptr[-1] = 0xAA;
    tig_memory_free(ptr, AT);

    mem_end();
}

// `validate_all` walks the whole list, so a block corrupted anywhere in it is
// caught without that block being the one passed to anything.
TEST(validate_all_checks_every_block_not_just_the_head)
{
    unsigned char* first;
    unsigned char* second;
    unsigned char* third;

    mem_begin();

    first = (unsigned char*)tig_memory_alloc(8, AT);
    second = (unsigned char*)tig_memory_alloc(8, AT);
    third = (unsigned char*)tig_memory_alloc(8, AT);

    EXPECT_NO_FATAL(tig_memory_validate_all(AT));

    // `first` is deepest in the list - the head is `third`.
    first[8] = 0x00;
    EXPECT_FATAL(tig_memory_validate_all(AT));
    CHECK(strstr(mem_fatal_message, "ending guard byte") != NULL);
    first[8] = 0xBB;

    EXPECT_NO_FATAL(tig_memory_validate_all(AT));

    tig_memory_free(first, AT);
    tig_memory_free(second, AT);
    tig_memory_free(third, AT);
    mem_end();
}

// The message carries the allocation site as well as the detection site, which
// is the whole reason `__FILE__`/`__LINE__` are threaded through every entry
// point.
TEST(the_fatal_message_names_the_allocation_site)
{
    unsigned char* ptr;

    mem_begin();

    ptr = (unsigned char*)tig_memory_alloc(8, "alloc_site.c", 4321);
    ptr[8] = 0x00;

    EXPECT_FATAL(tig_memory_validate_all("detect_site.c", 8765));
    CHECK(strstr(mem_fatal_message, "alloc_site.c:4321") != NULL);
    CHECK(strstr(mem_fatal_message, "detect_site.c:8765") != NULL);

    ptr[8] = 0xBB;
    tig_memory_free(ptr, AT);
    mem_end();
}

// ---------------------------------------------------------------------------
// Reporting.

// With no output function installed the module returns immediately - which is
// the state every shipped build is in until `tig_debug_init` runs.
TEST(print_stats_without_an_output_func_does_nothing)
{
    void* ptr;

    mem_begin();

    ptr = tig_memory_alloc(16, AT);
    tig_memory_set_output_func(NULL);
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_OVERHEAD
        | TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);

    tig_memory_free(ptr, AT);
    mem_end();
}

TEST(print_stats_reports_the_same_figures_as_the_stats_hook)
{
    TigMemoryStats stats;
    void* a;
    void* b;

    mem_begin();

    a = tig_memory_alloc(100, AT);
    b = tig_memory_alloc(200, AT);
    tig_memory_stats(&stats);

    mem_capture_begin();
    tig_memory_print_stats(0);

    CHECK(mem_line_with("Dynamic memory usage") >= 0);
    CHECK_INT(mem_number_after("Peak program memory usage"), (long long)stats.max_blocks);
    CHECK_INT(mem_number_after("Current program memory usage"), (long long)stats.current_blocks);
    // Neither overhead line is printed without the flag.
    CHECK_INT(mem_line_with("overhead"), -1);

    tig_memory_set_output_func(NULL);
    tig_memory_free(a, AT);
    tig_memory_free(b, AT);
    mem_end();
}

// The overhead is a flat per-block charge, so it scales exactly with the block
// count and not at all with the sizes. Pinned as that relationship rather than
// as a constant on purpose: it is `sizeof(TigMemoryBlock)` plus two guards,
// which is 7 pointers wide - 28 bytes in the original (`sub ecx,0x1c` at
// 0x4FE4AA) and 56 in CE's 64-bit build. An absolute here would be a number
// that means something different on each side.
TEST(the_overhead_is_a_flat_per_block_charge)
{
    long long one;
    long long three;
    void* a;
    void* b;
    void* c;

    mem_begin();

    a = tig_memory_alloc(8, AT);
    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_OVERHEAD);
    one = mem_number_after("Current memory management overhead");
    CHECK(one > 0);
    CHECK_INT(one, (long long)(7 * sizeof(void*)));

    // Two more blocks, and much larger ones: the overhead must treble, not grow
    // with the payload.
    b = tig_memory_alloc(4096, AT);
    c = tig_memory_alloc(65536, AT);
    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_OVERHEAD);
    three = mem_number_after("Current memory management overhead");
    CHECK_INT(three, one * 3);

    tig_memory_set_output_func(NULL);
    tig_memory_free(a, AT);
    tig_memory_free(b, AT);
    tig_memory_free(c, AT);
    mem_end();
}

TEST(all_blocks_lists_every_block_with_its_site)
{
    void* a;
    void* b;

    mem_begin();

    a = tig_memory_alloc(100, "one.c", 11);
    b = tig_memory_alloc(200, "two.c", 22);

    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS);

    CHECK(mem_line_with("one.c:11:") >= 0);
    CHECK(mem_line_with("two.c:22:") >= 0);
    CHECK(mem_line_with("100 bytes at") >= 0);
    CHECK(mem_line_with("200 bytes at") >= 0);

    tig_memory_set_output_func(NULL);
    tig_memory_free(a, AT);
    tig_memory_free(b, AT);
    mem_end();
}

// Grouped output collapses blocks sharing a file and line into one row with a
// count and a total - which is what `mainmenu_ui.c:6370` prints.
TEST(grouped_blocks_collapses_a_shared_site_into_one_row)
{
    void* a;
    void* b;
    void* c;

    mem_begin();

    a = tig_memory_alloc(10, "same.c", 7);
    b = tig_memory_alloc(20, "same.c", 7);
    c = tig_memory_alloc(30, "other.c", 9);

    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_GROUPED_BLOCKS);

    CHECK_INT(mem_lines_with("same.c:7:"), 1);
    CHECK(mem_line_with("same.c:7:  2 blocks totaling 30 bytes") >= 0);
    CHECK(mem_line_with("other.c:9:  1 blocks totaling 30 bytes") >= 0);

    tig_memory_set_output_func(NULL);
    tig_memory_free(a, AT);
    tig_memory_free(b, AT);
    tig_memory_free(c, AT);
    mem_end();
}

// Same file, different lines: not one group. This is the half of
// `tig_memory_sort_blocks` that the grouping loop leans on, and a comparator
// that ignored the line would still pass the case above.
// The lines are allocated ALTERNATING on purpose. The grouping loop only ever
// compares neighbours, so it is `tig_memory_sort_blocks`' line tiebreak that
// has to bring same-line blocks together first; with two blocks the loop
// separates them whether the comparator looked at the line or not, and the
// case is vacuous. Measured: dropping the tiebreak leaves this input in list
// order and produces five single-block rows instead of two grouped ones.
TEST(grouping_gathers_one_files_lines_however_they_were_allocated)
{
    void* p[5];
    int index;

    mem_begin();

    p[0] = tig_memory_alloc(1, "same.c", 1);
    p[1] = tig_memory_alloc(2, "same.c", 2);
    p[2] = tig_memory_alloc(4, "same.c", 1);
    p[3] = tig_memory_alloc(8, "same.c", 2);
    p[4] = tig_memory_alloc(16, "same.c", 1);

    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_GROUPED_BLOCKS);

    // One row per site, not one per block.
    CHECK_INT(mem_lines_with("same.c:1:"), 1);
    CHECK_INT(mem_lines_with("same.c:2:"), 1);
    CHECK(mem_line_with("same.c:1:  3 blocks totaling 21 bytes") >= 0);
    CHECK(mem_line_with("same.c:2:  2 blocks totaling 10 bytes") >= 0);

    tig_memory_set_output_func(NULL);
    for (index = 0; index < 5; index++) {
        tig_memory_free(p[index], AT);
    }
    mem_end();
}

// The comparator is `SDL_strcasecmp`, so two spellings of one file group
// together - the original calls 0x58ADB0, the same case-insensitive compare
// that decides archive load order in `tig_file_list_create`.
TEST(grouping_matches_file_names_case_insensitively)
{
    void* a;
    void* b;
    void* c;

    mem_begin();

    // The third name sorts BETWEEN the other two under a case-sensitive
    // comparator ('M' 0x4D < 'a' 0x61 < 'm' 0x6D) and outside them under a
    // case-insensitive one. Without it the pair ends up adjacent either way and
    // the grouping loop's own `SDL_strcasecmp` joins them regardless, so the
    // comparator's case-folding is untested - measured, as a MISSED mutation.
    a = tig_memory_alloc(10, "Mixed.c", 5);
    c = tig_memory_alloc(40, "aaa.c", 5);
    b = tig_memory_alloc(20, "mixed.c", 5);

    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_GROUPED_BLOCKS);

    // The needle carries the site. "2 blocks totaling 30 bytes" alone also
    // matches the unconditional summary line, which says the same thing about
    // the whole heap - so the bare form passed whether the two names grouped or
    // not. Measured: a case-sensitive comparator did not fail this check until
    // the ":5:" was added.
    CHECK(mem_line_with(":5:  2 blocks totaling 30 bytes") >= 0);
    // ...and if they had not grouped there would be two single-block rows.
    CHECK_INT(mem_lines_with("blocks totaling 10 bytes"), 0);

    tig_memory_set_output_func(NULL);
    tig_memory_free(a, AT);
    tig_memory_free(b, AT);
    tig_memory_free(c, AT);
    mem_end();
}

// The header calls the two listing flags mutually exclusive; the code makes
// ALL_BLOCKS win, because GROUPED_BLOCKS is reached through an `else if`.
TEST(all_blocks_wins_when_both_listing_flags_are_set)
{
    void* a;
    void* b;

    mem_begin();

    a = tig_memory_alloc(10, "same.c", 3);
    b = tig_memory_alloc(20, "same.c", 3);

    mem_capture_begin();
    tig_memory_print_stats(TIG_MEMORY_STATS_PRINT_ALL_BLOCKS
        | TIG_MEMORY_STATS_PRINT_GROUPED_BLOCKS);

    // Two per-block rows, and no grouped row. The needle has to carry the
    // site: the unconditional summary line is also worded "N blocks totaling
    // M bytes", so a bare "blocks totaling" matches whether grouping ran or
    // not - which made this check pass for the wrong reason on first write.
    CHECK_INT(mem_lines_with("same.c:3:"), 2);
    CHECK(mem_line_with("bytes at") >= 0);
    CHECK_INT(mem_line_with("same.c:3:  2 blocks totaling"), -1);

    tig_memory_set_output_func(NULL);
    tig_memory_free(a, AT);
    tig_memory_free(b, AT);
    mem_end();
}

// ---------------------------------------------------------------------------
// The leak hook, and the input it gets wrong.

TEST(validate_memory_leaks_is_true_when_nothing_is_held)
{
    mem_begin();
    CHECK(tig_memory_validate_memory_leaks());
    mem_end();
}

TEST(validate_memory_leaks_is_false_while_a_block_is_outstanding)
{
    void* ptr;

    mem_begin();

    ptr = tig_memory_alloc(64, AT);
    CHECK(!tig_memory_validate_memory_leaks());

    tig_memory_free(ptr, AT);
    CHECK(tig_memory_validate_memory_leaks());

    mem_end();
}

// `current_blocks == 0 || current_allocated == 0` - an `||` where the reader
// expects `&&`. A leaked zero-byte block satisfies the second half, so the hook
// reports "no leak" while holding a block. Upstream's, unreachable with
// `TIG_DEBUG_MEMORY` undefined, and pinned rather than fixed because tightening
// it would change what a debug build reports rather than what it does.
TEST(validate_memory_leaks_misses_an_outstanding_zero_byte_block)
{
    TigMemoryStats stats;
    void* ptr;

    mem_begin();

    ptr = tig_memory_alloc(0, AT);
    tig_memory_stats(&stats);
    CHECK_INT(stats.current_blocks, 1);
    CHECK_INT(stats.current_allocated, 0);

    CHECK(tig_memory_validate_memory_leaks());

    tig_memory_free(ptr, AT);
    mem_end();
}

// ---------------------------------------------------------------------------
// The one live caller.

// `tig_art_init` reads this to size the art cache. CE answers with SDL's
// installed-RAM figure for both, where the original calls `GlobalMemoryStatus`
// and gets two different numbers - a documented CE deviation. It is harmless at
// the only call site, which uses `total` for both of its budgets and never
// reads `available` (art.c:497-501), and that is what makes it worth stating.
TEST(get_system_status_answers_the_same_figure_for_both)
{
    size_t total = 0;
    size_t available = 0;

    mem_begin();

    tig_memory_get_system_status(&total, &available);
    CHECK(total > 0);
    CHECK(available == total);
    // A whole number of megabytes: SDL reports MB and the module scales it.
    CHECK_INT(total % (1024 * 1024), 0);

    mem_end();
}

void test_memory_suite(void)
{
    MEM_RUN(alloc_hands_back_a_block_and_counts_it);
    MEM_RUN(guards_bracket_the_payload_with_the_documented_bytes);
    MEM_RUN(the_maxima_are_high_water_marks);
    MEM_RUN(calloc_zeroes_the_product_of_count_and_size);
    MEM_RUN(strdup_copies_the_string_and_accounts_the_terminator);
    MEM_RUN(a_zero_byte_allocation_is_still_a_block);
    MEM_RUN(realloc_of_null_allocates);
    MEM_RUN(realloc_grows_preserving_contents_and_re_guards_the_end);
    MEM_RUN(realloc_shrinks_and_the_accounting_goes_down);
    MEM_RUN(realloc_moves_the_block_to_the_head_of_the_list);
    MEM_RUN(freeing_null_is_fatal_rather_than_a_no_op);
    MEM_RUN(freeing_a_pointer_the_module_never_handed_out_is_fatal);
    MEM_RUN(reallocating_an_unknown_pointer_is_fatal);
    MEM_RUN(every_start_guard_byte_is_checked);
    MEM_RUN(every_end_guard_byte_is_checked);
    MEM_RUN(the_fatal_message_names_the_guard_byte_that_went);
    MEM_RUN(realloc_does_not_validate_guards_but_free_does);
    MEM_RUN(validate_all_checks_every_block_not_just_the_head);
    MEM_RUN(the_fatal_message_names_the_allocation_site);
    MEM_RUN(print_stats_without_an_output_func_does_nothing);
    MEM_RUN(print_stats_reports_the_same_figures_as_the_stats_hook);
    MEM_RUN(the_overhead_is_a_flat_per_block_charge);
    MEM_RUN(all_blocks_lists_every_block_with_its_site);
    MEM_RUN(grouped_blocks_collapses_a_shared_site_into_one_row);
    MEM_RUN(grouping_gathers_one_files_lines_however_they_were_allocated);
    MEM_RUN(grouping_matches_file_names_case_insensitively);
    MEM_RUN(all_blocks_wins_when_both_listing_flags_are_set);
    MEM_RUN(validate_memory_leaks_is_true_when_nothing_is_held);
    MEM_RUN(validate_memory_leaks_is_false_while_a_block_is_outstanding);
    MEM_RUN(validate_memory_leaks_misses_an_outstanding_zero_byte_block);
    MEM_RUN(get_system_status_answers_the_same_figure_for_both);
}
