#include "ringbuffer.h"
#include <stdlib.h>

void rb_init(ringbuffer_t *rb, int size) {
    rb->buf = calloc(size, sizeof(float));
    rb->size = size;
    rb->w = rb->r = 0;
}

void rb_write(ringbuffer_t *rb, float v) {
    rb->buf[rb->w] = v;
    rb->w = (rb->w + 1) % rb->size;
}

int rb_read(ringbuffer_t *rb, float *out, int n) {
    for (int i = 0; i < n; i++) {
        if (rb->r == rb->w)
            return i;

        out[i] = rb->buf[rb->r];
        rb->r = (rb->r + 1) % rb->size;
    }
    return n;
}