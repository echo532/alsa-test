#include "config.h"
#include "ringbuffer.h"

#include <fftw3.h>
#include <math.h>
#include <pthread.h>

extern ringbuffer_t audio_rb;

float shared_freq = 0.0f;

static float in[FRAME_SIZE];
static fftwf_complex out[FRAME_SIZE];

void *pitch_thread(void *arg) {

    fftwf_plan plan =
        fftwf_plan_dft_r2c_1d(
            FRAME_SIZE,
            in,
            out,
            FFTW_MEASURE);

    while (1) {

        rb_read_block(&audio_rb, in, FRAME_SIZE);

        // window
        for (int i = 0; i < FRAME_SIZE; i++) {
            float w = 0.5f * (1 - cosf(2*M_PI*i/(FRAME_SIZE-1)));
            in[i] *= w;
        }

        fftwf_execute(plan);

        float best = 0;
        int best_i = 0;

        for (int i = 1; i < FRAME_SIZE/2; i++) {

            float re = out[i][0];
            float im = out[i][1];

            float mag = re*re + im*im;

            if (mag > best) {
                best = mag;
                best_i = i;
            }
        }

        float freq = best_i * RATE / FRAME_SIZE;

        if (freq > MIN_FREQ && freq < MAX_FREQ)
            shared_freq = freq;
    }
}