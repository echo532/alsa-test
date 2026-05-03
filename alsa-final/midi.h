#ifndef MIDI_H
#define MIDI_H

#define HISTORY_SIZE 5

int freq_to_midi(float f);
float midi_to_freq(int m);

int update_stable_midi(int midi);

#endif
