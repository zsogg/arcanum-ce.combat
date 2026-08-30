#include "tig/debug.h"

#include <time.h>

#include "tig/memory.h"

// 0x4FEB10
int tig_debug_init(TigInitInfo* init_info)
{
    time_t now;

    (void)init_info;

    tig_memory_set_output_func(tig_debug_println);
    tig_debug_println("\n");

    time(&now);
    tig_debug_println(asctime(localtime(&now)));

    return 0;
}

// 0x4FEB60
void tig_debug_exit(void)
{
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
        buffer[0] = '\0';
    }

    va_end(args);
}

// 0x4FEBC0
void tig_debug_println(const char* string)
{
    tig_debug_printf("%s\n", string);
}
