// Tests for `game/provenance.c` - the diagnostic record of what wrote a save.
//
// Card H9, item 4. The record lives in the save folder, so `gamelib_save`'s
// existing `tig_file_archive` carries it into the `.tfai`/`.tfaf` pair and
// `gamelib_load` brings it back: it survives save, load and a copy of the
// save, and **no stock subsystem's framing changes** - `data.sav`,
// `data2.sav` and the `.gsi` are byte-for-byte what they were.
//
// Three properties are worth a test here and two of them are about what this
// must *not* do:
//
//  * It must be **deterministic**. `arcanum-work/harness/determinism-check.sh`
//    runs CE twice on one save and compares the two save trees byte for byte,
//    and these files are in that comparison. A timestamp would turn a green
//    gate red and would be discovered as a flake rather than as this.
//  * It must **never fail a save**. `gamelib_save` logs and continues; here
//    the corresponding property is that a missing mods record is an ordinary
//    answer (`"mods": null`) rather than an error.
//  * A **stale** copy must not survive. A load unarchives the previous save's
//    record into the folder, so saving after uninstalling every mod would
//    otherwise archive the old mod list again.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/area.h"
#include "game/gamelib.h"
#include "game/provenance.h"

#include "tig/tig.h"

#include "test_harness.h"

#define SAVE_DIR "./Save"
#define CURRENT_DIR "./Save/Current"
#define PROVENANCE_FILE "./Save/Current/ce-provenance.json"
#define MODS_COPY_FILE "./Save/Current/ce-mods.json"
#define MODS_SOURCE_FILE "./ce-mods.json"

#define MAX_RECORD 4096

static bool write_file(const char* path, const char* contents)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }
    fwrite(contents, 1, strlen(contents), stream);
    fclose(stream);
    return true;
}

// Returns the file's bytes in `buffer`, NUL-terminated, or false if it is
// absent. Absence is a real answer in several tests below, not a failure.
static bool read_file(const char* path, char* buffer, size_t size)
{
    FILE* stream = fopen(path, "rb");
    size_t got;

    if (stream == NULL) {
        return false;
    }
    got = fread(buffer, 1, size - 1, stream);
    fclose(stream);
    buffer[got] = '\0';
    return true;
}

static bool contains(const char* haystack, const char* needle)
{
    return strstr(haystack, needle) != NULL;
}

// Lines that start with exactly two spaces and a quote: the record's
// top-level keys. Counting them is how this suite notices a field being
// *added*, which comparing two back-to-back writes cannot - see
// `the_record_has_exactly_the_fields_it_should`.
static int count_top_level_keys(const char* record)
{
    int count = 0;
    const char* line = record;

    while (line != NULL && *line != '\0') {
        if (line[0] == ' ' && line[1] == ' ' && line[2] == '"') {
            count++;
        }
        line = strchr(line, '\n');
        if (line != NULL) {
            line++;
        }
    }

    return count;
}

static void clean(void)
{
    remove(PROVENANCE_FILE);
    remove(MODS_COPY_FILE);
    remove(MODS_SOURCE_FILE);
}

TEST(a_record_is_written_with_the_engine_facts)
{
    char record[MAX_RECORD];
    char expected[64];

    clean();
    CHECK(provenance_save());
    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));

    CHECK(contains(record, "\"schema\": 1"));
    CHECK(contains(record, "\"port\": \"arcanum-ce\""));
    // The save stream's version, and the one CE departure a reader has to
    // know about to make sense of the Area block (card H4, quirk 17).
    CHECK(contains(record, "\"save_version\": 25"));
    CHECK(contains(record, "\"area-count-prefix\""));

    // Whatever the area list currently holds - asserted against the engine's
    // own answer rather than a constant, so this does not depend on which
    // suite ran before it.
    snprintf(expected, sizeof(expected), "\"areas\": %d", area_count());
    CHECK(contains(record, expected));

    // Every declared feature, by its `arcanum.cfg` key.
    CHECK(contains(record, "\"uap fixes\""));
}

TEST(the_module_is_named)
{
    char record[MAX_RECORD];
    char expected[320];

    clean();
    CHECK(provenance_save());
    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));

    snprintf(expected, sizeof(expected), "\"name\": \"%s\"",
        gamelib_current_module_name_get());
    CHECK(contains(record, expected));
    CHECK(contains(record, "\"guid\":"));
}

TEST(two_runs_produce_identical_bytes)
{
    char first[MAX_RECORD];
    char second[MAX_RECORD];

    clean();
    CHECK(provenance_save());
    CHECK(read_file(PROVENANCE_FILE, first, sizeof(first)));

    CHECK(provenance_save());
    CHECK(read_file(PROVENANCE_FILE, second, sizeof(second)));

    // The determinism gate compares two save trees byte for byte. Anything
    // that varied here - a clock, a path, a pointer - would fail it as a
    // flake somewhere else entirely.
    CHECK_STR(second, first);
}

// **This case exists because a mutation was missed.** Adding a timestamp to
// the record left `two_runs_produce_identical_bytes` green: the two writes
// happen inside one timer tick, so the clock returned the same value both
// times. The real determinism gate runs two processes minutes apart and would
// have caught it - as a flake, somewhere else entirely, hours later.
//
// A missed mutation indicts the measurer, so the property is measured a
// second way that does not depend on time passing: the record has exactly the
// fields it is supposed to have, and *any* added one fails.
TEST(the_record_has_exactly_the_fields_it_should)
{
    char record[MAX_RECORD];

    clean();
    CHECK(provenance_save());
    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));

    // schema, engine, module, areas, features, mods.
    CHECK_INT(count_top_level_keys(record), 6);
}

TEST(no_mods_record_is_an_answer_rather_than_a_failure)
{
    char record[MAX_RECORD];

    clean();
    CHECK(provenance_save());
    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));

    CHECK(contains(record, "\"mods\": null"));
    // Nothing was invented to stand in for the missing record.
    CHECK(!read_file(MODS_COPY_FILE, record, sizeof(record)));
}

TEST(a_mods_record_is_copied_verbatim)
{
    static const char* const TOOLKIT_RECORD = "{\"schema\": 1, \"mods\": [{\"id\": \"ce-test-ground\"}]}\n";
    char record[MAX_RECORD];

    clean();
    CHECK(write_file(MODS_SOURCE_FILE, TOOLKIT_RECORD));
    CHECK(provenance_save());

    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));
    CHECK(contains(record, "\"mods\": \"ce-mods.json\""));

    // Verbatim, and never parsed: a malformed or newer toolkit record must
    // travel intact rather than corrupting the file beside it.
    CHECK(read_file(MODS_COPY_FILE, record, sizeof(record)));
    CHECK_STR(record, TOOLKIT_RECORD);
}

TEST(a_record_the_engine_cannot_parse_still_travels)
{
    static const char* const NOT_JSON = "this is not json at all\n";
    char record[MAX_RECORD];

    clean();
    CHECK(write_file(MODS_SOURCE_FILE, NOT_JSON));
    CHECK(provenance_save());

    CHECK(read_file(MODS_COPY_FILE, record, sizeof(record)));
    CHECK_STR(record, NOT_JSON);
    // And the provenance file itself is unaffected by it.
    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));
    CHECK(contains(record, "\"schema\": 1"));
}

TEST(a_stale_copy_from_a_loaded_save_is_removed)
{
    char record[MAX_RECORD];

    clean();
    // What `gamelib_load` leaves behind: the previous save's record,
    // unarchived into the folder.
    CHECK(write_file(MODS_COPY_FILE, "{\"mods\": [{\"id\": \"gone\"}]}\n"));

    // Every mod has since been uninstalled, so there is no source.
    CHECK(provenance_save());

    CHECK(!read_file(MODS_COPY_FILE, record, sizeof(record)));
    CHECK(read_file(PROVENANCE_FILE, record, sizeof(record)));
    CHECK(contains(record, "\"mods\": null"));
    CHECK(!contains(record, "gone"));
}

TEST(a_missing_save_folder_is_reported_rather_than_crashing)
{
    clean();
    tig_file_rmdir(CURRENT_DIR);

    // `gamelib_save` checks for the folder before it gets here, so this is
    // the defensive half: a false, and no file.
    CHECK(!provenance_save());

    tig_file_mkdir(CURRENT_DIR);
}

void test_provenance_suite(void)
{
    tig_file_repository_add(".");
    tig_file_mkdir(SAVE_DIR);
    tig_file_mkdir(CURRENT_DIR);

    RUN(a_record_is_written_with_the_engine_facts);
    RUN(the_module_is_named);
    RUN(two_runs_produce_identical_bytes);
    RUN(the_record_has_exactly_the_fields_it_should);
    RUN(no_mods_record_is_an_answer_rather_than_a_failure);
    RUN(a_mods_record_is_copied_verbatim);
    RUN(a_record_the_engine_cannot_parse_still_travels);
    RUN(a_stale_copy_from_a_loaded_save_is_removed);
    RUN(a_missing_save_folder_is_reported_rather_than_crashing);

    clean();
    tig_file_rmdir(CURRENT_DIR);
    tig_file_rmdir(SAVE_DIR);
}
