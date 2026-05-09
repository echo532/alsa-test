#include "pitch.h"
#include "config.h"
#include "ringbuffer.h"

#include <math.h>
#include <string.h>

extern ringbuffer_t audio_rb;
extern float shared_freq;

/* analysis buffers */
static float buffer[FRAME_SIZE];
static float real[FRAME_SIZE];
static float imag[FRAME_SIZE];

#define VAD_THRESHOLD 0.01f

/* ---------------------------------------
   VERY SIMPLE DFT-STYLE FFT (no libs)
   (fast enough at FRAME_SIZE=256)
   --------------------------------------- */
static void compute_spectrum() {

    memset(real, 0, sizeof(real));
    memset(imag, 0, sizeof(imag));

    for (int k = 0; k < FRAME_SIZE / 2; k++) {

        float sum_re = 0.0f;
        float sum_im = 0.0f;

        for (int n = 0; n < FRAME_SIZE; n++) {

            float phase =
                2.0f * M_PI * k * n / FRAME_SIZE;

            sum_re += buffer[n] * cosf(phase);
            sum_im -= buffer[n] * sinf(phase);
        }

        real[k] = sum_re;
        imag[k] = sum_im;
    }
}

void *pitch_thread(void *arg) {

    while (1) {

        rb_read_block(&audio_rb,
                      buffer,
                      FRAME_SIZE);

        /* -------------------------
           1. VAD (energy gate)
           ------------------------- */
        float energy = 0.0f;

        for (int i = 0; i < FRAME_SIZE; i++) {
            energy += buffer[i] * buffer[i];
        }

        energy = sqrtf(energy / FRAME_SIZE);

        if (energy < VAD_THRESHOLD) {
            shared_freq = 0.0f;
            continue;
        }

        /* -------------------------
           2. FFT / DFT magnitude
           ------------------------- */
        compute_spectrum();

        float best_mag = 0.0f;
        int best_bin = 0;

        for (int k = 1; k < FRAME_SIZE / 2; k++) {

            float mag =
                real[k] * real[k] +
                imag[k] * imag[k];

            if (mag > best_mag) {
                best_mag = mag;
                best_bin = k;
            }
        }

        /* -------------------------
           3. BIN → FREQUENCY
           ------------------------- */
        float freq =
            (float)best_bin * RATE / FRAME_SIZE;

        /* -------------------------
           4. VALIDATION
           ------------------------- */
        if (freq > MIN_FREQ &&
            freq < MAX_FREQ) {

            shared_freq = freq;
        } else {
            shared_freq = 0.0f;
        }
    }
}