#include "audio.h"
#include "config.h"
#include "ringbuffer.h"

#include <alsa/asoundlib.h>
#include <stdio.h>

extern ringbuffer_t audio_rb;

void *audio_capture_thread(void *arg) {

    snd_pcm_t *capture;

    snd_pcm_open(&capture,
                 "plughw:2,0",
                 SND_PCM_STREAM_CAPTURE,
                 0);

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(capture, params);

    snd_pcm_hw_params_set_access(capture, params,
        SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(capture, params,
        SND_PCM_FORMAT_S16_LE);

    snd_pcm_hw_params_set_channels(capture, params, 1);

    snd_pcm_hw_params_set_rate(capture, params, RATE, 0);

    snd_pcm_set_params(capture,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        1,
        RATE,
        1,
        20000);

    int16_t buf[FRAME_SIZE];

    while (1) {

        snd_pcm_readi(capture, buf, FRAME_SIZE);

        for (int i = 0; i < FRAME_SIZE; i++) {

            float v = buf[i] / 32768.0f;

            rb_write(&audio_rb, v);
        }
    }
}