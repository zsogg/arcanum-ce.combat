// Tests for `tig/file_memory.c`, `Save\Current` in memory (`save scratch in
// memory`, card HW1).
//
// The overlay's promise is that the game cannot tell it from the disk, so most
// cases here are differential: the same calls on a real file and on one in the
// overlay, every answer written into a trace, and the two traces compared. The
// archive cases go one step further and pack the same tree from disk and from
// memory into slots that must be byte-identical, because those bytes are the
// player's save.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tig/file.h"

#include "test_harness.h"

#define DISK_DIR "./arcanum_test_memfs"
#define DISK_FILE DISK_DIR "/stream.bin"
#define MEM_FILE "save\\current\\stream.bin"
#define DISK_SLOT_A "./arcanum_test_memfs_slot_a"
#define DISK_SLOT_B "./arcanum_test_memfs_slot_b"

static void trace_add(char* trace, size_t size, const char* format, long value)
{
    char item[64];

    snprintf(item, sizeof(item), format, value);
    strncat(trace, item, size - strlen(trace) - 1);
}

// Every stream call the game makes on a `Current` file, and the corners the C
// library defines: a partial last element, a seek past the end and the zeros
// a write there leaves, append, update, pushback, a line read.
static void stream_script(const char* path, char* trace, size_t size)
{
    TigFile* stream;
    unsigned char bytes[64];
    char line[16];
    int pos;
    size_t index;

    trace[0] = '\0';

    stream = tig_file_fopen(path, "wb");
    trace_add(trace, size, "open:%ld ", stream != NULL);
    if (stream == NULL) {
        return;
    }
    trace_add(trace, size, "w:%ld ", (long)tig_file_fwrite("abcdefghij", 1, 10, stream));
    trace_add(trace, size, "tell:%ld ", tig_file_ftell(stream));
    trace_add(trace, size, "seek:%ld ", tig_file_fseek(stream, 3, SEEK_SET));
    trace_add(trace, size, "w:%ld ", (long)tig_file_fwrite("XY", 2, 1, stream));
    trace_add(trace, size, "seek:%ld ", tig_file_fseek(stream, 4, SEEK_END));
    trace_add(trace, size, "w:%ld ", (long)tig_file_fwrite("\nend\n", 1, 5, stream));
    trace_add(trace, size, "neg:%ld ", tig_file_fseek(stream, -100, SEEK_CUR) != 0);
    trace_add(trace, size, "puts:%ld ", tig_file_fputs("tail", stream) >= 0);
    trace_add(trace, size, "putc:%ld ", tig_file_fputc('!', stream));
    trace_add(trace, size, "printf:%ld ", tig_file_fprintf(stream, "%d-%s", 42, "x"));
    trace_add(trace, size, "len:%ld ", tig_file_filelength(stream));
    trace_add(trace, size, "close:%ld ", tig_file_fclose(stream));

    stream = tig_file_fopen(path, "rb");
    trace_add(trace, size, "open:%ld ", stream != NULL);
    if (stream == NULL) {
        return;
    }
    trace_add(trace, size, "len:%ld ", tig_file_filelength(stream));
    trace_add(trace, size, "r:%ld ", (long)tig_file_fread(bytes, 4, 3, stream));
    trace_add(trace, size, "eof:%ld ", tig_file_feof(stream) != 0);
    trace_add(trace, size, "getc:%ld ", tig_file_fgetc(stream));
    trace_add(trace, size, "unget:%ld ", tig_file_ungetc('Q', stream));
    trace_add(trace, size, "tell:%ld ", tig_file_ftell(stream));
    trace_add(trace, size, "getc:%ld ", tig_file_fgetc(stream));
    trace_add(trace, size, "getc:%ld ", tig_file_fgetc(stream));
    trace_add(trace, size, "gets:%ld ", tig_file_fgets(line, sizeof(line), stream) != NULL);
    strncat(trace, line, size - strlen(trace) - 1);
    // No write on this read stream: the two C libraries disagree (glibc
    // answers 0 and sets the error, Microsoft's answers 1 and does not;
    // measured under Wine, 2026-11-11), and nothing in the game does it.
    tig_file_clearerr(stream);
    trace_add(trace, size, "err:%ld ", tig_file_ferror(stream) != 0);
    trace_add(trace, size, "seek:%ld ", tig_file_fseek(stream, 1000, SEEK_SET));
    trace_add(trace, size, "r:%ld ", (long)tig_file_fread(bytes, 1, 1, stream));
    trace_add(trace, size, "eof:%ld ", tig_file_feof(stream) != 0);
    tig_file_rewind(stream);
    trace_add(trace, size, "eof:%ld ", tig_file_feof(stream) != 0);
    trace_add(trace, size, "getpos:%ld ", tig_file_fgetpos(stream, &pos));
    trace_add(trace, size, "pos:%ld ", pos);
    trace_add(trace, size, "close:%ld ", tig_file_fclose(stream));

    stream = tig_file_fopen(path, "ab");
    trace_add(trace, size, "open:%ld ", stream != NULL);
    if (stream == NULL) {
        return;
    }
    tig_file_fseek(stream, 0, SEEK_SET);
    trace_add(trace, size, "w:%ld ", (long)tig_file_fwrite("AP", 1, 2, stream));
    tig_file_fclose(stream);

    stream = tig_file_fopen(path, "r+b");
    trace_add(trace, size, "open:%ld ", stream != NULL);
    if (stream == NULL) {
        return;
    }
    tig_file_fseek(stream, 1, SEEK_SET);
    trace_add(trace, size, "w:%ld ", (long)tig_file_fwrite("RW", 1, 2, stream));
    tig_file_fseek(stream, 0, SEEK_SET);
    memset(bytes, 0, sizeof(bytes));
    trace_add(trace, size, "r:%ld ", (long)tig_file_fread(bytes, 1, sizeof(bytes), stream));
    tig_file_fclose(stream);

    for (index = 0; index < sizeof(bytes) && index < 40; index++) {
        trace_add(trace, size, "%02lx", bytes[index]);
    }
}

TEST(a_memory_stream_answers_every_call_as_a_disk_file_does)
{
    char disk[2048];
    char memory[2048];

    tig_file_mkdir(DISK_DIR);
    stream_script(DISK_FILE, disk, sizeof(disk));

    tig_file_set_scratch_in_memory(true);
    stream_script(MEM_FILE, memory, sizeof(memory));
    tig_file_set_scratch_in_memory(false);

    CHECK_STR(memory, disk);
    CHECK(strstr(disk, "open:1") != NULL);

    remove(DISK_FILE);
    remove(DISK_DIR);
}

// What the game's own checks depend on: a read open of a missing file and a
// write open into a missing folder both fail, a folder cannot be opened as a
// file, and the scratch folder itself always exists.
static void path_script(const char* root, char* trace, size_t size)
{
    char path[256];
    char other[256];
    TigFileInfo info;
    TigFileList list;
    TigFile* stream;
    unsigned int index;

    trace[0] = '\0';

#define P(buffer, rest) snprintf((buffer), sizeof(buffer), "%s%s", root, (rest))
    P(path, "\\nothing.dat");
    trace_add(trace, size, "missing:%ld ", tig_file_fopen(path, "rb") == NULL);
    P(path, "\\nodir\\a.dat");
    trace_add(trace, size, "nodir:%ld ", tig_file_fopen(path, "wb") == NULL);
    P(path, "\\maps\\Arcanum1-024-fixed");
    trace_add(trace, size, "mkdir:%ld ", tig_file_mkdir(path));
    trace_add(trace, size, "again:%ld ", tig_file_mkdir(path));
    trace_add(trace, size, "isdir:%ld ", tig_file_is_directory(path));
    trace_add(trace, size, "dir-as-file:%ld ", tig_file_fopen(path, "rb") == NULL);
    trace_add(trace, size, "empty:%ld ", tig_file_is_empty_directory(path));

    P(path, "\\maps\\Arcanum1-024-fixed\\65431142903.dif");
    stream = tig_file_fopen(path, "wb");
    trace_add(trace, size, "create:%ld ", stream != NULL);
    if (stream != NULL) {
        tig_file_fwrite("diff", 1, 4, stream);
        tig_file_fclose(stream);
    }
    trace_add(trace, size, "exists:%ld ", tig_file_exists(path, &info));
    strncat(trace, info.path, size - strlen(trace) - 1);
    trace_add(trace, size, " size:%ld ", (long)info.size);
    trace_add(trace, size, "attr:%ld ", (long)info.attributes);
    P(path, "\\maps\\ARCANUM1-024-FIXED\\65431142903.DIF");
    trace_add(trace, size, "case:%ld ", tig_file_exists(path, NULL));

    P(path, "\\maps\\Arcanum1-024-fixed\\mobile.md");
    stream = tig_file_fopen(path, "wb");
    if (stream != NULL) {
        tig_file_fclose(stream);
    }
    P(path, "\\maps\\Arcanum1-024-fixed\\*.*");
    tig_file_list_create(&list, path);
    trace_add(trace, size, "list:%ld", (long)list.count);
    for (index = 0; index < list.count; index++) {
        strncat(trace, " ", size - strlen(trace) - 1);
        strncat(trace, list.entries[index].path, size - strlen(trace) - 1);
    }
    tig_file_list_destroy(&list);
    P(path, "\\maps\\Arcanum1-024-fixed\\*.dif");
    tig_file_list_create(&list, path);
    trace_add(trace, size, " dif:%ld ", (long)list.count);
    tig_file_list_destroy(&list);
    P(path, "\\*.*");
    tig_file_list_create(&list, path);
    trace_add(trace, size, "top:%ld ", (long)list.count);
    if (list.count == 1) {
        strncat(trace, list.entries[0].path, size - strlen(trace) - 1);
        trace_add(trace, size, ":%ld ", (long)list.entries[0].attributes);
    }
    tig_file_list_destroy(&list);

    P(path, "\\maps\\Arcanum1-024-fixed\\mobile.md");
    P(other, "\\maps\\Arcanum1-024-fixed\\mobile.des");
    trace_add(trace, size, "rename:%ld ", tig_file_rename(path, other));
    trace_add(trace, size, "old:%ld ", tig_file_exists(path, NULL));
    trace_add(trace, size, "new:%ld ", tig_file_exists(other, NULL));
    P(path, "\\maps");
    trace_add(trace, size, "rmdir-full:%ld ", tig_file_rmdir(path));
    trace_add(trace, size, "remove:%ld ", tig_file_remove(other));
    trace_add(trace, size, "remove-again:%ld ", tig_file_remove(other) != 0);

    trace_add(trace, size, "empty-tree:%ld ", tig_file_empty_directory(root));
    trace_add(trace, size, "root-empty:%ld ", tig_file_is_empty_directory(root));
    trace_add(trace, size, "root-dir:%ld ", tig_file_is_directory(root));
#undef P
}

TEST(the_folder_answers_every_path_call_as_a_disk_folder_does)
{
    char disk[2048];
    char memory[2048];

    tig_file_mkdir(DISK_DIR);
    path_script(DISK_DIR, disk, sizeof(disk));

    tig_file_set_scratch_in_memory(true);
    path_script("save\\current", memory, sizeof(memory));
    tig_file_set_scratch_in_memory(false);

    // The two differ only where they must: the disk folder's name.
    CHECK_STR(strstr(memory, "missing:"), strstr(disk, "missing:"));
    CHECK(strstr(memory, "create:1") != NULL);
    CHECK(strstr(memory, "list:2 65431142903.dif mobile.md") != NULL);

    remove(DISK_DIR);
}

static void tree_write(const char* root)
{
    static const unsigned char binary[] = { 0x00, 0x01, 0xFF, 0x7F, 0x00, 0x41 };
    char path[256];
    TigFile* stream;

    snprintf(path, sizeof(path), "%s\\maps\\Arcanum1-024-fixed", root);
    tig_file_mkdir(path);
    snprintf(path, sizeof(path), "%s\\maps\\Empty Map", root);
    tig_file_mkdir(path);

    snprintf(path, sizeof(path), "%s\\data.sav", root);
    stream = tig_file_fopen(path, "wb");
    tig_file_fwrite("the save", 1, 8, stream);
    tig_file_fclose(stream);

    snprintf(path, sizeof(path), "%s\\maps\\Arcanum1-024-fixed\\65431142903.dif", root);
    stream = tig_file_fopen(path, "wb");
    tig_file_fwrite(binary, 1, sizeof(binary), stream);
    tig_file_fclose(stream);

    snprintf(path, sizeof(path), "%s\\maps\\Arcanum1-024-fixed\\mobile.des", root);
    stream = tig_file_fopen(path, "ab");
    tig_file_fwrite("ab", 1, 2, stream);
    tig_file_fclose(stream);
}

static long file_bytes(const char* path, unsigned char* buffer, size_t size)
{
    FILE* stream = fopen(path, "rb");
    size_t length;

    if (stream == NULL) {
        return -1;
    }
    length = fread(buffer, 1, size, stream);
    fclose(stream);
    return (long)length;
}

static bool same_file(const char* a, const char* b)
{
    static unsigned char left[4096];
    static unsigned char right[4096];
    long left_length = file_bytes(a, left, sizeof(left));
    long right_length = file_bytes(b, right, sizeof(right));

    return left_length >= 0 && left_length == right_length && memcmp(left, right, (size_t)left_length) == 0;
}

static void slot_remove(const char* slot)
{
    char path[256];

    snprintf(path, sizeof(path), "%s.tfai", slot);
    remove(path);
    snprintf(path, sizeof(path), "%s.tfaf", slot);
    remove(path);
}

// The player's save: a tree packed from memory is the slot the same tree on
// disk packs into, byte for byte, and unpacking a slot into memory and packing
// it again gives the same slot back.
TEST(a_slot_packed_from_memory_is_the_slot_the_disk_packs)
{
    tig_file_mkdir(DISK_DIR);
    tree_write(DISK_DIR);
    CHECK(tig_file_archive(DISK_SLOT_A, DISK_DIR));

    tig_file_set_scratch_in_memory(true);
    tree_write("save\\current");
    CHECK(tig_file_archive(DISK_SLOT_B, "save\\current"));
    CHECK(same_file(DISK_SLOT_A ".tfai", DISK_SLOT_B ".tfai"));
    CHECK(same_file(DISK_SLOT_A ".tfaf", DISK_SLOT_B ".tfaf"));

    CHECK(tig_file_empty_directory("save\\current"));
    CHECK(tig_file_is_empty_directory("save\\current"));
    CHECK(tig_file_unarchive(DISK_SLOT_A, "save\\current"));
    CHECK(tig_file_exists("save\\current\\maps\\Empty Map", NULL));
    slot_remove(DISK_SLOT_B);
    CHECK(tig_file_archive(DISK_SLOT_B, "save\\current"));
    CHECK(same_file(DISK_SLOT_A ".tfai", DISK_SLOT_B ".tfai"));
    CHECK(same_file(DISK_SLOT_A ".tfaf", DISK_SLOT_B ".tfaf"));
    tig_file_set_scratch_in_memory(false);

    tig_file_empty_directory(DISK_DIR);
    remove(DISK_DIR);
    slot_remove(DISK_SLOT_A);
    slot_remove(DISK_SLOT_B);
}

// Nothing under the folder reaches the disk, and with the switch off nothing
// is held: the disk is used exactly as before.
TEST(the_folder_never_touches_the_disk_and_off_leaves_the_disk_alone)
{
    TigFile* stream;

    tig_file_set_scratch_in_memory(true);
    tig_file_mkdir("save\\current\\maps");
    stream = tig_file_fopen("save\\current\\maps\\x.dif", "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        tig_file_fclose(stream);
    }
    CHECK(fopen("./save/current/maps/x.dif", "rb") == NULL);
    CHECK(fopen("./save/current", "rb") == NULL);
    tig_file_set_scratch_in_memory(false);

    CHECK(!tig_file_exists("save\\current\\maps\\x.dif", NULL));
}

// With `ce save folder` the game's `save\...` becomes `SaveCE\...` before the
// overlay sees it, and the overlay follows that name.
TEST(the_folder_follows_the_save_folder_redirect)
{
    TigFile* stream;

    tig_file_set_save_dir("SaveCE");
    tig_file_set_scratch_in_memory(true);
    stream = tig_file_fopen("save\\current\\data.sav", "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        tig_file_fclose(stream);
    }
    CHECK(tig_file_exists("SaveCE\\Current\\data.sav", NULL));
    CHECK(fopen("./SaveCE/Current/data.sav", "rb") == NULL);
    tig_file_set_scratch_in_memory(false);
    tig_file_set_save_dir(NULL);
}

// A file removed while a stream has it open stays readable through that
// stream, as on Linux; the folder forgets it at once.
TEST(a_removed_file_stays_readable_through_an_open_stream)
{
    TigFile* writer;
    TigFile* reader;
    char buffer[8] = { 0 };

    tig_file_set_scratch_in_memory(true);
    writer = tig_file_fopen("save\\current\\a.dat", "wb");
    tig_file_fwrite("kept", 1, 4, writer);
    tig_file_fclose(writer);

    reader = tig_file_fopen("save\\current\\a.dat", "rb");
    CHECK(reader != NULL);
    CHECK_INT(tig_file_remove("save\\current\\a.dat"), 0);
    CHECK(!tig_file_exists("save\\current\\a.dat", NULL));
    if (reader != NULL) {
        CHECK_INT((int)tig_file_fread(buffer, 1, 4, reader), 4);
        CHECK_STR(buffer, "kept");
        tig_file_fclose(reader);
    }
    tig_file_set_scratch_in_memory(false);
}

void test_file_memory_suite(void)
{
    RUN(a_memory_stream_answers_every_call_as_a_disk_file_does);
    RUN(the_folder_answers_every_path_call_as_a_disk_folder_does);
    RUN(a_slot_packed_from_memory_is_the_slot_the_disk_packs);
    RUN(the_folder_never_touches_the_disk_and_off_leaves_the_disk_alone);
    RUN(the_folder_follows_the_save_folder_redirect);
    RUN(a_removed_file_stays_readable_through_an_open_stream);
}
