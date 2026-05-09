#include "pitch.h"
#include "config.h"
#include "ringbuffer.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

extern ringbuffer_t audio_rb;
extern float shared_freq;

/* ---------------- buffers ---------------- */
static float buffer[FRAME_SIZE];
static float windowed[FRAME_SIZE];

#define VAD_THRESHOLD 0.01f

/* ---------------- smoothing ---------------- */
static float last_freq = 0.0f;

/* ---------------- Hann window ---------------- */
static void apply_window() {

    for (int i = 0; i < FRAME_SIZE; i++) {
        float w =
            0.5f * (1.0f - cosf(2.0f * M_PI * i / (FRAME_SIZE - 1)));

        windowed[i] = buffer[i] * w;
    }
}

/* ---------------- energy gate ---------------- */
static float compute_energy() {

    float e = 0.0f;

    for (int i = 0; i < FRAME_SIZE; i++) {
        e += windowed[i] * windowed[i];
    }

    return sqrtf(e / FRAME_SIZE);
}

/* ---------------- FFT (simple DFT magnitude) ---------------- */
static void compute_spectrum(float *mag) {

    for (int k = 0; k < FRAME_SIZE / 2; k++) {

        float re = 0.0f;
        float im = 0.0f;

        for (int n = 0; n < FRAME_SIZE; n++) {

            float phase =
                2.0f * M_PI * k * n / FRAME_SIZE;

            re += windowed[n] * cosf(phase);
            im -= windowed[n] * sinf(phase);
        }

        mag[k] = re * re + im * im;
    }
}

/* ---------------- parabolic interpolation ---------------- */
static float refine_peak(int k, float *mag) {

    if (k <= 0 || k >= FRAME_SIZE / 2 - 1)
        return k;

    float a = mag[k - 1];
    float b = mag[k];
    float c = mag[k + 1];

    float offset =
        0.5f * (a - c) / (a - 2 * b + c);

    return (float)k + offset;
}

/* ---------------- harmonic correction ---------------- */
static float harmonic_fix(float freq) {

    float candidates[3] = {
        freq,
        freq / 2.0f,
        freq / 3.0f
    };

    float best = freq;

    for (int i = 0; i < 3; i++) {

        float f = candidates[i];

        if (f >= MIN_FREQ && f <= MAX_FREQ) {
            best = f;
            break;
        }
    }

    return best;
}

/* ---------------- main thread ---------------- */
void *pitch_thread(void *arg) {

    float mag[FRAME_SIZE / 2];

    while (1) {

        rb_read_block(&audio_rb,
                      buffer,
                      FRAME_SIZE);

        /* ---------------- window ---------------- */
        apply_window();

        /* ---------------- VAD ---------------- */
        float energy = compute_energy();

        if (energy < VAD_THRESHOLD) {
            shared_freq = 0.0f;
            continue;
        }

        /* ---------------- spectrum ---------------- */
        compute_spectrum(mag);

        /* ---------------- peak search ---------------- */
        int best_k = 1;
        float best_v = 0.0f;

        for (int k = 1; k < FRAME_SIZE / 2; k++) {

            if (mag[k] > best_v) {
                best_v = mag[k];
                best_k = k;
            }
        }

        /* ---------------- refine peak ---------------- */
        float refined =
            refine_peak(best_k, mag);

        float freq =
            refined * RATE / FRAME_SIZE;

        /* ---------------- harmonic correction ---------------- */
        freq = harmonic_fix(freq);

        /* ---------------- validation ---------------- */
        if (freq >= MIN_FREQ &&
            freq <= MAX_FREQ) {

            /* ---------------- smoothing ---------------- */
            shared_freq =
                0.7f * last_freq +
                0.3f * freq;

            last_freq = shared_freq;
        } else {
            shared_freq = 0.0f;
        }
    }
}