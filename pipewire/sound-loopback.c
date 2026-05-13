#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 48000
#define CHANNELS 1
#define BUFFER_SAMPLES 48000

struct data {
    struct pw_main_loop *loop;

    struct pw_stream *input;
    struct pw_stream *output;

    float ring[BUFFER_SAMPLES];

    volatile unsigned int write_pos;
    volatile unsigned int read_pos;
};

static void input_process(void *userdata)
{
    struct data *d = userdata;

    struct pw_buffer *b;
    struct spa_buffer *buf;

    if (!(b = pw_stream_dequeue_buffer(d->input)))
        return;

    buf = b->buffer;

    if (buf->datas[0].data) {

        float *src = buf->datas[0].data;

        uint32_t bytes = buf->datas[0].chunk->size;
        uint32_t samples = bytes / sizeof(float);

        for (uint32_t i = 0; i < samples; i++) {

            d->ring[d->write_pos] = src[i];

            d->write_pos =
                (d->write_pos + 1) % BUFFER_SAMPLES;
        }
    }

    pw_stream_queue_buffer(d->input, b);
}

static void output_process(void *userdata)
{
    struct data *d = userdata;

    struct pw_buffer *b;
    struct spa_buffer *buf;

    if (!(b = pw_stream_dequeue_buffer(d->output)))
        return;

    buf = b->buffer;

    if (buf->datas[0].data) {

        float *dst = buf->datas[0].data;

        uint32_t max_bytes = buf->datas[0].maxsize;
        uint32_t samples = max_bytes / sizeof(float);

        for (uint32_t i = 0; i < samples; i++) {

            if (d->read_pos != d->write_pos) {

                dst[i] = d->ring[d->read_pos];

                d->read_pos =
                    (d->read_pos + 1) % BUFFER_SAMPLES;

            } else {

                dst[i] = 0.0f;
            }
        }

        buf->datas[0].chunk->offset = 0;
        buf->datas[0].chunk->stride = sizeof(float);
        buf->datas[0].chunk->size =
            samples * sizeof(float);
    }

    pw_stream_queue_buffer(d->output, b);
}

static const struct pw_stream_events input_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = input_process,
};

static const struct pw_stream_events output_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = output_process,
};

static void setup_format(
    struct spa_audio_info_raw *info)
{
    info->format = SPA_AUDIO_FORMAT_F32;
    info->rate = SAMPLE_RATE;
    info->channels = CHANNELS;
}

int main(int argc, char *argv[])
{
    struct data d;

    memset(&d, 0, sizeof(d));

    pw_init(&argc, &argv);

    d.loop = pw_main_loop_new(NULL);

    struct spa_audio_info_raw audio_info;
    setup_format(&audio_info);

    uint8_t buffer[1024];

    struct spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[1];

    params[0] =
        spa_format_audio_raw_build(
            &builder,
            SPA_PARAM_EnumFormat,
            &audio_info);

    // ---------------- INPUT STREAM ----------------

    d.input = pw_stream_new_simple(
        pw_main_loop_get_loop(d.loop),
        "input",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "DSP",
            PW_KEY_NODE_LATENCY, "64/48000",
            NULL),
        &input_events,
        &d);

    pw_stream_connect(
        d.input,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1);

    // ---------------- OUTPUT STREAM ----------------

    d.output = pw_stream_new_simple(
        pw_main_loop_get_loop(d.loop),
        "output",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "DSP",
            PW_KEY_NODE_LATENCY, "64/48000",
            NULL),
        &output_events,
        &d);

    pw_stream_connect(
        d.output,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1);

    printf("Running PipeWire loopback...\n");

    pw_main_loop_run(d.loop);

    pw_stream_destroy(d.input);
    pw_stream_destroy(d.output);

    pw_main_loop_destroy(d.loop);

    pw_deinit();

    return 0;
}