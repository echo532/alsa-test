#include "synth.h"
#include "config.h"

#include <alsa/asoundlib.h>
#include <math.h>

extern float shared_freq;

static snd_pcm_t *playback;
static float phase = 0;

void *synth_thread(void *arg) {

    snd_pcm_open(&playback,
                 "plughw:1,0",
                 SND_PCM_STREAM_PLAYBACK,
                 0);

    snd_pcm_set_params(playback,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        CHANNELS,
        RATE,
        1,
        20000);

    int16_t buffer[FRAME_SIZE];

    while (1) {

        float freq = shared_freq;

        float inc = 2 * M_PI * freq / RATE;

        for (int i = 0; i < FRAME_SIZE; i++) {

            buffer[i] =
                (int16_t)(sinf(phase) * 8000);

            phase += inc;
        }

        snd_pcm_writei(playback,
                       buffer,
                       FRAME_SIZE);
    }
}