// CE (`save scratch in memory`, card HW1, zsogg 2026-11-11): `Save\Current` in
// memory.
//
// `Save\Current` is the save's scratch folder. A load or a new game empties it
// and unpacks the slot into it, a save packs it back into the slot, and in
// between the game writes it all the time: every sector the 16-sector cache
// evicts (`sector_save_game`), every map change. Nothing outside the running
// game reads it, so it can live in memory: the same files, the same answers to
// every call, and no disk write until a save packs the slot. zsogg's 11-09 crash
// was one of those writes refused for a moment on Windows (BR2).
//
// The folder is a flat table of nodes keyed by their path below the root,
// lowercased (the disk folded case on Windows and `compat_resolve_path` does on
// Linux), each keeping the spelling it was created with for listings and so for
// the slot's index. The root itself is not a node and always exists. A file's
// bytes are shared by its node and every open stream, so a file removed while
// open stays readable through that stream, as on Linux.

#include "file_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "tig/memory.h"

typedef struct TigMemData {
    unsigned char* bytes;
    size_t size;
    size_t capacity;
    int refs;
} TigMemData;

typedef struct TigMemNode {
    char* key;
    char* name;
    bool is_dir;
    TigMemData* data;
    time_t modify_time;
} TigMemNode;

struct TigMemFile {
    TigMemData* data;
    size_t pos;
    bool readable;
    bool writable;
    bool append;
    bool eof;
    bool error;
    int pushback;
};

static bool tig_memfs_on;

// shortcut: a linear table; a game's `Current` holds a few hundred to a few
// thousand entries and each open scans it once. A hash if a profile asks.
static TigMemNode* tig_memfs_nodes;
static int tig_memfs_node_count;
static int tig_memfs_node_capacity;

static bool is_sep(char ch)
{
    return ch == '/' || ch == '\\';
}

static void data_release(TigMemData* data)
{
    if (data != NULL && --data->refs == 0) {
        FREE(data->bytes);
        FREE(data);
    }
}

void tig_memfs_set_enabled(bool enabled)
{
    tig_memfs_on = enabled;
}

bool tig_memfs_enabled(void)
{
    return tig_memfs_on;
}

void tig_memfs_clear(void)
{
    int index;

    for (index = 0; index < tig_memfs_node_count; index++) {
        data_release(tig_memfs_nodes[index].data);
        FREE(tig_memfs_nodes[index].key);
        FREE(tig_memfs_nodes[index].name);
    }
    FREE(tig_memfs_nodes);
    tig_memfs_nodes = NULL;
    tig_memfs_node_count = 0;
    tig_memfs_node_capacity = 0;
}

// The key below the root ("" for the root), lowercased with `/` between
// components, or false when the overlay is off or `path` is not under it.
static bool memfs_key(const char* path, char* key, size_t size)
{
    const char* save_dir;
    const char* p;
    size_t length;
    size_t k = 0;

    if (!tig_memfs_on) {
        return false;
    }

    if (path[0] == '\0' || path[0] == '.' || is_sep(path[0]) || path[1] == ':') {
        return false;
    }

    save_dir = tig_file_save_dir()[0] != '\0' ? tig_file_save_dir() : "save";
    length = strlen(save_dir);
    if (SDL_strncasecmp(path, save_dir, length) != 0 || !is_sep(path[length])) {
        return false;
    }

    p = path + length + 1;
    if (SDL_strncasecmp(p, "current", 7) != 0 || (p[7] != '\0' && !is_sep(p[7]))) {
        return false;
    }

    for (p += 7; *p != '\0'; p++) {
        if (is_sep(*p)) {
            if (k != 0 && key[k - 1] != '/') {
                key[k++] = '/';
            }
        } else {
            key[k++] = (char)SDL_tolower((unsigned char)*p);
        }
        if (k + 1 >= size) {
            return false;
        }
    }
    if (k != 0 && key[k - 1] == '/') {
        k--;
    }
    key[k] = '\0';

    return true;
}

bool tig_memfs_claims(const char* path)
{
    char key[TIG_MAX_PATH];

    return memfs_key(path, key, sizeof(key));
}

static int node_find(const char* key)
{
    int index;

    for (index = 0; index < tig_memfs_node_count; index++) {
        if (strcmp(tig_memfs_nodes[index].key, key) == 0) {
            return index;
        }
    }

    return -1;
}

static bool is_directory_key(const char* key)
{
    int index;

    if (key[0] == '\0') {
        return true;
    }

    index = node_find(key);
    return index != -1 && tig_memfs_nodes[index].is_dir;
}

static bool parent_exists(const char* key)
{
    char parent[TIG_MAX_PATH];
    const char* slash = strrchr(key, '/');

    if (slash == NULL) {
        return true;
    }

    memcpy(parent, key, (size_t)(slash - key));
    parent[slash - key] = '\0';
    return is_directory_key(parent);
}

static bool has_children(const char* key)
{
    size_t length = strlen(key);
    int index;

    for (index = 0; index < tig_memfs_node_count; index++) {
        if (length == 0
            || (strncmp(tig_memfs_nodes[index].key, key, length) == 0
                && tig_memfs_nodes[index].key[length] == '/')) {
            return true;
        }
    }

    return false;
}

// Past the first two components of a claimed path, `<save>\Current`, and the
// separators after them.
static const char* skip_root(const char* path)
{
    const char* p = path;
    int component;

    for (component = 0; component < 2; component++) {
        while (*p != '\0' && !is_sep(*p)) {
            p++;
        }
        while (is_sep(*p)) {
            p++;
        }
    }

    return p;
}

// The last component of `path` as the caller spelled it.
static const char* path_basename(const char* path)
{
    const char* name = path;
    const char* p;

    for (p = path; *p != '\0'; p++) {
        if (is_sep(*p) && p[1] != '\0' && !is_sep(p[1])) {
            name = p + 1;
        }
    }

    return name;
}

static int node_add(const char* key, const char* name, size_t name_length, bool is_dir)
{
    TigMemNode* node;

    if (tig_memfs_node_count == tig_memfs_node_capacity) {
        tig_memfs_node_capacity = tig_memfs_node_capacity != 0 ? tig_memfs_node_capacity * 2 : 64;
        tig_memfs_nodes = (TigMemNode*)REALLOC(tig_memfs_nodes, sizeof(*tig_memfs_nodes) * tig_memfs_node_capacity);
    }

    node = &(tig_memfs_nodes[tig_memfs_node_count]);
    node->key = SDL_strdup(key);
    node->name = (char*)MALLOC(name_length + 1);
    memcpy(node->name, name, name_length);
    node->name[name_length] = '\0';
    while (name_length != 0 && is_sep(node->name[name_length - 1])) {
        node->name[--name_length] = '\0';
    }
    node->is_dir = is_dir;
    node->data = NULL;
    node->modify_time = time(NULL);
    if (!is_dir) {
        node->data = (TigMemData*)CALLOC(1, sizeof(*node->data));
        node->data->refs = 1;
    }

    return tig_memfs_node_count++;
}

static void node_remove(int index)
{
    data_release(tig_memfs_nodes[index].data);
    FREE(tig_memfs_nodes[index].key);
    FREE(tig_memfs_nodes[index].name);
    tig_memfs_nodes[index] = tig_memfs_nodes[--tig_memfs_node_count];
}

static void node_info(const TigMemNode* node, TigFileInfo* info)
{
    SDL_strlcpy(info->path, node->name, sizeof(info->path));
    info->attributes = node->is_dir ? TIG_FILE_ATTRIBUTE_SUBDIR : 0;
    info->size = node->is_dir ? 0 : node->data->size;
    info->modify_time = node->modify_time;
}

TigMemFile* tig_memfs_open(const char* path, const char* mode)
{
    char key[TIG_MAX_PATH];
    TigMemFile* file;
    TigMemData* data;
    bool update = strchr(mode, '+') != NULL;
    int index;

    if (!memfs_key(path, key, sizeof(key)) || key[0] == '\0') {
        return NULL;
    }

    // Text and binary are the same here: every write in `Current` is "wb",
    // "ab" or "r+b"; the one "wt" is the editor's `startloc.txt`.
    index = node_find(key);
    if (index != -1 && tig_memfs_nodes[index].is_dir) {
        return NULL;
    }

    switch (mode[0]) {
    case 'r':
        if (index == -1) {
            return NULL;
        }
        break;
    case 'w':
    case 'a':
        if (index == -1) {
            if (!parent_exists(key)) {
                return NULL;
            }
            index = node_add(key, path_basename(path), strlen(path_basename(path)), false);
        } else if (mode[0] == 'w') {
            tig_memfs_nodes[index].data->size = 0;
        }
        tig_memfs_nodes[index].modify_time = time(NULL);
        break;
    default:
        return NULL;
    }

    data = tig_memfs_nodes[index].data;
    data->refs++;

    file = (TigMemFile*)MALLOC(sizeof(*file));
    file->data = data;
    file->pos = mode[0] == 'a' ? data->size : 0;
    file->readable = mode[0] == 'r' || update;
    file->writable = mode[0] != 'r' || update;
    file->append = mode[0] == 'a';
    file->eof = false;
    file->error = false;
    file->pushback = EOF;

    return file;
}

bool tig_memfs_exists(const char* path, TigFileInfo* info)
{
    char key[TIG_MAX_PATH];
    int index;

    if (!memfs_key(path, key, sizeof(key))) {
        return false;
    }

    if (key[0] == '\0') {
        if (info != NULL) {
            SDL_strlcpy(info->path, "Current", sizeof(info->path));
            info->attributes = TIG_FILE_ATTRIBUTE_SUBDIR;
            info->size = 0;
            info->modify_time = 0;
        }
        return true;
    }

    index = node_find(key);
    if (index == -1) {
        return false;
    }

    if (info != NULL) {
        node_info(&(tig_memfs_nodes[index]), info);
    }

    return true;
}

// `*` and `?`, case-insensitive: what `SDL_GlobDirectory` matches a name with.
static bool wildcard_match(const char* pattern, const char* name)
{
    while (*pattern != '\0') {
        if (*pattern == '*') {
            pattern++;
            do {
                if (wildcard_match(pattern, name)) {
                    return true;
                }
            } while (*name++ != '\0');
            return false;
        }
        if (*name == '\0'
            || (*pattern != '?' && SDL_tolower((unsigned char)*pattern) != SDL_tolower((unsigned char)*name))) {
            return false;
        }
        pattern++;
        name++;
    }

    return *name == '\0';
}

void tig_memfs_list(const char* pattern, void (*add)(TigFileList*, TigFileInfo*), TigFileList* list)
{
    char dir[TIG_MAX_PATH];
    char key[TIG_MAX_PATH];
    const char* name_pattern;
    size_t length;
    TigFileInfo info;
    int index;

    name_pattern = path_basename(pattern);
    length = (size_t)(name_pattern - pattern);
    if (length == 0 || length >= sizeof(dir)) {
        return;
    }
    memcpy(dir, pattern, length);
    dir[length] = '\0';

    if (!memfs_key(dir, key, sizeof(key)) || !is_directory_key(key)) {
        return;
    }

    length = strlen(key);
    for (index = 0; index < tig_memfs_node_count; index++) {
        const char* node_key = tig_memfs_nodes[index].key;
        const char* rest;

        if (length != 0 && (strncmp(node_key, key, length) != 0 || node_key[length] != '/')) {
            continue;
        }
        rest = length != 0 ? node_key + length + 1 : node_key;
        if (strchr(rest, '/') != NULL || !wildcard_match(name_pattern, tig_memfs_nodes[index].name)) {
            continue;
        }

        node_info(&(tig_memfs_nodes[index]), &info);
        add(list, &info);
    }
}

// As `SDL_RemovePath`, which `tig_file_remove_native` answers with: a file or
// an empty directory goes, and a path that is not there counts as removed.
// 0, or 1.
int tig_memfs_remove(const char* path)
{
    char key[TIG_MAX_PATH];
    int index;

    if (!memfs_key(path, key, sizeof(key)) || key[0] == '\0') {
        return 1;
    }

    index = node_find(key);
    if (index == -1) {
        return 0;
    }
    if (tig_memfs_nodes[index].is_dir && has_children(key)) {
        return 1;
    }

    node_remove(index);
    return 0;
}

// As `SDL_RenamePath`: a file replaces a file in the way, a directory takes
// its tree with it. Both paths must be in the overlay; 0, or 1.
int tig_memfs_rename(const char* old_path, const char* new_path)
{
    char old_key[TIG_MAX_PATH];
    char new_key[TIG_MAX_PATH];
    char moved[TIG_MAX_PATH];
    const char* name;
    size_t old_length;
    int index;
    int target;

    if (!memfs_key(old_path, old_key, sizeof(old_key)) || old_key[0] == '\0'
        || !memfs_key(new_path, new_key, sizeof(new_key)) || new_key[0] == '\0') {
        return 1;
    }

    index = node_find(old_key);
    if (index == -1 || !parent_exists(new_key)) {
        return 1;
    }
    if (strcmp(old_key, new_key) == 0) {
        return 0;
    }

    target = node_find(new_key);
    if (target != -1) {
        if (tig_memfs_nodes[target].is_dir || tig_memfs_nodes[index].is_dir) {
            return 1;
        }
        node_remove(target);
        index = node_find(old_key);
    }

    old_length = strlen(old_key);
    if (tig_memfs_nodes[index].is_dir) {
        int child;

        for (child = 0; child < tig_memfs_node_count; child++) {
            char* key = tig_memfs_nodes[child].key;

            if (strncmp(key, old_key, old_length) == 0 && key[old_length] == '/') {
                SDL_snprintf(moved, sizeof(moved), "%s%s", new_key, key + old_length);
                FREE(key);
                tig_memfs_nodes[child].key = SDL_strdup(moved);
            }
        }
    }

    FREE(tig_memfs_nodes[index].key);
    tig_memfs_nodes[index].key = SDL_strdup(new_key);
    name = path_basename(new_path);
    FREE(tig_memfs_nodes[index].name);
    tig_memfs_nodes[index].name = SDL_strdup(name);

    return 0;
}

// As `SDL_CreateDirectory`: the missing parents too, and true for a directory
// already there. 0, or -1.
int tig_memfs_mkdir(const char* path)
{
    char key[TIG_MAX_PATH];
    char prefix[TIG_MAX_PATH];
    const char* p;
    size_t k;

    if (!memfs_key(path, key, sizeof(key))) {
        return -1;
    }

    // Walk the caller's spelling and the key together, one component at a
    // time; the two have the same components, the key lowercased. `p` starts
    // past `<save>\Current\`.
    p = skip_root(path);

    k = 0;
    while (key[k] != '\0') {
        int index;
        const char* end;

        while (key[k] != '\0' && key[k] != '/') {
            k++;
        }
        memcpy(prefix, key, k);
        prefix[k] = '\0';

        for (end = p; *end != '\0' && !is_sep(*end); end++) {
        }

        index = node_find(prefix);
        if (index == -1) {
            node_add(prefix, p, (size_t)(end - p), true);
        } else if (!tig_memfs_nodes[index].is_dir) {
            return -1;
        }

        p = end;
        while (is_sep(*p)) {
            p++;
        }
        if (key[k] == '/') {
            k++;
        }
    }

    return 0;
}

// As `SDL_RemovePath`, which `tig_file_rmdir_ex_native` answers with: an
// empty directory goes (a file too), and a path that is not there counts as
// removed. The root stays, as the folder always exists for the game; 0, or -1.
int tig_memfs_rmdir(const char* path)
{
    char key[TIG_MAX_PATH];
    int index;

    if (!memfs_key(path, key, sizeof(key)) || has_children(key)) {
        return -1;
    }

    if (key[0] == '\0') {
        return 0;
    }

    index = node_find(key);
    if (index == -1) {
        return 0;
    }

    node_remove(index);
    return 0;
}

int tig_memfile_close(TigMemFile* file)
{
    data_release(file->data);
    FREE(file);
    return 0;
}

// The position a read or write starts from, with an `ungetc` byte undone.
static size_t logical_pos(const TigMemFile* file)
{
    return file->pushback != EOF && file->pos > 0 ? file->pos - 1 : file->pos;
}

size_t tig_memfile_read(void* buffer, size_t size, size_t count, TigMemFile* file)
{
    unsigned char* out = (unsigned char*)buffer;
    size_t total;
    size_t done = 0;
    size_t available;

    if (size == 0 || count == 0) {
        return 0;
    }
    if (!file->readable) {
        file->error = true;
        return 0;
    }
    if (count > SIZE_MAX / size) {
        return 0;
    }
    total = size * count;

    if (file->pushback != EOF) {
        out[done++] = (unsigned char)file->pushback;
        file->pushback = EOF;
    }

    available = file->pos < file->data->size ? file->data->size - file->pos : 0;
    if (available > total - done) {
        available = total - done;
    }
    if (available != 0) {
        memcpy(out + done, file->data->bytes + file->pos, available);
    }
    file->pos += available;
    done += available;

    if (done < total) {
        file->eof = true;
    }

    return done / size;
}

size_t tig_memfile_write(const void* buffer, size_t size, size_t count, TigMemFile* file)
{
    TigMemData* data = file->data;
    size_t total;
    size_t end;

    if (size == 0 || count == 0) {
        return 0;
    }
    if (!file->writable || count > SIZE_MAX / size) {
        file->error = true;
        return 0;
    }
    total = size * count;

    if (file->pushback != EOF) {
        file->pos = logical_pos(file);
        file->pushback = EOF;
    }
    if (file->append) {
        file->pos = data->size;
    }

    end = file->pos + total;
    if (end > data->capacity) {
        size_t capacity = data->capacity != 0 ? data->capacity : 4096;

        while (capacity < end) {
            capacity *= 2;
        }
        data->bytes = (unsigned char*)REALLOC(data->bytes, capacity);
        data->capacity = capacity;
    }
    if (file->pos > data->size) {
        memset(data->bytes + data->size, 0, file->pos - data->size);
    }
    memcpy(data->bytes + file->pos, buffer, total);
    file->pos = end;
    if (end > data->size) {
        data->size = end;
    }

    return count;
}

int tig_memfile_seek(TigMemFile* file, long offset, int origin)
{
    long base;

    switch (origin) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (long)logical_pos(file);
        break;
    case SEEK_END:
        base = (long)file->data->size;
        break;
    default:
        return -1;
    }

    if (base + offset < 0) {
        return -1;
    }

    file->pos = (size_t)(base + offset);
    file->pushback = EOF;
    file->eof = false;
    return 0;
}

long tig_memfile_tell(TigMemFile* file)
{
    return (long)logical_pos(file);
}

long tig_memfile_length(TigMemFile* file)
{
    return (long)file->data->size;
}

int tig_memfile_getc(TigMemFile* file)
{
    unsigned char ch;

    return tig_memfile_read(&ch, 1, 1, file) == 1 ? ch : EOF;
}

// One byte of pushback, as the C library guarantees.
int tig_memfile_ungetc(int ch, TigMemFile* file)
{
    if (ch == EOF || file->pushback != EOF) {
        return EOF;
    }

    file->pushback = (unsigned char)ch;
    file->eof = false;
    return (unsigned char)ch;
}

char* tig_memfile_gets(char* buffer, int max_count, TigMemFile* file)
{
    int length = 0;
    int ch;

    if (max_count <= 0) {
        return NULL;
    }

    while (length < max_count - 1) {
        ch = tig_memfile_getc(file);
        if (ch == EOF) {
            break;
        }
        buffer[length++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (length == 0) {
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

int tig_memfile_putc(int ch, TigMemFile* file)
{
    unsigned char byte = (unsigned char)ch;

    return tig_memfile_write(&byte, 1, 1, file) == 1 ? byte : EOF;
}

int tig_memfile_puts(const char* str, TigMemFile* file)
{
    size_t length = strlen(str);

    if (length == 0) {
        return file->writable ? 0 : EOF;
    }

    return tig_memfile_write(str, length, 1, file) == 1 ? 0 : EOF;
}

int tig_memfile_vprintf(TigMemFile* file, const char* format, va_list args)
{
    char* text = NULL;
    int length;

    length = SDL_vasprintf(&text, format, args);
    if (length < 0) {
        file->error = true;
        return -1;
    }

    if (length != 0 && tig_memfile_write(text, (size_t)length, 1, file) != 1) {
        length = -1;
    }
    SDL_free(text);

    return length;
}

int tig_memfile_eof(TigMemFile* file)
{
    return file->eof ? 1 : 0;
}

int tig_memfile_error(TigMemFile* file)
{
    return file->error ? 1 : 0;
}

void tig_memfile_clearerr(TigMemFile* file)
{
    file->eof = false;
    file->error = false;
}

void tig_memfile_rewind(TigMemFile* file)
{
    tig_memfile_seek(file, 0, SEEK_SET);
    tig_memfile_clearerr(file);
}
