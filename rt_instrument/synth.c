#include "synth.h"
#include "config.h"

#include <alsa/asoundlib.h>
#include <math.h>
#include <stdio.h>

extern float shared_freq;

/* ---------------- ALSA ---------------- */
static snd_pcm_t *playback = NULL;

/* ---------------- synthesis state ---------------- */
static float phase = 0.0f;

/* ---------------- init ---------------- */
static void init_audio() {

    snd_pcm_open(&playback,
                 "plughw:1,0",
                 SND_PCM_STREAM_PLAYBACK,
                 0);

    snd_pcm_set_params(playback,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        1,
        RATE,
        1,
        20000);
}

/* ---------------- synth thread ---------------- */
void *synth_thread(void *arg) {

    if (!playback)
        init_audio();

    int16_t buffer[FRAME_SIZE];

    while (1) {

        float freq = shared_freq;

        /* ---------------- SILENCE SAFETY ---------------- */
        if (freq <= 0.0f) {

            for (int i = 0; i < FRAME_SIZE; i++)
                buffer[i] = 0;

            snd_pcm_writei(playback,
                           buffer,
                           FRAME_SIZE);

            continue;
        }

        /* ---------------- OSCILLATOR ---------------- */
        float inc =
            2.0f * M_PI * freq / RATE;

        for (int i = 0; i < FRAME_SIZE; i++) {

            /* simple sine wave synth */
            buffer[i] =
                (int16_t)(sinf(phase) * 8000);

            phase += inc;

            if (phase > 2.0f * M_PI)
                phase -= 2.0f * M_PI;
        }

        /* ---------------- OUTPUT ---------------- */
        int err =
            snd_pcm_writei(playback,
                           buffer,
                           FRAME_SIZE);

        /* ---------------- XRUN RECOVERY ---------------- */
        if (err == -EPIPE) {
            snd_pcm_prepare(playback);
        }

        if (err < 0 && err != -EPIPE) {
            snd_pcm_recover(playback, err, 1);
        }
    }
}