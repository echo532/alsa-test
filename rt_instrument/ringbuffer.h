#ifndef RINGBUFFER_H
#define RINGBUFFER_H

typedef struct {
    float *buffer;
    int size;
    int write_idx;
    int read_idx;
} ringbuffer_t;

void rb_init(ringbuffer_t *rb, int size);
void rb_write(ringbuffer_t *rb, float v);
int rb_read_block(ringbuffer_t *rb, float *out, int n);

#endif