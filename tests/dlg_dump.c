// Dumps .dlg files as CE's own dialogue loader stores them.
//
// The point is differential, like `dat_dump.c`: `arcanum-work/scripts/
// quest_domains.py` reads every shipped .dlg through a Python parser
// (`mods/lib/dlgfile.py`), and a gate built on that parser can only be strict
// if it reads what the engine reads. This replaces a model of the engine's
// parser with the parser.
//
//     arcanum-dlg-dump <repo-list> <path-list>
//
// `<repo-list>` is one repository per line in mount order, lowest priority
// first, as for `arcanum-file-probe`. `<path-list>` is one game-relative path
// per line. For each path this prints
//
//     F <path> <entry count>
//
// then one line per stored entry, in stored (sorted) order:
//
//     E <num> <str> <f|g> <female_str|gender> <iq> <conditions> <response_val> <actions> <parent>
//
// tab-separated, every string as hex so no byte is lost, and `-` for a
// conditions or actions field CE stored as NULL (empty or whitespace-only).
// Not registered as a test: it needs the game's archives.
//
// `dialog.c` is included rather than linked because the loader and its
// `DialogFile` are static. Nothing in it is changed; this translation unit
// supplies every dialog.c symbol, so the archive's copy is never pulled.
// Mount only `.dat` files: mounting a directory deletes its `TIGCache`.

#include "../src/game/dialog.c"

static int read_lines(const char* path, char*** out)
{
    FILE* stream;
    char** lines = NULL;
    int count = 0;
    int capacity = 0;
    char buffer[TIG_MAX_PATH];

    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "cannot read %s\n", path);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        size_t length = strlen(buffer);

        while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            buffer[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        if (count == capacity) {
            capacity = capacity != 0 ? capacity * 2 : 64;
            lines = (char**)realloc(lines, sizeof(*lines) * capacity);
        }

        lines[count] = strdup(buffer);
        count++;
    }

    fclose(stream);
    *out = lines;
    return count;
}

static void put_hex(const char* str)
{
    putchar('\t');
    if (str == NULL) {
        putchar('-');
        return;
    }
    for (; *str != '\0'; str++) {
        printf("%02x", (unsigned char)*str);
    }
}

int main(int argc, char** argv)
{
    char** repos;
    char** paths;
    int repos_count;
    int paths_count;
    int index;
    int entry_index;
    int missing = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <repo-list> <path-list>\n", argv[0]);
        return 2;
    }

    repos_count = read_lines(argv[1], &repos);
    paths_count = read_lines(argv[2], &paths);
    if (repos_count < 0 || paths_count < 0) {
        return 1;
    }

    for (index = 0; index < repos_count; index++) {
        if (!tig_file_repository_add(repos[index])) {
            fprintf(stderr, "cannot mount %s\n", repos[index]);
            return 1;
        }
    }

    for (index = 0; index < paths_count; index++) {
        DialogFile dialog;

        memset(&dialog, 0, sizeof(dialog));
        snprintf(dialog.path, sizeof(dialog.path), "%s", paths[index]);

        if (!tig_file_exists(dialog.path, NULL)) {
            printf("F\t%s\tMISSING\n", paths[index]);
            missing++;
            continue;
        }

        dialog_load_internal(&dialog);

        printf("F\t%s\t%d\n", paths[index], dialog.entries_length);
        for (entry_index = 0; entry_index < dialog.entries_length; entry_index++) {
            DialogFileEntry* entry = &(dialog.entries[entry_index]);

            printf("E\t%d", entry->num);
            put_hex(entry->str);
            if (entry->iq == 0) {
                printf("\tf");
                put_hex(entry->data.female_str);
            } else {
                printf("\tg\t%d", entry->data.gender);
            }
            printf("\t%d", entry->iq);
            put_hex(entry->conditions);
            printf("\t%d", entry->response_val);
            put_hex(entry->actions);
            printf("\t%d\n", entry->parent);

            dialog_entry_free(entry);
        }
        FREE(dialog.entries);
    }

    fprintf(stderr, "%d paths, %d missing\n", paths_count, missing);

    return 0;
}
