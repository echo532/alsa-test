#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define RATE 48000
#define CHANNELS 2
#define FRAME_SIZE 2048   // analysis window (tradeoff: latency vs stability)

static const char *notes[] = {
    "C","C#","D","D#","E","F",
    "F#","G","G#","A","A#","B"
};

// Convert frequency to MIDI note number
int freq_to_midi(float f) {
    return (int)(69 + 12 * log2f(f / 440.0f));
}

// Autocorrelation pitch detection
float detect_pitch(float *x, int size) {
    int min_lag = RATE / 1000;  // ~1000 Hz max
    int max_lag = RATE / 50;    // ~50 Hz min

    float best_corr = 0.0f;
    int best_lag = 0;

    for (int lag = min_lag; lag < max_lag; lag++) {
        float sum = 0.0f;

        for (int i = 0; i < size - lag; i++) {
            sum += x[i] * x[i + lag];
        }

        if (sum > best_corr) {
            best_corr = sum;
            best_lag = lag;
        }
    }

    if (best_lag == 0) return -1.0f;

    return (float)RATE / best_lag;
}

int main() {
    snd_pcm_t *capture;
    snd_pcm_hw_params_t *params;
    int rc;

    int16_t *buffer = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));
    float *mono = malloc(FRAME_SIZE * sizeof(float));

    if (!buffer || !mono) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    // Open capture device (YOU already confirmed this works)
    rc = snd_pcm_open(&capture, "plughw:2,0",
                      SND_PCM_STREAM_CAPTURE, 0);

    if (rc < 0) {
        fprintf(stderr, "open error: %s\n", snd_strerror(rc));
        return 1;
    }

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(capture, params);
    snd_pcm_hw_params_set_access(capture, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(capture, params,
                                 SND_PCM_FORMAT_S16_LE);

    snd_pcm_hw_params_set_channels(capture, params, CHANNELS);
    snd_pcm_hw_params_set_rate(capture, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(capture, params, FRAME_SIZE, 0);

    rc = snd_pcm_hw_params(capture, params);

    if (rc < 0) {
        fprintf(stderr, "hw_params error: %s\n", snd_strerror(rc));
        return 1;
    }

    snd_pcm_prepare(capture);

    printf("Running pitch detector...\n");

    while (1) {

        rc = snd_pcm_readi(capture, buffer, FRAME_SIZE);

        if (rc < 0) {
            snd_pcm_prepare(capture);
            continue;
        }

        // Convert stereo → mono float
        for (int i = 0; i < FRAME_SIZE; i++) {
            mono[i] = buffer[i * 2] / 32768.0f;
        }

        float freq = detect_pitch(mono, FRAME_SIZE);

        if (freq > 50 && freq < 2000) {
            int midi = freq_to_midi(freq);
            int note = midi % 12;
            int octave = midi / 12 - 1;

            printf("\rFreq: %7.2f Hz  →  %s%d        ",
                   freq, notes[note], octave);

            fflush(stdout);
        }
    }

    free(buffer);
    free(mono);
    snd_pcm_close(capture);

    return 0;
}
