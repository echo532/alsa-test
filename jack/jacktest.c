#include <jack/jack.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define SR 48000
#define BUFFER_SIZE 2048
#define HOP_SIZE 256
#define MAX_OSC 32

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// JACK
// ============================================================

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

// ============================================================
// RUN STATE
// ============================================================

volatile int running = 1;

// ============================================================
// ANALYSIS BUFFER (circular)
// ============================================================

float analysis_buffer[BUFFER_SIZE];
int write_pos = 0;
int hop_counter = 0;

// ============================================================
// OSCILLATORS
// ============================================================

typedef struct {
    float freq;
    float target_freq;

    float amp;
    float target_amp;

    float phase;
} Osc;

Osc osc_bank[MAX_OSC];
int osc_count = 0;

float harmony_ratio = 1.5f;

// ============================================================
// UTIL
// ============================================================

float clamp(float x, float a, float b)
{
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

// ============================================================
// RMS
// ============================================================

float compute_rms(float *buf, int size)
{
    float sum = 0.0f;

    for (int i = 0; i < size; i++)
        sum += buf[i] * buf[i];

    return sqrtf(sum / size);
}

// ============================================================
// COPY CIRCULAR BUFFER
// ============================================================

void get_analysis_frame(float *dst)
{
    int idx = write_pos;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        dst[i] = analysis_buffer[(idx + i) % BUFFER_SIZE];
    }
}

// ============================================================
// NORMALIZED AUTOCORRELATION
// ============================================================

float detect_pitch(float *buf, int size)
{
    float best_corr = 0.0f;
    int best_lag = 0;

    int min_lag = SR / 1000;
    int max_lag = SR / 80;

    for (int lag = min_lag; lag < max_lag; lag++) {

        float corr = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;

        for (int i = 0; i < size - lag; i++) {

            float a = buf[i];
            float b = buf[i + lag];

            corr += a * b;
            norm1 += a * a;
            norm2 += b * b;
        }

        float norm = sqrtf(norm1 * norm2);

        if (norm > 0.0f)
            corr /= norm;

        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }
    }

    // reject weak matches
    if (best_corr < 0.35f)
        return 0.0f;

    if (best_lag == 0)
        return 0.0f;

    return (float)SR / best_lag;
}

// ============================================================
// UPDATE OSCILLATORS
// ============================================================

void update_oscillators(float f0, float rms)
{
    // silence gate
    if (rms < 0.01f || f0 <= 0.0f) {

        for (int i = 0; i < osc_count; i++)
            osc_bank[i].target_amp = 0.0f;

        return;
    }

    int count = 0;

    for (int i = 1; i <= 8; i++) {

        float freq = f0 * harmony_ratio * i;

        if (freq > SR * 0.45f)
            break;

        osc_bank[count].target_freq = freq;
        osc_bank[count].target_amp = 0.15f / i;

        count++;
    }

    // fade unused oscillators out
    for (int i = count; i < osc_count; i++)
        osc_bank[i].target_amp = 0.0f;

    osc_count = count;
}

// ============================================================
// ANALYSIS
// ============================================================

void analyze()
{
    float frame[BUFFER_SIZE];

    get_analysis_frame(frame);

    // remove DC
    float mean = 0.0f;

    for (int i = 0; i < BUFFER_SIZE; i++)
        mean += frame[i];

    mean /= BUFFER_SIZE;

    for (int i = 0; i < BUFFER_SIZE; i++)
        frame[i] -= mean;

    float rms = compute_rms(frame, BUFFER_SIZE);

    float f0 = detect_pitch(frame, BUFFER_SIZE);

    update_oscillators(f0, rms);
}

// ============================================================
// SYNTHESIS
// ============================================================

float synth_sample()
{
    float out = 0.0f;

    for (int i = 0; i < osc_count; i++) {

        Osc *o = &osc_bank[i];

        // smooth parameters
        o->freq += 0.001f * (o->target_freq - o->freq);
        o->amp  += 0.001f * (o->target_amp  - o->amp);

        out += sinf(o->phase) * o->amp;

        o->phase += 2.0f * M_PI * o->freq / SR;

        if (o->phase >= 2.0f * M_PI)
            o->phase -= 2.0f * M_PI;
    }

    return clamp(out, -1.0f, 1.0f);
}

// ============================================================
// JACK PROCESS CALLBACK
// ============================================================

int process(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *in =
        jack_port_get_buffer(input_port, nframes);

    jack_default_audio_sample_t *out =
        jack_port_get_buffer(output_port, nframes);

    for (unsigned int i = 0; i < nframes; i++) {

        float sample = in[i];

        // write circular buffer
        analysis_buffer[write_pos] = sample;

        write_pos++;
        write_pos %= BUFFER_SIZE;

        hop_counter++;

        if (hop_counter >= HOP_SIZE) {
            analyze();
            hop_counter = 0;
        }

        float wet = synth_sample();

        // dry/wet mix
        out[i] = sample * 0.7f + wet * 0.5f;
    }

    return 0;
}

// ============================================================
// CLEAN SHUTDOWN
// ============================================================

void signal_handler(int sig)
{
    running = 0;
}

void jack_shutdown(void *arg)
{
    running = 0;
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char *argv[])
{
    if (argc > 1)
        harmony_ratio = atof(argv[1]);

    printf("Harmony ratio: %.2f\n", harmony_ratio);

    signal(SIGINT, signal_handler);

    client = jack_client_open(
        "harmonizer",
        JackNullOption,
        NULL);

    if (!client) {
        fprintf(stderr, "Failed to connect to JACK\n");
        return 1;
    }

    jack_set_process_callback(client, process, NULL);
    jack_on_shutdown(client, jack_shutdown, NULL);

    input_port = jack_port_register(
        client,
        "input",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0);

    output_port = jack_port_register(
        client,
        "output",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0);

    if (!input_port || !output_port) {
        fprintf(stderr, "Failed to create ports\n");
        return 1;
    }

    if (jack_activate(client)) {
        fprintf(stderr, "Cannot activate JACK client\n");
        return 1;
    }

    printf("Running. Press CTRL+C to quit.\n");

    while (running)
        sleep(1);

    printf("Shutting down...\n");

    jack_client_close(client);

    return 0;
}