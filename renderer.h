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
pr_set_line_width (struct polygon_renderer* pr, float line_width);

void
pr_draw (struct polygon_renderer* pr, struct vertex* points, GLsizei num);

void
pr_deinit(struct polygon_renderer* pr);
