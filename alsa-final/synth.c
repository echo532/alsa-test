#include "synth.h"
#include "audio_config.h"

#include <alsa/asoundlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static snd_pcm_t *playback = NULL;

static float phase = 0.0f;

// persistent buffer (IMPORTANT)
static int16_t buffer[FRAME_SIZE * CHANNELS];

int synth_init(void) {

    snd_pcm_hw_params_t *params;

    if (snd_pcm_open(&playback,
                     "plughw:1,0",
                     SND_PCM_STREAM_PLAYBACK,
                     0) < 0) {
        printf("failed to open playback device\n");
        return -1;
    }

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(playback, params);

    snd_pcm_hw_params_set_access(
        playback,
        params,
        SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(
        playback,
        params,
        SND_PCM_FORMAT_S16_LE);

    snd_pcm_hw_params_set_channels(
        playback,
        params,
        CHANNELS);

    snd_pcm_hw_params_set_rate(
        playback,
        params,
        RATE,
        0);

    snd_pcm_hw_params_set_period_size(
        playback,
        params,
        FRAME_SIZE,
        0);

    if (snd_pcm_hw_params(playback, params) < 0) {
        printf("failed hw params\n");
        return -1;
    }

    snd_pcm_prepare(playback);

    return 0;
}

void synth_play(float freq) {

    if (!playback)
        return;

    if (freq <= 0.0f)
        return;

    float inc =
        2.0f * M_PI * freq / RATE;

    for (int i = 0; i < FRAME_SIZE; i++) {

        float s = sinf(phase);

        int16_t v =
            (int16_t)(s * 8000);

        buffer[2 * i] = v;
        buffer[2 * i + 1] = v;

        phase += inc;

        if (phase > 2.0f * M_PI)
            phase -= 2.0f * M_PI;
    }

    int err = snd_pcm_writei(
        playback,
        buffer,
        FRAME_SIZE);

    // 🔥 CRITICAL FIX: handle underrun
    if (err == -EPIPE) {
        snd_pcm_prepare(playback);
    }

    // optional safety fallback
    if (err < 0 && err != -EPIPE) {
        snd_pcm_recover(playback, err, 1);
    }
}

void synth_close(void) {

    if (playback) {
        snd_pcm_drain(playback);
        snd_pcm_close(playback);
        playback = NULL;
    }
}