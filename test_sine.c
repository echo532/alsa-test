#include <alsa/asoundlib.h>
#include <math.h>
#include <stdint.h>

#define RATE 48000
#define FRAME 256
#define CHANNELS 2

int main() {

    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *params;

    snd_pcm_open(&pcm, "plughw:1,0",
                 SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm, params);
    snd_pcm_hw_params_set_access(pcm, params,
        SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, params,
        SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, params,
        CHANNELS);
    snd_pcm_hw_params_set_rate(pcm, params,
        RATE, 0);

    snd_pcm_hw_params(pcm, params);
    snd_pcm_prepare(pcm);

    int16_t buffer[FRAME * 2];

    float phase = 0.0f;
    float freq = 440.0f;

    float inc = 2.0f * M_PI * freq / RATE;

    while (1) {

        for (int i = 0; i < FRAME; i++) {

            int16_t v =
                (int16_t)(sinf(phase) * 8000);

            buffer[2*i] = v;
            buffer[2*i+1] = v;

            phase += inc;

            if (phase > 2*M_PI)
                phase -= 2*M_PI;
        }

        snd_pcm_writei(pcm, buffer, FRAME);
    }
}