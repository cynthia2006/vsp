#include <stddef.h>

#include "pr.h"

void
pr_init (struct polygon_renderer* pr, GLfloat line_width)
{
    pr->program = glCreateProgram();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    static const char* polygon_renderer_vs = "#version 330 core\n"
    "in vec2 coord;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(coord.xy, 0.0, 1.0);\n"
    "}\n";

    static const char* polygon_renderer_fs = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "\n"
    "void main() {\n"
    "    FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "}\n";

    glShaderSource(vertex_shader, 1, &polygon_renderer_vs, NULL);
    glCompileShader(vertex_shader);
    glShaderSource(fragment_shader, 1, &polygon_renderer_fs, NULL);
    glCompileShader(fragment_shader);
    glAttachShader(pr->program, vertex_shader);
    glAttachShader(pr->program, fragment_shader);
    glLinkProgram(pr->program);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    glGenVertexArrays(1, &pr->vao);
    glGenBuffers(1, &pr->vbo);
    glBindVertexArray(pr->vao);
    glBindBuffer(GL_ARRAY_BUFFER, pr->vbo);
    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          2 * sizeof(float),
                          (const void*)0);
    glEnableVertexAttribArray(0);

    pr->line_width = line_width;
}

void
pr_clear(struct polygon_renderer* pr)
{
    (void)pr; // Currently, not used.

    glClearColor(1.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void
pr_draw(struct polygon_renderer* pr, struct vertex* points, GLsizei num)
{
    glUseProgram(pr->program);
    glBindVertexArray(pr->vao);
    glBufferData(GL_ARRAY_BUFFER,
                 num * sizeof(struct vertex),
                 points,
                 GL_STREAM_DRAW);
    glLineWidth(pr->line_width);
    glDrawArrays(GL_LINE_STRIP, 0, num);
}

void
pr_deinit(struct polygon_renderer* pr)
{
    glDeleteVertexArrays(1, &pr->vao);
    glDeleteBuffers(1, &pr->vbo);
    glDeleteProgram(pr->program);
}
