#include "ringbuffer.h"
#include <stdlib.h>
#include <string.h>

void rb_init(ringbuffer_t *rb, int size) {
    rb->buffer = calloc(size, sizeof(float));
    rb->size = size;
    rb->write_idx = 0;
    rb->read_idx = 0;
}

void rb_write(ringbuffer_t *rb, float v) {
    rb->buffer[rb->write_idx] = v;
    rb->write_idx = (rb->write_idx + 1) % rb->size;
}

int rb_read_block(ringbuffer_t *rb, float *out, int n) {

    for (int i = 0; i < n; i++) {

        if (rb->read_idx == rb->write_idx)
            return i;

        out[i] = rb->buffer[rb->read_idx];
        rb->read_idx = (rb->read_idx + 1) % rb->size;
    }

    return n;
}