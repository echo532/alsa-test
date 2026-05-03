// detector.c
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define RATE 48000
#define MIN_FREQ 80
#define MAX_FREQ 1200

// simple autocorrelation pitch detection
float detect_pitch(float *x, int N) {
    int min_lag = RATE / MAX_FREQ;
    int max_lag = RATE / MIN_FREQ;

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

// simple adaptive gate
int is_active(float *x, int N) {
    float rms = 0;

    for (int i = 0; i < N; i++)
        rms += x[i] * x[i];

    rms = sqrtf(rms / N);

    return rms > 0.003f;
}
