#include "game/save_transaction.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tig/debug.h"
#include "tig/file.h"

// CE: process-interruption atomicity for a four-file save; no fsync/durability
// guarantee. The game has one save writer. Do not run recovery concurrently
// with it. The live file layout and serialized game bytes stay unchanged.
//
// Maintainer/LLM invariants (read before changing the ordering):
//  1. No marker: only preparation (or an empty directory) can remain.
//  2. pending: all old bytes exist in immutable backups. ALWAYS roll back.
//  3. settled: either commit or rollback finished. ONLY clean up.
// The atomic pending -> settled rename is the decision point. Deleting a
// backup is NEVER a decision point. Backups are copied, not consumed during
// rollback, so a second interruption can repeat the whole restoration.
// Cleanup removes settled LAST. Failed restoration always retains pending.
#define PENDING SAVE_TRANSACTION_DIR "\\pending"
#define SETTLED SAVE_TRANSACTION_DIR "\\settled"
#define RECORD_TMP SAVE_TRANSACTION_DIR "\\record.tmp"
#define RESTORE_TMP SAVE_TRANSACTION_DIR "\\restore.tmp"

typedef struct SaveTransaction {
    uint32_t magic;
    uint32_t existed;
    char name[9];
    char metadata[TIG_MAX_PATH];
} SaveTransaction;

#define TRANSACTION_MAGIC 0x31545343u
static const char* const extensions[] = { "tfai", "tfaf", "bmp", "gsi" };

static bool component_valid(const char* value)
{
    const unsigned char* p = (const unsigned char*)value;
    for (; *p != 0; p++) {
        if (*p < 32 || strchr("\\/:*?\"<>|", *p) != NULL) {
            return false;
        }
    }
    return true;
}

static bool record_valid(const SaveTransaction* record)
{
    size_t length;
    if (record->magic != TRANSACTION_MAGIC || record->existed > 15
        || record->name[8] != 0 || strlen(record->name) != 8
        || !component_valid(record->name)
        || memchr(record->metadata, 0, sizeof(record->metadata)) == NULL) {
        return false;
    }
    length = strlen(record->metadata);
    return component_valid(record->metadata) && length >= 12
        && length + 6 < TIG_MAX_PATH
        && strncmp(record->metadata, record->name, 8) == 0
        && strcmp(record->metadata + length - 4, ".gsi") == 0;
}

static void live_path(const SaveTransaction* record, int index, char* path)
{
    if (index == 3) {
        // record_valid/commit bound this basename before any path is used.
        strcpy(path, "save\\");
        strcat(path, record->metadata);
    } else {
        snprintf(path, TIG_MAX_PATH, "save\\%s.%s", record->name, extensions[index]);
    }
}

static void backup_path(int index, char* path)
{
    snprintf(path, TIG_MAX_PATH, SAVE_TRANSACTION_DIR "\\old.%s", extensions[index]);
}

static bool remove_if_present(const char* path)
{
    return !tig_file_exists(path, NULL) || tig_file_remove(path) == 0;
}

// tig_file_copy's contract does not check both buffered closes. Recovery must
// never replace a live file with a copy whose final flush failed.
static bool checked_copy(const char* src, const char* dst)
{
    TigFile* input = tig_file_fopen(src, "rb");
    TigFile* output;
    char buffer[16384];
    size_t count;
    bool ok = true;
    if (input == NULL) {
        return false;
    }
    output = tig_file_fopen(dst, "wb");
    if (output == NULL) {
        tig_file_fclose(input);
        return false;
    }
    while ((count = tig_file_fread(buffer, 1, sizeof(buffer), input)) != 0) {
        if (tig_file_fwrite(buffer, 1, count, output) != count) {
            ok = false;
            break;
        }
    }
    if (tig_file_ferror(input)) {
        ok = false;
    }
    if (tig_file_fclose(input) != 0) {
        ok = false;
    }
    if (tig_file_fclose(output) != 0) {
        ok = false;
    }
    return ok;
}

static bool cleanup(void)
{
    char path[TIG_MAX_PATH];
    int index;
    bool ok = true;
    // Enumerating a directory can itself fail. Use the protocol's fixed file
    // set so a failed listing can never trick us into deleting settled first.
    for (index = 0; index < 4; index++) {
        backup_path(index, path);
        if (!remove_if_present(path)) {
            ok = false;
        }
        snprintf(path, sizeof(path), SAVE_TRANSACTION_STAGE ".%s", extensions[index]);
        if (!remove_if_present(path)) {
            ok = false;
        }
    }
    if (!remove_if_present(SAVE_TRANSACTION_STAGE ".tfai.tmp")) ok = false;
    if (!remove_if_present(SAVE_TRANSACTION_STAGE ".tfaf.tmp")) ok = false;
    if (!remove_if_present(RECORD_TMP)) ok = false;
    if (!remove_if_present(RESTORE_TMP)) ok = false;
    if (!ok || !remove_if_present(SETTLED)) {
        return false;
    }
    return remove_if_present(SAVE_TRANSACTION_DIR);
}

bool save_transaction_recover(void)
{
    SaveTransaction record;
    TigFile* stream;
    char live[TIG_MAX_PATH];
    char backup[TIG_MAX_PATH];
    int index;
    bool ok;

    // The previous R2 implementation left no commitment evidence. A surviving
    // .bak might be old data after a SUCCESSFUL save. Preserve it for manual
    // inspection; never silently import that ambiguous generation.
    for (index = 0; index < 2; index++) {
        TigFileList list;
        snprintf(live, sizeof(live), "save\\*.%s.bak", extensions[index]);
        tig_file_list_create(&list, live);
        ok = list.count == 0;
        tig_file_list_destroy(&list);
        if (!ok) {
            tig_debug_printf("save recovery: ambiguous legacy archive .bak files; preserve and inspect Save before retrying\n");
            return false;
        }
    }

    if (tig_file_exists(PENDING, NULL)) {
        if (tig_file_exists(SETTLED, NULL)) {
            return false;
        }
        stream = tig_file_fopen(PENDING, "rb");
        if (stream == NULL) {
            return false;
        }
        ok = tig_file_fread(&record, sizeof(record), 1, stream) == 1;
        if (tig_file_filelength(stream) != sizeof(record)) {
            ok = false;
        }
        if (tig_file_fclose(stream) != 0) {
            ok = false;
        }
        if (!ok || !record_valid(&record)) {
            tig_debug_printf("save recovery: invalid transaction record; retained for inspection\n");
            return false;
        }
        for (index = 0; index < 4; index++) {
            live_path(&record, index, live);
            backup_path(index, backup);
            if ((record.existed & (1u << index)) != 0) {
                if (!checked_copy(backup, RESTORE_TMP)
                    || tig_file_rename(RESTORE_TMP, live) != 0) {
                    return false;
                }
            } else if (!remove_if_present(live)) {
                return false;
            }
        }
        // Rollback is complete. Cleanup may now discard backups without
        // making another recovery attempt depend on any of them.
        if (tig_file_rename(PENDING, SETTLED) != 0) {
            return false;
        }
    }
    return cleanup();
}

bool save_transaction_begin(void)
{
    char directory[TIG_MAX_PATH];
    char path[TIG_MAX_PATH];
    int length;
    // Resolve the existing parent through the repository before mkdir: its
    // case-folding shim otherwise sees the working directory, not the repo.
    if (!save_transaction_recover() || !tig_file_extract("save", directory)) {
        return false;
    }
    length = snprintf(path, sizeof(path), "%s/ce-save-transaction", directory);
    return length > 0 && (size_t)length < sizeof(path) && tig_file_mkdir(path);
}

bool save_transaction_commit(const char* name, const char* description)
{
    SaveTransaction record = { 0 };
    char live[TIG_MAX_PATH];
    char backup[TIG_MAX_PATH];
    char staged[TIG_MAX_PATH];
    TigFile* stream;
    int index;
    bool ok;

    if (strlen(name) != 8 || !component_valid(name) || !component_valid(description)
        || strlen(name) + strlen(description) + 10 >= TIG_MAX_PATH
        || tig_file_exists(PENDING, NULL) || tig_file_exists(SETTLED, NULL)) {
        return false;
    }
    record.magic = TRANSACTION_MAGIC;
    strcpy(record.name, name);
    snprintf(record.metadata, sizeof(record.metadata), "%s%s.gsi", name, description);
    for (index = 0; index < 4; index++) {
        snprintf(staged, sizeof(staged), SAVE_TRANSACTION_STAGE ".%s", extensions[index]);
        if (!tig_file_exists(staged, NULL)) {
            return false;
        }
        live_path(&record, index, live);
        backup_path(index, backup);
        if (tig_file_exists(live, NULL)) {
            record.existed |= 1u << index;
            if (!checked_copy(live, backup)) {
                return false;
            }
        }
    }
    stream = tig_file_fopen(RECORD_TMP, "wb");
    if (stream == NULL) {
        return false;
    }
    ok = tig_file_fwrite(&record, sizeof(record), 1, stream) == 1;
    if (tig_file_fclose(stream) != 0) {
        ok = false;
    }
    if (!ok || tig_file_rename(RECORD_TMP, PENDING) != 0) {
        return false;
    }

    for (index = 0; index < 4; index++) {
        live_path(&record, index, live);
        snprintf(staged, sizeof(staged), SAVE_TRANSACTION_STAGE ".%s", extensions[index]);
        if (tig_file_rename(staged, live) != 0) {
            save_transaction_recover();
            return false;
        }
    }
    if (tig_file_rename(PENDING, SETTLED) != 0) {
        save_transaction_recover();
        return false;
    }
    // Success is already committed. Failed cleanup is retried on the next
    // save/list/load, and MUST NOT turn a committed generation into rollback.
    if (!cleanup()) {
        tig_debug_printf("save transaction: committed; cleanup deferred\n");
    }
    return true;
}
