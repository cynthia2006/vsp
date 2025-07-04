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
#include <math.h>
#include <assert.h>

#include "renderer.h"

#include <GLFW/glfw3.h>
#include <pipewire/pipewire.h>
#include <kiss_fftr.h>

#include "pipewire.h"

/**
 * The following a set of options that can be tweaked in order to change
 * the behaviour of this program.
 *
 * And, you thought there was a config file?! Fool! Real people write
 * configuration right in their code!
 */

// Initial width of the visualizer window.
const int INIT_WIDTH = 1280;
// Initial height of the visualizer window.
const int INIT_HEIGHT = 720;
// Number of bins; controls the granularity of the spectrum.
//
// NOTE The frequency spectrum is in Mel units roughly from 31 to 3178,
// correspoding to exactly to range of 20 to 20000 Hz.
const int NUM_POINTS = 360;
// Number of MSAA samples; controls the strength of anti-aliasing.
const int MSAA_HINT = 8;
// Margin around the ends of the visualizer polygon (in viewport units).
const float MARGIN_VW = 0.01;
// Line-width to pass to OpenGL for drawing the polygon.
//
// NOTE This would scale automatically on window resize; see resize_callback()
// TODO Make line-width variable.
const float INIT_LINE_WIDTH = 1.75;
// Initial amplitude gain (in decibels).
//
// NOTE Output of FFT is amplified, because the values are small in magnitude.
// For that reason, conventionally a log-scale is used, but it produces an unpleasant
// visualization, so we use a linear scale instead.
const float INIT_GAIN = 20.0;
// Initial exponential smoothing factor (ranging from 0 to 1); controls the strength of
// the (RC) low-pass filter applied to the spectrum. This is done in addition to 50%
// overlap (optimial for Hann window) to ensure a smooth visualizer experience.
//
// Higher values reduce reactivity to transients, lower values do the opposite.
// WARNING Setting this below zero or above one is undefined behaviour.
const float INIT_SMOOTHING_FACTOR = 0.8;

/**
 * WARNING Don't change the following options, UNLESS YOU KNOW WHAT YOU ARE DOING.
 */

// Size of audio-ring buffer; controls the FFT analysis length.
const int WINDOW_SIZE = 4096;
// Sampling rate to capture audio at.
//
// NOTE This is often the sample rate on consumer PCs, however on mismatch libpipewire
// would resample it up/down to match this sample rate.
const int SAMPLERATE = 48000;
// Lower limit of human perception.
const float MIN_FREQ = 20.0;
// Higher limit of human perception.
const float MAX_FREQ = 20000.0;

struct vsp_state
{
    float smoothing_factor;
    float gain;
    float gain_multiplier;
    float line_width;
};

static void
gen_hann_window(int N, float *win)
{
    for (int i = 0; i < N; ++i)
        win[i] = 0.5 * (1.0 - cosf(2.0 * M_PI * (float)i / N));
}

static inline
float db_rms_to_power(float db)
{
    return powf(10, M_SQRT2 * db / 20);
}

static inline
float mel_to_freq(float mel)
{
    return 700.0 * (expf(mel / 1127.0) - 1.0);
}

static void
update_window_title (GLFWwindow *window, struct vsp_state *state)
{
    char title[32];
    snprintf(title, sizeof title,
             "vsp (%.1f dB, τ=%.2f)",
             state->gain,
             state->smoothing_factor);

    glfwSetWindowTitle(window, title);
}

static void
error_callback (int error, const char* description)
{
    fprintf(stderr, "GLFW error: %s\n", description);
}

static void
key_callback (GLFWwindow* window, int key, int scancode, int action, int mods)
{
    struct vsp_state *state = glfwGetWindowUserPointer(window);

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT && action == GLFW_PRESS)
    {
        state->smoothing_factor -= 0.01;
        if (state->smoothing_factor < 0)
            state->smoothing_factor = 0;

        update_window_title(window, state);
    } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
    {
        state->smoothing_factor += 0.01;
        if (state->smoothing_factor > 1)
            state->smoothing_factor = 1;

        update_window_title(window, state);
    } else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        state->gain += 0.1;
        state->gain_multiplier = db_rms_to_power(state->gain);

        update_window_title(window, state);
    } else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        state->gain -= 0.1;
        state->gain_multiplier = db_rms_to_power(state->gain);

        update_window_title(window, state);
    }
}

static void
resize_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);

    struct vsp_state *state = glfwGetWindowUserPointer(window);
    state->line_width = INIT_LINE_WIDTH / INIT_WIDTH * width;
}

int main()
{
    const int FFT_SIZE = WINDOW_SIZE / 2 + 1;

    const float BIN_WIDTH = (float)WINDOW_SIZE / SAMPLERATE;
    const int BEGIN_BIN = BIN_WIDTH * MIN_FREQ;
    const int BANDWIDTH = BIN_WIDTH * MAX_FREQ - BEGIN_BIN;

    GLFWwindow *window;
    struct polygon_renderer pr;
    struct pipewire_backend pwb;
    struct pw_thread_loop *loop;

    kiss_fftr_cfg fft;
    float sample_win[WINDOW_SIZE];
    float hann_win[WINDOW_SIZE];
    kiss_fft_cpx freq_bins[FFT_SIZE];
    float smoothed_fft[BANDWIDTH];

    struct vertex points[NUM_POINTS];

    struct vsp_state state = {
        .gain = INIT_GAIN,
        .gain_multiplier = db_rms_to_power(INIT_GAIN),
        .smoothing_factor = INIT_SMOOTHING_FACTOR,
        .line_width = INIT_LINE_WIDTH
    };

    glfwInit();
    pw_init(NULL, NULL);

    glfwSetErrorCallback(error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, MSAA_HINT);

    loop = pw_thread_loop_new("pw-rvsp", NULL);
    pw_thread_loop_lock(loop);
    pw_thread_loop_start(loop);

    assert(pipewire_backend_init(&pwb,
                          pw_thread_loop_get_loop(loop),
                          "vsp",
                          WINDOW_SIZE,
                          WINDOW_SIZE / 2,
                          SAMPLERATE) == 1);

    gen_hann_window(WINDOW_SIZE, hann_win);
    fft = kiss_fftr_alloc(WINDOW_SIZE, 0, NULL, NULL);

    window = glfwCreateWindow(INIT_WIDTH, INIT_HEIGHT, "vsp", NULL, NULL);
    if (!window)
        goto error;

    glfwSetWindowUserPointer(window, &state);
    update_window_title(window, &state);

    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, resize_callback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    gladLoadGL(glfwGetProcAddress);

    static const float X_STEP = 2.0 * (1.0 - MARGIN_VW) / NUM_POINTS;
    float x = -1.0 + MARGIN_VW;

    for (int i = 0; i < NUM_POINTS; ++i) {
        points[i] = (struct vertex) {x, 0};
        x += X_STEP;
    }

    pr_init(&pr, INIT_LINE_WIDTH);
    assert(pipewire_backend_connect(&pwb) == 0);

    pw_thread_loop_unlock(loop);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        pw_thread_loop_lock(loop);
        pipewire_backend_capture(&pwb, sample_win);
        pw_thread_loop_unlock(loop);

        for (int i = 0; i < WINDOW_SIZE; ++i)
            sample_win[i] *= hann_win[i];

        kiss_fftr(fft, sample_win, freq_bins);

        for (int i = 0; i < BANDWIDTH; ++i)
        {
            kiss_fft_cpx bin = freq_bins[i + BEGIN_BIN];

            static const float FFT_SCALE = 1.0 / WINDOW_SIZE;

            const float mag = FFT_SCALE * hypotf(bin.r, bin.i);
            const float tau = state.smoothing_factor;

            // Exponential smoothing for smoother animations.
            smoothed_fft[i] = tau * smoothed_fft[i] + (1.0 - tau) * mag;
        }

        float sign = 1.0;
        const float gain = state.gain_multiplier;

        for (int i = 0; i < NUM_POINTS; ++i) {
            static const float DELTA_MEL = 1127.0 * logf((20000.0 + 700.0) / (20.0 + 700.0));
            static const float MEL_MIN = 1127.0 * logf(1.0 + 20.0/700.0);

            const float mel = DELTA_MEL * (float)i / NUM_POINTS + MEL_MIN;
            const float freq = mel_to_freq(mel);

            float bin_index;
            const float bin_alpha = modff(freq * BIN_WIDTH, &bin_index);
            const float bin = smoothed_fft[(int)bin_index];
            const int next_bin_index = bin_index + 1; // Addition, then cast to int.
            const float next_bin = next_bin_index > BANDWIDTH ? 0 : smoothed_fft[next_bin_index];

            // Linear interpolation; why? Because we're working with lines!
            const float lerp = (1.0 - bin_alpha) * bin + bin_alpha * next_bin;
            const float y = lerp * gain * sign;

            points[i + 1].y = y;
            sign *= -1.0;
        }

        // Update line width to ensure it is uniform across all viewports.
        pr_set_line_width(&pr, state.line_width);
        pr_draw(&pr, points, NUM_POINTS);
        glfwSwapBuffers(window);
    }

    pw_thread_loop_stop(loop);

error:
    if (window)
        glfwDestroyWindow(window);


    pr_deinit(&pr);
    pipewire_backend_deinit(&pwb);
    pw_thread_loop_destroy(loop);

    pw_deinit();
    glfwTerminate();
}
