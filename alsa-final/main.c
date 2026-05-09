#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
        printf("memory alloc failed\n");
        return 1;
    }

    float current_freq = 0.0f;

    // for reducing terminal spam / jitter
    float last_printed_freq = -1.0f;

    printf("running...\n");

    while (1) {

        if (audio_read(capture, buf) < 0)
            continue;

        for (int i = 0; i < FRAME_SIZE; i++) {
            mono[i] =
                buf[i * 2] / 32768.0f;
        }

        float freq =
            detect_pitch(mono);

        // update stable pitch memory
        if (freq > 0)
            current_freq = freq;

        // only process valid pitch
        if (current_freq <= 0)
            continue;

        int midi =
            freq_to_midi(current_freq);

        const char *note =
            midi_to_note(midi);

        // print only if stable enough change (reduces jitter spam)
        if (fabsf(current_freq - last_printed_freq) > 0.5f) {

            printf(
                "\r%s  MIDI:%d  %.2f Hz        ",
                note,
                midi,
                current_freq);

            fflush(stdout);

            last_printed_freq = current_freq;
        }

        // synth output (unchanged, stable)
        if (playback_mode)
            synth_play(current_freq);
    }

    return 0;
}