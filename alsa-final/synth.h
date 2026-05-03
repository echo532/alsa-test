#ifndef SYNTH_H
#define SYNTH_H

#include <alsa/asoundlib.h>

#define FRAME_SIZE 256

int synth_init(void);
void synth_render(float freq, int16_t *out);

#endif
