#include <stdio.h>
#include <stdlib.h>

#include <alsa/asoundlib.h>

#include "audio.h"
#include "pitch.h"
#include "synth.h"
#include "audio_config.h"

int main(int argc, char **argv) {

    int playback_mode = 0;

    if (argc > 1)
        playback_mode = atoi(argv[1]);

    snd_pcm_t *capture;

    if (audio_init(&capture) < 0) {
        printf("audio init failed\n");
        return 1;
    }

    if (playback_mode) {

        if (synth_init() < 0) {
            printf("synth init failed\n");
            return 1;
        }
    }

    int16_t *buf =
        malloc(sizeof(int16_t) *
               FRAME_SIZE *
               CHANNELS);

    float *mono =
        malloc(sizeof(float) *
               FRAME_SIZE);

    if (!buf || !mono) {
        printf("memory allocation failed\n");
        return 1;
    }

    // current active pitch for playback
    float current_freq = 0.0f;

    // allows brief detection dropouts
    int silent_frames = 0;

    // track terminal state
    int displaying = 0;

    printf("Pitch tracker running...\n");

    while (1) {

        if (audio_read(capture, buf) < 0)
            continue;

        // stereo -> mono
        for (int i = 0; i < FRAME_SIZE; i++) {

            mono[i] =
                buf[i * 2] / 32768.0f;
        }

        float freq =
            detect_pitch(mono);

        // valid pitch detected
        if (freq > 0) {

            current_freq = freq;
            silent_frames = 0;

            int midi =
                freq_to_midi(freq);

            const char *note =
                midi_to_note(midi);

            printf(
                "\r%s  %.2f Hz      ",
                note,
                freq);

            fflush(stdout);

            displaying = 1;
        }
        else {

            silent_frames++;

            // after enough silence,
            // stop playback + clear terminal
            if (silent_frames > 20) {

                current_freq = 0.0f;

                if (displaying) {

                    printf(
                        "\r                             \r");

                    fflush(stdout);

                    displaying = 0;
                }
            }
        }

        // continuous playback
        if (playback_mode &&
            current_freq > 0.0f) {

            synth_play(current_freq);
        }
    }

    free(buf);
    free(mono);

    return 0;
}