#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include <spa/pod/builder.h>
#include <spa/param/audio/format-utils.h>

#include "pw.h"

static bool
sample_ring_buffer_init (struct sample_ring_buffer* rb,
                         size_t capacity)
{
    rb->capacity = capacity;
    rb->buffer = calloc (rb->capacity, sizeof(float));
    if (rb->buffer == NULL)
    {
        return false;
    }
    rb->cursor = 0;

    return true;
}

// Put doesn't care if the data was actually read, thus allowing overwrites.
static void
sample_ring_buffer_put (struct sample_ring_buffer* rb, float sample)
{
    rb->buffer[rb->cursor] = sample;
    rb->cursor = (rb->cursor + 1) % rb->capacity;
}

static void
sample_ring_buffer_drop (struct sample_ring_buffer* rb)
{
    free(rb->buffer);

    rb->buffer = NULL;
    rb->capacity = 0;
    rb->cursor = 0;
}

static void
fill_audio_buffer(void *_userdata)
{
    struct state_carrier *state = _userdata;
    struct sample_ring_buffer *rb = &state->ring_buffer;
    struct pw_buffer *b = pw_stream_dequeue_buffer(state->stream);

    float *samples = b->buffer->datas[0].data;
    uint32_t n_samples = b->buffer->datas[0].chunk->size / sizeof(float);

    for (uint32_t i = 0; i < n_samples; ++i)
        sample_ring_buffer_put(rb, samples[i]);

    pw_stream_queue_buffer(state->stream, b);
}


void
pipewire_backend_init (struct pipewire_backend *backend,
                       struct pw_loop* loop,
                       const char* stream_name,
                       int window_size,
                       int hop_size,
                       uint32_t sample_rate)
{
    struct pw_properties* props;

    backend->stream_events = (struct pw_stream_events)
    {
        PW_VERSION_STREAM_EVENTS,
        .process = fill_audio_buffer
    };

    char tmp[32];
    snprintf(tmp, sizeof tmp, "%d/%d", hop_size, sample_rate);

    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_MEDIA_CATEGORY, "Monitor",
                              PW_KEY_MEDIA_ROLE, "DSP",
                              PW_KEY_NODE_LATENCY, tmp,
                              PW_KEY_STREAM_CAPTURE_SINK, "true",
                              NULL);

    sample_ring_buffer_init(&backend->state.ring_buffer, window_size);
    backend->state.sample_rate = sample_rate;
    backend->state.stream = pw_stream_new_simple(loop,
                                                 stream_name,
                                                 props,
                                                 &backend->stream_events,
                                                 &backend->state);
}

int
pipewire_backend_connect (struct pipewire_backend *backend)
{
    char raw_params[1024];
    const struct spa_pod *params[1];

    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(raw_params, sizeof raw_params);

    params[0] = spa_format_audio_raw_build(&b,
                                           SPA_PARAM_EnumFormat,
                                           &SPA_AUDIO_INFO_RAW_INIT(
                                               .channels = 1,
                                               .rate = backend->state.sample_rate,
                                               .format = SPA_AUDIO_FORMAT_F32
                                           ));

    return pw_stream_connect(backend->state.stream,
                            PW_DIRECTION_INPUT,
                            PW_ID_ANY,
                            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
                            params,
                            1);
}

void
pipewire_backend_capture(struct pipewire_backend *backend,
                         float *sample_buf)
{
    struct sample_ring_buffer *rb = &backend->state.ring_buffer;
    size_t index = rb->cursor;

    for (size_t i = 0; i < rb->capacity; ++i)
    {
        sample_buf[i] = rb->buffer[index];
        index = (index + 1) % rb->capacity;
    }
}

void
pipewire_backend_deinit (struct pipewire_backend *backend)
{
    pw_stream_destroy (backend->state.stream);
    sample_ring_buffer_drop(&backend->state.ring_buffer);
}
