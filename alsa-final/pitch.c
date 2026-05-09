#include "pitch.h"
#include "audio_config.h"

#include <fftw3.h>
#include <math.h>
#include <stdlib.h>

static fftwf_plan plan;
static float *fft_in;
static fftwf_complex *fft_out;

static int initialized = 0;

static void init_fft() {

    fft_in = fftwf_malloc(sizeof(float) * FRAME_SIZE);

    fft_out = fftwf_malloc(
        sizeof(fftwf_complex) *
        (FRAME_SIZE / 2 + 1));

    plan = fftwf_plan_dft_r2c_1d(
        FRAME_SIZE,
        fft_in,
        fft_out,
        FFTW_MEASURE);

    initialized = 1;
}

static void apply_window(float *x) {

    for (int i = 0; i < FRAME_SIZE; i++) {

        float w =
            0.5f *
            (1.0f - cosf(
                2.0f * M_PI * i /
                (FRAME_SIZE - 1)));

        x[i] *= w;
    }
}

float detect_pitch(float *input) {

    if (!initialized)
        init_fft();

    // copy input
    for (int i = 0; i < FRAME_SIZE; i++)
        fft_in[i] = input[i];

    apply_window(fft_in);

    fftwf_execute(plan);

    static float mag[FRAME_SIZE / 2];

    // magnitude spectrum
    for (int i = 0; i < FRAME_SIZE / 2; i++) {

        float re = fft_out[i][0];
        float im = fft_out[i][1];

        mag[i] = sqrtf(re*re + im*im);
    }

    // Harmonic Product Spectrum
    static float hps[FRAME_SIZE / 2];

    for (int i = 0; i < FRAME_SIZE / 2; i++)
        hps[i] = mag[i];

    for (int h = 2; h <= 4; h++) {

        for (int i = 0;
             i < FRAME_SIZE / (2 * h);
             i++) {

            hps[i] *= mag[i * h];
        }
    }

    int min_bin =
        (int)(MIN_FREQ * FRAME_SIZE / RATE);

    int max_bin =
        (int)(MAX_FREQ * FRAME_SIZE / RATE);

    float best = 0.0f;
    int best_bin = 0;

    for (int i = min_bin;
         i < max_bin;
         i++) {

        if (hps[i] > best) {
            best = hps[i];
            best_bin = i;
        }
    }

    // simple confidence gate
    if (best < 1000.0f)
        return -1;

    float freq =
        (float)best_bin *
        RATE /
        FRAME_SIZE;

    return freq;
}
