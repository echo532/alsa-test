#include <alsa/asoundlib.h>
#include <stdio.h>

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define FRAMES 256   // smaller = lower latency

int main() {
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *params;
    int rc;
    snd_pcm_uframes_t frames = FRAMES;

    char *buffer;
    int buffer_size = FRAMES * CHANNELS * 2; // 16-bit = 2 bytes

    buffer = (char *)malloc(buffer_size);

    // Open devices
    snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

    // Allocate params
    snd_pcm_hw_params_alloca(&params);

    // Configure both devices
    for (int i = 0; i < 2; i++) {
        snd_pcm_t *handle = (i == 0) ? capture_handle : playback_handle;

        snd_pcm_hw_params_any(handle, params);
        snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
        snd_pcm_hw_params_set_rate(handle, params, SAMPLE_RATE, 0);
        snd_pcm_hw_params_set_period_size(handle, params, FRAMES, 0);

        snd_pcm_hw_params(handle, params);
    }

    // Main loop
    while (1) {
        rc = snd_pcm_readi(capture_handle, buffer, frames);
        if (rc == -EPIPE) {
            snd_pcm_prepare(capture_handle);
            continue;
        }

        rc = snd_pcm_writei(playback_handle, buffer, frames);
        if (rc == -EPIPE) {
            snd_pcm_prepare(playback_handle);
        }
    }

    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    free(buffer);

    return 0;
}
