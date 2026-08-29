// Tests for `tig/message.c` - the queue every input event in the game passes
// through, and the key-handler table beside it.
//
// 320 lines, no suite of its own. Written 2026-08-29.
//
// **Reachability, checked before writing anything.** Unlike `memory.c`, this
// module is live on every frame: `tig_message_ping` is called from
// `tig_ping`, and `tig_message_dequeue` is what the game loop reads. Every
// function carries an original address, so `Arcanum.exe` is the oracle.
//
// **What is testable headless, and why.** `tig_message_ping`'s SDL half needs a
// renderer and returns early without one (`tig_video_renderer_get`), but its
// key-handler dispatch runs *before* that check - so the dispatch is reachable
// here and the event pump is not. `tig_message_dequeue` calls out to
// `tig_button_process_mouse_msg` and `tig_window_filter_message`; with no
// windows and no buttons created, the window filter's loop runs zero times and
// the button path declines anything that is not a left button or a move. So the
// queue can be driven end to end with no display, provided the cases stay off
// mouse messages except where they mean to exercise that boundary.
//
// **The ordering rule is the thing to pin.** `tig_message_enqueue` is FIFO
// except for `TIG_MESSAGE_REDRAW`, which goes to the *head*. That is not a
// detail: it is why a redraw is serviced before the input backlog rather than
// after it.

#include <string.h>

#include "tig/message.h"
#include "tig/timer.h"

#include "test_harness.h"

// ---------------------------------------------------------------------------

// Every drain in this file is BOUNDED, and that is not defensive style.
// `tig_message_dequeue` returning success on an empty queue turns
// `while (tig_message_dequeue(&m) == TIG_OK)` into an infinite loop - and that
// idiom is the engine's own, at `movie.c:124` and `:143`. Measured 2026-08-29:
// with unbounded drains the mutation that makes dequeue report success on empty
// hung this binary for the full 900s timeout instead of failing a check, which
// is the `find_file` lesson recorded in `tests_main.c` arriving in a new module.
// The engine would hang the same way; that is a property of the callers, not
// something these tests can fix. A bounded drain turns it into a named failure.
#define MSG_DRAIN_LIMIT 256

static int msg_drain_all(void)
{
    TigMessage drain;
    int count = 0;

    while (count < MSG_DRAIN_LIMIT && tig_message_dequeue(&drain) == TIG_OK) {
        count++;
    }

    return count;
}

static void msg_begin(void)
{
    tig_message_init(NULL);

    // `init` does not empty the queue - it only reassigns the head - so a case
    // that left something behind would hand it to the next one.
    msg_drain_all();
}

static void msg_end(void)
{
    msg_drain_all();
    tig_message_exit();
}

static void msg_put(TigMessageType type, int payload)
{
    TigMessage message;

    memset(&message, 0, sizeof(message));
    message.type = type;
    // `quit.exit_code` is the only int payload shared by every type here, so
    // it doubles as the tag that says which message came back.
    message.data.quit.exit_code = payload;
    tig_message_enqueue(&message);
}

// Drain the queue into a comma-separated list of payloads, so an ordering
// assertion reads as one string rather than a sequence of dequeues.
static void msg_drain(char* out, size_t size)
{
    TigMessage message;
    char item[32];
    int guard = 0;

    out[0] = '\0';
    while (guard < MSG_DRAIN_LIMIT && tig_message_dequeue(&message) == TIG_OK) {
        guard++;
        snprintf(item, sizeof(item), "%s%d",
            out[0] == '\0' ? "" : ",", message.data.quit.exit_code);
        strncat(out, item, size - strlen(out) - 1);
    }
}

// ---------------------------------------------------------------------------
// The queue.

// First in the suite, and deliberately so: it is the one check that does not go
// through a drain loop, so if `tig_message_dequeue` ever reports success on an
// empty queue this names it before any other case can spin on it.
TEST(an_empty_queue_reports_empty_rather_than_succeeding)
{
    TigMessage message;

    tig_message_init(NULL);
    CHECK_INT(tig_message_dequeue(&message), TIG_ERR_MESSAGE_QUEUE_EMPTY);
    msg_end();
}

// And the drain itself terminates. A queue of three must hand back exactly
// three, not the bound.
TEST(a_drain_stops_when_the_queue_is_empty)
{
    msg_begin();

    msg_put(TIG_MESSAGE_QUIT, 1);
    msg_put(TIG_MESSAGE_QUIT, 2);
    msg_put(TIG_MESSAGE_QUIT, 3);

    CHECK_INT(msg_drain_all(), 3);
    CHECK_INT(msg_drain_all(), 0);

    msg_end();
}

// A failed dequeue must not write through its out-parameter: the caller's
// message keeps whatever it held. `tig_message_dequeue` returns before touching
// it when the head is NULL.
TEST(a_failed_dequeue_leaves_the_callers_message_alone)
{
    TigMessage message;

    msg_begin();

    memset(&message, 0, sizeof(message));
    message.type = TIG_MESSAGE_TYPE_7;
    message.data.quit.exit_code = 4242;

    CHECK_INT(tig_message_dequeue(&message), TIG_ERR_MESSAGE_QUEUE_EMPTY);
    CHECK_INT(message.type, TIG_MESSAGE_TYPE_7);
    CHECK_INT(message.data.quit.exit_code, 4242);

    msg_end();
}

TEST(ordinary_messages_come_back_in_the_order_they_went_in)
{
    char order[128];

    msg_begin();

    msg_put(TIG_MESSAGE_QUIT, 1);
    msg_put(TIG_MESSAGE_QUIT, 2);
    msg_put(TIG_MESSAGE_QUIT, 3);
    msg_put(TIG_MESSAGE_QUIT, 4);

    msg_drain(order, sizeof(order));
    CHECK_STR(order, "1,2,3,4");

    msg_end();
}

// **The rule worth knowing.** A redraw jumps the queue - it is pushed at the
// head, not appended - so it is serviced before an input backlog rather than
// behind it.
TEST(a_redraw_jumps_to_the_head_of_the_queue)
{
    char order[128];

    msg_begin();

    msg_put(TIG_MESSAGE_QUIT, 1);
    msg_put(TIG_MESSAGE_QUIT, 2);
    msg_put(TIG_MESSAGE_REDRAW, 99);
    msg_put(TIG_MESSAGE_QUIT, 3);

    msg_drain(order, sizeof(order));
    CHECK_STR(order, "99,1,2,3");

    msg_end();
}

// Two redraws stack in reverse: each is pushed onto the head, so the later one
// is serviced first. Worth pinning because "redraw is prioritised" would be
// satisfied by either order and only one of them is what the code does.
TEST(the_later_of_two_redraws_is_serviced_first)
{
    char order[128];

    msg_begin();

    msg_put(TIG_MESSAGE_QUIT, 1);
    msg_put(TIG_MESSAGE_REDRAW, 10);
    msg_put(TIG_MESSAGE_REDRAW, 20);

    msg_drain(order, sizeof(order));
    CHECK_STR(order, "20,10,1");

    msg_end();
}

// The head-insert branch is taken for a redraw OR for an empty queue, and the
// two must not be confused: an ordinary message into an empty queue is still
// the head, and the next ordinary message goes behind it.
TEST(an_ordinary_message_into_an_empty_queue_is_still_first)
{
    char order[128];

    msg_begin();

    msg_put(TIG_MESSAGE_QUIT, 7);
    msg_put(TIG_MESSAGE_QUIT, 8);

    msg_drain(order, sizeof(order));
    CHECK_STR(order, "7,8");

    msg_end();
}

TEST(the_message_body_survives_the_round_trip)
{
    TigMessage sent;
    TigMessage got;

    msg_begin();

    memset(&sent, 0, sizeof(sent));
    sent.type = TIG_MESSAGE_KEYBOARD;
    sent.timestamp = 123456;
    sent.data.keyboard.key = 42;
    sent.data.keyboard.scancode = 7;
    sent.data.keyboard.pressed = true;
    tig_message_enqueue(&sent);

    CHECK_INT(tig_message_dequeue(&got), TIG_OK);
    CHECK_INT(got.type, TIG_MESSAGE_KEYBOARD);
    CHECK_INT(got.timestamp, 123456);
    CHECK_INT(got.data.keyboard.key, 42);
    CHECK_INT(got.data.keyboard.scancode, 7);
    CHECK_INT(got.data.keyboard.pressed, 1);

    msg_end();
}

TEST(post_quit_enqueues_a_quit_carrying_its_exit_code)
{
    TigMessage message;

    msg_begin();

    CHECK_INT(tig_message_post_quit(3), TIG_OK);
    CHECK_INT(tig_message_dequeue(&message), TIG_OK);
    CHECK_INT(message.type, TIG_MESSAGE_QUIT);
    CHECK_INT(message.data.quit.exit_code, 3);

    msg_end();
}

// The free list is refilled 32 nodes at a time, so crossing that boundary is
// where an off-by-one in `tig_message_node_reserve` would show. 40 in, 40 out,
// in order.
TEST(the_queue_survives_more_messages_than_one_node_batch)
{
    TigMessage message;
    int index;
    int seen = 0;
    int wrong = 0;

    msg_begin();

    for (index = 0; index < 40; index++) {
        msg_put(TIG_MESSAGE_QUIT, index);
    }

    while (seen < MSG_DRAIN_LIMIT && tig_message_dequeue(&message) == TIG_OK) {
        if (message.data.quit.exit_code != seen) {
            wrong++;
        }
        seen++;
    }

    CHECK_INT(seen, 40);
    CHECK_INT(wrong, 0);

    msg_end();
}

// Nodes are recycled rather than freed, so a queue that has been drained and
// refilled many times must still behave. This is also what makes the module's
// ASan cleanliness meaningful: `tig_message_exit` has to free both lists.
TEST(nodes_are_recycled_across_many_fill_and_drain_cycles)
{
    char order[128];
    int cycle;

    msg_begin();

    for (cycle = 0; cycle < 50; cycle++) {
        msg_put(TIG_MESSAGE_QUIT, 1);
        msg_put(TIG_MESSAGE_QUIT, 2);
        msg_drain(order, sizeof(order));
        if (strcmp(order, "1,2") != 0) {
            break;
        }
    }

    CHECK_INT(cycle, 50);
    CHECK_STR(order, "1,2");

    msg_end();
}

// ---------------------------------------------------------------------------
// The key-handler table.

static int handler_calls;
static int handler_last_key;

static int msg_handler(int key)
{
    handler_calls++;
    handler_last_key = key;
    return 0;
}

static int msg_handler_two(int key)
{
    (void)key;
    return 0;
}

TEST(a_negative_key_is_refused)
{
    msg_begin();
    CHECK_INT(tig_message_set_key_handler(msg_handler, -1), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_message_set_key_handler(NULL, -5), TIG_ERR_INVALID_PARAM);
    msg_end();
}

TEST(a_handler_can_be_added_and_removed)
{
    msg_begin();

    CHECK_INT(tig_message_set_key_handler(msg_handler, 100), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 100), TIG_OK);
    // Gone: removing it a second time must fail.
    CHECK_INT(tig_message_set_key_handler(NULL, 100), TIG_ERR_INVALID_PARAM);

    msg_end();
}

TEST(removing_a_key_that_was_never_registered_is_refused)
{
    msg_begin();

    CHECK_INT(tig_message_set_key_handler(msg_handler, 100), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 101), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_message_set_key_handler(NULL, 100), TIG_OK);

    msg_end();
}

// **The defect this suite was written around.** Removing from an EMPTY table
// used to search all MAX_KEY_HANDLERS slots and find slot 0, whose `key` is 0
// because the array is a zero-initialised static - so it reported success and
// decremented the count to -1. The original bounds the search by the count and
// returns early when it is zero (`test edi,edi; jle` at 0x52B940).
TEST(removing_from_an_empty_table_is_refused)
{
    msg_begin();

    CHECK_INT(tig_message_set_key_handler(NULL, 0), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_message_set_key_handler(NULL, 100), TIG_ERR_INVALID_PARAM);

    // The count must still be zero: if it went negative, the next add writes
    // out of bounds. Adding and removing cleanly is the observable proof.
    CHECK_INT(tig_message_set_key_handler(msg_handler, 100), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 100), TIG_OK);

    msg_end();
}

// The same defect's other half: removal does not clear the slot, so the entry
// stays in the array as a stale copy below the count. A search bounded by
// MAX_KEY_HANDLERS finds that ghost and decrements the count for a removal that
// did not happen.
TEST(a_removed_handler_does_not_linger_as_a_removable_ghost)
{
    msg_begin();

    CHECK_INT(tig_message_set_key_handler(msg_handler, 100), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(msg_handler_two, 101), TIG_OK);

    // Take the last one out; its slot still physically holds key 101.
    CHECK_INT(tig_message_set_key_handler(NULL, 101), TIG_OK);
    // Asking again must fail rather than find the ghost.
    CHECK_INT(tig_message_set_key_handler(NULL, 101), TIG_ERR_INVALID_PARAM);
    // And the surviving handler is untouched.
    CHECK_INT(tig_message_set_key_handler(NULL, 100), TIG_OK);

    msg_end();
}

// Key 0 is a legal key as far as this table is concerned - only negatives are
// refused - and it is the value a zeroed slot carries, which is what made the
// empty-table case above go wrong.
TEST(key_zero_is_an_ordinary_key)
{
    msg_begin();

    CHECK_INT(tig_message_set_key_handler(msg_handler, 0), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 0), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 0), TIG_ERR_INVALID_PARAM);

    msg_end();
}

TEST(the_table_holds_sixteen_and_refuses_the_seventeenth)
{
    int index;
    int refused = -1;

    msg_begin();

    for (index = 0; index < 16; index++) {
        if (tig_message_set_key_handler(msg_handler, 200 + index) != TIG_OK) {
            refused = index;
            break;
        }
    }
    CHECK_INT(refused, -1);

    CHECK_INT(tig_message_set_key_handler(msg_handler, 300), TIG_ERR_OUT_OF_HANDLES);

    for (index = 0; index < 16; index++) {
        CHECK_INT(tig_message_set_key_handler(NULL, 200 + index), TIG_OK);
    }

    msg_end();
}

// Removing from the middle shifts the entries above it down, so the survivors
// stay addressable. A shift that dropped one would leave a handler unremovable.
TEST(removing_from_the_middle_keeps_the_others_removable)
{
    int index;

    msg_begin();

    for (index = 0; index < 5; index++) {
        CHECK_INT(tig_message_set_key_handler(msg_handler, 400 + index), TIG_OK);
    }

    CHECK_INT(tig_message_set_key_handler(NULL, 402), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 402), TIG_ERR_INVALID_PARAM);

    // The four that remain must all still be found.
    CHECK_INT(tig_message_set_key_handler(NULL, 400), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 401), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 403), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(NULL, 404), TIG_OK);

    msg_end();
}

// After a removal the table must still accept new handlers in the freed slot,
// which is the case a count left too high or too low would break.
TEST(a_slot_freed_by_removal_is_reusable)
{
    int index;

    msg_begin();

    for (index = 0; index < 16; index++) {
        CHECK_INT(tig_message_set_key_handler(msg_handler, 500 + index), TIG_OK);
    }
    CHECK_INT(tig_message_set_key_handler(msg_handler, 460), TIG_ERR_OUT_OF_HANDLES);

    CHECK_INT(tig_message_set_key_handler(NULL, 507), TIG_OK);
    // One slot back, so exactly one more fits.
    CHECK_INT(tig_message_set_key_handler(msg_handler, 460), TIG_OK);
    CHECK_INT(tig_message_set_key_handler(msg_handler, 461), TIG_ERR_OUT_OF_HANDLES);

    for (index = 0; index < 16; index++) {
        if (index != 7) {
            CHECK_INT(tig_message_set_key_handler(NULL, 500 + index), TIG_OK);
        }
    }
    CHECK_INT(tig_message_set_key_handler(NULL, 460), TIG_OK);

    msg_end();
}

// `tig_message_ping` always enqueues a PING before anything else it does, and
// its SDL event pump returns early with no renderer - so this reaches the
// dispatch half without a display.
TEST(ping_enqueues_a_ping_message)
{
    TigMessage message;

    msg_begin();

    tig_message_ping();
    CHECK_INT(tig_message_dequeue(&message), TIG_OK);
    CHECK_INT(message.type, TIG_MESSAGE_PING);

    msg_end();
}

// A handler fires on the edge, not the level: `tig_message_key_triggers` holds
// the last state so a key held down calls back once. Not exercised here beyond
// the not-pressed case, because pressing a key needs `tig_kb` state this suite
// deliberately does not fake - recorded rather than pretended.
//
// **The key must be a real scancode.** `tig_message_ping` passes it straight to
// `tig_kb_is_key_pressed`, which indexes the keyboard state array with no bound
// check - faithfully, see the NOTE at kb.c:34. An earlier draft of this case
// registered key 700, past `SDL_SCANCODE_COUNT`, and ASan reported a
// global-buffer-overflow on the unmutated tree. That was the test's bug, not the
// engine's, and it was caught by `mutate.py` refusing to run against a tree that
// is not already green.
TEST(a_registered_handler_is_not_called_while_its_key_is_up)
{
    msg_begin();

    handler_calls = 0;
    handler_last_key = -1;

    CHECK_INT(tig_message_set_key_handler(msg_handler, 470), TIG_OK);
    tig_message_ping();
    tig_message_ping();
    CHECK_INT(handler_calls, 0);

    CHECK_INT(tig_message_set_key_handler(NULL, 470), TIG_OK);
    msg_end();
}

void test_message_suite(void)
{
    RUN(an_empty_queue_reports_empty_rather_than_succeeding);
    RUN(a_drain_stops_when_the_queue_is_empty);
    RUN(a_failed_dequeue_leaves_the_callers_message_alone);
    RUN(ordinary_messages_come_back_in_the_order_they_went_in);
    RUN(a_redraw_jumps_to_the_head_of_the_queue);
    RUN(the_later_of_two_redraws_is_serviced_first);
    RUN(an_ordinary_message_into_an_empty_queue_is_still_first);
    RUN(the_message_body_survives_the_round_trip);
    RUN(post_quit_enqueues_a_quit_carrying_its_exit_code);
    RUN(the_queue_survives_more_messages_than_one_node_batch);
    RUN(nodes_are_recycled_across_many_fill_and_drain_cycles);
    RUN(a_negative_key_is_refused);
    RUN(a_handler_can_be_added_and_removed);
    RUN(removing_a_key_that_was_never_registered_is_refused);
    RUN(removing_from_an_empty_table_is_refused);
    RUN(a_removed_handler_does_not_linger_as_a_removable_ghost);
    RUN(key_zero_is_an_ordinary_key);
    RUN(the_table_holds_sixteen_and_refuses_the_seventeenth);
    RUN(removing_from_the_middle_keeps_the_others_removable);
    RUN(a_slot_freed_by_removal_is_reusable);
    RUN(ping_enqueues_a_ping_message);
    RUN(a_registered_handler_is_not_called_while_its_key_is_up);
}
