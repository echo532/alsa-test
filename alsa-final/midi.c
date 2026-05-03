#include "midi.h"
#include <math.h>

static int history[HISTORY_SIZE] = {0};
static int hist_index = 0;

static int locked_midi = -1;
static int stable_count = 0;

// convert Hz → MIDI
int freq_to_midi(float f) {
    return (int)roundf(69 + 12 * log2f(f / 440.0f));
}

// MIDI → Hz
float midi_to_freq(int m) {
    return 440.0f * powf(2.0f, (m - 69) / 12.0f);
}

// majority vote (stability core)
static int majority_note() {
    int best = -1;
    int best_count = 0;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        int count = 0;

        for (int j = 0; j < HISTORY_SIZE; j++) {
            if (history[j] == history[i])
                count++;
        }

        if (count > best_count) {
            best_count = count;
            best = history[i];
        }
    }

    return best;
}

// compute cents deviation
static float cents(float freq, float ref) {
    return 1200.0f * log2f(freq / ref);
}

// MAIN STABLE UPDATE FUNCTION
int update_stable_midi(float freq, int *out_midi, float *out_cents) {

    if (freq < 50.0f || freq > 2000.0f)
        return 0;

    int midi = freq_to_midi(freq);

    // store history
    history[hist_index] = midi;
    hist_index = (hist_index + 1) % HISTORY_SIZE;

    int candidate = majority_note();

    // hysteresis lock
    if (candidate == locked_midi) {
        stable_count = 0;
    } else {
        stable_count++;

        if (stable_count >= 3) {
            locked_midi = candidate;
            stable_count = 0;
        }
    }

    if (locked_midi < 0)
        return 0;

    float ref = midi_to_freq(locked_midi);

    *out_midi = locked_midi;
    *out_cents = cents(freq, ref);

    return 1;
}
