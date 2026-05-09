#ifndef RINGBUFFER_H
#define RINGBUFFER_H

typedef struct {
    float *buf;
    int size;
    int w;
    int r;
} ringbuffer_t;

void rb_init(ringbuffer_t *rb, int size);
void rb_write(ringbuffer_t *rb, float v);
int rb_read(ringbuffer_t *rb, float *out, int n);

#endif