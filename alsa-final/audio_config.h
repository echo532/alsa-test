#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#define RATE 48000
#define CHANNELS 2

// Larger FFT = much better pitch stability
#define FRAME_SIZE 4096

#define MIN_FREQ 80.0f
#define MAX_FREQ 1200.0f

#endif
