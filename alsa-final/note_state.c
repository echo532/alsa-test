#include "note_state.h"
#include <math.h>

int freq_to_midi(float f) {
    return (int)roundf(69 + 12 * log2f(f / 440.0f));
}

float midi_to_freq(int m) {
    return 440.0f * powf(2.0f, (m - 69) / 12.0f);
}

static int majority(int *h) {
    int best = -1;
    int best_count = 0;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        int count = 0;

        for (int j = 0; j < HISTORY_SIZE; j++) {
            if (h[j] == h[i])
                count++;
        }

        if (count > best_count) {
            best_count = count;
            best = h[i];
        }
    }

    return best;
}

int update_note_state(note_state_t *s, float freq, int *out_midi, float *out_freq) {

    int midi = freq_to_midi(freq);

    // store history
    s->history[s->index] = midi;
    s->index = (s->index + 1) % HISTORY_SIZE;

    int candidate = majority(s->history);

    // hysteresis lock
    if (candidate == s->locked_midi) {
        s->stable_count = 0;
    } else {
        s->stable_count++;

        if (s->stable_count >= 1) {
            s->locked_midi = candidate;
            s->stable_count = 0;
        }
    }

    if (s->locked_midi < 0)
        return 0;

    *out_midi = s->locked_midi;
    *out_freq = midi_to_freq(s->locked_midi);

    return 1;
}
