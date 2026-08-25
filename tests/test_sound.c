// Tests for `tig/sound.c`'s handle table - who gets which slot, and what an
// accessor answers when it cannot answer.
//
// `sound.c` had no suite, no differential and no emudiff case, and the reason
// given was that it reaches the mixer. Most of it does not: of its 44
// functions roughly thirty are handle-table state and accessors that never
// touch `mss_compat`, and twelve of its fourteen FIXMEs sat in that band. What
// the mixer is needed for is only `tig_sound_init` returning true, and SDL's
// `dummy` audio driver supplies that with no device (see `sound_up`) - so
// these run anywhere the other suites run.
//
// **Every sentinel asserted here was read out of `Arcanum.exe` 1.0.7.4**, not
// inferred from CE, because most of them look like placeholders and are not:
// `tig_sound_get_loops` answers 1 when the subsystem is down but 0 for a bad
// handle (0x5336C9 `mov eax,1` against 0x5336F2 `xor eax,eax`), while
// `tig_sound_get_extra_volume` answers 64 to both (0x53396C). Pinning them
// stops a future tidy-up from "fixing" an asymmetry the original has.
// Evidence: `arcanum-work/audits/tig-sound-20260825/handle-table.md`.

#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "tig/sound.h"
#include "tig/types.h"

#include "test_harness.h"

// The handle ranges, from `sound.c`'s own constants. Attested at 0x5331B8
// (voice scans entries 0..1) and 0x533299 (an effect handle wrapping resets to
// 6), and by `sizeof(TigSound)` being 0x150 in the original - the index
// arithmetic `[eax*336]` appears at every accessor.
#define VOICE_FIRST 0
#define MUSIC_FIRST 2
#define EFFECT_FIRST 6
#define HANDLE_MAX 60

// `tig_sound_init` only succeeds if `AIL_quick_startup` opens a mixer, so the
// suite asks SDL for its dummy driver rather than a sound card. Set before
// SDL touches audio, and left set - nothing else in the binary plays anything.
static bool sound_up(void)
{
    TigInitInfo init_info;

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    memset(&init_info, 0, sizeof(init_info));
    tig_sound_init(&init_info);

    return tig_sound_is_initialized();
}

static void sound_down(void)
{
    tig_sound_exit();
}

// Acquire every handle a type can hold, recording them; returns how many came
// back before the table said no.
static int drain_type(TigSoundType type, tig_sound_handle_t* out, int max)
{
    int count = 0;
    tig_sound_handle_t handle;

    while (count < max) {
        tig_sound_create(&handle, type);
        if (handle == TIG_SOUND_HANDLE_INVALID) {
            break;
        }
        out[count++] = handle;
    }

    return count;
}

static void release_all(const tig_sound_handle_t* handles, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        tig_sound_destroy(handles[index]);
    }
}

// The suite is worth nothing if the mixer never came up, because every
// accessor would take its not-initialized branch and agree with anything.
TEST(the_dummy_driver_brings_the_subsystem_up)
{
    const char* driver;

    CHECK(sound_up());
    CHECK(tig_sound_is_initialized());

    // And that it is genuinely the dummy device, not a sound card that
    // happened to be there - otherwise this suite would pass here and fail on
    // any machine without audio, which is the opposite of what it is for.
    driver = SDL_GetCurrentAudioDriver();
    CHECK(driver != NULL);
    CHECK_STR(driver != NULL ? driver : "(none)", "dummy");

    sound_down();
    CHECK(!tig_sound_is_initialized());
}

// Three fixed partitions, not one pool: voice 0-1, music 2-5, effects 6-59.
// A type that runs out does not borrow from another.
TEST(each_type_draws_from_its_own_range)
{
    tig_sound_handle_t handles[HANDLE_MAX];
    int count;
    int index;

    CHECK(sound_up());

    count = drain_type(TIG_SOUND_TYPE_VOICE, handles, HANDLE_MAX);
    CHECK_INT(count, MUSIC_FIRST - VOICE_FIRST);
    for (index = 0; index < count; index++) {
        CHECK(handles[index] >= VOICE_FIRST && handles[index] < MUSIC_FIRST);
    }
    release_all(handles, count);

    count = drain_type(TIG_SOUND_TYPE_MUSIC, handles, HANDLE_MAX);
    CHECK_INT(count, EFFECT_FIRST - MUSIC_FIRST);
    for (index = 0; index < count; index++) {
        CHECK(handles[index] >= MUSIC_FIRST && handles[index] < EFFECT_FIRST);
    }
    release_all(handles, count);

    count = drain_type(TIG_SOUND_TYPE_EFFECT, handles, HANDLE_MAX);
    CHECK_INT(count, HANDLE_MAX - EFFECT_FIRST);
    for (index = 0; index < count; index++) {
        CHECK(handles[index] >= EFFECT_FIRST && handles[index] < HANDLE_MAX);
    }
    release_all(handles, count);

    sound_down();
}

// Exhaustion is reported, not papered over - and it does not corrupt the
// table: after one release the next request gets that slot back.
TEST(an_exhausted_type_says_so_and_recovers)
{
    tig_sound_handle_t handles[HANDLE_MAX];
    tig_sound_handle_t extra;
    int count;

    CHECK(sound_up());

    count = drain_type(TIG_SOUND_TYPE_MUSIC, handles, HANDLE_MAX);
    CHECK_INT(count, EFFECT_FIRST - MUSIC_FIRST);

    tig_sound_create(&extra, TIG_SOUND_TYPE_MUSIC);
    CHECK_INT(extra, TIG_SOUND_HANDLE_INVALID);

    tig_sound_destroy(handles[1]);
    tig_sound_create(&extra, TIG_SOUND_TYPE_MUSIC);
    CHECK_INT(extra, handles[1]);

    release_all(handles, count);
    sound_down();
}

// Effects are round-robin rather than lowest-free: the cursor keeps moving, so
// a slot released early is not handed straight back while others are free.
// That is what `tig_sound_next_effect_handle` is for, and it wraps to 6 rather
// than 0 (0x533299 `mov eax,6`), which is what keeps effects out of the voice
// and music ranges.
TEST(effect_handles_advance_and_wrap_within_their_range)
{
    tig_sound_handle_t first;
    tig_sound_handle_t second;
    tig_sound_handle_t handles[HANDLE_MAX];
    tig_sound_handle_t after_wrap;
    int count;

    CHECK(sound_up());

    tig_sound_create(&first, TIG_SOUND_TYPE_EFFECT);
    tig_sound_create(&second, TIG_SOUND_TYPE_EFFECT);
    CHECK_INT(first, EFFECT_FIRST);
    CHECK_INT(second, EFFECT_FIRST + 1);

    // Free the first, then fill the rest of the range. The cursor has moved
    // past it, so it is only reused once the scan wraps around to it again.
    tig_sound_destroy(first);
    count = drain_type(TIG_SOUND_TYPE_EFFECT, handles, HANDLE_MAX);
    CHECK_INT(count, HANDLE_MAX - EFFECT_FIRST - 1);
    after_wrap = handles[count - 1];
    CHECK_INT(after_wrap, EFFECT_FIRST);

    release_all(handles, count);
    tig_sound_destroy(second);
    sound_down();
}

// A fresh handle is not zeroed - it carries the defaults `tig_sound_create`
// writes, and a caller that never sets a volume gets these.
TEST(a_new_sound_carries_the_documented_defaults)
{
    tig_sound_handle_t handle;

    CHECK(sound_up());

    tig_sound_create(&handle, TIG_SOUND_TYPE_EFFECT);
    CHECK(handle != TIG_SOUND_HANDLE_INVALID);
    CHECK_INT(tig_sound_get_loops(handle), 1);
    CHECK_INT(tig_sound_get_volume(handle), 127);
    CHECK_INT(tig_sound_get_extra_volume(handle), 64);
    CHECK_INT(tig_sound_get_type(handle), TIG_SOUND_TYPE_EFFECT);
    CHECK(tig_sound_is_active(handle));
    CHECK(!tig_sound_is_positional(handle));

    tig_sound_destroy(handle);
    CHECK(!tig_sound_is_active(handle));

    sound_down();
}

// The sentinels, and the asymmetry between them. These are read out of the
// original, not chosen: 1 / 0 / 0 / 64 for an out-of-range handle, and the
// same four values again with the subsystem down except that `get_loops`
// answers 1 there instead of 0. A caller cannot tell failure from a real
// value, which is what the four upstream FIXMEs are about - the shape is the
// original's and is not a defect to fix here.
TEST(accessors_answer_the_originals_sentinels_for_a_bad_handle)
{
    CHECK(sound_up());

    CHECK_INT(tig_sound_get_loops(-1), 0);
    CHECK_INT(tig_sound_get_loops(HANDLE_MAX), 0);
    CHECK_INT(tig_sound_get_volume(-1), 0);
    CHECK_INT(tig_sound_get_volume(HANDLE_MAX), 0);
    CHECK_INT(tig_sound_get_type(-1), 0);
    CHECK_INT(tig_sound_get_type(HANDLE_MAX), 0);
    CHECK_INT(tig_sound_get_extra_volume(-1), 64);
    CHECK_INT(tig_sound_get_extra_volume(HANDLE_MAX), 64);
    CHECK(!tig_sound_is_active(-1));
    CHECK(!tig_sound_is_active(HANDLE_MAX));
    CHECK(!tig_sound_is_playing(-1));
    CHECK(!tig_sound_is_positional(HANDLE_MAX));

    sound_down();

    // With the subsystem down `get_loops` answers 1, not 0 - 0x5336C9 against
    // 0x5336F2. The other three answer the same as above.
    CHECK_INT(tig_sound_get_loops(0), 1);
    CHECK_INT(tig_sound_get_volume(0), 0);
    CHECK_INT(tig_sound_get_type(0), 0);
    CHECK_INT(tig_sound_get_extra_volume(0), 64);
}

// Position is stored per handle, and asking for it sets the positional flag.
TEST(a_position_round_trips_and_marks_the_sound_positional)
{
    tig_sound_handle_t handle;
    int64_t x = -1;
    int64_t y = -1;

    CHECK(sound_up());

    tig_sound_create(&handle, TIG_SOUND_TYPE_EFFECT);
    CHECK(!tig_sound_is_positional(handle));

    tig_sound_set_position(handle, 4242, 8484);
    CHECK(tig_sound_is_positional(handle));

    tig_sound_get_position(handle, &x, &y);
    CHECK_INT(x, 4242);
    CHECK_INT(y, 8484);

    tig_sound_destroy(handle);
    sound_down();
}

// The quirk worth pinning: `tig_sound_get_position` writes nothing unless the
// sound is both active and positional, so the caller's variables keep whatever
// they held - uninitialised, if the caller did not set them. Faithful, and the
// reason a caller must not treat the outputs as always-written.
TEST(get_position_leaves_the_output_alone_when_there_is_none)
{
    tig_sound_handle_t handle;
    int64_t x = 111;
    int64_t y = 222;

    CHECK(sound_up());

    tig_sound_create(&handle, TIG_SOUND_TYPE_EFFECT);

    tig_sound_get_position(handle, &x, &y);
    CHECK_INT(x, 111);
    CHECK_INT(y, 222);

    tig_sound_get_position(-1, &x, &y);
    CHECK_INT(x, 111);
    CHECK_INT(y, 222);

    tig_sound_destroy(handle);
    sound_down();
}

// `tig_sound_set_volume_by_type` walks the whole table and touches only the
// sounds whose flags carry that type - so turning music down leaves effects
// where they were.
TEST(setting_a_volume_by_type_leaves_the_other_types_alone)
{
    tig_sound_handle_t music;
    tig_sound_handle_t effect;

    CHECK(sound_up());

    tig_sound_create(&music, TIG_SOUND_TYPE_MUSIC);
    tig_sound_create(&effect, TIG_SOUND_TYPE_EFFECT);
    CHECK(music != TIG_SOUND_HANDLE_INVALID);
    CHECK(effect != TIG_SOUND_HANDLE_INVALID);

    tig_sound_set_volume_by_type(TIG_SOUND_TYPE_MUSIC, 33);
    CHECK_INT(tig_sound_get_volume(music), 33);
    CHECK_INT(tig_sound_get_volume(effect), 127);

    tig_sound_destroy(music);
    tig_sound_destroy(effect);
    sound_down();
}

// The failure a modder actually meets: a sound file that is not there. It is
// not reported - `tig_sound_play` returns TIG_OK either way, and the only
// trace is that the handle goes inactive. `tig_file_cache_acquire` hands back
// a static zeroed entry for a path it cannot open (`&null_entry`), so the
// `data != NULL` branch is skipped and `active` is cleared. A mistyped name in
// a mod is silence with no diagnostic, by design on both sides.
TEST(playing_a_missing_file_quietly_deactivates_the_handle)
{
    tig_sound_handle_t handle;

    CHECK(sound_up());

    tig_sound_create(&handle, TIG_SOUND_TYPE_EFFECT);
    CHECK(tig_sound_is_active(handle));

    CHECK_INT(tig_sound_play(handle, "./arcanum_test_sound_absent.wav", 7), TIG_OK);
    CHECK(!tig_sound_is_active(handle));
    CHECK(!tig_sound_is_playing(handle));

    sound_down();
}

// And the slot really is free again afterwards, not merely marked inactive -
// music scans lowest-first, so the same handle comes straight back.
TEST(a_slot_freed_by_a_failed_play_is_handed_out_again)
{
    tig_sound_handle_t first;
    tig_sound_handle_t again;

    CHECK(sound_up());

    tig_sound_create(&first, TIG_SOUND_TYPE_MUSIC);
    CHECK_INT(tig_sound_play(first, "./arcanum_test_sound_absent.wav", 0), TIG_OK);
    CHECK(!tig_sound_is_active(first));

    tig_sound_create(&again, TIG_SOUND_TYPE_MUSIC);
    CHECK_INT(again, first);

    tig_sound_destroy(again);
    sound_down();
}

void test_sound_suite(void)
{
    RUN(the_dummy_driver_brings_the_subsystem_up);
    RUN(each_type_draws_from_its_own_range);
    RUN(an_exhausted_type_says_so_and_recovers);
    RUN(effect_handles_advance_and_wrap_within_their_range);
    RUN(a_new_sound_carries_the_documented_defaults);
    RUN(accessors_answer_the_originals_sentinels_for_a_bad_handle);
    RUN(a_position_round_trips_and_marks_the_sound_positional);
    RUN(get_position_leaves_the_output_alone_when_there_is_none);
    RUN(setting_a_volume_by_type_leaves_the_other_types_alone);
    RUN(playing_a_missing_file_quietly_deactivates_the_handle);
    RUN(a_slot_freed_by_a_failed_play_is_handed_out_again);
}
