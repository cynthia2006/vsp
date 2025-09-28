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

#include "renderer.h"

#include <GLFW/glfw3.h>
#include <pipewire/pipewire.h>

#include <kiss_fftr.h>

#include "pipewire.h"

/**
 * The following is a set of options that could be tweaked; choose carefully.
 */

// Initial width of the visualizer window.
const int INIT_WIDTH = 1280;
// Initial height of the visualizer window.
const int INIT_HEIGHT = 720;
// Number of bins; controls the granularity of the spectrum.
//
// NOTE Number of points on the Mel spectrum to sample.
const int NUM_POINTS = 360;
// Number of MSAA samples; controls the strength of anti-aliasing.
const int MSAA_HINT = 8;
// Margin around the ends of the visualizer polygon (in viewport units).
const float MARGIN_VW = 0.01;
// Line-width to pass to OpenGL for drawing the polygon.
//
// NOTE This would scale automatically on window resize; see resize_callback()
const float LINE_WIDTH = 1.75;
// Initial gain of spectrum (in decibels).
const float INIT_GAIN = 13.0;
// Initial exponential smoothing factor (ranging from 0 to 1).
//
// WARNING Setting this below zero or above one is undefined behaviour.
const float INIT_SMOOTHING_FACTOR = 0.8;

/**
 * WARNING The following options are intended for advanced users; its best not to fiddle with
 * these because they're optimized for best results.
 */

// Size of audio-ring buffer; controls the FFT analysis length.
const int WINDOW_SIZE = 4096;
// Sampling rate to capture audio at; however, it may/may not match the samplerate
// configured for the PipeWire server, in such a case resampling will occur.
const int SAMPLERATE = 48000;
const int FFT_SIZE = WINDOW_SIZE / 2 + 1;

struct vsp_state
{
    float tau, gain;
};

// Genererates a von Hann window of length N.
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
update_window_title (GLFWwindow *window, struct vsp_state *s)
{
    char title[32];
    snprintf(title, sizeof title, "vsp (%.1f dB, τ=%.2f)", s->gain, s->tau);

    glfwSetWindowTitle(window, title);
}

static void
error_callback (int error, const char* desc)
{
    fprintf(stderr, "GLFW error (%s) :(\n", desc);
}

static void
key_callback (GLFWwindow* window, int key, int scancode, int action, int mods)
{
    struct vsp_state *s = glfwGetWindowUserPointer(window);

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        #define clamp_min(x, min) (x) < (min) ? (min) : (x)
        #define clamp_max(x, max) (x) > (max) ? (max) : (x)

        switch (key)
        {
            case GLFW_KEY_LEFT:
                s->tau = clamp_min(s->tau - 0.01, 0);
            break;
            case GLFW_KEY_RIGHT:
                s->tau = clamp_max(s->tau + 0.01, 1);
            break;
            case GLFW_KEY_UP:
                s->gain += 0.1;
            break;
            case GLFW_KEY_DOWN:
                s->gain -= 0.1;
            break;
        }

        update_window_title(window, s);
    }
}

static void
resize_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    glLineWidth(LINE_WIDTH / INIT_WIDTH * width);
}

int main()
{
    GLFWwindow *window;

    struct polygon_renderer pr;
    struct pipewire_backend pwb;
    struct pw_thread_loop *loop;

    int ret;

    kiss_fftr_cfg fft;
    float sample_win[WINDOW_SIZE];
    float hann_win[WINDOW_SIZE];
    kiss_fft_cpx freq_bins[FFT_SIZE];

    struct vertex points[NUM_POINTS + 1];
    // Exponential smoothing is applied on freq_bins.
    float sm_freqs[NUM_POINTS];

    struct vsp_state state = {
        .gain = INIT_GAIN,
        .tau = INIT_SMOOTHING_FACTOR,
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

    ret = pipewire_backend_init(&pwb,
                                pw_thread_loop_get_loop(loop),
                                "vsp",           /* app name */
                                WINDOW_SIZE,     /* ring buffer length */
                                WINDOW_SIZE / 2, /* hop length */
                                SAMPLERATE);
    if (ret != 0)
    {
        fputs("PipeWire backend initialisation failed :(", stderr);
        goto error;
    }

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

    // Generate x-coords; do it here because if it were to be done in the hot-loop,
    // it would be a waste of CPU cycles.
    static const float X_STEP = 2.0 * (1.0 - MARGIN_VW) / NUM_POINTS;
    float x = -1.0 + MARGIN_VW;

    for (int i = 0; i < NUM_POINTS; ++i)
    {
        points[i].x = x;
        x += X_STEP;
    }

    pr_init(&pr);
    glLineWidth(LINE_WIDTH);

    ret = pipewire_backend_connect(&pwb);
    if (ret != 0)
    {
        fputs ("PipeWire stream connection failed :(\n", stderr);
        goto error;
    }

    pw_thread_loop_unlock(loop);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        pw_thread_loop_lock(loop);
        pipewire_backend_capture(&pwb, sample_win);
        pw_thread_loop_unlock(loop);

        // Tapering the window.
        for (int i = 0; i < WINDOW_SIZE; ++i)
            sample_win[i] *= hann_win[i];

        // FFT.
        kiss_fftr(fft, sample_win, freq_bins);

        const float gain = db_rms_to_power(state.gain);

        static const float FFT_SCALE = 4.0 / WINDOW_SIZE;
        static const float DELTA_MEL = 3785.184764; // 1127 * ln((20000.0 + 700.0) / (20.0 + 700.0))
        static const float MEL_MIN = 31.748578; // 1127.0 * ln(1.0 + 20.0/700.0)
        static const float BIN_WIDTH = (float)WINDOW_SIZE / SAMPLERATE;

        #define index_to_mel(i) (DELTA_MEL * (float)(i) / NUM_POINTS + MEL_MIN)

        float sign = 1.0;
        for (int i = 0; i < NUM_POINTS; ++i)
        {
            const int bi = mel_to_freq(index_to_mel(i)) * BIN_WIDTH;
            const kiss_fft_cpx bin = freq_bins[bi];
            const float mag = FFT_SCALE * hypotf(bin.r, bin.i);

            // Exponential smoothing to make animation smoother.
            sm_freqs[i] = sm_freqs[i] * state.tau + (1.0 - state.tau) * mag;

            points[i + 1].y = gain * sign * sm_freqs[i];

            // Flipping sign creates the characteristic saw pattern.
            sign = -sign;
        }

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
