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

jack_client_t *client;
jack_port_t *input_port;
jack_port_t *output_port;

// ============================================================
// RUN STATE
// ============================================================

volatile sig_atomic_t running = 1;

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
// UTILS
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

    return sqrtf(sum / (float)size);
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
// PITCH DETECTION
// ============================================================

float detect_pitch(float *buf, int size)
{
    float best_corr = 0.0f;
    int best_lag = 0;

    int min_lag = SR / 1000; // 1000 Hz
    int max_lag = SR / 80;   // 80 Hz

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

    // reject weak detections
    if (best_corr < 0.35f)
        return 0.0f;

    if (best_lag == 0)
        return 0.0f;

    return (float)SR / (float)best_lag;
}

// ============================================================
// OSCILLATOR UPDATE
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
        osc_bank[count].target_amp = 0.15f / (float)i;

        count++;
    }

    // fade out unused oscillators
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

    // remove DC offset
    float mean = 0.0f;

    for (int i = 0; i < BUFFER_SIZE; i++)
        mean += frame[i];

    mean /= (float)BUFFER_SIZE;

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

        // smooth frequency
        o->freq += 0.001f * (o->target_freq - o->freq);

        // smooth amplitude
        o->amp += 0.001f * (o->target_amp - o->amp);

        out += sinf(o->phase) * o->amp;

        o->phase += 2.0f * M_PI * o->freq / (float)SR;

        if (o->phase >= 2.0f * M_PI)
            o->phase -= 2.0f * M_PI;
    }

    return clamp(out, -1.0f, 1.0f);
}

// ============================================================
// JACK CALLBACK
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
// SHUTDOWN
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

    printf("====================================\n");
    printf("Simple JACK Harmonizer\n");
    printf("Harmony Ratio: %.2f\n", harmony_ratio);
    printf("CTRL+C to stop\n");
    printf("====================================\n");

    // signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // create JACK client
    client = jack_client_open(
        "harmonizer",
        JackNullOption,
        NULL);

    if (!client) {
        fprintf(stderr, "Failed to connect to JACK\n");
        return 1;
    }

    // callbacks
    jack_set_process_callback(client, process, NULL);
    jack_on_shutdown(client, jack_shutdown, NULL);

    // ports
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

        fprintf(stderr, "Failed to register JACK ports\n");

        jack_client_close(client);

        return 1;
    }

    // activate JACK
    if (jack_activate(client)) {

        fprintf(stderr, "Failed to activate JACK client\n");

        jack_client_close(client);

        return 1;
    }

    printf("Running...\n");

    // responsive loop
    while (running) {
        usleep(10000);
    }

    printf("\nStopping...\n");

    // stop processing
    jack_deactivate(client);

    // disconnect input connections
    const char **connections;

    connections = jack_port_get_connections(input_port);

    if (connections) {

        for (int i = 0; connections[i]; i++) {

            jack_disconnect(
                client,
                connections[i],
                jack_port_name(input_port));
        }

        free(connections);
    }

    // disconnect output connections
    connections = jack_port_get_connections(output_port);

    if (connections) {

        for (int i = 0; connections[i]; i++) {

            jack_disconnect(
                client,
                jack_port_name(output_port),
                connections[i]);
        }

        free(connections);
    }

    // close JACK
    jack_client_close(client);

    printf("Exited cleanly.\n");

    return 0;
}