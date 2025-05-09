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

#include "pr.h"

#include <GLFW/glfw3.h>
#include <pipewire/pipewire.h>
#include <kiss_fftr.h>

#include "pw.h"

const int WINDOW_SIZE = 2048;
const float INIT_SMOOTHING_FACTOR = 0.7;
const float INIT_GAIN = 23.0;
const int HOP_SIZE = WINDOW_SIZE / 2;
const int FFT_SIZE = WINDOW_SIZE / 2 + 1;
const float FFT_SCALE = 1.0 / WINDOW_SIZE;
const int SAMPLERATE = 48000;
const float BIN_WIDTH = (float)WINDOW_SIZE / SAMPLERATE;
const float MIN_FREQ = 50.0;
const float MAX_FREQ = 10000.0;
const int BEGIN_BIN = BIN_WIDTH * MIN_FREQ;
const int END_BIN = BIN_WIDTH * MAX_FREQ;
const int BANDWIDTH = END_BIN - BEGIN_BIN;

const int INIT_WIDTH = 1280;
const int INIT_HEIGHT = 720;
const float INIT_LINE_WIDTH = 1.5;
const float MARGIN_VW = 0.01;
const int MSAA_HINT = 8;

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

static void
glfw_update_window_title (GLFWwindow *window, struct vsp_state *state)
{
    char title[32];
    snprintf(title, sizeof title,
             "vsp (%.1f dB, τ=%.2f)",
             state->gain,
             state->smoothing_factor);

    glfwSetWindowTitle(window, title);
}

static void
glfw_error_callback (int error, const char* description)
{
    fprintf(stderr, "GLFW error: %s\n", description);
}

static void
glfw_key_callback (GLFWwindow* window, int key, int scancode, int action, int mods)
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

        glfw_update_window_title(window, state);
    } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
    {
        state->smoothing_factor += 0.01;
        if (state->smoothing_factor > 1)
            state->smoothing_factor = 1;

        glfw_update_window_title(window, state);
    } else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        state->gain += 0.1;
        state->gain_multiplier = db_rms_to_power(state->gain);

        glfw_update_window_title(window, state);
    } else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        state->gain -= 0.1;
        state->gain_multiplier = db_rms_to_power(state->gain);

        glfw_update_window_title(window, state);
    }
}

static void
glfw_resize_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);

    struct vsp_state *state = glfwGetWindowUserPointer(window);
    state->line_width = INIT_LINE_WIDTH / INIT_WIDTH * width;
}

int main()
{
    GLFWwindow *window;
    struct polygon_renderer pr;
    struct pipewire_backend pwb;
    struct pw_thread_loop *loop;

    kiss_fftr_cfg fft;
    float sample_win[WINDOW_SIZE];
    float hann_win[WINDOW_SIZE];
    kiss_fft_cpx freq_bins[FFT_SIZE];
    float smoothed_fft[BANDWIDTH];

    const int NUM_POINTS = BANDWIDTH + 1;
    struct vertex points[NUM_POINTS];

    struct vsp_state state = {
        .gain = INIT_GAIN,
        .gain_multiplier = db_rms_to_power(INIT_GAIN),
        .smoothing_factor = INIT_SMOOTHING_FACTOR,
        .line_width = INIT_LINE_WIDTH
    };

    glfwInit();
    pw_init(NULL, NULL);

    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, MSAA_HINT);

    loop = pw_thread_loop_new("pw-rvsp", NULL);
    pw_thread_loop_lock(loop);
    pw_thread_loop_start(loop);


    pipewire_backend_init(&pwb,
                          pw_thread_loop_get_loop(loop),
                          "vsp",
                          WINDOW_SIZE,
                          HOP_SIZE,
                          SAMPLERATE);

    gen_hann_window(WINDOW_SIZE, hann_win);
    fft = kiss_fftr_alloc(WINDOW_SIZE, 0, NULL, NULL);

    window = glfwCreateWindow(INIT_WIDTH, INIT_HEIGHT, "vsp", NULL, NULL);
    if (!window)
        goto error;

    glfwSetWindowUserPointer(window, &state);
    glfw_update_window_title(window, &state);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetFramebufferSizeCallback(window, glfw_resize_callback);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    gladLoadGL(glfwGetProcAddress);

    const float X_STEP = (2.0 - 2.0 * MARGIN_VW) / NUM_POINTS;
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

        pr_clear(&pr);

        float sign = 1.0;
        for (int i = 0; i < BANDWIDTH; ++i)
        {
            kiss_fft_cpx bin = freq_bins[i + BEGIN_BIN];
            float mag = FFT_SCALE * hypotf(bin.r, bin.i);
            float tau = state.smoothing_factor;

            smoothed_fft[i] = tau * smoothed_fft[i] + (1.0 - tau) * mag;

            float y = smoothed_fft[i] * state.gain_multiplier * sign;
            points[i + 1].y = y;

            sign *= -1.0;
        }

        // Update line width, to ensure it is uniform.
        pr.line_width = state.line_width;

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
