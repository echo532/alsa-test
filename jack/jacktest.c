#include <jack/jack.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

volatile int running = 1;

jack_client_t *client;
jack_port_t *input_port;
jack_port_t *output_port;

// simple gain control
float gain = 1.0f;

void signal_handler(int sig)
{
    running = 0;
}

// ============================================================
// JACK AUDIO CALLBACK
// ============================================================

int process(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *in =
        jack_port_get_buffer(input_port, nframes);

    jack_default_audio_sample_t *out =
        jack_port_get_buffer(output_port, nframes);

    for (unsigned int i = 0; i < nframes; i++) {

        // SIMPLE PASS-THROUGH
        out[i] = in[i] * gain;
    }

    return 0;
}

// ============================================================
// JACK SHUTDOWN CALLBACK
// ============================================================

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
        gain = atof(argv[1]);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Starting JACK passthrough...\n");
    printf("Gain: %.2f\n", gain);

    client = jack_client_open(
        "passthrough",
        JackNullOption,
        NULL
    );

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
        0
    );

    output_port = jack_port_register(
        client,
        "output",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0
    );

    if (!input_port || !output_port) {
        fprintf(stderr, "Failed to register ports\n");
        jack_client_close(client);
        return 1;
    }

    if (jack_activate(client)) {
        fprintf(stderr, "Cannot activate JACK client\n");
        jack_client_close(client);
        return 1;
    }

    printf("Running. Ctrl+C to stop.\n");

    while (running) {
        usleep(10000);
    }

    printf("Stopping...\n");

    jack_deactivate(client);
    jack_client_close(client);

    printf("Exited cleanly.\n");

    return 0;
}