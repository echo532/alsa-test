#ifndef PITCH_H
#define PITCH_H

float detect_pitch(float *input);

int freq_to_midi(float freq);

const char* midi_to_note(int midi);

#endif
