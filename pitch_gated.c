#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define RATE 48000
#define CHANNELS 2
#define FRAME_SIZE 256   // latency-focused
#define RMS_GATE 0.003f  // tune this for your mic

static const char *notes[] = {
    "C","C#","D","D#","E","F",
    "F#","G","G#","A","A#","B"
};

int freq_to_midi(float f) {
    return (int)(69 + 12 * log2f(f / 440.0f));
}

float autocorr_pitch(float *x, int N) {
    int min_lag = RATE / 1000;
    int max_lag = RATE / 50;

    float best = 0;
    int best_lag = 0;

    for (int lag = min_lag; lag < max_lag; lag++) {
        float sum = 0;

        for (int i = 0; i < N - lag; i++) {
            sum += x[i] * x[i + lag];
        }

        if (sum > best) {
            best = sum;
            best_lag = lag;
        }
    }

    if (best_lag == 0) return -1;

    return (float)RATE / best_lag;
}

/*
 * FAST INSTRUMENT GATE
 * - RMS (energy check)
 * - zero-crossing sanity check (reject noise)
 */
int instrument_present(float *x, int N) {
    float rms = 0;
    int zero_crossings = 0;

    for (int i = 0; i < N; i++) {
        rms += x[i] * x[i];

        if (i > 0) {
            if ((x[i] > 0) != (x[i - 1] > 0)) {
                zero_crossings++;
            }
        }
    }

    rms = sqrtf(rms / N);

    // silence/noise rejection
    if (rms < RMS_GATE) return 0;

    // too noisy (white noise has high zero crossings)
    // float zcr = (float)zero_crossings / N;
    // if (zcr > 0.55f) return 0;

    return 1;
}

int main() {
    snd_pcm_t *capture;
    snd_pcm_hw_params_t *params;

    int16_t *buffer = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));
    float *mono = malloc(FRAME_SIZE * sizeof(float));

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

    printf("Running gated real-time pitch detector...\n");

    int stable_count = 0;
    float last_freq = -1;

    while (1) {

        snd_pcm_readi(capture, buffer, FRAME_SIZE);

        // mono conversion
        for (int i = 0; i < FRAME_SIZE; i++) {
            mono[i] = buffer[i * 2] / 32768.0f;
        }

        //FAST GATE FIRST
        if (!instrument_present(mono, FRAME_SIZE)) {
            stable_count = 0;
            continue;
        }

        //ONLY NOW run expensive pitch detection
        float freq = autocorr_pitch(mono, FRAME_SIZE);

        if (freq < 50 || freq > 2000) continue;

        // stability filter (removes flicker)
        if (fabs(freq - last_freq) < 5.0f) {
            stable_count++;
        } else {
            stable_count = 0;
        }

        last_freq = freq;

        if (stable_count >= 2) {
            int midi = freq_to_midi(freq);
            int note = midi % 12;
            int octave = midi / 12 - 1;

            printf("\r%s%d  (%.2f Hz)        ",
                   notes[note], octave, freq);

            fflush(stdout);
        }
    }

    return 0;
}
