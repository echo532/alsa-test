#ifndef DETECTOR_H
#define DETECTOR_H

#include "audio_config.h"

int is_active(float *x, int N);
float detect_pitch(float *x, int N);

#endif
