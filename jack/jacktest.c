#include <jack/jack.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SR 48000

// TEMPORARY FIXES:
// smaller buffer + slower analysis

#define BUFFER_SIZE 1024
#define HOP_SIZE 1024

#define MAX_OSC 16

#define YIN_THRESHOLD 0.12f

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

volatile int running = 1;

// ============================================================
// ANALYSIS BUFFER
// ============================================================

float analysis_buffer[BUFFER_SIZE];

int write_pos = 0;
int hop_counter = 0;

// worker thread synchronization

pthread_t analysis_thread;

pthread_mutex_t analysis_mutex =
    PTHREAD_MUTEX_INITIALIZER;

volatile int analysis_ready = 0;

// ============================================================
// PITCH STATE
// ============================================================

volatile float detected_f0 = 0.0f;
volatile float detected_rms = 0.0f;

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

// ============================================================
// USER PARAMS
// ============================================================

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

    return sqrtf(sum / (float)size);
}

// ============================================================
// HANN WINDOW
// ============================================================

void apply_hann(float *buf, int size)
{
    for (int i = 0; i < size; i++) {

        float w =
            0.5f *
            (1.0f - cosf(
                (2.0f * M_PI * i) /
                (float)(size - 1)));

        buf[i] *= w;
    }
}

// ============================================================
// YIN
// ============================================================

float detect_pitch_yin(float *buf, int size)
{
    static float diff[BUFFER_SIZE / 2];
    static float cmnd[BUFFER_SIZE / 2];

    int tau_max = BUFFER_SIZE / 2;

    int tau_estimate = -1;

    // TEMPORARY FIX:
    // narrower vocal range

    int min_tau = SR / 500;
    int max_tau = SR / 80;

    // --------------------------------------------------------

    for (int tau = min_tau; tau < max_tau; tau++) {

        diff[tau] = 0.0f;

        for (int i = 0; i < tau_max; i++) {

            float delta =
                buf[i] - buf[i + tau];

            diff[tau] += delta * delta;
        }
    }

    // --------------------------------------------------------

    cmnd[0] = 1.0f;

    float running_sum = 0.0f;

    for (int tau = min_tau; tau < max_tau; tau++) {

        running_sum += diff[tau];

        if (running_sum == 0.0f)
            cmnd[tau] = 1.0f;
        else
            cmnd[tau] =
                diff[tau] *
                tau /
                running_sum;
    }

    // --------------------------------------------------------

    for (int tau = min_tau; tau < max_tau; tau++) {

        if (cmnd[tau] < YIN_THRESHOLD) {

            while (tau + 1 < max_tau &&
                   cmnd[tau + 1] < cmnd[tau]) {

                tau++;
            }

            tau_estimate = tau;

            break;
        }
    }

    if (tau_estimate == -1)
        return 0.0f;

    return (float)SR / (float)tau_estimate;
}

// ============================================================
// ANALYSIS THREAD
// ============================================================

void *analysis_worker(void *arg)
{
    float frame[BUFFER_SIZE];

    while (running) {

        if (!analysis_ready) {

            usleep(1000);

            continue;
        }

        pthread_mutex_lock(&analysis_mutex);

        memcpy(frame,
               analysis_buffer,
               sizeof(float) * BUFFER_SIZE);

        analysis_ready = 0;

        pthread_mutex_unlock(&analysis_mutex);

        // remove DC

        float mean = 0.0f;

        for (int i = 0; i < BUFFER_SIZE; i++)
            mean += frame[i];

        mean /= BUFFER_SIZE;

        for (int i = 0; i < BUFFER_SIZE; i++)
            frame[i] -= mean;

        apply_hann(frame, BUFFER_SIZE);

        float rms =
            compute_rms(frame, BUFFER_SIZE);

        float f0 =
            detect_pitch_yin(frame, BUFFER_SIZE);

        if (f0 < 50.0f || f0 > 500.0f)
            f0 = 0.0f;

        detected_f0 = f0;
        detected_rms = rms;
    }

    return NULL;
}

// ============================================================
// OSC UPDATE
// ============================================================

void update_oscillators()
{
    float f0 = detected_f0;
    float rms = detected_rms;

    if (rms < 0.01f ||
        f0 <= 0.0f ||
        isnan(f0)) {

        for (int i = 0; i < osc_count; i++)
            osc_bank[i].target_amp = 0.0f;

        return;
    }

    int count = 0;

    for (int i = 1; i <= 6; i++) {

        float freq =
            f0 *
            harmony_ratio *
            (float)i;

        if (freq > SR * 0.45f)
            break;

        osc_bank[count].target_freq = freq;

        osc_bank[count].target_amp =
            0.18f / (float)i;

        count++;
    }

    for (int i = count; i < osc_count; i++)
        osc_bank[i].target_amp = 0.0f;

    osc_count = count;
}

// ============================================================
// SYNTH
// ============================================================

float synth_sample()
{
    float out = 0.0f;

    update_oscillators();

    for (int i = 0; i < osc_count; i++) {

        Osc *o = &osc_bank[i];

        o->freq +=
            0.002f *
            (o->target_freq - o->freq);

        o->amp +=
            0.002f *
            (o->target_amp - o->amp);

        out +=
            sinf(o->phase) *
            o->amp;

        o->phase +=
            2.0f *
            M_PI *
            o->freq /
            (float)SR;

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
    float *in =
        jack_port_get_buffer(input_port, nframes);

    float *out =
        jack_port_get_buffer(output_port, nframes);

    for (unsigned int i = 0; i < nframes; i++) {

        float sample = in[i];

        // lightweight realtime work only

        analysis_buffer[write_pos] = sample;

        write_pos++;
        write_pos %= BUFFER_SIZE;

        hop_counter++;

        if (hop_counter >= HOP_SIZE) {

            pthread_mutex_lock(&analysis_mutex);

            analysis_ready = 1;

            pthread_mutex_unlock(&analysis_mutex);

            hop_counter = 0;
        }

        float wet = synth_sample();

        out[i] =
            sample * 0.75f +
            wet * 0.45f;
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

// ============================================================
// MAIN
// ============================================================

int main(int argc, char *argv[])
{
    if (argc > 1)
        harmony_ratio = atof(argv[1]);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Starting harmonizer...\n");

    client = jack_client_open(
        "harmonizer",
        JackNullOption,
        NULL);

    if (!client) {

        fprintf(stderr,
                "Failed to connect to JACK\n");

        return 1;
    }

    jack_set_process_callback(
        client,
        process,
        NULL);

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

        fprintf(stderr,
                "Failed to register JACK ports\n");

        return 1;
    }

    if (jack_activate(client)) {

        fprintf(stderr,
                "Failed to activate JACK\n");

        return 1;
    }

    // PERMANENT FIX:
    // separate analysis thread

    pthread_create(
        &analysis_thread,
        NULL,
        analysis_worker,
        NULL);

    printf("Running.\n");

    while (running)
        usleep(10000);

    printf("Stopping...\n");

    pthread_join(analysis_thread, NULL);

    jack_deactivate(client);

    jack_client_close(client);

    printf("Exited cleanly.\n");

    return 0;
}