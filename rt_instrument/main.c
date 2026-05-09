#include <pthread.h>
#include <stdio.h>

#include "ringbuffer.h"
#include "config.h"

ringbuffer_t audio_rb;
float shared_freq = 0.0f;

void *audio_capture_thread(void *);
void *pitch_thread(void *);
void *synth_thread(void *);

int main() {

    rb_init(&audio_rb, RING_SIZE);

    pthread_t t1, t2, t3;

    pthread_create(&t1, NULL, audio_capture_thread, NULL);
    pthread_create(&t2, NULL, pitch_thread, NULL);
    pthread_create(&t3, NULL, synth_thread, NULL);

    pthread_join(t1, NULL);
}