#include <stdio.h>
#include <math.h>

#include "pr.h"

#include <GLFW/glfw3.h>
#include <pipewire/pipewire.h>
#include <kiss_fftr.h>

#include "pw.h"

const int INIT_WIDTH = 1280;
const int INIT_HEIGHT = 720;
const float INIT_LINE_WIDTH = 1.5;

const int WINDOW_SIZE = 2048;
const float SMOOTHING_FACTOR = 0.7;
const float GAIN = 20.0;
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

static
void gen_hann_window(int N, float *win)
{
    for (int i = 0; i < N; ++i)
        win[i] = 0.5 * (1.0 - cosf(2.0 * M_PI * (float)i / N));
}


static void
glfw_error_callback (int error, const char* description)
{
    fprintf(stderr, "GLFW error: %s\n", description);
}

static void
glfw_key_callback (GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void
glfw_resize_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    GLFWwindow *win;
    struct polygon_renderer pr;
    struct pipewire_backend pw_backend;
    struct pw_thread_loop *loop;

    kiss_fftr_cfg fft;
    float sample_win[WINDOW_SIZE];
    float hann_win[WINDOW_SIZE];
    kiss_fft_cpx freq_bins[FFT_SIZE];
    float smoothed_fft[BANDWIDTH];

    struct vertex points[BANDWIDTH];

    glfwInit();
    pw_init(NULL, NULL);
    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    loop = pw_thread_loop_new("pw-rvsp", NULL);
    pw_thread_loop_lock(loop);
    pw_thread_loop_start(loop);

    pipewire_backend_init(&pw_backend,
                          pw_thread_loop_get_loop(loop),
                          "vsp",
                          WINDOW_SIZE,
                          HOP_SIZE,
                          SAMPLERATE);

    gen_hann_window(WINDOW_SIZE, hann_win);
    fft = kiss_fftr_alloc(WINDOW_SIZE, 0, NULL, NULL);

    win = glfwCreateWindow(INIT_WIDTH, INIT_HEIGHT, "vsp", NULL, NULL);
    if (!win)
        goto error;

    glfwSetKeyCallback(win, glfw_key_callback);
    glfwSetFramebufferSizeCallback(win, glfw_resize_callback);
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    gladLoadGL(glfwGetProcAddress);

    GLfloat x = -1.0 + 0.5 / BANDWIDTH;
    for (int i = 0; i < BANDWIDTH; ++i) {
        points[i] = (struct vertex) {x, 0};
        x += 2.0/BANDWIDTH;
    }

    pr_init(&pr, INIT_LINE_WIDTH);
    pipewire_backend_connect(&pw_backend);

    pw_thread_loop_unlock(loop);

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        pw_thread_loop_lock(loop);
        pipewire_backend_capture(&pw_backend, sample_win);

        for (int i = 0; i < WINDOW_SIZE; ++i)
            sample_win[i] *= hann_win[i];
        pw_thread_loop_unlock(loop);

        kiss_fftr(fft, sample_win, freq_bins);

        pr_clear(&pr);

        float sign = 1.0;
        for (int i = 0; i < BANDWIDTH; ++i)
        {
            // This is the normalised output of FFT
            float mag = FFT_SCALE * hypotf(freq_bins[i].r, freq_bins[i].r);

            smoothed_fft[i] = SMOOTHING_FACTOR * smoothed_fft[i] + (1.0 - SMOOTHING_FACTOR) * mag;

            float y = smoothed_fft[i] * GAIN * sign;

            points[i].y = y;
            sign *= -1.0;
        }

        pr_draw(&pr, points, BANDWIDTH);
        glfwSwapBuffers(win);
    }

    pw_thread_loop_stop(loop);

    error:
        if (win)
            glfwDestroyWindow(win);

        pr_deinit(&pr);
        pipewire_backend_deinit(&pw_backend);

    pw_deinit();
    glfwTerminate();
}
