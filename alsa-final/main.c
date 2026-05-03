#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <alsa/asoundlib.h>

#include "detector.h"
#include "synth.h"

#define CHANNELS 2

static const char *notes[] = {
    "C","C#","D","D#","E","F",
    "F#","G","G#","A","A#","B"
};

int freq_to_midi(float f) {
    return (int)roundf(69 + 12 * log2f(f / 440.0f));
}

float midi_to_freq(int m) {
    return 440.0f * powf(2.0f, (m - 69) / 12.0f);
}

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
        printf("Synth init failed\n");
        return 1;
    }

    printf("Running quantized pitch mirror...\n");

    int last_midi = -1;

    while (1) {

        snd_pcm_readi(capture, buf, FRAME_SIZE);

        for (int i = 0; i < FRAME_SIZE; i++)
            mono[i] = buf[i * 2] / 32768.0f;

        if (!is_active(mono, FRAME_SIZE))
            continue;

        float freq = detect_pitch(mono, FRAME_SIZE);

        if (freq < 50 || freq > 2000)
            continue;

        // 🎯 QUANTIZATION STEP (THIS FIXES CHATTER)
        int midi = freq_to_midi(freq);

        // hysteresis (prevents note flicker)
        if (last_midi != -1 && abs(midi - last_midi) <= 1)
            midi = last_midi;

        last_midi = midi;

        float quantized_freq = midi_to_freq(midi);

        int note = midi % 12;
        int octave = midi / 12 - 1;

        printf("\r%s%d (%.2f Hz)        ",
               notes[note], octave, quantized_freq);

        fflush(stdout);

        synth_render(quantized_freq, out);
    }

    return 0;
}
