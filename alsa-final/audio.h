#ifndef AUDIO_H
#define AUDIO_H

#include <alsa/asoundlib.h>

int audio_init(snd_pcm_t **capture);
int audio_read(snd_pcm_t *capture, int16_t *buf);

#endif
