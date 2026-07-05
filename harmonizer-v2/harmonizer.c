#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <unistd.h>

struct app {
    struct pw_main_loop *loop;
    struct pw_stream *stream;

    atomic_uint_fast64_t frames;
    atomic_uint_fast64_t callbacks;
};

static void on_process(void *userdata)
{
    struct app *app = userdata;

    struct pw_buffer *b;

    if ((b = pw_stream_dequeue_buffer(app->stream)) == NULL)
        return;

    struct spa_buffer *buf = b->buffer;
    struct spa_data *d = &buf->datas[0];

    if (d->data && d->chunk) {

        uint32_t nbytes = d->chunk->size;

        uint32_t frames = nbytes /
            (sizeof(float));   // mono float32

        atomic_fetch_add(&app->frames, frames);
        atomic_fetch_add(&app->callbacks, 1);
    }

    pw_stream_queue_buffer(app->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

int main(void)
{
    struct app app = {0};

    pw_init(NULL, NULL);

    app.loop = pw_main_loop_new(NULL);

    app.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(app.loop),
        "capture",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "DSP",
            NULL),
        &stream_events,
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

    params[0] =
        spa_format_audio_raw_build(
            &b,
            SPA_PARAM_EnumFormat,
            &info);

    if (pw_stream_connect(
            app.stream,
            PW_DIRECTION_INPUT,
            PW_ID_ANY,
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS |
            PW_STREAM_FLAG_RT_PROCESS,
            params,
            1) < 0) {

        fprintf(stderr, "stream connect failed\n");
        return 1;
    }

    pid_t pid = fork();

    if (pid == 0) {
        pw_main_loop_run(app.loop);
        return 0;
    }

    while (1) {

        sleep(1);

        uint64_t frames =
            atomic_exchange(&app.frames, 0);

        uint64_t callbacks =
            atomic_exchange(&app.callbacks, 0);

        printf(
            "callbacks=%lu  frames=%lu\n",
            callbacks,
            frames);

        fflush(stdout);
    }
}