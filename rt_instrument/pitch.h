#ifndef PITCH_H
#define PITCH_H

void *pitch_thread(void *arg);

/* shared state between threads */
extern float shared_freq;

#endif