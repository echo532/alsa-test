#include "audio_config.h"
#include "detector.h"
#include <math.h>
#include <fftw3.h>
#include <stdlib.h>

static fftwf_plan plan;
static float *in;
static fftwf_complex *out;
static int initialized = 0;

// Hann window
static void apply_window(float *x, int N) {
    for (int i = 0; i < N; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (N - 1)));
        x[i] *= w;
    }
}

static void init_fft(int N) {
    in = fftwf_malloc(sizeof(float) * N);
    out = fftwf_malloc(sizeof(fftwf_complex) * (N/2 + 1));

    plan = fftwf_plan_dft_r2c_1d(N, in, out, FFTW_MEASURE);

    initialized = 1;
}

int is_active(float *x, int N) {
    float rms = 0.0f;

    for (int i = 0; i < N; i++)
        rms += x[i] * x[i];

    rms = sqrtf(rms / N);

    return rms > 0.004f;
}

float detect_pitch(float *x, int N) {

    if (!initialized)
        init_fft(N);

    // copy input
    for (int i = 0; i < N; i++)
        in[i] = x[i];

    // apply window
    apply_window(in, N);

    // run FFT
    fftwf_execute(plan);

    int peak = 0;
    float max = 0.0f;

    for (int i = 1; i < N/2; i++) {

        float re = out[i][0];
        float im = out[i][1];

        float mag = re*re + im*im; // no sqrt needed

        if (mag > max) {
            max = mag;
            peak = i;
        }
    }

    if (peak == 0)
        return -1;

    float freq = (float)peak * RATE / N;

    if (freq < MIN_FREQ || freq > MAX_FREQ)
        return -1;

    return freq;
}
