// Regression test for quirk 30 - the shutdown segfault in `dialog_unload`.
//
// Written 2026-08-29, after the driven tig census
// (`arcanum-work/harness/tig-census-input.py`) crashed CE on `Slot0010` with
// rc=-11 while quitting from inside a conversation:
//
//     #0  dialog_unload (dlg=0)             dialog.c:452
//     #1  dialog_ui_end_dialog              dialog_ui.c:236
//     #2  dialog_ui_notify_object_destroyed dialog_ui.c:485
//     #5  object_delete -> map_clear_objects -> map_close -> map_exit
//         -> gamelib_exit
//
// `gamelib_exit` tears its module table down in reverse and "Dialog"
// (gamelib.c:157) sits after "Map" (:150), so `dialog_exit` frees `dialog_files`
// and NULLs it - and then map teardown deletes every object, announcing each
// through the UI. If the destroyed object is the one you were talking to, the
// notify reaches `dialog_unload`, which decremented through the NULL.
//
// **This is one case rather than a suite** on purpose. What a unit test can
// reach here is the last frame of that stack - the state `dialog_files == NULL`
// and a call arriving anyway. Reproducing the full path needs a map, objects, a
// live conversation and a shutdown, which is what the driven census does and
// what a test that runs with no assets cannot. So this pins the guard, and the
// census remains the thing that pins the path.

#include "game/dialog.h"

#include "test_harness.h"

// The bug exactly: no `dialog_init`, so `dialog_files` is NULL, and a call
// arrives anyway. Before the guard this segfaulted; arriving at the next line
// IS the assertion.
TEST(unloading_with_no_dialog_table_is_refused_rather_than_a_segfault)
{
    dialog_unload(0);
    CHECK(1);
}

// Any index, not just zero. The crashing call passed `dlg=0`, but the guard is
// on the table rather than on the index and this says so.
TEST(unloading_any_index_with_no_table_is_refused)
{
    dialog_unload(3);
    dialog_unload(127);
    CHECK(1);
}

// NOT tested: reaching this state the way shutdown does, by calling
// `dialog_exit` to free the table first. An earlier draft did exactly that and
// segfaulted - `dialog_exit` opens by walking `dialog_mes_files[0..GD_COUNT)`,
// which `dialog_init` allocates, so without a matching init it dereferences a
// NULL of its own before it ever reaches the `dialog_files` free. That is a
// different unguarded pointer on a path no shipped caller takes (`gamelib_exit`
// never runs without `gamelib_init`), and pinning it would mean asserting on a
// crash rather than preventing one. Recorded here rather than chased.
//
// The state these cases do reach - a NULL table and a call arriving anyway - is
// the same one shutdown produces, which is what makes them a regression test for
// quirk 30 rather than a test of an arbitrary null.

void test_dialog_shutdown_suite(void)
{
    RUN(unloading_with_no_dialog_table_is_refused_rather_than_a_segfault);
    RUN(unloading_any_index_with_no_table_is_refused);
}
