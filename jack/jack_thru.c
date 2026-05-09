#include <jack/jack.h>
#include <stdio.h>
#include <stdlib.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

// This is the real-time audio callback
int process(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *in =
        (jack_default_audio_sample_t *)jack_port_get_buffer(input_port, nframes);

    jack_default_audio_sample_t *out =
        (jack_default_audio_sample_t *)jack_port_get_buffer(output_port, nframes);

    // SIMPLE PASS-THROUGH (no processing)
    for (jack_nframes_t i = 0; i < nframes; i++)
    {
        out[i] = in[i];
    }

    return 0;
}

// Called if JACK shuts down
void shutdown_callback(void *arg)
{
    fprintf(stderr, "JACK shut down\n");
    exit(1);
}

int main()
{
    const char *client_name = "jack_thru";
    jack_options_t options = JackNullOption;
    jack_status_t status;

    client = jack_client_open(client_name, options, &status, NULL);

    if (client == NULL)
    {
        fprintf(stderr, "Failed to create JACK client\n");
        return 1;
    }

    jack_set_process_callback(client, process, 0);
    jack_on_shutdown(client, shutdown_callback, 0);

    input_port = jack_port_register(client, "input",
                                     JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsInput, 0);

    output_port = jack_port_register(client, "output",
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);

    if (jack_activate(client))
    {
        fprintf(stderr, "Cannot activate JACK client\n");
        return 1;
    }

    printf("JACK client running: jack_thru\n");

    // Keep program alive
    while (1)
    {
        sleep(1);
    }

    jack_client_close(client);
    return 0;
}