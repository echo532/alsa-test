#include "pitch.h"
#include "config.h"
#include "ringbuffer.h"

#include <fftw3.h>
#include <math.h>
#include <stdio.h>

extern ringbuffer_t audio_rb;
extern float shared_freq;

/* ---------------- FFTW ---------------- */
static fftwf_plan plan;
static float *fft_in;
static fftwf_complex *fft_out;

static int initialized = 0;

/* ---------------- buffers ---------------- */
static float mag[FRAME_SIZE / 2];
static float hps[FRAME_SIZE / 2];

/* ---------------- smoothing state ---------------- */
static float last_freq = 0.0f;

/* ---------------- confidence memory ---------------- */
static float last_confidence = 0.0f;

/* ---------------- init ---------------- */
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

/* ---------------- window ---------------- */
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

/* ---------------- main pitch thread ---------------- */
void *pitch_thread(void *arg) {

    if (!initialized)
        init_fft();

    float input[FRAME_SIZE];

    while (1) {

        rb_read_block(&audio_rb,
                      input,
                      FRAME_SIZE);

        /* ---------------- FFT INPUT ---------------- */
        for (int i = 0; i < FRAME_SIZE; i++)
            fft_in[i] = input[i];

        apply_window(fft_in);

        fftwf_execute(plan);

        /* ---------------- MAGNITUDE ---------------- */
        for (int i = 0; i < FRAME_SIZE / 2; i++) {

            float re = fft_out[i][0];
            float im = fft_out[i][1];

            mag[i] = sqrtf(re*re + im*im);
        }

        /* ---------------- HPS ---------------- */
        for (int i = 0; i < FRAME_SIZE / 2; i++)
            hps[i] = mag[i];

        for (int h = 2; h <= 4; h++) {

            for (int i = 0;
                 i < FRAME_SIZE / (2 * h);
                 i++) {

                hps[i] *= mag[i * h];
            }
        }

        /* ---------------- SEARCH RANGE ---------------- */
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

        /* ---------------- CONFIDENCE MODEL ---------------- */
        float confidence = best;

        /* normalize (important for stability) */
        confidence = confidence / 10000.0f;

        /* smooth confidence */
        confidence = 0.7f * last_confidence +
                     0.3f * confidence;

        last_confidence = confidence;

        /* ---------------- FREQUENCY ESTIMATE ---------------- */
        float freq =
            (float)best_bin *
            RATE /
            FRAME_SIZE;

        /* ---------------- MUSICAL SMOOTHING (NO HARD GATING) ---------------- */

        if (confidence > 0.002f) {

            /* strong signal → trust FFT */
            shared_freq =
                0.65f * last_freq +
                0.35f * freq;

            last_freq = shared_freq;

        } else {

            /* weak signal → decay slowly instead of zeroing */
            shared_freq *= 0.92f;
        }

        /* ---------------- DEBUG OUTPUT (SAFE) ---------------- */
        static int counter = 0;
        counter++;

        if (counter % 10 == 0) {

            if (shared_freq > 0.5f) {

                printf("\rPitch: %.2f Hz   Confidence: %.4f   ",
                       shared_freq,
                       confidence);

            } else {

                printf("\r(quiet / decaying signal)        ");
            }

            fflush(stdout);
        }
    }
}