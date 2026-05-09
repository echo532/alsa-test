#include "audio.h"
#include "audio_config.h"

int audio_init(snd_pcm_t **capture) {

    snd_pcm_hw_params_t *params;

    if (snd_pcm_open(capture,
                     "plughw:2,0",
                     SND_PCM_STREAM_CAPTURE,
                     0) < 0)
        return -1;

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(*capture, params);

    snd_pcm_hw_params_set_access(
        *capture,
        params,
        SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(
        *capture,
        params,
        SND_PCM_FORMAT_S16_LE);

    snd_pcm_hw_params_set_channels(
        *capture,
        params,
        CHANNELS);

    snd_pcm_hw_params_set_rate(
        *capture,
        params,
        RATE,
        0);

    snd_pcm_hw_params_set_period_size(
        *capture,
        params,
        FRAME_SIZE,
        0);

    snd_pcm_hw_params(*capture, params);

    snd_pcm_prepare(*capture);

    return 0;
}

int audio_read(snd_pcm_t *capture, int16_t *buf) {

    int rc = snd_pcm_readi(
        capture,
        buf,
        FRAME_SIZE);

    if (rc < 0) {
        snd_pcm_prepare(capture);
        return -1;
    }

    return 0;
}
