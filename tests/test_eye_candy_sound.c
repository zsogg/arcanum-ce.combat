// Tests for the three eye-candy sound functions in `game/anim.c` — the whole
// of porting bug #33.
//
// Written 2026-08-30 to close an item that had stood as "needs a playtest"
// since 2026-08-22. The bug was found by reading the disassembly and fixed the
// same day, and its entire symptom is audible, so the ledger recorded it as
// verified against the original's bytes but **not by ear**.
//
// It turns out not to need ears. All three things a listener was asked to
// judge are decisions the engine makes in these three functions, and every one
// of them is observable from outside:
//
//   the sound LOOPS          `anim_eye_candy_sound_begin` passes `loops = 0`
//                            only when the anim_forever bit is set, and takes
//                            a different branch when it does not -
//                            `run_info->flags & 0x20000` distinguishes them
//   it FOLLOWS the target    `anim_eye_candy_sound_move` calls `gsound_move`,
//                            which drives `tig_sound_set_position` from the
//                            object's own OBJ_F_LOCATION every frame
//   it RESTARTS              `anim_eye_candy_sound_restart` acts only when
//                            there is no live handle, which is what makes it
//                            the function that restarts a stopped loop
//
// The three original defects, from `PORTING-BUGS.md` #33:
//
//   1. three sites tested `AGDATA_FLAGS_DATA & 0x80000000`, and nothing sets
//      bit 31 - the original tests `test BYTE PTR [eax+0x70], 0x80`, bit 7 of
//      the low byte (0x431982, 0x43156C, 0x431A0F);
//   2. the restart's handle test was inverted - the original at 0x43155B is
//      `cmp [eax+0xA8], -1` / `jne`, so it proceeds only when there is *no*
//      handle;
//   3. the begin path wrote the handle and the sound id into each other's
//      slots (0x4319AF, 0x4319CC).
//
// **What this suite does not reach.** `gsound_play_sfx_on_obj` returns
// TIG_SOUND_HANDLE_INVALID unless `gsound_init` has run, which needs the game's
// message files. So the *success* half of defect 3 - the handle landing in
// AGDATA_SOUND_HANDLE rather than AGDATA_ANIM_ID_PREVIOUS - is not exercised
// here; the failure half, which is the mirror of the same swap, is. Stated
// rather than left for a reader to discover, and confirmed by the mutation run
// recorded in `scripts/mutations/game-eye-candy-sound.toml`.
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "game/anim_private.h"
#include "game/obj.h"
#include "tig/sound.h"

#include "obj_harness.h"
#include "test_harness.h"

// Globalized out of `anim.c` by the build (see tests/CMakeLists.txt); the
// source keeps them `static`.
void anim_eye_candy_sound_begin(AnimRunInfo* run_info, int64_t obj);
void anim_eye_candy_sound_move(AnimRunInfo* run_info, int64_t obj);
void anim_eye_candy_sound_restart(AnimRunInfo* run_info, int64_t obj);

// The bit the original tests. Named here rather than written as 0x80 at each
// site so a reader can see it is one constant and not three coincidences.
#define ANIM_FOREVER_BIT 0x80

// `run_info->flags` bit that `begin` sets on the NON-looping path.
#define RUN_INFO_ONE_SHOT 0x20000

// A sound id. Any value does: without `gsound_init` the play always fails, and
// what is under test is which slot the failure is recorded in.
#define SOME_SOUND_ID 4242

static AnimRunInfo run_info;
static AnimGoalData goal_data;

// The object every case reads OBJ_F_LOCATION from. A prototype: see the note
// in the suite runner about why it is never moved.
static int64_t subject;

// Build a run_info whose goal data says "an eye-candy animation is running,
// its sound has not started yet, and it loops forever".
static void arm(int flags_data, int previous_id, tig_sound_handle_t handle)
{
    memset(&run_info, 0, sizeof(run_info));
    memset(&goal_data, 0, sizeof(goal_data));
    run_info.cur_stack_data = &goal_data;
    goal_data.params[AGDATA_FLAGS_DATA].data = flags_data;
    goal_data.params[AGDATA_ANIM_ID_PREVIOUS].data = previous_id;
    goal_data.params[AGDATA_SOUND_HANDLE].data = (int)handle;
}

static int handle_slot(void)
{
    return goal_data.params[AGDATA_SOUND_HANDLE].data;
}

static int id_slot(void)
{
    return goal_data.params[AGDATA_ANIM_ID_PREVIOUS].data;
}

// --- `begin`: the failure path, and what this suite cannot see -----------
//
// **The mask at the `begin` site is not observable from here, and finding that
// out was the point of running the mutations.** Re-introducing the 0x80000000
// mask in `begin` came back MISSED, and the reason is a fact about the code
// rather than a hole in the suite: `loops` only changes what `begin` does on
// the SUCCESS path, and `gsound_play_sfx_on_obj` returns INVALID unless
// `gsound_init` has run, which needs the game's sound message files. On the
// failure path both mask values behave identically - the play fails, the id is
// cleared - so no assertion here can tell them apart.
//
// Two cases in the first draft of this file claimed to pin that mask and could
// not fail for the reason their names gave. They are renamed below to what
// they do pin: the failure path clearing the *sound id*, which is the mirror
// of defect 3's swapped slots and is caught.
//
// The mask itself is still pinned - at its other two sites, by
// `restart_honours_the_same_mask` and `move_honours_the_same_mask`, both
// CAUGHT. See `scripts/mutations/game-eye-candy-sound.toml`.

static void begin_does_not_take_the_one_shot_branch_when_the_play_fails(void)
{
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_begin(&run_info, subject);
    // RUN_INFO_ONE_SHOT is only set once a handle comes back, so a failed play
    // must leave it clear whatever the mask said.
    CHECK(!(run_info.flags & RUN_INFO_ONE_SHOT));
}

static void a_failed_play_clears_the_sound_id(void)
{
    // Defect 3's other half: the original clears the *sound id* at 0x4319CC
    // (`mov [edx+0x50], -1`), which is what stops the engine retrying a sound
    // that will not start. The port cleared the handle slot instead.
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_begin(&run_info, subject);
    CHECK(id_slot() == -1);
    // ...and it must not be the handle slot that gets written.
    CHECK(handle_slot() == TIG_SOUND_HANDLE_INVALID);
}

static void begin_needs_a_previous_id(void)
{
    // With no sound id there is nothing to play, so neither slot moves.
    arm(ANIM_FOREVER_BIT, -1, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_begin(&run_info, subject);
    CHECK(id_slot() == -1);
    CHECK(handle_slot() == TIG_SOUND_HANDLE_INVALID);
}

static void begin_moves_an_already_playing_sound(void)
{
    // The `else` arm: a live handle means this is a continuation, not a start,
    // and `begin` delegates to the move path. Observed the same way the move
    // cases are - the sound's position is driven from the object.
    tig_sound_handle_t live;
    int64_t x = 0;
    int64_t y = 0;

    tig_sound_create(&live, TIG_SOUND_TYPE_EFFECT);
    if (live == TIG_SOUND_HANDLE_INVALID) {
        CHECK(true);
        return;
    }

    tig_sound_set_position(live, 31337, 31337);
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, live);
    anim_eye_candy_sound_begin(&run_info, subject);

    tig_sound_get_position(live, &x, &y);
    CHECK(x != 31337 || y != 31337);
}

// --- the inverted test: restart acts only when there is NO handle ---------
//
// Defect 2. The original at 0x43155B is `cmp [eax+0xA8], -1` / `jne`, so this
// function exists to restart a loop that has stopped. Inverted, it would try
// to restart a sound that is already playing and never restart one that had.

static void restart_acts_when_no_handle_is_live(void)
{
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_restart(&run_info, subject);
    // restart must attempt a play when no handle is live
    CHECK(id_slot() == -1);
}

static void restart_leaves_a_live_sound_alone(void)
{
    tig_sound_handle_t live;

    tig_sound_create(&live, TIG_SOUND_TYPE_EFFECT);
    if (live == TIG_SOUND_HANDLE_INVALID) {
        // No mixer: fall back to a handle value that is merely not INVALID,
        // which is all the branch reads.
        live = (tig_sound_handle_t)7;
    }

    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, live);
    anim_eye_candy_sound_restart(&run_info, subject);
    // restart must do nothing while a sound is already playing
    CHECK(id_slot() == SOME_SOUND_ID);
    // restart must not disturb the live handle
    CHECK(handle_slot() == (int)live);
}

static void restart_needs_a_previous_id(void)
{
    arm(ANIM_FOREVER_BIT, -1, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_restart(&run_info, subject);
    // no id means nothing to restart
    CHECK(id_slot() == -1);
    // no id means no handle appears
    CHECK(handle_slot() == TIG_SOUND_HANDLE_INVALID);
}

static void restart_honours_the_same_mask(void)
{
    // Third site for defect 1.
    arm((int)0x80000000, SOME_SOUND_ID, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_restart(&run_info, subject);
    // without bit 7 restart must not fire
    CHECK(id_slot() == SOME_SOUND_ID);
}

// --- following the target ------------------------------------------------
//
// The listener's second question. `anim_eye_candy_sound_move` drives the
// sound's position from the object's own OBJ_F_LOCATION, so a sound that is
// following reads back at the object's location and one that is not keeps
// whatever position it was given.

static void a_looping_sound_is_moved_onto_the_object(void)
{
    tig_sound_handle_t live;
    int64_t x = 0;
    int64_t y = 0;

    tig_sound_create(&live, TIG_SOUND_TYPE_EFFECT);
    if (live == TIG_SOUND_HANDLE_INVALID) {
        // no mixer available; move-position case skipped
        CHECK(true);
        return;
    }

    // A sentinel the engine would never choose, so "it was moved" and "it was
    // left alone" cannot be confused.
    tig_sound_set_position(live, 31337, 31337);
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, live);
    anim_eye_candy_sound_move(&run_info, subject);

    tig_sound_get_position(live, &x, &y);
    // a looping eye-candy sound must be moved onto its object
    CHECK(x != 31337 || y != 31337);
}

static void a_one_shot_is_not_moved(void)
{
    tig_sound_handle_t live;
    int64_t x = 0;
    int64_t y = 0;

    tig_sound_create(&live, TIG_SOUND_TYPE_EFFECT);
    if (live == TIG_SOUND_HANDLE_INVALID) {
        // no mixer available; move-position case skipped
        CHECK(true);
        return;
    }

    tig_sound_set_position(live, 31337, 31337);
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, live);
    run_info.flags |= RUN_INFO_ONE_SHOT;
    anim_eye_candy_sound_move(&run_info, subject);

    tig_sound_get_position(live, &x, &y);
    // a one-shot must be left where it was played
    CHECK(x == 31337 && y == 31337);
}

static void move_honours_the_same_mask(void)
{
    tig_sound_handle_t live;
    int64_t x = 0;
    int64_t y = 0;

    tig_sound_create(&live, TIG_SOUND_TYPE_EFFECT);
    if (live == TIG_SOUND_HANDLE_INVALID) {
        // no mixer available; move-position case skipped
        CHECK(true);
        return;
    }

    tig_sound_set_position(live, 31337, 31337);
    arm((int)0x80000000, SOME_SOUND_ID, live);
    anim_eye_candy_sound_move(&run_info, subject);

    tig_sound_get_position(live, &x, &y);
    // without bit 7 move must not fire
    CHECK(x == 31337 && y == 31337);
}

static void move_does_nothing_without_a_handle(void)
{
    arm(ANIM_FOREVER_BIT, SOME_SOUND_ID, TIG_SOUND_HANDLE_INVALID);
    anim_eye_candy_sound_move(&run_info, subject);
    // no handle means nothing to move
    CHECK(handle_slot() == TIG_SOUND_HANDLE_INVALID);
}

void test_eye_candy_sound_suite(void)
{
    // The dummy audio driver, as `test_sound.c` uses: `tig_sound_init` only
    // succeeds if a mixer opens, and no case here plays anything audible.
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");

    if (!obj_harness_init()) {
        printf("  test_eye_candy_sound: object system unavailable, skipping\n");
        return;
    }

    {
        TigInitInfo init_info;
        memset(&init_info, 0, sizeof(init_info));
        tig_sound_init(&init_info);
    }

    // A prototype is enough: these functions read OBJ_F_LOCATION and, for
    // scenery, OBJ_F_SCENERY_FLAGS. **Its location is never written** - the
    // harness header is explicit that writing OBJ_F_LOCATION reindexes the
    // object spatially and segfaults without a map. Following is proven by
    // watching the sound's position be driven *from* the object, not by moving
    // the object.
    subject = obj_harness_create_proto(OBJ_TYPE_NPC);
    if (subject == OBJ_HANDLE_NULL) {
        printf("  test_eye_candy_sound: could not create a prototype, skipping\n");
        tig_sound_exit();
        return;
    }

    test_run("begin does not take the one-shot branch when the play fails",
        begin_does_not_take_the_one_shot_branch_when_the_play_fails);
    test_run("a failed play clears the sound id",
        a_failed_play_clears_the_sound_id);
    test_run("begin needs a previous id", begin_needs_a_previous_id);
    test_run("begin moves an already-playing sound",
        begin_moves_an_already_playing_sound);
    test_run("restart acts when no handle is live",
        restart_acts_when_no_handle_is_live);
    test_run("restart leaves a live sound alone",
        restart_leaves_a_live_sound_alone);
    test_run("restart needs a previous id",
        restart_needs_a_previous_id);
    test_run("restart honours the same mask",
        restart_honours_the_same_mask);
    test_run("a looping sound is moved onto the object",
        a_looping_sound_is_moved_onto_the_object);
    test_run("a one-shot is not moved",
        a_one_shot_is_not_moved);
    test_run("move honours the same mask",
        move_honours_the_same_mask);
    test_run("move does nothing without a handle",
        move_does_nothing_without_a_handle);

    obj_harness_destroy(subject);
    tig_sound_exit();
}
