#include <jack/jack.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SR 48000
#define BUFFER_SIZE 2048
#define HOP_SIZE 256
#define MAX_OSC 64

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// JACK
// ============================================================

jack_port_t *in_port;
jack_port_t *out_port;
jack_client_t *client;

// ============================================================
// AUDIO BUFFERS
// ============================================================

float buffer[BUFFER_SIZE];
int buffer_pos = 0;

// ============================================================
// HARMONIC MODEL
// ============================================================

typedef struct {
    float freq;
    float amp;
    float phase;
} Osc;

Osc osc_bank[MAX_OSC];
int osc_count = 0;

volatile float harmony_ratio = 1.5f;

// ============================================================
// AUTOCORRELATION PITCH DETECTION
// ============================================================

float detect_pitch(float *buf, int size)
{
    int best_lag = 0;
    float best_corr = 0.0f;

    int min_lag = SR / 1000;   // 1000 Hz max
    int max_lag = SR / 50;     // 50 Hz min

    for (int lag = min_lag; lag < max_lag; lag++) {

        float corr = 0.0f;

        for (int i = 0; i < size - lag; i++) {
            corr += buf[i] * buf[i + lag];
        }

        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }
    }

    if (best_lag == 0) return 0.0f;

    return (float)SR / best_lag;
}

// ============================================================
// OSCILLATOR UPDATE
// ============================================================

void update_oscillators(float f0)
{
    osc_count = 0;

    if (f0 <= 0.0f) return;

    // generate harmonic series
    for (int i = 1; i < MAX_OSC; i++) {

        float freq = f0 * i * harmony_ratio;

        if (freq > SR * 0.5f)
            break;

        osc_bank[osc_count].freq = freq;
        osc_bank[osc_count].amp = 1.0f / i;
        osc_bank[osc_count].phase = 0.0f;

        osc_count++;
    }
}

// ============================================================
// SYNTHESIS
// ============================================================

float synth()
{
    float out = 0.0f;

    for (int i = 0; i < osc_count; i++) {

        Osc *o = &osc_bank[i];

        out += o->amp * sinf(o->phase);

        o->phase += 2.0f * M_PI * o->freq / SR;

        if (o->phase > 2.0f * M_PI)
            o->phase -= 2.0f * M_PI;
    }

    return out * 0.1f;
}

// ============================================================
// ANALYSIS STEP
// ============================================================

void analyze()
{
    float f0 = detect_pitch(buffer, BUFFER_SIZE);

    update_oscillators(f0);

    printf("Detected f0: %.2f Hz | Oscillators: %d\n",
           f0, osc_count);
}

// ============================================================
// JACK CALLBACK
// ============================================================

int process(jack_nframes_t nframes, void *arg)
{
    float *in = jack_port_get_buffer(in_port, nframes);
    float *out = jack_port_get_buffer(out_port, nframes);

    static int hop = 0;

    for (int i = 0; i < nframes; i++) {

        buffer[buffer_pos++] = in[i];

        if (buffer_pos >= BUFFER_SIZE) {
            memmove(buffer,
                    buffer + HOP_SIZE,
                    sizeof(float) * (BUFFER_SIZE - HOP_SIZE));

            buffer_pos = BUFFER_SIZE - HOP_SIZE;
        }

        hop++;

        if (hop >= HOP_SIZE) {
            analyze();
            hop = 0;
        }

        out[i] = in[i] * 0.5f + synth() * 0.8f;
    }

    return 0;
}

// ============================================================
// JACK SETUP
// ============================================================

void shutdown_cb(void *arg)
{
    exit(1);
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char *argv[])
{
    if (argc > 1)
        harmony_ratio = atof(argv[1]);

    printf("Harmony ratio: %.2f\n", harmony_ratio);

    client = jack_client_open("harmonizer", 0, NULL);

    if (!client) {
        fprintf(stderr, "Failed JACK\n");
        return 1;
    }

    jack_set_process_callback(client, process, NULL);
    jack_on_shutdown(client, shutdown_cb, NULL);

    in_port = jack_port_register(client,
                                 "in",
                                 JACK_DEFAULT_AUDIO_TYPE,
                                 JackPortIsInput, 0);

    out_port = jack_port_register(client,
                                  "out",
                                  JACK_DEFAULT_AUDIO_TYPE,
                                  JackPortIsOutput, 0);

    jack_activate(client);

    printf("Running...\n");

    while (1)
        sleep(1);

    return 0;
}