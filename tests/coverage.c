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
// Threading is handled explicitly, with no C data races. The seen table is a
// lock-free open-addressed loop: all slot reads and inserts are C11 atomics.
// The log FILE itself is guarded by an atomic_flag spinlock, so at most one
// thread ever touches stdio, `coverage_out` or `coverage_count`; the open is
// done under that same lock, which removes the old "one thread is opening"
// state machine entirely. Duplicate lines across runs are still possible and
// harmless; the consumer dedupes. A lost line would only under-report, which
// the census already disclaims - it reports what *was* reached, never what
// cannot be.

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SEEN_CAP (1u << 20) // 1M slots, ~8MB, far more than the ~20k functions

#define NOINSTR __attribute__((no_instrument_function))

void __cyg_profile_func_enter(void* this_fn, void* call_site) NOINSTR;
void __cyg_profile_func_exit(void* this_fn, void* call_site) NOINSTR;

static void* _Atomic coverage_seen[SEEN_CAP];
static atomic_bool coverage_failed;
static atomic_flag coverage_log_lock = ATOMIC_FLAG_INIT;

// Everything below is guarded by coverage_log_lock; none of it is read or
// written outside the critical section.
static FILE* coverage_out;
static unsigned coverage_count;

static unsigned coverage_hash(void* p) NOINSTR;
static unsigned coverage_hash(void* p)
{
    unsigned long long x = (unsigned long long)(uintptr_t)p;

    x *= 0x9E3779B97F4A7C15ull;

    return (unsigned)(x >> 44) & (SEEN_CAP - 1);
}

static void coverage_open_log(void) NOINSTR;
static void coverage_open_log(void)
{
    const char* path = getenv("ARCANUM_COVERAGE_OUT");
    if (path == NULL) {
        path = "coverage.txt";
    }
    // Append, not truncate: a coverage session is usually several runs of the
    // game, and opening with "w" would silently discard everything the previous
    // run learned. Duplicate lines across runs are harmless - the consumer
    // dedupes.
    coverage_out = fopen(path, "a");
    if (coverage_out == NULL) {
        atomic_store_explicit(&coverage_failed, true, memory_order_relaxed);
    }
}

void __cyg_profile_func_enter(void* this_fn, void* call_site)
{
    unsigned h;

    (void)call_site;

    if (atomic_load_explicit(&coverage_failed, memory_order_relaxed)) {
        return;
    }

    // Lock-free insert into the open-addressed table. The table has no
    // deletions and far more slots than the build has functions, so the probe
    // always terminates. A lost race is handled by restarting the probe from
    // the hash.
    for (;;) {
        h = coverage_hash(this_fn);
        while (1) {
            void* slot = atomic_load_explicit(&coverage_seen[h], memory_order_relaxed);
            if (slot == NULL) {
                break;
            }
            if (slot == this_fn) {
                return;
            }
            h = (h + 1) & (SEEN_CAP - 1);
        }

        void* expected = NULL;
        if (atomic_compare_exchange_strong_explicit(&coverage_seen[h], &expected,
                this_fn, memory_order_relaxed, memory_order_relaxed)) {
            break;
        }
    }

    // Serialize every stdio access. The open, the write and the periodic flush
    // all happen under this lock, so there is no concurrent use of the FILE.
    while (atomic_flag_test_and_set_explicit(&coverage_log_lock, memory_order_acquire)) {
    }

    if (!atomic_load_explicit(&coverage_failed, memory_order_relaxed)) {
        if (coverage_out == NULL) {
            coverage_open_log();
        }
        if (coverage_out != NULL) {
            fprintf(coverage_out, "%p\n", this_fn);

            // Cheap insurance against a SIGKILL landing between writes.
            if ((++coverage_count & 0x3F) == 0) {
                fflush(coverage_out);
            }
        }
    }

    atomic_flag_clear_explicit(&coverage_log_lock, memory_order_release);
}

void __cyg_profile_func_exit(void* this_fn, void* call_site)
{
    (void)this_fn;
    (void)call_site;
}
