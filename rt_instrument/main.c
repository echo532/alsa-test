#include <pthread.h>

#include "config.h"
#include "ringbuffer.h"
#include "audio.h"
#include "pitch.h"

ringbuffer_t audio_rb;

/* IMPORTANT: only ONE definition */
float shared_freq = 0.0f;

int main() {

    rb_init(&audio_rb, FRAME_SIZE * 32);

    pthread_t t1, t2;

    pthread_create(&t1, NULL, audio_thread, NULL);
    pthread_create(&t2, NULL, pitch_thread, NULL);

    pthread_join(t1, NULL);
}