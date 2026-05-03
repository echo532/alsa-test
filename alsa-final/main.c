#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include "detector.h"
#include "synth.h"
#include "audio_config.h"

#define CHANNELS 2

int main() {
    snd_pcm_t *capture;
    snd_pcm_hw_params_t *params;

    int16_t *buf = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));
    float *mono = malloc(FRAME_SIZE * sizeof(float));
    int16_t *out = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));

    snd_pcm_open(&capture, "plughw:2,0",
                 SND_PCM_STREAM_CAPTURE, 0);

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(capture, params);
    snd_pcm_hw_params_set_access(capture, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture, params,
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(capture, params, 2);
    snd_pcm_hw_params_set_rate(capture, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(capture, params, FRAME_SIZE, 0);

    snd_pcm_hw_params(capture, params);
    snd_pcm_prepare(capture);

    if (synth_init() < 0) {
        printf("Synth init failed\n");
        return 1;
    }

    printf("Running stable pitch mirror...\n");

    float freq = 440;
    int update_counter = 0;

    while (1) {

        snd_pcm_readi(capture, buf, FRAME_SIZE);

        for (int i = 0; i < FRAME_SIZE; i++)
            mono[i] = buf[i * 2] / 32768.0f;

        if (!is_active(mono, FRAME_SIZE))
            continue;

        float new_freq = detect_pitch(mono, FRAME_SIZE);

        if (new_freq < 50 || new_freq > 2000)
            continue;

        // ⛔ SLOW DOWN UPDATES (FIX DISTORTION)
        if (++update_counter % 3 == 0) {
            freq = new_freq;
            printf("\r%.2f Hz      ", freq);
            fflush(stdout);
        }

        // 🎯 stable synthesis
        synth_render(freq, out);
    }

    return 0;
}
