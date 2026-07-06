#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <stdio.h>
#include <stdatomic.h>

struct app {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    atomic_uint_fast64_t frames;
};

static void on_process(void *userdata)
{
    struct app *app = userdata;

    struct pw_buffer *b = pw_stream_dequeue_buffer(app->stream);
    if (!b)
        return;

    printf("callback fired\n");

    pw_stream_queue_buffer(app->stream, b);
}

static const struct pw_stream_events events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

int main(void)
{
    pw_init(NULL, NULL);

    struct app app = {0};

    app.loop = pw_main_loop_new(NULL);

    struct pw_loop *loop = pw_main_loop_get_loop(app.loop);

    app.stream = pw_stream_new_simple(
        loop,
        "harmonizer-capture",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Music",
            NULL),
        &events,
        &app);

    struct spa_audio_info_raw info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .channels = 1,
        .rate = 48000,
    };

    uint8_t buffer[1024];

    struct spa_pod_builder b =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[1];

    params[1] = spa_format_audio_raw_build(
        &b,
        SPA_PARAM_EnumFormat,
        &info);

    pw_stream_connect(
        app.stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1);

    printf("running...\n");

    pw_main_loop_run(app.loop);
}