#include "pitch.h"
#include "config.h"
#include "ringbuffer.h"

#include <math.h>

extern ringbuffer_t audio_rb;

extern float shared_freq;

/* simple autocorrelation-lite detector */
static float buffer[FRAME_SIZE];

void *pitch_thread(void *arg) {

    while (1) {

        rb_read_block(&audio_rb, buffer, FRAME_SIZE);

        int best_lag = 0;
        float best_score = 0;

        for (int lag = 20; lag < FRAME_SIZE / 2; lag++) {

            float sum = 0;

            for (int i = 0; i < FRAME_SIZE - lag; i++) {
                sum += buffer[i] * buffer[i + lag];
            }

            if (sum > best_score) {
                best_score = sum;
                best_lag = lag;
            }
        }

        float freq = RATE / (float)best_lag;

        if (freq > MIN_FREQ && freq < MAX_FREQ)
            shared_freq = freq;
    }
}