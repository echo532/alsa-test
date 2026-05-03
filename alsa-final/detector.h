#ifndef DETECTOR_H
#define DETECTOR_H

#define RATE 48000
#define MIN_FREQ 80
#define MAX_FREQ 1200

int is_active(float *x, int N);
float detect_pitch(float *x, int N);

#endif
