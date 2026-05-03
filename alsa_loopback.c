#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 48000
#define CHANNELS 2
#define FORMAT SND_PCM_FORMAT_S16_LE
#define PERIOD_FRAMES 128

int main() {
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *params;
    int rc;

    snd_pcm_uframes_t frames = PERIOD_FRAMES;

    int buffer_size = PERIOD_FRAMES * CHANNELS * 2; // 16-bit = 2 bytes/sample
    char *buffer = malloc(buffer_size);

    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 1;
    }

    // ---------------- OPEN DEVICES ----------------

    rc = snd_pcm_open(&capture_handle, "plughw:2,0",
                      SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "Capture open error: %s\n", snd_strerror(rc));
        return 1;
    }

    rc = snd_pcm_open(&playback_handle, "plughw:1,0",
                      SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "Playback open error: %s\n", snd_strerror(rc));
        return 1;
    }

    snd_pcm_hw_params_alloca(&params);

    // ---------------- CONFIGURE CAPTURE ----------------

    snd_pcm_hw_params_any(capture_handle, params);
    snd_pcm_hw_params_set_access(capture_handle, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, params, FORMAT);
    snd_pcm_hw_params_set_channels(capture_handle, params, CHANNELS);
    snd_pcm_hw_params_set_rate(capture_handle, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(capture_handle, params, frames, 0);

    rc = snd_pcm_hw_params(capture_handle, params);
    if (rc < 0) {
        fprintf(stderr, "Capture hw_params error: %s\n", snd_strerror(rc));
        return 1;
    }

    // ---------------- CONFIGURE PLAYBACK ----------------

    snd_pcm_hw_params_any(playback_handle, params);
    snd_pcm_hw_params_set_access(playback_handle, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, params, FORMAT);
    snd_pcm_hw_params_set_channels(playback_handle, params, CHANNELS);
    snd_pcm_hw_params_set_rate(playback_handle, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(playback_handle, params, frames, 0);

    rc = snd_pcm_hw_params(playback_handle, params);
    if (rc < 0) {
        fprintf(stderr, "Playback hw_params error: %s\n", snd_strerror(rc));
        return 1;
    }

    snd_pcm_prepare(capture_handle);
    snd_pcm_prepare(playback_handle);

    printf("Running full-duplex audio (plughw mode)...\n");

    // ---------------- MAIN LOOP ----------------

    while (1) {
        rc = snd_pcm_readi(capture_handle, buffer, frames);

        if (rc == -EPIPE) {
            snd_pcm_prepare(capture_handle);
            continue;
        } else if (rc < 0) {
            fprintf(stderr, "Read error: %s\n", snd_strerror(rc));
            break;
        }

        int frames_to_write = rc;
        int offset = 0;

        while (frames_to_write > 0) {
            rc = snd_pcm_writei(playback_handle,
                               buffer + offset * CHANNELS * 2,
                               frames_to_write);

            if (rc == -EPIPE) {
                snd_pcm_prepare(playback_handle);
            } else if (rc < 0) {
                fprintf(stderr, "Write error: %s\n", snd_strerror(rc));
                break;
            } else {
                frames_to_write -= rc;
                offset += rc;
            }
        }
    }

    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    free(buffer);

    return 0;
}
