/**
 *   Copyright (C) 2025 Cynthia
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <spa/pod/builder.h>
#include <spa/param/audio/format-utils.h>

#include "pw.h"

static void
pipewire_backend_store (struct pwb_sample_buffer* rb, float* samples, size_t len);

static void
fill_audio_buffer(void *_userdata)
{
    struct pwb_state_carrier *state = _userdata;
    struct pwb_sample_buffer *rb = &state->ring_buffer;
    struct pw_buffer *b = pw_stream_dequeue_buffer(state->stream);

    float *samples = b->buffer->datas[0].data;
    uint32_t n_samples = b->buffer->datas[0].chunk->size / sizeof(float);

    pipewire_backend_store(rb, samples, n_samples);
    pw_stream_queue_buffer(state->stream, b);
}


bool
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

    backend->state.ring_buffer.capacity = window_size;
    backend->state.ring_buffer.cursor = 0;

    backend->state.ring_buffer.buffer = calloc (window_size, sizeof(float));
    if (!backend->state.ring_buffer.buffer)
        goto cleanup;

    backend->state.sample_rate = sample_rate;
    backend->state.stream = pw_stream_new_simple (loop,
                                                 stream_name,
                                                 props,
                                                 &backend->stream_events,
                                                 &backend->state);
    if (!backend->state.stream)
        goto cleanup;

    return true;
cleanup:
    if (backend->state.stream)
        pw_stream_destroy(backend->state.stream);

    if (props)
        pw_properties_free (props);

    return false;
}

// 0 for success; <0 for failure.
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
pipewire_backend_capture(struct pipewire_backend *backend, float *sample_buf)
{
    struct pwb_sample_buffer *rb = &backend->state.ring_buffer;
    size_t index = rb->cursor;

    for (size_t i = 0; i < rb->capacity; ++i)
    {
        sample_buf[i] = rb->buffer[index];
        index = (index + 1) % rb->capacity;
    }
}

// Put doesn't care if the data was actually read, thus allowing overwrites.
static void
pipewire_backend_store (struct pwb_sample_buffer* rb, float* samples, size_t len)
{
    for(size_t i = 0; i < len; ++i)
    {
        rb->buffer[rb->cursor] = samples[i];
        rb->cursor = (rb->cursor + 1) % rb->capacity;
    }
}

void
pipewire_backend_deinit (struct pipewire_backend *backend)
{
    pw_stream_destroy (backend->state.stream);
    free(backend->state.ring_buffer.buffer);
}
