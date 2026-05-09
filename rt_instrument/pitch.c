#include "pitch.h"
#include "config.h"
#include "ringbuffer.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

extern ringbuffer_t audio_rb;
extern float shared_freq;

/* buffers */
static float buffer[FRAME_SIZE];
static float real[FRAME_SIZE];
static float imag[FRAME_SIZE];

#define VAD_THRESHOLD 0.01f

/* print throttling */
static float last_print_freq = 0.0f;
static int frame_counter = 0;

/* ---------------- FFT (simple DFT version) ---------------- */
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

        /* ---------------- ENERGY GATE ---------------- */
        float energy = 0.0f;

        for (int i = 0; i < FRAME_SIZE; i++) {
            energy += buffer[i] * buffer[i];
        }

        energy = sqrtf(energy / FRAME_SIZE);

        if (energy < VAD_THRESHOLD) {
            shared_freq = 0.0f;
            continue;
        }

        /* ---------------- FFT ---------------- */
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

        float freq =
            (float)best_bin * RATE / FRAME_SIZE;

        /* ---------------- VALIDATION ---------------- */
        if (freq > MIN_FREQ && freq < MAX_FREQ) {
            shared_freq = freq;
        } else {
            shared_freq = 0.0f;
        }

        /* ---------------- SAFE PRINTING ---------------- */

        frame_counter++;

        /* print ~15 times/sec max (FRAME_SIZE=256 → ~187 fps) */
        if (frame_counter % 12 == 0) {

            if (shared_freq > 0.0f) {

                /* reduce spam: only print if changed meaningfully */
                if (fabsf(shared_freq - last_print_freq) > 1.0f) {

                    printf("\rPitch: %.2f Hz        ",
                           shared_freq);

                    fflush(stdout);

                    last_print_freq = shared_freq;
                }

            } else {
                printf("\rSilence              ");
                fflush(stdout);
            }
        }
    }
}