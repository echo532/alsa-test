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

volatile sig_atomic_t running = 1;

// ============================================================
// AUDIO BUFFER
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
// HANN WINDOW
// ============================================================

void apply_hann(float *buf, int size)
{
    for (int i = 0; i < size; i++) {

        float w =
            0.5f *
            (1.0f - cosf((2.0f * M_PI * i) / (size - 1)));

        buf[i] *= w;
    }
}

// ============================================================
// YIN PITCH DETECTOR
// ============================================================

float detect_pitch_yin(float *buf, int size)
{
    static float diff[BUFFER_SIZE / 2];
    static float cmnd[BUFFER_SIZE / 2];

    int tau_max = BUFFER_SIZE / 2;
    int tau_estimate = -1;

    // --------------------------------------------------------
    // Difference function
    // --------------------------------------------------------

    for (int tau = 0; tau < tau_max; tau++) {

        diff[tau] = 0.0f;

        for (int i = 0; i < tau_max; i++) {

            float delta = buf[i] - buf[i + tau];

            diff[tau] += delta * delta;
        }
    }

    // --------------------------------------------------------
    // Cumulative mean normalized difference
    // --------------------------------------------------------

    cmnd[0] = 1.0f;

    float running_sum = 0.0f;

    for (int tau = 1; tau < tau_max; tau++) {

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
    // Absolute threshold
    // --------------------------------------------------------

    for (int tau = SR / 1000; tau < SR / 50; tau++) {

        if (cmnd[tau] < YIN_THRESHOLD) {

            while (tau + 1 < tau_max &&
                   cmnd[tau + 1] < cmnd[tau]) {
                tau++;
            }

            tau_estimate = tau;
            break;
        }
    }

    if (tau_estimate == -1)
        return 0.0f;

    // --------------------------------------------------------
    // Parabolic interpolation
    // --------------------------------------------------------

    if (tau_estimate > 0 &&
        tau_estimate < tau_max - 1) {

        float s0 = cmnd[tau_estimate - 1];
        float s1 = cmnd[tau_estimate];
        float s2 = cmnd[tau_estimate + 1];

        float better_tau =
            tau_estimate +
            (s2 - s0) /
            (2.0f * (2.0f * s1 - s2 - s0));

        return (float)SR / better_tau;
    }

    return (float)SR / (float)tau_estimate;
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

    // only a few harmonics for cleaner sound

    for (int i = 1; i <= 6; i++) {

        float freq =
            f0 *
            harmony_ratio *
            i;

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

    // apply window

    apply_hann(frame, BUFFER_SIZE);

    // RMS gate

    float rms = compute_rms(frame, BUFFER_SIZE);

    // YIN pitch detection

    float f0 = detect_pitch_yin(frame, BUFFER_SIZE);

    // reject unrealistic pitches

    if (f0 < 50.0f || f0 > 1000.0f)
        f0 = 0.0f;

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

        // smoother interpolation

        o->freq +=
            0.002f *
            (o->target_freq - o->freq);

        o->amp +=
            0.002f *
            (o->target_amp - o->amp);

        out += sinf(o->phase) * o->amp;

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
    jack_default_audio_sample_t *in =
        jack_port_get_buffer(input_port, nframes);

    jack_default_audio_sample_t *out =
        jack_port_get_buffer(output_port, nframes);

    for (unsigned int i = 0; i < nframes; i++) {

        float sample = in[i];

        // circular analysis buffer

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
    printf("YIN JACK Harmonizer\n");
    printf("Harmony Ratio: %.2f\n", harmony_ratio);
    printf("CTRL+C to stop\n");
    printf("====================================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

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

    jack_on_shutdown(
        client,
        jack_shutdown,
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
                "Failed to register ports\n");

        jack_client_close(client);

        return 1;
    }

    if (jack_activate(client)) {

        fprintf(stderr,
                "Failed to activate JACK\n");

        jack_client_close(client);

        return 1;
    }

    printf("Running...\n");

    while (running) {
        usleep(10000);
    }

    printf("\nStopping...\n");

    jack_deactivate(client);

    jack_client_close(client);

    printf("Exited cleanly.\n");

    return 0;
}