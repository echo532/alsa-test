#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "detector.h"
#include "midi.h"
#include "synth.h"

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
    snd_pcm_hw_params_set_channels(capture, params, CHANNELS);
    snd_pcm_hw_params_set_rate(capture, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(capture, params, FRAME_SIZE, 0);

    snd_pcm_hw_params(capture, params);
    snd_pcm_prepare(capture);

    if (synth_init() < 0) {
        printf("synth init failed\n");
        return 1;
    }

    printf("Stable MIDI pitch system running...\n");

    while (1) {

        snd_pcm_readi(capture, buf, FRAME_SIZE);

        for (int i = 0; i < FRAME_SIZE; i++)
            mono[i] = buf[i * 2] / 32768.0f;

        if (!is_active(mono, FRAME_SIZE))
            continue;

        float freq = detect_pitch(mono, FRAME_SIZE);

        if (freq < 50 || freq > 2000)
            continue;

        int midi = freq_to_midi(freq);
        int stable_midi = update_stable_midi(midi);

        if (stable_midi < 0)
            continue;

        float out_freq = midi_to_freq(stable_midi);

        printf("\rMIDI %d -> %.2f Hz     ", stable_midi, out_freq);
        fflush(stdout);

        synth_render(out_freq, out);
    }
}
