#ifndef _DRAWABLE_H
#define _DRAWABLE_H

#include "math4.h"
#include "shader.h"
#include "graphics.h"
#include "geometry.h"

struct drawable {
	GLenum	draw_mode;
	GLuint	vertex_count;
	GLuint	vbo;
	GLuint	vao;
};

void		drawable_free(struct drawable *d);

void		drawable_new_circle_outline(struct drawable *dst, struct circle *circle, int segments, struct shader *s);
void		drawable_new_circle_outlinef(struct drawable *dst, float x, float y, float r, int segments, struct shader *s);

void		drawable_new_rect_outline(struct drawable *dst, struct rect *rect, struct shader *s);
void		drawable_new_rect_outlinef(struct drawable *dst, float x, float y, float w, float h, struct shader *s);
void		drawable_new_rect_solidf(struct drawable *dst, float x, float y, float w, float h, struct shader *s);
void		drawable_new_rect_fullscreen(struct drawable *dst, struct shader *s);
void		drawable_new_plane_subdivided(struct drawable *dst, vec2 origin, vec2 size, int divisions_x, int divisions_y, struct shader *s);
void		drawable_new_plane_subdivided_vertex(struct drawable *dst, vec2 origin, vec2 size, int divisions_x, int divisions_y, struct shader *s);

void		drawable_new_linef(struct drawable *dst, float x1, float y1, float x2, float y2, struct shader *s);

void		drawable_new_cube(struct drawable *dst, struct shader *shader);

GLfloat*	drawable_get_vertices_plane_quad(GLfloat* dst, float x, float y, float w, float h, int mirror);

void		drawable_render(struct drawable *d);
void		drawable_render_detailed(GLenum mode, GLuint vao, GLuint vertex_count, GLuint *tex, vec4 color, struct shader *s, struct mvp mvp);
void		drawable_render_simple(struct drawable *d, struct shader *s, GLuint *tex, vec4 color, mat4 transform);

#endif
