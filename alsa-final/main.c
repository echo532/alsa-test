#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include "detector.h"
#include "note_state.h"
#include "synth.h"
#include "audio_config.h"

int main() {

    snd_pcm_t *capture;
    snd_pcm_hw_params_t *params;

    int16_t *buf = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));
    float *mono = malloc(FRAME_SIZE * sizeof(float));
    int16_t *out = malloc(FRAME_SIZE * CHANNELS * sizeof(int16_t));

    snd_pcm_open(&capture, "plughw:2,0",
                 SND_PCM_STREAM_CAPTURE, 0);

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(capture, params);
    snd_pcm_hw_params_set_access(capture, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture, params,
                                 SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(capture, params, CHANNELS);
    snd_pcm_hw_params_set_rate(capture, params, RATE, 0);
    snd_pcm_hw_params_set_period_size(capture, params, FRAME_SIZE, 0);

    snd_pcm_hw_params(capture, params);
    snd_pcm_prepare(capture);

    if (synth_init() < 0) {
        printf("synth init failed\n");
        return 1;
    }

    note_state_t state = {
        .locked_midi = -1,
        .stable_count = 0,
        .index = 0
    };

    int midi;
    float freq;

    printf("Stable note state machine running...\n");

    while (1) {

	// ignore mic completely for now
    int midi = 60; // Middle C
    float freq = 261.63f;

    printf("\rFORCED OUTPUT: MIDI %d  %.2f Hz", midi, freq);
    fflush(stdout);

    synth_render(freq, out);


    }
}
