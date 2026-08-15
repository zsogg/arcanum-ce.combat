// The CE half of the emulation differential.
//
// `arcanum-work/harness/emudiff` runs a function out of the original
// Arcanum.exe under emulation and compares its answers against CE's C on the
// same inputs. This binary is how the driver reaches the C: it speaks a line
// protocol on stdin/stdout so thousands of calls cost one process, not one
// process each.
//
//     > list                     -> one "name arity" line per entry, then "."
//     > stat_cost 7              -> "12"
//     > <unknown or bad arity>   -> "!<reason>"
//
// Arguments and results are signed 64-bit decimal. A wrapper narrows to
// whatever the real signature takes, so the protocol stays uniform while the
// function under test is called exactly as the game calls it.
//
// Only add functions here that are *pure with respect to reachable state*:
// no object handles, no loaded data files, no allocation. The emulator has no
// Windows under it and no game state in it, so anything else diverges for
// reasons that say nothing about the port. Seeded generator state is fine
// when both sides expose the same setter (see `random_seed`).

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game/location.h"
#include "game/random.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/timeevent.h"

#define MAX_ARGS 8
#define MAX_LINE 512

typedef int64_t (*OracleFunc)(const int64_t* args);

typedef struct {
    const char* name;
    int arity;
    OracleFunc func;
} OracleEntry;

static int64_t o_random_seed(const int64_t* a)
{
    random_seed((unsigned int)a[0]);
    return 0;
}

static int64_t o_random_rand(const int64_t* a)
{
    (void)a;
    return random_rand();
}

static int64_t o_random_between(const int64_t* a)
{
    return random_between((int)a[0], (int)a[1]);
}

static int64_t o_stat_cost(const int64_t* a)
{
    return stat_cost((int)a[0]);
}

static int64_t o_basic_skill_money(const int64_t* a)
{
    return basic_skill_money((int)a[0], (int)a[1], (int)a[2]);
}

static int64_t o_basic_skill_min_stat_level_required(const int64_t* a)
{
    return basic_skill_min_stat_level_required((int)a[0]);
}

static int64_t o_basic_skill_stat(const int64_t* a)
{
    return basic_skill_stat((int)a[0]);
}

static int64_t o_tech_skill_stat(const int64_t* a)
{
    return tech_skill_stat((int)a[0]);
}

static int64_t o_location_rot(const int64_t* a)
{
    return location_rot(a[0], a[1]);
}

static int64_t o_location_dist(const int64_t* a)
{
    return location_dist(a[0], a[1]);
}

// A function whose answer is written through a pointer rather than returned.
//
// `DateTime` is a union over a `uint64_t`, so the whole struct travels as one
// integer and the line protocol stays what it is: the packed value goes in and
// the packed value comes back out. On the emulator's side the driver allocates
// the eight bytes, writes them, passes the address and reads them back - see
// `DateTimeArg` in `cases.py`.
static int64_t o_datetime_sub_milliseconds(const int64_t* a)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    datetime_sub_milliseconds(&datetime, (unsigned int)a[1]);

    return (int64_t)datetime.value;
}

static int64_t o_datetime_add_milliseconds(const int64_t* a)
{
    DateTime datetime;

    datetime.value = (uint64_t)a[0];
    datetime_add_milliseconds(&datetime, (unsigned int)a[1]);

    return (int64_t)datetime.value;
}

static const OracleEntry oracle_entries[] = {
    { "random_seed", 1, o_random_seed },
    { "random_rand", 0, o_random_rand },
    { "random_between", 2, o_random_between },
    { "stat_cost", 1, o_stat_cost },
    { "basic_skill_money", 3, o_basic_skill_money },
    { "basic_skill_min_stat_level_required", 1, o_basic_skill_min_stat_level_required },
    { "basic_skill_stat", 1, o_basic_skill_stat },
    { "tech_skill_stat", 1, o_tech_skill_stat },
    { "location_rot", 2, o_location_rot },
    { "location_dist", 2, o_location_dist },
    { "datetime_sub_milliseconds", 2, o_datetime_sub_milliseconds },
    { "datetime_add_milliseconds", 2, o_datetime_add_milliseconds },
};

#define ORACLE_ENTRY_COUNT ((int)(sizeof(oracle_entries) / sizeof(oracle_entries[0])))

static const OracleEntry* oracle_find(const char* name)
{
    int index;

    for (index = 0; index < ORACLE_ENTRY_COUNT; index++) {
        if (strcmp(oracle_entries[index].name, name) == 0) {
            return &oracle_entries[index];
        }
    }

    return NULL;
}

int main(void)
{
    char line[MAX_LINE];

    // The driver alternates writing a request with reading its reply, so
    // anything still sitting in a buffer here is a deadlock.
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        const OracleEntry* entry;
        int64_t args[MAX_ARGS];
        char* token;
        char* save;
        int argc = 0;

        token = strtok_r(line, " \t\r\n", &save);
        if (token == NULL) {
            continue;
        }

        if (strcmp(token, "list") == 0) {
            int index;

            for (index = 0; index < ORACLE_ENTRY_COUNT; index++) {
                printf("%s %d\n", oracle_entries[index].name, oracle_entries[index].arity);
            }
            printf(".\n");
            continue;
        }

        entry = oracle_find(token);
        if (entry == NULL) {
            printf("!unknown function %s\n", token);
            continue;
        }

        while ((token = strtok_r(NULL, " \t\r\n", &save)) != NULL) {
            if (argc >= MAX_ARGS) {
                argc = MAX_ARGS + 1;
                break;
            }

            if (sscanf(token, "%" SCNd64, &args[argc]) != 1) {
                argc = -1;
                break;
            }

            argc++;
        }

        if (argc != entry->arity) {
            printf("!%s takes %d args\n", entry->name, entry->arity);
            continue;
        }

        printf("%" PRId64 "\n", entry->func(args));
    }

    return 0;
}
