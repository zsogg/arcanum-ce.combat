// The save slot's commit point and its recovery pass. Card R2, review F2.
//
// `gamelib_save` used to write a slot's four files - `.gsi`, `.bmp`, `.tfai`,
// `.tfaf` - with nothing tying them together, and the `.gsi` first. Overwriting
// a slot therefore destroyed the previous save in stages, and a failure at any
// point left new metadata describing an archive that was old, half-written or
// gone.
//
// Two things fix that, and this suite covers the half that can be reached
// without a running game:
//
//   * the `.gsi` is written last, which makes it the commit point for free -
//     `gamelib_savelist_create` enumerates `save\slot*.*` and keeps only the
//     `.gsi` names, so a slot without one is not a save the player can see;
//   * the previous archive is moved to `.bak` first, and `gamelib_slot_recover`
//     puts it back when a save did not finish.
//
// `gamelib_save` itself needs a loaded module, a player and a screen to
// thumbnail, so the ordering is covered end to end by
// `arcanum-work/harness/save-matrix.sh --slots SlotXXXX` instead. What is
// tested here is the recovery pass, which is pure file handling.

#include "game/gamelib.h"

#include <stdio.h>
#include <string.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tig/file.h"

// Globalized for the suite; see tests/CMakeLists.txt.
extern bool gamelib_slot_backup(const char* name);
extern void gamelib_slot_backup_undo(const char* name);
extern void gamelib_slot_backup_release(const char* name);

#include "test_harness.h"

#define SLOT_REPO "./arcanum_test_slot_repo"
#define SLOT_SAVE SLOT_REPO "/save"
#define SLOT_INDEX SLOT_SAVE "/Slot0001.tfai"
#define SLOT_DATA SLOT_SAVE "/Slot0001.tfaf"
#define SLOT_INDEX_BAK SLOT_SAVE "/Slot0001.tfai.bak"
#define SLOT_DATA_BAK SLOT_SAVE "/Slot0001.tfaf.bak"
#define SLOT_INDEX_TMP SLOT_SAVE "/Slot0001.tfai.tmp"
#define SLOT_DATA_TMP SLOT_SAVE "/Slot0001.tfaf.tmp"

static const char* const SLOT_PATHS[] = {
    SLOT_INDEX,
    SLOT_DATA,
    SLOT_INDEX_BAK,
    SLOT_DATA_BAK,
    SLOT_INDEX_TMP,
    SLOT_DATA_TMP,
    SLOT_SAVE,
    SLOT_REPO,
};

static void slot_clear(void)
{
    size_t index;

    for (index = 0; index < sizeof(SLOT_PATHS) / sizeof(SLOT_PATHS[0]); index++) {
        remove(SLOT_PATHS[index]);
    }
}

static void slot_begin(void)
{
    slot_clear();
    tig_file_mkdir(SLOT_REPO);
    tig_file_mkdir(SLOT_SAVE);
    CHECK(tig_file_repository_add(SLOT_REPO));
}

static int slot_write(const char* path, const char* text)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return 0;
    }

    fputs(text, stream);
    fclose(stream);

    return 1;
}

// Returns the file's contents, or NULL when it is not there.
static const char* slot_read(const char* path, char* buffer, size_t size)
{
    FILE* stream = fopen(path, "rb");
    size_t got;

    if (stream == NULL) {
        return NULL;
    }

    got = fread(buffer, 1, size - 1, stream);
    fclose(stream);
    buffer[got] = '\0';

    return buffer;
}

// The case the whole design exists for: a save that did not finish leaves the
// previous generation in `.bak`, and the player gets it back.
TEST(recovery_restores_the_previous_generation)
{
    char buffer[64];

    slot_begin();

    // The half-written replacement, and the good save it displaced.
    CHECK(slot_write(SLOT_INDEX, "half written"));
    CHECK(slot_write(SLOT_DATA, "half written"));
    CHECK(slot_write(SLOT_INDEX_BAK, "the good index"));
    CHECK(slot_write(SLOT_DATA_BAK, "the good payload"));

    gamelib_slot_recover();

    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the good index");
    CHECK_STR(slot_read(SLOT_DATA, buffer, sizeof(buffer)), "the good payload");

    // The backup is consumed, not left to be restored a second time over a
    // later good save.
    CHECK(slot_read(SLOT_INDEX_BAK, buffer, sizeof(buffer)) == NULL);
    CHECK(slot_read(SLOT_DATA_BAK, buffer, sizeof(buffer)) == NULL);

    slot_clear();
}

// A second interruption, this time during recovery itself: one half restored,
// the other still to do. Running it again has to finish the job rather than
// get confused by the half-done state.
TEST(recovery_is_idempotent_and_finishes_a_partial_recovery)
{
    char buffer[64];

    slot_begin();

    // As if the index had already been put back and the process died before
    // the payload was.
    CHECK(slot_write(SLOT_INDEX, "the good index"));
    CHECK(slot_write(SLOT_DATA, "half written"));
    CHECK(slot_write(SLOT_DATA_BAK, "the good payload"));

    gamelib_slot_recover();

    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the good index");
    CHECK_STR(slot_read(SLOT_DATA, buffer, sizeof(buffer)), "the good payload");

    // And again on an already-clean slot: nothing to do, nothing broken.
    gamelib_slot_recover();

    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the good index");
    CHECK_STR(slot_read(SLOT_DATA, buffer, sizeof(buffer)), "the good payload");

    slot_clear();
}

// `tig_file_archive`'s staging files, abandoned by an interrupted save. They
// are not a generation to restore - they are incomplete by definition - so
// they are discarded, and a good save beside them is left alone.
TEST(recovery_discards_staging_files_and_keeps_a_good_save)
{
    char buffer[64];

    slot_begin();

    CHECK(slot_write(SLOT_INDEX, "the good index"));
    CHECK(slot_write(SLOT_DATA, "the good payload"));
    CHECK(slot_write(SLOT_INDEX_TMP, "abandoned"));
    CHECK(slot_write(SLOT_DATA_TMP, "abandoned"));

    gamelib_slot_recover();

    CHECK(slot_read(SLOT_INDEX_TMP, buffer, sizeof(buffer)) == NULL);
    CHECK(slot_read(SLOT_DATA_TMP, buffer, sizeof(buffer)) == NULL);

    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the good index");
    CHECK_STR(slot_read(SLOT_DATA, buffer, sizeof(buffer)), "the good payload");

    slot_clear();
}

// The ordinary case, which must stay cheap and must not touch anything: no
// interrupted save, nothing to recover.
TEST(recovery_leaves_an_undisturbed_slot_exactly_as_it_was)
{
    char buffer[64];

    slot_begin();

    CHECK(slot_write(SLOT_INDEX, "the good index"));
    CHECK(slot_write(SLOT_DATA, "the good payload"));

    gamelib_slot_recover();

    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the good index");
    CHECK_STR(slot_read(SLOT_DATA, buffer, sizeof(buffer)), "the good payload");

    slot_clear();
}

// The acceptance item the constructed cases above cannot meet: a save killed
// at the commit boundary, with the state produced by the interruption rather
// than written out by hand.
//
// The child runs the real `gamelib_slot_backup` - globalized for the suite,
// see tests/CMakeLists.txt - then writes a replacement archive and dies with
// SIGKILL before the `.gsi` that would commit it. SIGKILL cannot be caught, so
// nothing in the child gets to tidy up: what the parent finds is exactly what
// a power cut or an OOM kill would leave.
TEST(a_save_killed_at_the_commit_boundary_recovers_the_previous_generation)
{
    char buffer[64];
    pid_t child;
    int status = 0;

    slot_begin();

    CHECK(slot_write(SLOT_INDEX, "the good index"));
    CHECK(slot_write(SLOT_DATA, "the good payload"));

    fflush(NULL);
    child = fork();
    if (child == 0) {
        // Exactly what `gamelib_save` does, up to the point of no return.
        if (!gamelib_slot_backup("Slot0001")) {
            _exit(1);
        }

        slot_write(SLOT_INDEX, "replacement, incomplete");
        slot_write(SLOT_DATA, "replacement, incomplete");

        // No `.gsi` is written: the save never commits.
        kill(getpid(), SIGKILL);
        _exit(2);
    }

    CHECK(child > 0);
    if (child > 0) {
        CHECK(waitpid(child, &status, 0) == child);
        // It must really have been killed, not have exited early - otherwise
        // this case is testing a path the child chose rather than a crash.
        CHECK(WIFSIGNALED(status));
        CHECK_INT(WIFSIGNALED(status) ? WTERMSIG(status) : 0, SIGKILL);
    }

    // The interrupted replacement is on disk and the previous generation is in
    // `.bak`, which is the state recovery exists for.
    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "replacement, incomplete");

    gamelib_slot_recover();

    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the good index");
    CHECK_STR(slot_read(SLOT_DATA, buffer, sizeof(buffer)), "the good payload");
    CHECK(slot_read(SLOT_INDEX_BAK, buffer, sizeof(buffer)) == NULL);
    CHECK(slot_read(SLOT_DATA_BAK, buffer, sizeof(buffer)) == NULL);

    slot_clear();
}

// A save killed at the same point in a slot that did not exist yet. There is
// no previous generation to restore, so the right outcome is no slot at all -
// never a half one that the save list would show and a load would choke on.
TEST(a_killed_save_of_a_new_slot_leaves_no_half_slot)
{
    char buffer[64];
    pid_t child;
    int status = 0;

    slot_begin();

    fflush(NULL);
    child = fork();
    if (child == 0) {
        if (!gamelib_slot_backup("Slot0001")) {
            _exit(1);
        }

        slot_write(SLOT_INDEX, "replacement, incomplete");
        slot_write(SLOT_DATA, "replacement, incomplete");
        kill(getpid(), SIGKILL);
        _exit(2);
    }

    CHECK(child > 0);
    if (child > 0) {
        CHECK(waitpid(child, &status, 0) == child);
        CHECK(WIFSIGNALED(status));
    }

    gamelib_slot_recover();

    // Nothing was set aside, because there was nothing there. Recovery has no
    // generation to restore - and the archive that is left has no `.gsi`, so
    // `gamelib_savelist_create` will not offer it as a save.
    CHECK(slot_read(SLOT_INDEX_BAK, buffer, sizeof(buffer)) == NULL);
    CHECK(slot_read(SLOT_DATA_BAK, buffer, sizeof(buffer)) == NULL);

    slot_clear();
}

// `gamelib_slot_backup_undo` on a slot that had no previous generation: the
// archive just written is removed, so a failed save of a new slot leaves the
// tree as it found it.
TEST(undo_removes_a_new_slots_archive_when_there_is_nothing_to_restore)
{
    char buffer[64];

    slot_begin();

    CHECK(slot_write(SLOT_INDEX, "written by the failed save"));
    CHECK(slot_write(SLOT_DATA, "written by the failed save"));

    gamelib_slot_backup_undo("Slot0001");

    CHECK(slot_read(SLOT_INDEX, buffer, sizeof(buffer)) == NULL);
    CHECK(slot_read(SLOT_DATA, buffer, sizeof(buffer)) == NULL);

    slot_clear();
}

// And `gamelib_slot_backup_release` drops the previous generation once the
// save has committed, so backups do not accumulate one per save.
TEST(release_drops_the_previous_generation_after_a_commit)
{
    char buffer[64];

    slot_begin();

    CHECK(slot_write(SLOT_INDEX, "the committed index"));
    CHECK(slot_write(SLOT_INDEX_BAK, "superseded"));
    CHECK(slot_write(SLOT_DATA_BAK, "superseded"));

    gamelib_slot_backup_release("Slot0001");

    CHECK(slot_read(SLOT_INDEX_BAK, buffer, sizeof(buffer)) == NULL);
    CHECK(slot_read(SLOT_DATA_BAK, buffer, sizeof(buffer)) == NULL);
    CHECK_STR(slot_read(SLOT_INDEX, buffer, sizeof(buffer)), "the committed index");

    slot_clear();
}

void test_save_slot_suite(void)
{
    RUN(recovery_restores_the_previous_generation);
    RUN(recovery_is_idempotent_and_finishes_a_partial_recovery);
    RUN(recovery_discards_staging_files_and_keeps_a_good_save);
    RUN(recovery_leaves_an_undisturbed_slot_exactly_as_it_was);
    RUN(a_save_killed_at_the_commit_boundary_recovers_the_previous_generation);
    RUN(a_killed_save_of_a_new_slot_leaves_no_half_slot);
    RUN(undo_removes_a_new_slots_archive_when_there_is_nothing_to_restore);
    RUN(release_drops_the_previous_generation_after_a_commit);
}
