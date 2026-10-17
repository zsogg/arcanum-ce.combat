#include "tig/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tig/memory.h"

// CE (release 1, R9): the log file. Until the game decides whether to write
// one, lines are kept here (up to a cap), so a log opened after settings are
// read still starts at the first line.
#define TIG_DEBUG_EARLY_CAP (256 * 1024)

static FILE* tig_debug_log_stream;
static char* tig_debug_early;
static size_t tig_debug_early_len;
static bool tig_debug_early_done;

static void tig_debug_sink(const char* line)
{
    size_t len;
    char* grown;

    if (tig_debug_log_stream != NULL) {
        fputs(line, tig_debug_log_stream);
        fflush(tig_debug_log_stream); // a crash keeps every line written
        return;
    }
    if (tig_debug_early_done) {
        return;
    }
    len = strlen(line);
    if (tig_debug_early_len + len + 1 > TIG_DEBUG_EARLY_CAP) {
        return;
    }
    grown = (char*)realloc(tig_debug_early, tig_debug_early_len + len + 1);
    if (grown == NULL) {
        return;
    }
    tig_debug_early = grown;
    memcpy(tig_debug_early + tig_debug_early_len, line, len + 1);
    tig_debug_early_len += len;
}

bool tig_debug_log_file_open(const char* path)
{
    FILE* stream;

    stream = fopen(path, "w");
    if (stream != NULL && tig_debug_early != NULL) {
        fwrite(tig_debug_early, 1, tig_debug_early_len, stream);
        fflush(stream);
    }
    tig_debug_log_file_discard_early();
    tig_debug_log_stream = stream;
    return stream != NULL;
}

void tig_debug_log_file_discard_early(void)
{
    free(tig_debug_early);
    tig_debug_early = NULL;
    tig_debug_early_len = 0;
    tig_debug_early_done = true;
}

// 0x4FEB10
int tig_debug_init(TigInitInfo* init_info)
{
    time_t now;

    (void)init_info;

    // CE: a session keeps its lines until the log file is decided (R9).
    tig_debug_early_done = false;

    tig_memory_set_output_func(tig_debug_println);
    tig_debug_println("\n");

    time(&now);
    tig_debug_println(asctime(localtime(&now)));

    return 0;
}

// 0x4FEB60
void tig_debug_exit(void)
{
    // CE: close the log file and drop anything still kept (release 1, R9).
    if (tig_debug_log_stream != NULL) {
        fclose(tig_debug_log_stream);
        tig_debug_log_stream = NULL;
    }
    tig_debug_log_file_discard_early();
}

// 0x4FEB70
void tig_debug_printf(const char* format, ...)
{
    // 0x604214
    static char buffer[1024];

    va_list args;
    size_t len;
    char tmp[1024];

    va_start(args, format);
    SDL_vsnprintf(tmp, sizeof(tmp), format, args);

    // NOTE: Typically, debug messages in Arcanum conclude with a newline.
    // However, in certain situations where the developers intended to display
    // progress-like messages on a single line, multiple consecutive messages
    // are utilized, such as "loading..." followed by "done, took: %d ms\n". SDL
    // does not permit such prints - it always appends a newline at the end of
    // log messages (for thread-safety reasons), which reduces the readability
    // of the logs. To support the previous approach, messages that do not end
    // with a newline are buffered.
    // FIX: measure what is actually IN the buffer, not what the concatenation
    // wanted to be. `SDL_strlcat` returns "the length of the string in `dst`
    // plus the length of `src`" - the would-be length - so once the two exceed
    // the buffer it reports a number larger than the buffer, and the old code
    // both mis-tested and mis-indexed on it:
    //
    //   - `len == sizeof(buffer) - 1` was meant to be "the buffer is full", but
    //     it only fires on an exact 1023, so a buffer that overshot never
    //     flushed. Every later call then appended nothing (`strlcat` has no room)
    //     and returned an even larger len, so the log stayed wedged for good.
    //   - `buffer[len - 1]` then indexed with `len - 1 >= 1024`, reading past a
    //     1024-byte static. Reachable by accumulating more than 1024 characters
    //     of messages that do not end in a newline, which is exactly what the
    //     progress-message style below is for.
    //
    // CE-only, with nothing to be faithful to: the original has no buffering at
    // all. 0x4FEB70 gates on a flag at 0x604614, formats straight into its own
    // 0x400 buffer at 0x604214 with `_vsnprintf`, and dispatches to a table of
    // up to eight output callbacks (0x604618..0x604638). The newline
    // accumulation is CE's answer to `SDL_Log` always appending one.
    //
    // Found 2026-08-29 while writing this module's first suite (roadmap P5.6).
    SDL_strlcat(buffer, tmp, sizeof(buffer));
    len = SDL_strlen(buffer);
    if (len != 0 && (len == sizeof(buffer) - 1 || buffer[len - 1] == '\n')) {
        SDL_Log("%s", buffer);
        tig_debug_sink(buffer);
        buffer[0] = '\0';
    }

    va_end(args);
}

// 0x4FEBC0
void tig_debug_println(const char* string)
{
    tig_debug_printf("%s\n", string);
}
