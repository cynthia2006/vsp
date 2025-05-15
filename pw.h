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

bool
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
