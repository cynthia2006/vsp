#include <stdbool.h>
#include <stddef.h>

#include <pipewire/pipewire.h>
#include <stdint.h>

struct pwb_sample_buffer
{
    float* buffer;
    size_t capacity;
    size_t cursor;
};

struct pwb_state_carrier
{
    struct pw_stream* stream;
    struct pwb_sample_buffer ring_buffer;

    uint32_t sample_rate;
};

struct pipewire_backend
{
    struct pw_stream_events stream_events;
    struct pwb_state_carrier state;
};

void
pipewire_backend_init (struct pipewire_backend *backend,
                       struct pw_loop* loop,
                       const char* stream_name,
                       int window_size,
                       int hop_size,
                       uint32_t sample_rate);

int
pipewire_backend_connect (struct pipewire_backend *backend);

void
pipewire_backend_capture(struct pipewire_backend *backend,
                         float *sample_buf);

void
pipewire_backend_deinit (struct pipewire_backend *backend);
