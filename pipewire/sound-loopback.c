#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <stdio.h>

#define SAMPLE_RATE 48000
#define CHANNELS 1

struct data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
};

static void on_process(void *userdata)
{
    struct data *d = userdata;

    struct pw_buffer *b;
    struct spa_buffer *buf;

    if (!(b = pw_stream_dequeue_buffer(d->stream)))
        return;

    buf = b->buffer;

    if (buf->datas[0].data) {
        float *samples = buf->datas[0].data;

        uint32_t n_bytes = buf->datas[0].chunk->size;
        uint32_t n_samples = n_bytes / sizeof(float);

        float peak = 0.0f;

        for (uint32_t i = 0; i < n_samples; i++) {
            float s = samples[i];
            if (s < 0) s = -s;

            if (s > peak)
                peak = s;
        }

        printf("Peak: %f\n", peak);
        fflush(stdout);
    }

    pw_stream_queue_buffer(d->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

int main(int argc, char *argv[])
{
    struct data data;

    pw_init(&argc, &argv);

    data.loop = pw_main_loop_new(NULL);

    struct pw_properties *props =
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL);

    data.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        "capture",
        props,
        &stream_events,
        &data);

    struct spa_audio_info_raw audio_info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .channels = CHANNELS,
        .rate = SAMPLE_RATE,
    };

    uint8_t buffer[1024];
    struct spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[1];

    params[0] = spa_format_audio_raw_build(
        &builder,
        SPA_PARAM_EnumFormat,
        &audio_info);

    pw_stream_connect(
        data.stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1);

    printf("Capturing audio...\n");

    pw_main_loop_run(data.loop);

    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);

    pw_deinit();

    return 0;
}