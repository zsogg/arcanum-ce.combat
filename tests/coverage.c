// Function-entry census.
//
// Answers one question the symbol count cannot: **which of the unnamed
// functions does single-player actually reach?** The naming backlog is a tail
// of ~669 functions and there is no reason to believe play touches all of them
// - editor-only paths, multiplayer, dead branches and cut features all still
// compile. Naming code nobody runs is the most expensive way to spend a session.
//
// Built only when `-DARCANUM_COVERAGE=ON`, which adds `-finstrument-functions`
// to `arcanum-game` (and nothing else - tig and SDL are left alone, since the
// question is about the port's own backlog).
//
// Design notes, each of which is load-bearing:
//
//   - **Dedupe in process, append on first sight.** The smoke and capture runs
//     end in SIGKILL at a timeout, so `atexit` never runs and a dump-at-exit
//     design would lose everything. Writing each address the first time it is
//     seen costs a few thousand `fprintf`s over a whole run and survives any
//     death.
//   - **Open-addressed table, no allocation.** The hooks run before `main` and
//     inside the allocator itself; anything that mallocs here recurses.
//   - **`no_instrument_function` on everything in this file**, for the same
//     reason.
//   - **Addresses, not names.** Most of the interesting functions are `static`,
//     so they are absent from `.dynsym` and `dladdr` cannot see them. The build
//     turns PIE off so a logged address is the link-time address, and `nm` maps
//     it afterwards.
//
// Races are tolerated. SDL runs threads, so two of them can log the same
// address; a duplicate line in the output is harmless and the consumer dedupes
// anyway. A lost line would only under-report, which the census already
// disclaims - it reports what *was* reached, never what cannot be.

#include <stdio.h>
#include <stdlib.h>

#define SEEN_CAP (1u << 20) // 1M slots, ~8MB, far more than the ~20k functions

#define NOINSTR __attribute__((no_instrument_function))

void __cyg_profile_func_enter(void* this_fn, void* call_site) NOINSTR;
void __cyg_profile_func_exit(void* this_fn, void* call_site) NOINSTR;

static void* coverage_seen[SEEN_CAP];
static FILE* coverage_out;
static unsigned coverage_count;
static int coverage_failed;

static unsigned coverage_hash(void* p) NOINSTR;
static unsigned coverage_hash(void* p)
{
    unsigned long long x = (unsigned long long)(size_t)p;

    x *= 0x9E3779B97F4A7C15ull;

    return (unsigned)(x >> 44) & (SEEN_CAP - 1);
}

void __cyg_profile_func_enter(void* this_fn, void* call_site)
{
    unsigned h;

    (void)call_site;

    if (coverage_failed) {
        return;
    }

    h = coverage_hash(this_fn);
    while (coverage_seen[h] != NULL) {
        if (coverage_seen[h] == this_fn) {
            return;
        }
        h = (h + 1) & (SEEN_CAP - 1);
    }
    coverage_seen[h] = this_fn;

    if (coverage_out == NULL) {
        const char* path = getenv("ARCANUM_COVERAGE_OUT");
        if (path == NULL) {
            path = "coverage.txt";
        }
        // Append, not truncate: a coverage session is usually several runs of
        // the game, and opening with "w" would silently discard everything the
        // previous run learned. Duplicate lines across runs are harmless - the
        // consumer dedupes.
        coverage_out = fopen(path, "a");
        if (coverage_out == NULL) {
            coverage_failed = 1;
            return;
        }
    }

    fprintf(coverage_out, "%p\n", this_fn);

    // Cheap insurance against a SIGKILL landing between writes.
    if ((++coverage_count & 0x3F) == 0) {
        fflush(coverage_out);
    }
}

void __cyg_profile_func_exit(void* this_fn, void* call_site)
{
    (void)this_fn;
    (void)call_site;
}
