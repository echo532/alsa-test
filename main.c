#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "detector.c"
#include "synth.c"

#define RATE 48000
#define CHANNELS 2
#define FRAME_SIZE 256

int main() {
    snd_pcm_t *capture;
    snd_pcm_hw_params_t *params;

    int16_t *buffer = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));
    float *mono = malloc(FRAME_SIZE * sizeof(float));
    int16_t *out = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));

    // open mic
    snd_pcm_open(&capture, "plughw:2,0",
                 SND_PCM_STREAM_CAPTURE, 0);

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(capture, params);
    snd_pcm_hw_params_set_access(capture, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture, params,
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(capture, params, CHANNELS);
    snd_pcm_hw_params_set_rate(capture, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(capture, params, FRAME_SIZE, 0);

    snd_pcm_hw_params(capture, params);
    snd_pcm_prepare(capture);

    if (synth_init() < 0) {
        printf("Playback init failed\n");
        return 1;
    }

    float last_freq = 0;
    float smooth = 0;

    printf("Running pitch mirror...\n");

    while (1) {

        snd_pcm_readi(capture, buffer, FRAME_SIZE);

        // mono conversion
        for (int i = 0; i < FRAME_SIZE; i++)
            mono[i] = buffer[i*2] / 32768.0f;

        if (!is_active(mono, FRAME_SIZE))
            continue;

        float freq = detect_pitch(mono, FRAME_SIZE);

        if (freq < 50 || freq > 2000)
            continue;

        // smoothing
        smooth = 0.8f * smooth + 0.2f * freq;

        printf("\r%.2f Hz      ", smooth);
        fflush(stdout);

        synth_generate(smooth, out);
    }

    return 0;
}
