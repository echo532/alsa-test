#include "pitch.h"
#include "config.h"
#include "ringbuffer.h"

#include <fftw3.h>
#include <math.h>
#include <stdio.h>

extern ringbuffer_t audio_rb;
extern float shared_freq;

/* FFTW */
static fftwf_plan plan;
static float *fft_in;
static fftwf_complex *fft_out;

static int initialized = 0;

/* buffers */
static float mag[FRAME_SIZE / 2];
static float hps[FRAME_SIZE / 2];

/* smoothing */
static float last_freq = 0.0f;

/* debug throttle */
static int print_counter = 0;

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

void *pitch_thread(void *arg) {

    if (!initialized)
        init_fft();

    float input[FRAME_SIZE];

    while (1) {

        rb_read_block(&audio_rb,
                      input,
                      FRAME_SIZE);

        /* ---------------- ENERGY ---------------- */
        float energy = 0.0f;

        for (int i = 0; i < FRAME_SIZE; i++)
            energy += input[i] * input[i];

        energy = sqrtf(energy / FRAME_SIZE);

        /* IMPORTANT: debug visibility even if silent */
        if (energy < 0.005f) {
            shared_freq = 0.0f;
        }

        /* ---------------- FFT ---------------- */
        for (int i = 0; i < FRAME_SIZE; i++)
            fft_in[i] = input[i];

        apply_window(fft_in);

        fftwf_execute(plan);

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

        /* ---------------- SEARCH ---------------- */
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

        float freq = 0.0f;

        if (best > 1000.0f) {

            freq =
                (float)best_bin *
                RATE /
                FRAME_SIZE;

            /* smoothing */
            shared_freq =
                0.6f * last_freq +
                0.4f * freq;

            last_freq = shared_freq;
        } else {
            shared_freq = 0.0f;
        }

        /* ---------------- DEBUG OUTPUT (SAFE) ---------------- */
        print_counter++;

        if (print_counter % 10 == 0) {

            if (shared_freq > 0.0f) {

                printf("\rFreq: %.2f Hz      Energy: %.5f      ",
                       shared_freq,
                       energy);

            } else {

                printf("\rSilence / No pitch detected      ");
            }

            fflush(stdout);
        }
    }
}