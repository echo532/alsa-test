#include <pthread.h>
#include <stdio.h>

#include "config.h"
#include "ringbuffer.h"
#include "audio.h"
#include "pitch.h"
#include "synth.h"

ringbuffer_t audio_rb;
float shared_freq = 0.0f;

int main() {

    rb_init(&audio_rb, RING_SIZE);

    pthread_t t1, t2, t3;

    pthread_create(&t1, NULL, audio_thread, NULL);
    pthread_create(&t2, NULL, pitch_thread, NULL);
    pthread_create(&t3, NULL, synth_thread, NULL);

    pthread_join(t1, NULL);
}