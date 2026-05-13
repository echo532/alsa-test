// harmonizer_pitch.c
#include <jack/jack.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 48000
#define BUFFER_SIZE 2048
#define YIN_MAX_LAG 2048
#define YIN_THRESHOLD 0.15f

jack_port_t *input_port;
jack_client_t *client;

float buffer[BUFFER_SIZE];
int buffer_index = 0;

// -----------------------------
// Ring buffer helper
// -----------------------------
void push_sample(float s) {
    buffer[buffer_index++] = s;
    if (buffer_index >= BUFFER_SIZE) {
        buffer_index = 0;
    }
}

// -----------------------------
// YIN difference function
// -----------------------------
void difference_function(const float *buf, float *df) {
    for (int tau = 0; tau < YIN_MAX_LAG; tau++) {
        df[tau] = 0.0f;
        for (int i = 0; i < YIN_MAX_LAG; i++) {
            float delta = buf[i] - buf[i + tau];
            df[tau] += delta * delta;
        }
    }
}

// -----------------------------
// Cumulative mean normalized difference
// -----------------------------
void cumulative_mean_normalization(float *df, float *cmndf) {
    cmndf[0] = 1.0f;
    float running_sum = 0.0f;

    for (int tau = 1; tau < YIN_MAX_LAG; tau++) {
        running_sum += df[tau];
        cmndf[tau] = df[tau] * tau / running_sum;
    }
}

// -----------------------------
// Absolute threshold search
// -----------------------------
int absolute_threshold(float *cmndf) {
    for (int tau = 2; tau < YIN_MAX_LAG; tau++) {
        if (cmndf[tau] < YIN_THRESHOLD) {
            while (tau + 1 < YIN_MAX_LAG && cmndf[tau + 1] < cmndf[tau]) {
                tau++;
            }
            return tau;
        }
    }
    return -1;
}

// -----------------------------
// Parabolic interpolation
// -----------------------------
float parabolic_interpolation(float *cmndf, int tau) {
    int x0 = (tau < 1) ? tau : tau - 1;
    int x2 = (tau + 1 < YIN_MAX_LAG) ? tau + 1 : tau;

    float s0 = cmndf[x0];
    float s1 = cmndf[tau];
    float s2 = cmndf[x2];

    float a = s0 + s2 - 2 * s1;
    if (fabsf(a) < 1e-6f) return tau;

    float b = (s2 - s0) / 2.0f;

    return tau + (b / a);
}

// -----------------------------
// YIN pitch detection
// -----------------------------
float yin_pitch(float *buf) {
    static float df[YIN_MAX_LAG];
    static float cmndf[YIN_MAX_LAG];

    difference_function(buf, df);
    cumulative_mean_normalization(df, cmndf);

    int tau = absolute_threshold(cmndf);
    if (tau < 0) return 0.0f;

    float period = parabolic_interpolation(cmndf, tau);
    if (period <= 0.0f) return 0.0f;

    return (float)SR / period;
}

// -----------------------------
// Harmonic consistency check (lightweight)
// -----------------------------
float harmonic_score(float f0, float *buf) {
    if (f0 <= 0) return 0.0f;

    float score = 0.0f;
    int harmonics = 5;

    for (int h = 1; h <= harmonics; h++) {
        float freq = f0 * h;
        int lag = (int)(SR / freq);

        if (lag < 2 || lag >= YIN_MAX_LAG) continue;

        float corr = 0.0f;
        for (int i = 0; i < 200; i++) {
            corr += buf[i] * buf[i + lag];
        }

        score += corr / h;
    }

    return score;
}

// -----------------------------
// JACK process callback
// -----------------------------
int process(jack_nframes_t nframes, void *arg) {
    float *in = jack_port_get_buffer(input_port, nframes);

    for (int i = 0; i < nframes; i++) {
        push_sample(in[i]);
    }

    if (buffer_index < BUFFER_SIZE - 1)
        return 0;

    float f0 = yin_pitch(buffer);

    float score = harmonic_score(f0, buffer);

    if (f0 > 0) {
        printf("Pitch: %.2f Hz | Harmonic score: %.2f\n", f0, score);
    }

    return 0;
}

// -----------------------------
// JACK setup
// -----------------------------
void shutdown_cb(void *arg) {
    exit(1);
}

int main() {
    client = jack_client_open("pitch_detector", JackNullOption, NULL);
    if (!client) {
        fprintf(stderr, "Failed to open JACK client\n");
        return 1;
    }

    input_port = jack_port_register(
        client,
        "input",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0
    );

    jack_set_process_callback(client, process, NULL);
    jack_on_shutdown(client, shutdown_cb, NULL);

    jack_activate(client);

    printf("Running pitch detector...\n");

    while (1) sleep(1);

    return 0;
}