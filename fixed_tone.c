#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>

#define RATE 48000
#define CHANNELS 2
#define FRAME_SIZE 256

static snd_pcm_t *playback;
static float phase = 0.0f;

// 🎯 Middle C (C4)
#define FREQUENCY 261.63f

int init_audio() {
    snd_pcm_hw_params_t *params;

    int rc = snd_pcm_open(&playback, "plughw:1,0",
                          SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) return rc;

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

void generate_tone(int16_t *out) {
    float inc = 2.0f * M_PI * FREQUENCY / RATE;

    for (int i = 0; i < FRAME_SIZE; i++) {

        float s = sinf(phase);

        // keep amplitude safe
        int16_t v = (int16_t)(s * 1500);

        out[2*i] = v;
        out[2*i + 1] = v;

        phase += inc;

        if (phase > 2.0f * M_PI)
            phase -= 2.0f * M_PI;
    }

    snd_pcm_writei(playback, out, FRAME_SIZE);
}

int main() {
    int16_t buffer[FRAME_SIZE * CHANNELS];

    if (init_audio() < 0) {
        printf("Audio init failed\n");
        return 1;
    }

    printf("Playing Middle C (261.63 Hz)...\n");

    while (1) {
        generate_tone(buffer);
    }

    return 0;
}
