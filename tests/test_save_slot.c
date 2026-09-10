// R2: exercise the production four-file transaction, not a second model of it.
// Every fault/crash boundary is in real I/O. Archives are generated and loaded
// by TIG; metadata and thumbnail fixtures must agree with their generation.
#include "game/gamelib.h"
#include "game/save_transaction.h"

#include <dirent.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_harness.h"
#include "tig/file.h"

#define REPO "./arcanum_test_slot_repo"
#define TXN REPO "/Save/ce-save-transaction"
#define OLD_DATA "the previous save's complete data"
#define NEW_DATA "replacement data, deliberately a different length"

static int events;
static int fail_at;
static int kill_at;
static bool armed;
static bool fail_publish;
static bool fail_restore;
static bool fail_cleanup;
static const char* fail_open;
static const char* fail_close;
static TigFile* close_target;
static int fake_bmp; // 0: real renderer, 1: successful fixture, 2: failure
static const char* slot_name = "Slot0001";
static const char* const ext[] = { "tfai", "tfaf", "bmp", "gsi" };

extern int __real_tig_file_rename(const char*, const char*);
extern int __real_tig_file_remove(const char*);
extern TigFile* __real_tig_file_fopen(const char*, const char*);
extern int __real_tig_file_fclose(TigFile*);
extern size_t __real_tig_file_fwrite(const void*, size_t, size_t, TigFile*);
extern int __real_tig_video_buffer_save_to_bmp(TigVideoBufferSaveToBmpInfo*);

static bool fault(void)
{
    if (!armed) {
        return false;
    }
    events++;
    return fail_at != 0 && events == fail_at;
}

static void crash(void)
{
    if (armed && kill_at != 0 && events == kill_at) {
        kill(getpid(), SIGKILL);
        _exit(99);
    }
}

int __wrap_tig_file_rename(const char* src, const char* dst)
{
    int rc;
    if (fault()
        || (fail_publish && strstr(src, "new.tfaf") != NULL)
        || (fail_restore && strstr(src, "restore.tmp") != NULL)) {
        return 1;
    }
    rc = __real_tig_file_rename(src, dst);
    crash();
    return rc;
}

int __wrap_tig_file_remove(const char* path)
{
    int rc;
    if (fault() || (fail_cleanup && strstr(path, "old.tfaf") != NULL)) {
        return 1;
    }
    rc = __real_tig_file_remove(path);
    crash();
    return rc;
}

TigFile* __wrap_tig_file_fopen(const char* path, const char* mode)
{
    TigFile* stream;
    if (fault() || (fail_open != NULL && strstr(path, fail_open) != NULL)) {
        return NULL;
    }
    stream = __real_tig_file_fopen(path, mode);
    if (fail_close != NULL && strchr(mode, 'w') != NULL && strstr(path, fail_close) != NULL) {
        close_target = stream;
    }
    crash();
    return stream;
}

int __wrap_tig_file_fclose(TigFile* stream)
{
    bool failed = fault() || (stream != NULL && stream == close_target);
    int rc = __real_tig_file_fclose(stream);
    if (stream == close_target) {
        close_target = NULL;
    }
    crash();
    return failed ? EOF : rc;
}

size_t __wrap_tig_file_fwrite(const void* data, size_t size, size_t count, TigFile* stream)
{
    size_t written;
    if (fault()) {
        return 0;
    }
    written = __real_tig_file_fwrite(data, size, count, stream);
    crash();
    return written;
}

int __wrap_tig_video_buffer_save_to_bmp(TigVideoBufferSaveToBmpInfo* info)
{
    TigFile* stream;
    bool ok;
    if (fake_bmp == 0) {
        return __real_tig_video_buffer_save_to_bmp(info);
    }
    if (fake_bmp == 2) {
        return TIG_ERR_IO;
    }
    stream = tig_file_fopen(info->path, "wb");
    if (stream == NULL) {
        return TIG_ERR_IO;
    }
    ok = tig_file_fwrite("fixture bmp", 11, 1, stream) == 1;
    if (tig_file_fclose(stream) != 0) {
        ok = false;
    }
    return ok ? TIG_OK : TIG_ERR_IO;
}

// Fixture cleanup deliberately uses libc, independent of the code under test.
static void erase(const char* path)
{
    DIR* dir = opendir(path);
    struct dirent* entry;
    char child[1024];
    if (dir == NULL) {
        unlink(path);
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        erase(child);
    }
    closedir(dir);
    rmdir(path);
}

static void write_text(const char* path, const char* text)
{
    FILE* stream = fopen(path, "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK(fputs(text, stream) >= 0);
        CHECK_INT(fclose(stream), 0);
    }
}

static void check_text(const char* path, const char* text)
{
    char buffer[256] = { 0 };
    FILE* stream = fopen(path, "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        fread(buffer, 1, sizeof(buffer) - 1, stream);
        fclose(stream);
        CHECK_STR(buffer, text);
    }
}

static void reset_faults(void)
{
    armed = false;
    events = fail_at = kill_at = 0;
    fail_publish = fail_restore = fail_cleanup = false;
    fail_open = fail_close = NULL;
    close_target = NULL;
    fake_bmp = 0;
}

static void begin(bool old_exists)
{
    char path[TIG_MAX_PATH];
    reset_faults();
    erase(REPO);
    mkdir(REPO, 0700);
    mkdir(REPO "/Save", 0700); // The real installation's capital S.
    mkdir(REPO "/source", 0700);
    mkdir(REPO "/unpacked", 0700);
    CHECK(tig_file_repository_add(REPO));
    if (old_exists) {
        write_text(REPO "/source/data.sav", OLD_DATA);
        snprintf(path, sizeof(path), "save\\%s", slot_name);
        CHECK(tig_file_archive(path, "source"));
        snprintf(path, sizeof(path), REPO "/Save/%s.bmp", slot_name);
        write_text(path, "old bmp");
        snprintf(path, sizeof(path), REPO "/Save/%sdescription.gsi", slot_name);
        write_text(path, "old metadata");
    }
    CHECK(save_transaction_begin());
    write_text(REPO "/source/data.sav", NEW_DATA);
    CHECK(tig_file_archive(SAVE_TRANSACTION_STAGE, "source"));
    write_text(TXN "/new.bmp", "new bmp");
    write_text(TXN "/new.gsi", "new metadata");
}

static void end(void)
{
    reset_faults();
    CHECK(tig_file_repository_remove(REPO));
    erase(REPO);
}

// Asserts archive LOADABILITY and both companion files, not just existence.
static void check_generation(bool is_new, bool old_exists)
{
    char path[TIG_MAX_PATH];
    int i;
    if (!is_new && !old_exists) {
        for (i = 0; i < 4; i++) {
            snprintf(path, sizeof(path), REPO "/Save/%s%s.%s", slot_name,
                i == 3 ? "description" : "", ext[i]);
            CHECK(access(path, F_OK) != 0);
        }
        return;
    }
    snprintf(path, sizeof(path), "save\\%s", slot_name);
    CHECK(tig_file_unarchive(path, "unpacked"));
    check_text(REPO "/unpacked/data.sav", is_new ? NEW_DATA : OLD_DATA);
    snprintf(path, sizeof(path), REPO "/Save/%s.bmp", slot_name);
    check_text(path, is_new ? "new bmp" : "old bmp");
    snprintf(path, sizeof(path), REPO "/Save/%sdescription.gsi", slot_name);
    check_text(path, is_new ? "new metadata" : "old metadata");
}

TEST(clean_commit_preserves_slot_names_and_loadable_archives)
{
    const char* names[] = { "Slot0001", "SlotAuto", "Slot0000" };
    unsigned int i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        slot_name = names[i];
        begin(true);
        CHECK(save_transaction_commit(slot_name, "description"));
        CHECK(save_transaction_recover());
        check_generation(true, true);
        CHECK(access(TXN, F_OK) != 0);
        end();
    }
    slot_name = "Slot0001";
}

TEST(partial_cleanup_never_rolls_back_a_committed_generation)
{
    begin(true);
    fail_cleanup = true;
    CHECK(save_transaction_commit(slot_name, "description"));
    CHECK(access(TXN "/old.tfai", F_OK) != 0);
    CHECK(access(TXN "/old.tfaf", F_OK) == 0);
    CHECK(access(TXN "/settled", F_OK) == 0);
    CHECK(!save_transaction_recover());
    check_generation(true, true);
    fail_cleanup = false;
    CHECK(save_transaction_recover());
    CHECK(save_transaction_recover());
    check_generation(true, true);
    end();
}

TEST(failed_restore_retains_all_backups_and_blocks_readers)
{
    GameSaveList list;
    begin(true);
    fail_publish = true;
    fail_restore = true;
    CHECK(!save_transaction_commit(slot_name, "description"));
    CHECK(access(TXN "/pending", F_OK) == 0);
    CHECK(access(TXN "/old.tfai", F_OK) == 0);
    CHECK(access(TXN "/old.tfaf", F_OK) == 0);
    CHECK(!gamelib_slot_recover());
    gamelib_savelist_create(&list);
    CHECK_INT(list.count, 0);
    CHECK(!gamelib_load(slot_name)); // Must return before touching game state.
    reset_faults();
    CHECK(save_transaction_recover());
    check_generation(false, true);
    end();
}

TEST(metadata_and_thumbnail_failures_do_not_touch_the_old_slot)
{
    GameSaveInfo info = { 0 };
    int kind;
    strcpy(info.name, "Slot0001");
    strcpy(info.description, "description");
    strcpy(info.module_name, "Arcanum");
    strcpy(info.pc_name, "Player");
    for (kind = 0; kind < 4; kind++) {
        begin(true);
        fake_bmp = kind == 0 ? 2 : 1;
        if (kind == 1) {
            fail_open = "new.gsi";
        } else if (kind == 2) {
            fail_close = "new.gsi";
        } else if (kind == 3) {
            // Actual buffered write failure, not just an injected return code.
            unlink(TXN "/new.gsi");
            CHECK_INT(symlink("/dev/full", TXN "/new.gsi"), 0);
        }
        CHECK(!gamelib_saveinfo_save(&info));
        reset_faults();
        check_generation(false, true);
        CHECK(save_transaction_recover());
        end();
    }
}

TEST(every_commit_io_failure_recovers_a_whole_generation)
{
    int limit;
    int point;
    int old;
    bool committed;
    for (old = 0; old < 2; old++) {
        begin(old != 0);
        armed = true;
        CHECK(save_transaction_commit(slot_name, "description"));
        limit = events;
        armed = false;
        CHECK(limit > 10);
        end();
        for (point = 1; point <= limit; point++) {
            begin(old != 0);
            fail_at = point;
            armed = true;
            committed = save_transaction_commit(slot_name, "description");
            reset_faults();
            CHECK(save_transaction_recover());
            CHECK(save_transaction_recover());
            check_generation(committed, old != 0);
            end();
        }
    }
}

TEST(killing_the_real_commit_at_every_io_boundary_is_recoverable)
{
    int limit;
    int point;
    int old;
    for (old = 0; old < 2; old++) {
        begin(old != 0);
        armed = true;
        CHECK(save_transaction_commit(slot_name, "description"));
        limit = events;
        armed = false;
        CHECK(limit > 10);
        end();
        for (point = 1; point <= limit; point++) {
            pid_t child;
            int status;
            bool committed;
            begin(old != 0);
            fflush(NULL);
            child = fork();
            if (child == 0) {
                kill_at = point;
                armed = true;
                _exit(save_transaction_commit(slot_name, "description") ? 0 : 1);
            }
            CHECK(child > 0);
            if (child > 0) {
                CHECK(waitpid(child, &status, 0) == child);
                CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
                // settled can itself have been cleaned up; with neither
                // pending nor staged metadata left, publication completed.
                committed = access(TXN "/settled", F_OK) == 0
                    || (access(TXN "/pending", F_OK) != 0
                        && access(TXN "/new.gsi", F_OK) != 0);
                CHECK(save_transaction_recover());
                CHECK(save_transaction_recover());
                check_generation(committed, old != 0);
            }
            end();
        }
    }
}

// A second failure or SIGKILL DURING rollback must leave enough information
// for a third attempt. This exercises restoration copies and settled cleanup.
static void leave_pending(void)
{
    begin(true);
    fail_publish = true;
    fail_restore = true;
    CHECK(!save_transaction_commit(slot_name, "description"));
    reset_faults();
    CHECK(access(TXN "/pending", F_OK) == 0);
}

TEST(recovery_itself_survives_every_io_failure_and_interruption)
{
    int limit;
    int point;
    leave_pending();
    armed = true;
    CHECK(save_transaction_recover());
    limit = events;
    reset_faults();
    end();
    for (point = 1; point <= limit; point++) {
        pid_t child;
        int status;
        leave_pending();
        armed = true;
        fail_at = point;
        CHECK(!save_transaction_recover());
        CHECK(access(TXN "/pending", F_OK) == 0 || access(TXN "/settled", F_OK) == 0
            || access(TXN, F_OK) == 0); // Last directory removal may fail.
        reset_faults();
        CHECK(save_transaction_recover());
        check_generation(false, true);
        end();

        leave_pending();
        fflush(NULL);
        child = fork();
        if (child == 0) {
            armed = true;
            kill_at = point;
            _exit(save_transaction_recover() ? 0 : 1);
        }
        CHECK(child > 0);
        if (child > 0) {
            CHECK(waitpid(child, &status, 0) == child);
            CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
            CHECK(save_transaction_recover());
            CHECK(save_transaction_recover());
            check_generation(false, true);
        }
        end();
    }
}

TEST(legacy_backups_and_corrupt_records_are_preserved_not_guessed_at)
{
    begin(true);
    write_text(REPO "/Save/Slot0001.tfai.bak", "ambiguous previous R2 backup");
    CHECK(!save_transaction_recover());
    check_text(REPO "/Save/Slot0001.tfai.bak", "ambiguous previous R2 backup");
    check_generation(false, true);
    unlink(REPO "/Save/Slot0001.tfai.bak");
    write_text(TXN "/pending", "incomplete journal");
    CHECK(!save_transaction_recover());
    check_text(TXN "/pending", "incomplete journal");
    check_generation(false, true);
    end();
}

void test_save_slot_suite(void)
{
    RUN(clean_commit_preserves_slot_names_and_loadable_archives);
    RUN(partial_cleanup_never_rolls_back_a_committed_generation);
    RUN(failed_restore_retains_all_backups_and_blocks_readers);
    RUN(metadata_and_thumbnail_failures_do_not_touch_the_old_slot);
    RUN(every_commit_io_failure_recovers_a_whole_generation);
    RUN(killing_the_real_commit_at_every_io_boundary_is_recoverable);
    RUN(recovery_itself_survives_every_io_failure_and_interruption);
    RUN(legacy_backups_and_corrupt_records_are_preserved_not_guessed_at);
}
