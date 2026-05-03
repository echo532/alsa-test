#include "midi.h"
#include <math.h>

static int history[HISTORY_SIZE] = {0};
static int hist_index = 0;

static int locked_midi = -1;
static int stable_count = 0;

int freq_to_midi(float f) {
    return (int)roundf(69 + 12 * log2f(f / 440.0f));
}

float midi_to_freq(int m) {
    return 440.0f * powf(2.0f, (m - 69) / 12.0f);
}

static int most_common_note() {
    int best_note = -1;
    int best_count = 0;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        int count = 0;

        for (int j = 0; j < HISTORY_SIZE; j++) {
            if (history[j] == history[i])
                count++;
        }

        if (count > best_count) {
            best_count = count;
            best_note = history[i];
        }
    }

    return best_note;
}

int update_stable_midi(int midi) {

    history[hist_index] = midi;
    hist_index = (hist_index + 1) % HISTORY_SIZE;

    int candidate = most_common_note();

    if (candidate == locked_midi) {
        stable_count = 0;
    } else {
        stable_count++;

        if (stable_count >= 3) {
            locked_midi = candidate;
            stable_count = 0;
        }
    }

    return locked_midi;
}
