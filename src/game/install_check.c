#include "game/install_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "tig/compat.h"
#include "tig/database.h"

#define INSTALL_CHECK_MAX_ARCHIVES 32
#define INSTALL_CHECK_BACKUP_DIR "data/proto-shadowing-backup"

typedef struct InstallCheckArchive {
    char name[256];
    TigDatabase* database;
} InstallCheckArchive;

static InstallCheckArchive install_check_archives[INSTALL_CHECK_MAX_ARCHIVES];
static int install_check_archive_count;

bool install_check_is_main_archive(const char* name)
{
    size_t length;

    if (name == NULL) {
        return false;
    }
    length = strlen(name);
    return length >= 11
        && SDL_strncasecmp(name, "arcanum", 7) == 0
        && SDL_strcasecmp(name + length - 4, ".dat") == 0;
}

static bool install_check_exists(const char* path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

static bool install_check_is_dir(const char* path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

// A name in `dir` matched case-insensitively, as the engine resolves it; the
// real spelling is written to `out`.
static bool install_check_find(const char* dir, const char* name, char* out, size_t size)
{
    char** names;
    int count;
    int index;
    bool found = false;

    names = SDL_GlobDirectory(dir, NULL, 0, &count);
    if (names == NULL) {
        return false;
    }
    for (index = 0; index < count; index++) {
        if (SDL_strcasecmp(names[index], name) == 0) {
            if (dir[0] == '.' && dir[1] == '\0') {
                snprintf(out, size, "%s", names[index]);
            } else {
                snprintf(out, size, "%s/%s", dir, names[index]);
            }
            found = true;
            break;
        }
    }
    SDL_free(names);
    return found;
}

static void install_check_report(const char* what, bool present, const char* note)
{
    printf("  %-34s %s%s%s\n", what, present ? "present" : "MISSING",
        note != NULL ? "  - " : "", note != NULL ? note : "");
}

static int install_check_compare_names(const void* a, const void* b)
{
    return SDL_strcasecmp(((const InstallCheckArchive*)a)->name,
        ((const InstallCheckArchive*)b)->name);
}

// The main archives in mount order: case-insensitively sorted, as
// `tig_file_list_add` keeps them; the last holding a path wins.
static void install_check_open_archives(void)
{
    char** names;
    int count;
    int index;

    install_check_archive_count = 0;
    names = SDL_GlobDirectory(".", NULL, 0, &count);
    if (names == NULL) {
        return;
    }
    for (index = 0; index < count && install_check_archive_count < INSTALL_CHECK_MAX_ARCHIVES; index++) {
        if (install_check_is_main_archive(names[index])) {
            snprintf(install_check_archives[install_check_archive_count].name,
                sizeof(install_check_archives[0].name), "%s", names[index]);
            install_check_archives[install_check_archive_count].database = NULL;
            install_check_archive_count++;
        }
    }
    SDL_free(names);

    qsort(install_check_archives, (size_t)install_check_archive_count,
        sizeof(install_check_archives[0]), install_check_compare_names);
    for (index = 0; index < install_check_archive_count; index++) {
        install_check_archives[index].database = tig_database_open(install_check_archives[index].name);
    }
}

static void install_check_close_archives(void)
{
    int index;

    for (index = 0; index < install_check_archive_count; index++) {
        if (install_check_archives[index].database != NULL) {
            tig_database_close(install_check_archives[index].database);
        }
    }
    install_check_archive_count = 0;
}

static unsigned char* install_check_read_file(const char* path, size_t* size)
{
    FILE* stream;
    long length;
    unsigned char* data;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        return NULL;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) < 0 || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    data = (unsigned char*)malloc((size_t)length + 1);
    if (data != NULL && fread(data, 1, (size_t)length, stream) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(stream);
    *size = (size_t)length;
    return data;
}

// The archive copy of `entry_path` that would win without the loose file, or
// NULL; `archive` names it.
static unsigned char* install_check_archive_copy(const char* entry_path, size_t* size, const char** archive)
{
    int index;
    TigDatabaseEntry* entry;
    TigDatabaseFileHandle* stream;
    unsigned char* data;
    int length;

    for (index = install_check_archive_count - 1; index >= 0; index--) {
        if (install_check_archives[index].database == NULL
            || !tig_database_get_entry(install_check_archives[index].database, entry_path, &entry)) {
            continue;
        }
        stream = tig_database_fopen_entry(install_check_archives[index].database, entry, "rb");
        if (stream == NULL) {
            return NULL;
        }
        length = tig_database_filelength(stream);
        data = (unsigned char*)malloc(length > 0 ? (size_t)length : 1);
        if (data != NULL && length > 0
            && tig_database_fread(data, 1, (size_t)length, stream) != (size_t)length) {
            free(data);
            data = NULL;
        }
        tig_database_fclose(stream);
        *size = length > 0 ? (size_t)length : 0;
        *archive = install_check_archives[index].name;
        return data;
    }
    return NULL;
}

static bool install_check_move(const char* from, const char* name, FILE* manifest)
{
    char to[1024];

    snprintf(to, sizeof(to), "%s/%s", INSTALL_CHECK_BACKUP_DIR, name);
    if (!SDL_RenamePath(from, to)) {
        printf("    could not move %s: %s\n", from, SDL_GetError());
        return false;
    }
    if (manifest != NULL) {
        fprintf(manifest, "%s\n", name);
    }
    return true;
}

int install_check_run(bool fix)
{
    char path[1024];
    char proto_dir[1024];
    char entry_path[1024];
    char** names;
    int count;
    int index;
    int shadowing = 0;
    int same = 0;
    int moved = 0;
    bool base_ok = true;
    FILE* manifest = NULL;

    printf("Arcanum CE install check (%s)\n", fix ? "and fix" : "report only");
    printf("folder: %s\n\n", SDL_GetCurrentDirectory());

    printf("The base game (GOG Arcanum 1.0.7.4)\n");
    {
        static const char* required[] = { "tig.dat", "arcanum1.dat", "arcanum2.dat", "arcanum3.dat", "arcanum4.dat" };
        for (index = 0; index < (int)(sizeof(required) / sizeof(required[0])); index++) {
            bool present = install_check_find(".", required[index], path, sizeof(path));
            install_check_report(required[index], present, NULL);
            base_ok = base_ok && present;
        }
        bool module_dir = install_check_find(".", "modules", path, sizeof(path));
        bool module_dat = module_dir && install_check_find(path, "arcanum.dat", entry_path, sizeof(entry_path));
        install_check_report("modules/Arcanum.dat", module_dat, NULL);
        base_ok = base_ok && module_dat;
    }
    if (!base_ok) {
        printf("\nThis does not look like an Arcanum installation; run the check in the game folder.\n");
        return 2;
    }

    printf("\nThe Unofficial Arcanum Patch\n");
    install_check_report("Arcanum5.dat (UAP's data)", install_check_find(".", "arcanum5.dat", path, sizeof(path)),
        NULL);
    install_check_report("High Resolution Patch config",
        install_check_find(".", "highres", path, sizeof(path))
            && install_check_find(path, "config.ini", entry_path, sizeof(entry_path)),
        "optional; CE uses its settings when present");

    printf("\nArcanum CE's own files\n");
    install_check_report("arcanum-ce.cfg", install_check_exists("arcanum-ce.cfg"),
        "without it CE uses arcanum.cfg");
    install_check_report("ce-data/ (CE's own layer)", install_check_is_dir("ce-data"), "optional");
    install_check_report("ce-mods/ (optional mod bundles)", install_check_is_dir("ce-mods"), "optional");
    snprintf(path, sizeof(path), "modules/Arcanum/SaveCE");
    install_check_report("modules/Arcanum/SaveCE", install_check_is_dir(path),
        "created on CE's first start with `ce save folder`");

    printf("\nLoose prototypes that hide the archives' versions (data/proto)\n");
    if (!install_check_find(".", "data", path, sizeof(path))
        || !install_check_find(path, "proto", proto_dir, sizeof(proto_dir))) {
        printf("  no data/proto folder - nothing to check\n");
        return 0;
    }

    install_check_open_archives();
    names = SDL_GlobDirectory(proto_dir, "*.pro", SDL_GLOB_CASEINSENSITIVE, &count);
    if (fix && names != NULL) {
        SDL_CreateDirectory(INSTALL_CHECK_BACKUP_DIR);
        manifest = fopen(INSTALL_CHECK_BACKUP_DIR "/moved.txt", "a");
    }
    for (index = 0; names != NULL && index < count; index++) {
        size_t loose_size = 0;
        size_t archive_size = 0;
        const char* archive = NULL;
        unsigned char* loose;
        unsigned char* copy;

        snprintf(path, sizeof(path), "%s/%s", proto_dir, names[index]);
        snprintf(entry_path, sizeof(entry_path), "proto%c%s", PATH_SEPARATOR, names[index]);
        copy = install_check_archive_copy(entry_path, &archive_size, &archive);
        if (copy == NULL) {
            continue; // a prototype of its own (a mod's): nothing hidden
        }
        loose = install_check_read_file(path, &loose_size);
        if (loose != NULL && loose_size == archive_size && memcmp(loose, copy, loose_size) == 0) {
            same++;
        } else {
            shadowing++;
            printf("  %s hides the copy in %s\n", names[index], archive);
            if (fix && install_check_move(path, names[index], manifest)) {
                moved++;
            }
        }
        free(loose);
        free(copy);
    }
    SDL_free(names);
    if (manifest != NULL) {
        fclose(manifest);
    }
    install_check_close_archives();

    printf("  %d loose prototype(s) differ from the archives' copies; %d match them (harmless)\n",
        shadowing, same);
    if (shadowing > 0 && !fix) {
        printf("\nRun `arcanum-ce -check-install-fix` to move the %d into %s/ so the\n"
               "archives' versions - UAP's fixes among them - are used again. Moving them back\n"
               "restores the old state.\n",
            shadowing, INSTALL_CHECK_BACKUP_DIR);
    }
    if (fix && shadowing > 0) {
        printf("\nMoved %d of %d into %s/ (listed in moved.txt).\n", moved, shadowing, INSTALL_CHECK_BACKUP_DIR);
    }
    return shadowing > 0 ? 1 : 0;
}
