// synth.c
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>

#define RATE 48000
#define CHANNELS 2
#define FRAME_SIZE 256

static snd_pcm_t *playback;
static float phase = 0.0f;

int synth_init() {
    int rc;

    rc = snd_pcm_open(&playback, "plughw:1,0",
                      SND_PCM_STREAM_PLAYBACK, 0);

    if (rc < 0) return rc;

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(playback, params);
    snd_pcm_hw_params_set_access(playback, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback, params,
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(playback, params, CHANNELS);
    snd_pcm_hw_params_set_rate(playback, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(playback, params, FRAME_SIZE, 0);

    rc = snd_pcm_hw_params(playback, params);
    if (rc < 0) return rc;

    snd_pcm_prepare(playback);

    return 0;
}

void synth_generate(float freq, int16_t *out) {
    float inc = 2.0f * M_PI * freq / RATE;

    for (int i = 0; i < FRAME_SIZE; i++) {
        float s = sinf(phase);
        int16_t v = (int16_t)(s * 3000);

        out[2*i] = v;
        out[2*i+1] = v;

        phase += inc;
        if (phase > 2*M_PI) phase -= 2*M_PI;
    }

    snd_pcm_writei(playback, out, FRAME_SIZE);
}
