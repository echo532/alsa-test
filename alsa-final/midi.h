#ifndef MIDI_H
#define MIDI_H

#define HISTORY_SIZE 7

int freq_to_midi(float f);
float midi_to_freq(int m);

int update_stable_midi(float freq, int *out_midi, float *out_cents);

#endif
