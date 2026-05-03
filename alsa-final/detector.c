#include "detector.h"
#include <math.h>

int is_active(float *x, int N) {
    float rms = 0;

    for (int i = 0; i < N; i++)
        rms += x[i] * x[i];

    rms = sqrtf(rms / N);

    return rms > 0.003f;
}

float detect_pitch(float *x, int N) {
    int min_lag = RATE / MAX_FREQ;
    int max_lag = RATE / MIN_FREQ;

    float best = 0;
    int best_lag = 0;

    for (int lag = min_lag; lag < max_lag; lag++) {
        float sum = 0;

        for (int i = 0; i < N - lag; i++)
            sum += x[i] * x[i + lag];

        if (sum > best) {
            best = sum;
            best_lag = lag;
        }
    }

    if (best_lag == 0)
        return -1;

    return (float)RATE / best_lag;
}
