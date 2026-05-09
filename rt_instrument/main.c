#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "config.h"
#include "ringbuffer.h"
#include "audio.h"
#include "pitch.h"
#include "synth.h"

/*
    Shared state lives in other compilation units.
    We declare it here properly.
*/
extern ringbuffer_t audio_rb;
extern float shared_freq;

int main() {

    rb_init(&audio_rb, RING_SIZE);

    pthread_t t_audio;
    pthread_t t_pitch;
    pthread_t t_synth;

    printf("starting real-time instrument...\n");

    // IMPORTANT: correct thread signatures
    pthread_create(&t_audio, NULL,
        audio_capture_thread, NULL);

    pthread_create(&t_pitch, NULL,
        pitch_thread, NULL);

    pthread_create(&t_synth, NULL,
        synth_thread, NULL);

    pthread_join(t_audio, NULL);
    pthread_join(t_pitch, NULL);
    pthread_join(t_synth, NULL);

    return 0;
}