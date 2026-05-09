#include <jack/jack.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

jack_port_t *input_port1;
jack_port_t *input_port2;
jack_port_t *output_port1;
jack_port_t *output_port2;
jack_client_t *client;

/* Audio callback */
int process(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *in1 =
        (jack_default_audio_sample_t *)
        jack_port_get_buffer(input_port1, nframes);

    jack_default_audio_sample_t *in2 =
        (jack_default_audio_sample_t *)
        jack_port_get_buffer(input_port2, nframes);

    jack_default_audio_sample_t *out1 =
        (jack_default_audio_sample_t *)
        jack_port_get_buffer(output_port1, nframes);

    jack_default_audio_sample_t *out2 =
        (jack_default_audio_sample_t *)
        jack_port_get_buffer(output_port2, nframes);

    for (jack_nframes_t i = 0; i < nframes; i++) {
        out1[i] = in1[i];
        out2[i] = in2[i];
    }

    return 0;
}

/* JACK shutdown callback */
void jack_shutdown(void *arg)
{
    fprintf(stderr, "JACK server shut down\n");
    exit(1);
}

int main()
{
    const char *client_name = "passthrough_client";
    jack_status_t status;

    client = jack_client_open(client_name,
                              JackNullOption,
                              &status);

    if (client == NULL) {
        fprintf(stderr,
                "Failed to connect to JACK server\n");
        return 1;
    }

    printf("Connected to JACK server\n");

    jack_set_process_callback(client,
                              process,
                              0);

    jack_on_shutdown(client,
                     jack_shutdown,
                     0);

    /* Register ports */
    input_port1 = jack_port_register(
        client,
        "input_1",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0);

    input_port2 = jack_port_register(
        client,
        "input_2",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0);

    output_port1 = jack_port_register(
        client,
        "output_1",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0);

    output_port2 = jack_port_register(
        client,
        "output_2",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0);

    if (!input_port1 || !input_port2 ||
        !output_port1 || !output_port2) {

        fprintf(stderr,
                "Failed to register JACK ports\n");

        return 1;
    }

    /* Activate client */
    if (jack_activate(client)) {
        fprintf(stderr,
                "Cannot activate JACK client\n");

        return 1;
    }

    printf("JACK client activated\n");

    /* Auto-connect ports */

    if (jack_connect(client,
                     "system:capture_1",
                     "passthrough_client:input_1")) {

        fprintf(stderr,
                "Failed to connect capture_1\n");
    }

    if (jack_connect(client,
                     "system:capture_2",
                     "passthrough_client:input_2")) {

        fprintf(stderr,
                "Failed to connect capture_2\n");
    }

    if (jack_connect(client,
                     "passthrough_client:output_1",
                     "system:playback_1")) {

        fprintf(stderr,
                "Failed to connect playback_1\n");
    }

    if (jack_connect(client,
                     "passthrough_client:output_2",
                     "system:playback_2")) {

        fprintf(stderr,
                "Failed to connect playback_2\n");
    }

    printf("Audio routing established\n");
    printf("Mic input should now pass to speakers\n");

    while (1) {
        sleep(1);
    }

    jack_client_close(client);

    return 0;
}