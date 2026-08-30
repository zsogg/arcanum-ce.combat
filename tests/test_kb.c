// Tests for `tig/kb.c` - the keyboard shim.
//
// 53 lines. Two of its four functions are one-line SDL calls; the interest is in
// the init guard and in `tig_kb_set_key`, which is the door every keystroke in
// the game comes through. Written 2026-08-29.
//
// **`tig_kb_is_key_pressed` is barely tested here on purpose.** It reads
// `SDL_GetKeyboardState`, which this suite cannot set without a window and an
// event pump - so what it can honestly check is the contract at the boundary
// (a valid scancode answers, and the index is unchecked), not that a given key
// reads pressed. The unchecked index is faithful and is written up at the
// function; the case below pins that a nearby valid scancode is safe, which is
// what makes the NOTE's warning meaningful rather than abstract.

#include <string.h>

#include "tig/core.h"
#include "tig/kb.h"
#include "tig/message.h"

#include "test_harness.h"

#define KB_DRAIN_LIMIT 64

static int kb_drain(void)
{
    TigMessage drain;
    int count = 0;

    // Bounded, for the reason `test_message.c` records: a dequeue that reported
    // success on empty would turn this into a hang rather than a failure.
    while (count < KB_DRAIN_LIMIT && tig_message_dequeue(&drain) == TIG_OK) {
        count++;
    }

    return count;
}

static void kb_begin(void)
{
    tig_message_init(NULL);
    kb_drain();
}

// `tig_kb_init` refuses a second call, so a case that leaves it initialised
// changes what the next one sees.
//
// `tig_message_exit` matters as much: `tig_message_init` reassigns the queue and
// free-list heads WITHOUT freeing what they held, so a case that inits twice
// without an exit between orphans a batch of nodes. That is faithful - the
// original zeroes both heads the same way at 0x52B6E1/0x52B6E7 - and it is why
// ASan reported this suite leaking 6,600 bytes before the exit was added.
static void kb_end(void)
{
    kb_drain();
    tig_message_exit();
    tig_kb_exit();
}

TEST(init_reports_already_initialized_on_a_second_call)
{
    tig_kb_exit();

    CHECK_INT(tig_kb_init(NULL), TIG_OK);
    CHECK_INT(tig_kb_init(NULL), TIG_ERR_ALREADY_INITIALIZED);

    tig_kb_exit();
    // And after exit it can be brought up again.
    CHECK_INT(tig_kb_init(NULL), TIG_OK);
    tig_kb_exit();
}

// `tig_kb_exit` is guarded on the flag, so calling it twice - or without a
// matching init - is harmless.
TEST(exit_is_safe_without_a_matching_init)
{
    tig_kb_exit();
    tig_kb_exit();

    CHECK_INT(tig_kb_init(NULL), TIG_OK);
    tig_kb_exit();
}

// The door every keystroke comes through: it does not record state, it enqueues
// a message. Nothing in the module remembers the key.
TEST(set_key_enqueues_a_keyboard_message)
{
    TigMessage message;

    kb_begin();

    tig_kb_set_key(SDLK_A, SDL_SCANCODE_A, true);

    CHECK_INT(tig_message_dequeue(&message), TIG_OK);
    CHECK_INT(message.type, TIG_MESSAGE_KEYBOARD);
    CHECK_INT(message.data.keyboard.key, SDLK_A);
    CHECK_INT(message.data.keyboard.scancode, SDL_SCANCODE_A);
    CHECK_INT(message.data.keyboard.pressed, 1);

    kb_end();
}

TEST(a_key_release_is_the_same_message_with_pressed_false)
{
    TigMessage message;

    kb_begin();

    tig_kb_set_key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, false);

    CHECK_INT(tig_message_dequeue(&message), TIG_OK);
    CHECK_INT(message.type, TIG_MESSAGE_KEYBOARD);
    CHECK_INT(message.data.keyboard.scancode, SDL_SCANCODE_ESCAPE);
    CHECK_INT(message.data.keyboard.pressed, 0);

    kb_end();
}

// The message carries `tig_ping_timestamp` - the time the frame read, not the
// time the key arrived. So every key event in one frame shares a timestamp, and
// a consumer cannot order two of them by it.
TEST(the_message_carries_the_frames_timestamp)
{
    TigMessage first;
    TigMessage second;

    kb_begin();

    tig_ping_timestamp = 4321;
    tig_kb_set_key(SDLK_A, SDL_SCANCODE_A, true);
    tig_kb_set_key(SDLK_B, SDL_SCANCODE_B, true);

    CHECK_INT(tig_message_dequeue(&first), TIG_OK);
    CHECK_INT(tig_message_dequeue(&second), TIG_OK);
    CHECK_INT(first.timestamp, 4321);
    CHECK_INT(second.timestamp, 4321);
    // Same frame, same stamp - they are distinguishable only by order.
    CHECK_INT(first.data.keyboard.scancode, SDL_SCANCODE_A);
    CHECK_INT(second.data.keyboard.scancode, SDL_SCANCODE_B);

    kb_end();
}

// Keys queue in order, like any other message - `tig_kb_set_key` goes through
// `tig_message_enqueue` and gets no special treatment.
TEST(keys_queue_in_the_order_they_arrive)
{
    TigMessage message;
    int index;
    int wrong = 0;

    kb_begin();

    for (index = 0; index < 5; index++) {
        tig_kb_set_key(SDLK_A + index, (SDL_Scancode)(SDL_SCANCODE_A + index), true);
    }

    for (index = 0; index < 5; index++) {
        CHECK_INT(tig_message_dequeue(&message), TIG_OK);
        if (message.data.keyboard.scancode != (SDL_Scancode)(SDL_SCANCODE_A + index)) {
            wrong++;
        }
    }
    CHECK_INT(wrong, 0);

    kb_end();
}

// `set_key` does not require the subsystem to be up. It only enqueues, and the
// init flag guards nothing but init itself - worth knowing before anyone adds a
// guard that would silently drop keys.
TEST(set_key_works_without_init)
{
    TigMessage message;

    kb_begin();
    tig_kb_exit();

    tig_kb_set_key(SDLK_Z, SDL_SCANCODE_Z, true);
    CHECK_INT(tig_message_dequeue(&message), TIG_OK);
    CHECK_INT(message.data.keyboard.scancode, SDL_SCANCODE_Z);

    kb_end();
}

// The boundary the NOTE at kb.c warns about. A scancode inside the state array
// answers without complaint; one past the end would read out of bounds in both
// engines, which is why this case stays inside and the warning lives in a
// comment rather than in a test that would corrupt memory.
TEST(a_valid_scancode_can_be_queried_safely)
{
    bool pressed;

    pressed = tig_kb_is_key_pressed(SDL_SCANCODE_A);
    CHECK(pressed == true || pressed == false);

    pressed = tig_kb_is_key_pressed((SDL_Scancode)(SDL_SCANCODE_COUNT - 1));
    CHECK(pressed == true || pressed == false);
}

// `tig_kb_get_modifier` masks whatever SDL reports. With no window and no
// events there are no modifiers, so a query for none must be false.
TEST(no_modifier_is_reported_with_no_input)
{
    CHECK_INT(tig_kb_get_modifier(SDL_KMOD_NONE), 0);
}

void test_kb_suite(void)
{
    RUN(init_reports_already_initialized_on_a_second_call);
    RUN(exit_is_safe_without_a_matching_init);
    RUN(set_key_enqueues_a_keyboard_message);
    RUN(a_key_release_is_the_same_message_with_pressed_false);
    RUN(the_message_carries_the_frames_timestamp);
    RUN(keys_queue_in_the_order_they_arrive);
    RUN(set_key_works_without_init);
    RUN(a_valid_scancode_can_be_queried_safely);
    RUN(no_modifier_is_reported_with_no_input);
}
