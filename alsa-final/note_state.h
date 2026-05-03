#ifndef NOTE_STATE_H
#define NOTE_STATE_H

#define HISTORY_SIZE 7

typedef struct {
    int history[HISTORY_SIZE];
    int index;

    int locked_midi;
    int stable_count;

} note_state_t;

int freq_to_midi(float f);
float midi_to_freq(int m);
int get_provisional_midi(float freq);


int update_note_state(note_state_t *s, float freq, int *out_midi, float *out_freq);

#endif
