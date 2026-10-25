// Tests for `tig/window.c` - the window stack the whole UI draws into.
//
// Written 2026-10-24 for FC1 phase 5d. A window's buffers are SDL surfaces,
// so `tig_window_init`, `tig_window_create`, `tig_window_get_at_position` and
// `tig_window_destroy` run headless; displaying needs a renderer and is not
// reached here. `Arcanum.exe` is the oracle for each case.

#include <string.h>

#include "tig/window.h"

#include "test_harness.h"

static void win_begin(void)
{
    TigInitInfo init_info;

    memset(&init_info, 0, sizeof(init_info));
    init_info.width = 800;
    init_info.height = 600;
    tig_window_init(&init_info);
}

static int win_create(TigWindowFlags flags, int x, int y, int width, int height, tig_window_handle_t* window_handle)
{
    TigWindowData window_data;

    memset(&window_data, 0, sizeof(window_data));
    window_data.flags = flags;
    window_data.rect.x = x;
    window_data.rect.y = y;
    window_data.rect.width = width;
    window_data.rect.height = height;
    return tig_window_create(&window_data, window_handle);
}

// Porting bug #356: the original refuses a filtering window with no filter
// (0x51cbf2-0x51cc07), and a modal window is a filtering one.
TEST(a_filtering_window_without_a_filter_is_refused)
{
    tig_window_handle_t window_handle;

    win_begin();
    CHECK_INT(win_create(TIG_WINDOW_MESSAGE_FILTER, 0, 0, 10, 10, &window_handle), TIG_ERR_INVALID_PARAM);
    CHECK_INT(win_create(TIG_WINDOW_MODAL, 0, 0, 10, 10, &window_handle), TIG_ERR_INVALID_PARAM);
    CHECK_INT(win_create(0, 0, 0, 10, 10, &window_handle), TIG_OK);
    CHECK_INT(tig_window_destroy(window_handle), TIG_OK);
    tig_window_exit();
}

// Porting bug #359: the right and bottom edges are outside the window
// (0x51e6f3-0x51e702).
TEST(a_windows_right_and_bottom_edges_are_outside_it)
{
    tig_window_handle_t window_handle;
    tig_window_handle_t found;

    win_begin();
    CHECK_INT(win_create(0, 10, 20, 30, 40, &window_handle), TIG_OK);
    CHECK_INT(tig_window_get_at_position(10, 20, &found), TIG_OK);
    CHECK_INT(tig_window_get_at_position(39, 59, &found), TIG_OK);
    CHECK_INT(found == window_handle, 1);
    CHECK_INT(tig_window_get_at_position(40, 59, &found), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_window_get_at_position(39, 60, &found), TIG_ERR_INVALID_PARAM);
    CHECK_INT(tig_window_destroy(window_handle), TIG_OK);
    tig_window_exit();
}

void test_window_suite(void)
{
    RUN(a_filtering_window_without_a_filter_is_refused);
    RUN(a_windows_right_and_bottom_edges_are_outside_it);
}
