#include <stdio.h>
#include <stdlib.h>

#include <alsa/asoundlib.h>

#include "audio.h"
#include "pitch.h"
#include "audio_config.h"

int main() {

    snd_pcm_t *capture;

    if (audio_init(&capture) < 0) {
        printf("audio init failed\n");
        return 1;
    }

    int16_t *buf =
        malloc(sizeof(int16_t) *
               FRAME_SIZE *
               CHANNELS);

    float *mono =
        malloc(sizeof(float) *
               FRAME_SIZE);

    printf("Pitch tracker running...\n");

    while (1) {

        if (audio_read(capture, buf) < 0)
            continue;

        // stereo -> mono
        for (int i = 0; i < FRAME_SIZE; i++) {

            mono[i] =
                buf[i * 2] / 32768.0f;
        }

        float freq = detect_pitch(mono);

        if (freq < 0)
            continue;

        printf("\r%.2f Hz      ", freq);

        fflush(stdout);
    }

    return 0;
}
