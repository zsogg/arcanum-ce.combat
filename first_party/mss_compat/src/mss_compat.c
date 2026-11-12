#include "mss_compat.h"

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

struct AUDIO {
    MIX_Audio* audio;
    MIX_Track* track;
};

struct STREAM {
    MIX_Audio* audio;
    MIX_Track* track;
    int loops;
};

static MIX_Mixer* mixer;

void AILCALL AIL_close_stream(HSTREAM stream)
{
    MIX_DestroyTrack(stream->track);
    MIX_DestroyAudio(stream->audio);
    free(stream);
}

int AILCALL AIL_digital_handle_reacquire(HDIGDRIVER drvr)
{
    return 1;
}

int AILCALL AIL_digital_handle_release(HDIGDRIVER drvr)
{
    return 1;
}

// FIX: NULL when the file cannot be opened, as Miles returns, so
// `tig_sound_play_streamed`'s test frees the handle at once. This returned a
// stream with no audio (and dereferenced an unchecked malloc), which the mixer
// was then asked to play (card HW1).
HSTREAM AILCALL AIL_open_stream(HDIGDRIVER dig, const char* filename, int stream_mem)
{
    HSTREAM stream;
    MIX_Audio* audio;

    audio = MIX_LoadAudio(mixer, filename, false);
    if (audio == NULL) {
        return NULL;
    }

    stream = malloc(sizeof(*stream));
    if (stream == NULL) {
        MIX_DestroyAudio(audio);
        return NULL;
    }

    stream->audio = audio;
    stream->track = MIX_CreateTrack(mixer);
    if (stream->track == NULL) {
        MIX_DestroyAudio(audio);
        free(stream);
        return NULL;
    }

    MIX_SetTrackAudio(stream->track, stream->audio);
    return stream;
}

void AILCALL AIL_quick_handles(HDIGDRIVER* pdig, HMDIDRIVER* pmdi, HDLSDEVICE* pdls)
{
    if (pdig != NULL) {
        *pdig = NULL;
    }

    if (pmdi != NULL) {
        *pmdi = NULL;
    }

    if (pdls != NULL) {
        *pdls = NULL;
    }
}

HAUDIO AILCALL AIL_quick_load_mem(void const* mem, unsigned size)
{
    HAUDIO audio = malloc(sizeof(*audio));
    audio->track = MIX_CreateTrack(mixer);
    audio->audio = MIX_LoadAudio_IO(mixer, SDL_IOFromConstMem(mem, size), true, true);
    MIX_SetTrackAudio(audio->track, audio->audio);
    return audio;
}

int AILCALL AIL_quick_play(HAUDIO audio, unsigned loop_count)
{
    SDL_PropertiesID props;
    bool ret;

    props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop_count - 1);
    ret = MIX_PlayTrack(audio->track, props);
    SDL_DestroyProperties(props);

    return ret ? 1 : 0;
}

// CE (card HW1): Miles' balance, 0 left, 64 centre, 127 right, as the two
// gains `MIX_SetTrackStereo` takes. The near side stays whole and the far side
// falls linearly, Miles' own law being unknown. False for the centre, which
// needs no gains.
bool mss_compat_balance_gains(int extravol, float* left, float* right)
{
    if (extravol < 0) {
        extravol = 0;
    } else if (extravol > 127) {
        extravol = 127;
    }

    *left = extravol <= 64 ? 1.0f : (float)(127 - extravol) / 63.0f;
    *right = extravol >= 64 ? 1.0f : (float)extravol / 64.0f;
    return extravol != 64;
}

// FIX: `extravol` is the balance, as Miles takes it and gsound computes it
// (`GSOUND_BALANCE_CENTER`); this dropped it, so every positional sound and
// every scheme ambient played centred. A centred sound mixes exactly as
// before (card HW1).
void AILCALL AIL_quick_set_volume(HAUDIO audio, int volume, int extravol)
{
    MIX_StereoGains gains;

    MIX_SetTrackGain(audio->track, (float)volume / 128.0f);

    if (mss_compat_balance_gains(extravol, &gains.left, &gains.right)) {
        MIX_SetTrackStereo(audio->track, &gains);
    } else {
        MIX_SetTrackStereo(audio->track, NULL);
    }
}

void AILCALL AIL_quick_shutdown(void)
{
    MIX_DestroyMixer(mixer);
}

int AILCALL AIL_quick_startup(int use_digital, int use_MIDI, unsigned output_rate, int output_bits, int output_channels)
{
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (mixer == NULL) {
        return 0;
    }

    return 1;
}

int AILCALL AIL_quick_status(HAUDIO audio)
{
    if (MIX_TrackPlaying(audio->track)) {
        return QSTAT_PLAYING;
    } else if (MIX_GetTrackRemaining(audio->track) == 0) {
        return QSTAT_DONE;
    } else {
        return 0;
    }
}

void AILCALL AIL_quick_unload(HAUDIO audio)
{
    MIX_DestroyTrack(audio->track);
    MIX_DestroyAudio(audio->audio);
    free(audio);
}

void AILCALL AIL_set_stream_loop_count(HSTREAM stream, int count)
{
    stream->loops = count;
}

void AILCALL AIL_set_stream_volume(HSTREAM stream, int volume)
{
    MIX_SetTrackGain(stream->track, (float)volume / 128.0f);
}

void AILCALL AIL_start_stream(HSTREAM stream)
{
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, stream->loops - 1);
    MIX_PlayTrack(stream->track, props);
    SDL_DestroyProperties(props);
}

int AILCALL AIL_stream_status(HSTREAM stream)
{
    if (MIX_TrackPlaying(stream->track)) {
        return SMP_PLAYING;
    } else {
        return 0;
    }
}

bool mss_compat_init(void)
{
    if (!MIX_Init()) {
        return false;
    }

    return true;
}

void mss_compat_exit(void)
{
    MIX_Quit();
}
