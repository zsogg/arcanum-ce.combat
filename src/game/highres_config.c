#include "game/highres_config.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_stdinc.h>

#include "game/ce_parse.h"

static void highres_config_reset(void);
static void highres_config_parse_line(char* line);
static void highres_config_trim(char** start_ptr, char** end_ptr);

static HighResConfig highres_config;

void highres_config_load(void)
{
    FILE* stream;
    char line[512];

    highres_config_reset();

    stream = fopen("HighRes/config.ini", "rt");
    if (stream == NULL) {
        return;
    }

    highres_config.loaded = true;

    while (fgets(line, sizeof(line), stream) != NULL) {
        highres_config_parse_line(line);
    }

    fclose(stream);
}

/**
 * Sizes for each DialogFont setting, straight out of Dialog.tph: stock, then
 * the patch's size-14 and size-18 rows.
 */
static const HighResDialogMetrics highres_dialog_metrics_table[] = {
    { 229, 400, 100, 200 },
    { 230, 465, 115, 235 },
    { 133, 600, 150, 300 },
};

const HighResDialogMetrics* highres_dialog_metrics(void)
{
    return &highres_dialog_metrics_table[highres_config.dialog_font];
}

const HighResConfig* highres_config_get(void)
{
    return &highres_config;
}

void highres_config_reset(void)
{
    highres_config.loaded = false;
    highres_config.width = 800;
    highres_config.height = 600;
    highres_config.windowed = false;
    highres_config.show_fps = false;
    // FIX (porting bug #325): without `-scrollfps:` the original scrolls at
    // 60 (0x401392 pushes 0x3C to scroll_fps_set); CE's default was 35, so an
    // install without HighRes/config.ini scrolled more slowly.
    highres_config.scroll_fps = 60;
    highres_config.scroll_dist = 10;
    highres_config.preload_limit = 30;
    highres_config.broadcast_limit = 10;
    highres_config.menu_position = HIGHRES_MENU_POSITION_CENTER;
    highres_config.options_compact = false;
    highres_config.sprite_mirroring = true;
    highres_config.logbook_font = 0;
    highres_config.dialog_font = 0;
    highres_config.logos = true;
    highres_config.intro = true;
}

void highres_config_parse_line(char* line)
{
    char* comment;
    char* sep;
    char* key_start;
    char* key_end;
    char* value_start;
    char* value_end;
    int value;

    comment = strstr(line, "//");
    if (comment != NULL) {
        *comment = '\0';
    }

    key_start = line;
    key_end = key_start + strlen(key_start);
    highres_config_trim(&key_start, &key_end);
    *key_end = '\0';

    if (*key_start == '\0') {
        return;
    }

    sep = strchr(key_start, '=');
    if (sep == NULL) {
        return;
    }

    *sep = '\0';

    key_end = sep;
    highres_config_trim(&key_start, &key_end);
    *key_end = '\0';

    value_start = sep + 1;
    value_end = value_start + strlen(value_start);
    highres_config_trim(&value_start, &value_end);
    *value_end = '\0';

    if (!ce_parse_int(value_start, &value)) {
        return;
    }

    if (SDL_strcasecmp(key_start, "Width") == 0) {
        if (value >= 800) {
            highres_config.width = value;
        }
    } else if (SDL_strcasecmp(key_start, "Height") == 0) {
        if (value >= 600) {
            highres_config.height = value;
        }
    } else if (SDL_strcasecmp(key_start, "Windowed") == 0) {
        highres_config.windowed = value != 0;
    } else if (SDL_strcasecmp(key_start, "ShowFPS") == 0) {
        highres_config.show_fps = value != 0;
    } else if (SDL_strcasecmp(key_start, "ScrollFPS") == 0) {
        if (value > 0) {
            highres_config.scroll_fps = value;
        }
    } else if (SDL_strcasecmp(key_start, "ScrollDist") == 0) {
        if (value >= 0) {
            highres_config.scroll_dist = value;
        }
    } else if (SDL_strcasecmp(key_start, "PreloadLimit") == 0) {
        // The high resolution patch pokes both of these into the exe with
        // WRITE_BYTE, so its own documented ceiling is 255. Out-of-range values
        // are ignored here rather than wrapped, which is the one place this
        // deliberately differs from the patch.
        if (value >= 1 && value <= 255) {
            highres_config.preload_limit = value;
        }
    } else if (SDL_strcasecmp(key_start, "BroadcastLimit") == 0) {
        if (value >= 0 && value <= 255) {
            highres_config.broadcast_limit = value;
        }
    } else if (SDL_strcasecmp(key_start, "MenuPosition") == 0) {
        if (value >= HIGHRES_MENU_POSITION_TOP && value <= HIGHRES_MENU_POSITION_BOTTOM) {
            highres_config.menu_position = value;
        }
    } else if (SDL_strcasecmp(key_start, "Options") == 0) {
        highres_config.options_compact = value != 0;
    } else if (SDL_strcasecmp(key_start, "SpriteMirroring") == 0) {
        highres_config.sprite_mirroring = value != 0;
    } else if (SDL_strcasecmp(key_start, "LogbookFont") == 0) {
        if (value >= 0 && value <= 1) {
            highres_config.logbook_font = value;
        }
    } else if (SDL_strcasecmp(key_start, "DialogFont") == 0) {
        if (value >= 0 && value <= 2) {
            highres_config.dialog_font = value;
        }
    } else if (SDL_strcasecmp(key_start, "Logos") == 0) {
        highres_config.logos = value != 0;
    } else if (SDL_strcasecmp(key_start, "Intro") == 0) {
        highres_config.intro = value != 0;
    }
}

void highres_config_trim(char** start_ptr, char** end_ptr)
{
    while (*start_ptr < *end_ptr && isspace((unsigned char)**start_ptr)) {
        (*start_ptr)++;
    }

    while (*end_ptr > *start_ptr && isspace((unsigned char)*((*end_ptr) - 1))) {
        (*end_ptr)--;
    }
}
