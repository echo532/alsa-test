// ============================================================
// Realtime Sinusoidal Harmonizer (JACK + FFTW)
// ============================================================

#include <jack/jack.h>
#include <fftw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SAMPLE_RATE 48000
#define FFT_SIZE 1024
#define ANALYSIS_HOP 256
#define MAX_PARTIALS 32
#define MAG_THRESHOLD 5.0f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// DATA STRUCTURES
// ============================================================

typedef struct {
    float freq;
    float amp;
    float phase;
} Partial;

// ============================================================
// JACK
// ============================================================

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

// ============================================================
// BUFFERS
// ============================================================

float analysis_buffer[FFT_SIZE];
int analysis_pos = 0;

float fft_in[FFT_SIZE];
fftwf_complex fft_out[FFT_SIZE / 2 + 1];
fftwf_plan fft_plan;

float window[FFT_SIZE];

// ============================================================
// HARMONY MODEL
// ============================================================

Partial harmony_partials[MAX_PARTIALS];
int harmony_count = 0;

// Change THIS live:
// 1.0 = unison
// 1.25 = major third
// 1.5 = perfect fifth
// 2.0 = octave

volatile float harmony_ratio = 1.5f;

// ============================================================
// WINDOW FUNCTION
// ============================================================

void build_window() {
    for (int i = 0; i < FFT_SIZE; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }
}

// ============================================================
// FFT ANALYSIS
// ============================================================

void analyze() {
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_in[i] = analysis_buffer[i] * window[i];
    }

    fftwf_execute(fft_plan);

    harmony_count = 0;

    for (int i = 1; i < FFT_SIZE / 2 && harmony_count < MAX_PARTIALS; i++) {

        float re = fft_out[i][0];
        float im = fft_out[i][1];

        float mag = sqrtf(re * re + im * im);

        if (mag > MAG_THRESHOLD) {

            float freq = (float)i * SAMPLE_RATE / FFT_SIZE;

            harmony_partials[harmony_count].freq = freq * harmony_ratio;
            harmony_partials[harmony_count].amp = mag / FFT_SIZE;
            harmony_partials[harmony_count].phase = 0.0f;

            harmony_count++;
        }
    }
}

// ============================================================
// SYNTHESIS
// ============================================================

float synthesize() {
    float out = 0.0f;

    for (int i = 0; i < harmony_count; i++) {

        Partial *p = &harmony_partials[i];

        out += p->amp * sinf(p->phase);

        p->phase += 2.0f * M_PI * p->freq / SAMPLE_RATE;

        if (p->phase > 2.0f * M_PI)
            p->phase -= 2.0f * M_PI;
    }

    return out * 0.1f; // prevent clipping
}

// ============================================================
// JACK CALLBACK
// ============================================================

int process(jack_nframes_t nframes, void *arg) {

    jack_default_audio_sample_t *in =
        jack_port_get_buffer(input_port, nframes);

    jack_default_audio_sample_t *out =
        jack_port_get_buffer(output_port, nframes);

    static int hop = 0;

    for (int i = 0; i < nframes; i++) {

        // store analysis buffer
        analysis_buffer[analysis_pos++] = in[i];

        if (analysis_pos >= FFT_SIZE) {
            memmove(
                analysis_buffer,
                analysis_buffer + ANALYSIS_HOP,
                sizeof(float) * (FFT_SIZE - ANALYSIS_HOP));

            analysis_pos = FFT_SIZE - ANALYSIS_HOP;
        }

        hop++;

        if (hop >= ANALYSIS_HOP) {
            analyze();
            hop = 0;
        }

        float harmony = synthesize();

        out[i] = in[i] * 0.7f + harmony * 0.7f;
    }

    return 0;
}

// ============================================================
// JACK SHUTDOWN
// ============================================================

void shutdown_cb(void *arg) {
    exit(1);
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char *argv[]) {

    if (argc > 1) {
        harmony_ratio = atof(argv[1]);
    }

    printf("Harmony ratio: %f\n", harmony_ratio);

    build_window();

    fft_plan = fftwf_plan_dft_r2c_1d(
        FFT_SIZE,
        fft_in,
        fft_out,
        FFTW_MEASURE);

    client = jack_client_open(
        "harmonizer",
        JackNullOption,
        NULL);

    if (!client) {
        fprintf(stderr, "Failed JACK\n");
        return 1;
    }

    jack_set_process_callback(client, process, 0);
    jack_on_shutdown(client, shutdown_cb, 0);

    input_port = jack_port_register(
        client,
        "in",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0);

    output_port = jack_port_register(
        client,
        "out",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0);

    jack_activate(client);

    printf("Running...\n");

    while (1) sleep(1);

    return 0;
}