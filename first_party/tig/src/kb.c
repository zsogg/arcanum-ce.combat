#include "tig/kb.h"

#include "tig/core.h"
#include "tig/message.h"

// 0x62B1A0
static bool tig_kb_initialized;

// 0x52B2F0
int tig_kb_init(TigInitInfo* init_info)
{
    (void)init_info;

    if (tig_kb_initialized) {
        return TIG_ERR_ALREADY_INITIALIZED;
    }

    tig_kb_initialized = true;

    return TIG_OK;
}

// 0x52B320
void tig_kb_exit(void)
{
    if (tig_kb_initialized) {
        tig_kb_initialized = false;
    }
}

// NOTE: The scancode is not bounds-checked, and that is FAITHFUL. The original
// at 0x52B340 is three instructions - `mov eax,[esp+4]`,
// `mov al, BYTE PTR [eax+0x62B09C]`, `and eax,0x80` - indexing its 256-byte
// DirectInput key-state buffer (0x62B09C..0x62B19C, with `tig_kb_initialized`
// immediately after at 0x62B1A0) with whatever it is handed, and testing the
// high bit DirectInput sets for "down".
//
// It matters because `tig_message_set_key_handler` accepts any non-negative
// `int` as a key and `tig_message_ping` feeds it here on every frame. Register a
// handler with a scancode past the end of the state array and both engines read
// out of bounds, forever, silently. CE is the more tolerant of the two - SDL's
// array is `SDL_SCANCODE_COUNT` entries against the original's 256 - but neither
// refuses. The only caller in the tree passes `SDL_SCANCODE_F12` (main.c:326).
// Established 2026-08-29 when ASan caught a test registering key 700.
//
// 0x52B340
bool tig_kb_is_key_pressed(SDL_Scancode scancode)
{
    return SDL_GetKeyboardState(NULL)[scancode];
}

// 0x52B350
bool tig_kb_get_modifier(SDL_Keymod keymod)
{
    return (SDL_GetModState() & keymod) != 0;
}

void tig_kb_set_key(SDL_Keycode key, SDL_Scancode scancode, bool down)
{
    TigMessage message;

    message.type = TIG_MESSAGE_KEYBOARD;
    message.timestamp = tig_ping_timestamp;
    message.data.keyboard.key = key;
    message.data.keyboard.scancode = scancode;
    message.data.keyboard.pressed = down;
    tig_message_enqueue(&message);
}
