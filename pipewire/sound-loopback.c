#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE 48000
#define CHANNELS 1

struct data {
    struct pw_main_loop *loop;
    struct pw_stream *input;
    struct pw_stream *output;
};

static void on_process(void *userdata)
{
    struct data *d = userdata;

    struct pw_buffer *in_buf, *out_buf;
    struct spa_buffer *in_spa, *out_spa;

    in_buf = pw_stream_dequeue_buffer(d->input);
    out_buf = pw_stream_dequeue_buffer(d->output);

    if (!in_buf || !out_buf)
        return;

    in_spa = in_buf->buffer;
    out_spa = out_buf->buffer;

    if (!in_spa->datas[0].data || !out_spa->datas[0].data)
        goto done;

    float *src = in_spa->datas[0].data;
    float *dst = out_spa->datas[0].data;

    uint32_t n_bytes = in_spa->datas[0].chunk->size;
    uint32_t n_samples = n_bytes / sizeof(float);

    for (uint32_t i = 0; i < n_samples; i++) {
        dst[i] = src[i];
    }

    out_spa->datas[0].chunk->size = n_bytes;

done:
    pw_stream_queue_buffer(d->input, in_buf);
    pw_stream_queue_buffer(d->output, out_buf);
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

    struct pw_properties *in_props =
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL);

    struct pw_properties *out_props =
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL);

    data.input = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        "input",
        in_props,
        &stream_events,
        &data);

    data.output = pw_stream_new_simple(
        pw_main_loop_get_loop(data.loop),
        "output",
        out_props,
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
        data.input,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1);

    pw_stream_connect(
        data.output,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1);

    printf("Running PipeWire loopback...\n");

    pw_main_loop_run(data.loop);

    pw_stream_destroy(data.input);
    pw_stream_destroy(data.output);
    pw_main_loop_destroy(data.loop);

    pw_deinit();

    return 0;
}