#include "game/ce_mods.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/tig.h>

#include "game/feature.h"
#include "game/gamelib.h"
#include "game/settings.h"

// The bundles found and enabled at start, in the order found.
static char ce_mods_enabled[CE_MODS_MAX][CE_MODS_ID_MAX];
static int ce_mods_enabled_count;

// Which enabled bundles' `module/` folders are mounted now.
static bool ce_mods_module_mounted[CE_MODS_MAX];

bool ce_mods_id_valid(const char* id)
{
    size_t length;
    size_t index;

    if (id == NULL) {
        return false;
    }

    length = strlen(id);
    if (length == 0 || length >= CE_MODS_ID_MAX) {
        return false;
    }

    for (index = 0; index < length; index++) {
        if (!((id[index] >= 'a' && id[index] <= 'z')
                || (id[index] >= '0' && id[index] <= '9')
                || id[index] == '-')) {
            return false;
        }
    }

    return true;
}

static char* ce_mods_trim(char* text)
{
    char* end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return text;
}

bool ce_mods_parse_line(const char* line, char* key, size_t key_size, int* value)
{
    char buffer[256];
    char* text;
    char* equals;
    char* key_text;
    char* value_text;
    char* end;
    long parsed;

    if (line == NULL || strlen(line) >= sizeof(buffer)) {
        return false;
    }
    strcpy(buffer, line);
    text = ce_mods_trim(buffer);
    if (*text == '\0' || *text == '#' || *text == ';') {
        return false;
    }

    equals = strchr(text, '=');
    if (equals == NULL) {
        return false;
    }
    *equals = '\0';
    key_text = ce_mods_trim(text);
    value_text = ce_mods_trim(equals + 1);
    if (*key_text == '\0' || *value_text == '\0' || strlen(key_text) >= key_size) {
        return false;
    }

    parsed = strtol(value_text, &end, 10);
    if (*end != '\0') {
        return false;
    }

    strcpy(key, key_text);
    *value = (int)parsed;
    return true;
}

// The switches an enabled bundle sets. An unknown key is reported and skipped:
// a bundle built for a later engine must not stop this one from starting.
static void ce_mods_apply(const char* id)
{
    char path[TIG_MAX_PATH];
    char line[256];
    char key[128];
    int value;
    int applied = 0;
    TigFile* stream;
    FeatureId feature;

    snprintf(path, sizeof(path), "%s\\%s\\features.cfg", CE_MODS_DIR, id);
    stream = tig_file_fopen(path, "rt");
    if (stream == NULL) {
        tig_debug_printf("ce mods: %s has no features.cfg; no switches set\n", id);
        return;
    }

    while (tig_file_fgets(line, sizeof(line), stream) != NULL) {
        if (!ce_mods_parse_line(line, key, sizeof(key), &value)) {
            continue;
        }
        if (!feature_find(key, &feature)) {
            tig_debug_printf("ce mods: %s sets \"%s\", which this engine does not know; skipped\n", id, key);
            continue;
        }
        feature_override(feature, value != 0);
        applied++;
    }

    tig_file_fclose(stream);
    tig_debug_printf("ce mods: %s on - %d switch(es) set for this session\n", id, applied);
}

void ce_mods_init(void)
{
    TigFileList list;
    unsigned int index;
    char setting[CE_MODS_ID_MAX + 8];
    const char* id;

    ce_mods_enabled_count = 0;

    tig_file_list_create(&list, CE_MODS_DIR "\\*.*");
    for (index = 0; index < list.count; index++) {
        id = list.entries[index].path;
        if ((list.entries[index].attributes & TIG_FILE_ATTRIBUTE_SUBDIR) == 0
            || id[0] == '.') {
            continue;
        }
        if (!ce_mods_id_valid(id)) {
            tig_debug_printf("ce mods: \"%s\" is not a bundle id (a-z, 0-9, -); ignored\n", id);
            continue;
        }

        snprintf(setting, sizeof(setting), "mod %s", id);
        settings_register(&settings, setting, "0", NULL);
        if (settings_get_value(&settings, setting) == 0) {
            tig_debug_printf("ce mods: %s off\n", id);
            continue;
        }
        if (ce_mods_enabled_count == CE_MODS_MAX) {
            tig_debug_printf("ce mods: more than %d bundles on; %s ignored\n", CE_MODS_MAX, id);
            continue;
        }

        ce_mods_apply(id);
        strcpy(ce_mods_enabled[ce_mods_enabled_count++], id);
    }
    tig_file_list_destroy(&list);
}

void ce_mods_mount(void)
{
    char path[TIG_MAX_PATH];
    int index;

    for (index = 0; index < ce_mods_enabled_count; index++) {
        snprintf(path, sizeof(path), "%s\\%s", CE_MODS_DIR, ce_mods_enabled[index]);
        if (tig_file_repository_add(path)) {
            tig_debug_printf("ce mods: %s mounted\n", path);
        }
    }
}

static void ce_mods_module_path(int index, char* path, size_t size)
{
    snprintf(path, size, "%s\\%s\\module", CE_MODS_DIR, ce_mods_enabled[index]);
}

void ce_mods_mount_module(void)
{
    char path[TIG_MAX_PATH];
    int index;

    for (index = 0; index < ce_mods_enabled_count; index++) {
        ce_mods_module_path(index, path, sizeof(path));
        if (!ce_mods_module_mounted[index]
            && tig_file_is_directory(path)
            && tig_file_repository_add(path)) {
            ce_mods_module_mounted[index] = true;
            tig_debug_printf("ce mods: %s mounted above the module's archives\n", path);
        }
    }
}

void ce_mods_unmount_module(void)
{
    char path[TIG_MAX_PATH];
    int index;

    for (index = ce_mods_enabled_count - 1; index >= 0; index--) {
        if (ce_mods_module_mounted[index]) {
            ce_mods_module_path(index, path, sizeof(path));
            tig_file_repository_remove(path);
            ce_mods_module_mounted[index] = false;
        }
    }
}
