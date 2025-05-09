#include <stdbool.h>

#include "gl.h"

struct vertex
{
    GLfloat x, y;
};

struct polygon_renderer
{
    GLuint vbo;
    GLuint vao;
    GLuint program;
    GLfloat line_width;
};

bool
pr_init (struct polygon_renderer* pr, GLfloat line_width);

void
pr_clear (struct polygon_renderer* pr);

void
pr_draw (struct polygon_renderer* pr, struct vertex* points, GLsizei num);

void
pr_deinit(struct polygon_renderer* pr);
