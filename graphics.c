/**
 * Basic drawing functionality.
 *
 * Authors: Tim Sjöstrand <tim.sjostrand@gmail.com>
 *			Johan Yngman <johan.yngman@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <time.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef EMSCRIPTEN
	#include <emscripten/emscripten.h>
#endif

#include "graphics.h"
#include "math4.h"
#include "texture.h"
#include "color.h"

static const float rect_vertices[] = {
	// Vertex			 // Texcoord
	-0.5f,	0.5f,  0.0f, 0.0f, 0.0f, // Top-left
	-0.5f, -0.5f,  0.0f, 0.0f, 1.0f, // Bottom-left
	 0.5f,	0.5f,  0.0f, 1.0f, 0.0f, // Top-right
	 0.5f,	0.5f,  0.0f, 1.0f, 0.0f, // Top-right
	-0.5f, -0.5f,  0.0f, 0.0f, 1.0f, // Bottom-left
	 0.5f, -0.5f,  0.0f, 1.0f, 1.0f, // Bottom-right
};

#ifdef EMSCRIPTEN
struct graphics* graphics_global;
#endif

static int graphics_opengl_init(struct graphics *g, int view_width, int view_height)
{
	/* Global transforms. */
	translate(g->translate, 0.0f, 0.0f, 0.0f);
	scale(g->scale, 10.0f, 10.0f, 1);
	rotate_z(g->rotate, 0);
	ortho(g->projection, 0, view_width, view_height, 0, -1.0f, 1.0f);
	transpose_same(g->projection);

	/* OpenGL. */
	// glViewport( 0, 0, view_width, view_height );
	glClearColor(0.33f, 0.33f, 0.33f, 0.0f);

#ifdef DEPTH
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
#else
	glDisable(GL_DEPTH_TEST);
#endif

	glEnable(GL_CULL_FACE);
	//glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Vertex buffer. */
	glGenBuffers(1, &g->vbo_rect);
	glBindBuffer(GL_ARRAY_BUFFER, g->vbo_rect);
	glBufferData(GL_ARRAY_BUFFER, VBO_QUAD_LEN * sizeof(float),
			rect_vertices, GL_STATIC_DRAW);

	/* Vertex array. */
	glGenVertexArrays(1, &g->vao_rect);
	glBindVertexArray(g->vao_rect);

	GL_OK_OR_RETURN_NONZERO;

	return GRAPHICS_OK;
}

static void graphics_glfw_error_callback(int error_code, const char *msg)
{
	graphics_error("GLFW Error %d: %s\n", error_code, msg);
}

int graphics_libraries_init(struct graphics *g, int window_width, int window_height,
		int window_mode, const char *title)
{
	/* Initialize the library */
	if(!glfwInit()) {
		return GRAPHICS_GLFW_ERROR;
	}

#ifdef EMSCRIPTEN
	g->window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
	if(!g->window) {
		glfwTerminate();
		return GRAPHICS_GLFW_ERROR;
	}
#else
	/* QUIRK: Mac OSX */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	/* Create a windowed mode window and its OpenGL context */
	if (window_mode == GRAPHICS_MODE_WINDOWED) {
		g->window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
	}
	else if (window_mode == GRAPHICS_MODE_BORDERLESS) {
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *video_mode = glfwGetVideoMode(monitor);

		glfwWindowHint(GLFW_RED_BITS, video_mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, video_mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, video_mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, video_mode->refreshRate);
		glfwWindowHint(GLFW_AUTO_ICONIFY, GL_FALSE);
		glfwWindowHint(GLFW_FLOATING, GL_TRUE);
		glfwWindowHint(GLFW_DECORATED, GL_FALSE);

		g->window = glfwCreateWindow(video_mode->width, video_mode->height, title, NULL, NULL);
	}
	else {
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *video_mode = glfwGetVideoMode(monitor);
		g->window = glfwCreateWindow(video_mode->width, video_mode->height, title, monitor, NULL);
	}
	if(!g->window) {
		glfwTerminate();
		return GRAPHICS_GLFW_ERROR;
	}
#endif

	/* Be notified when window size changes. */
	glfwSetErrorCallback(&graphics_glfw_error_callback);

	/* Select the current OpenGL context. */
	glfwMakeContextCurrent(g->window);

	/* Init GLEW. */
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if(err != GLEW_OK) {
		return GRAPHICS_GLEW_ERROR;
	}

	/* NOTE: Something in the init code above is causing an 0x0500 OpenGL error,
	 * and it will linger in the error queue until the application pops it.
	 * Assuming the aforementioned init code does it's error checking properly,
	 * we can safely exhaust the error queue here to avoid ugly debug print
	 * statements later. */
	while(glGetError() != GL_NO_ERROR) {
		/* Ignore this error. */
	}

	return GRAPHICS_OK;
}

/**
 * @param g						A graphics struct to fill in.
 * @param view_width			The width of the view, used for ortho().
 * @param view_height			The height of the view, used for ortho().
 * @param windowed				If applicable, whether to start in windowed mode.
 */
int graphics_init(struct graphics *g, think_func_t think, render_func_t render,
		fps_func_t fps_callback, int view_width, int view_height, int window_mode,
		const char *title, int window_width, int window_height)
{
	int ret = 0;

#ifdef EMSCRIPTEN
	/* HACK: Need to store reference for emscripten. */
	graphics_global = g;
#endif

	/* Set up the graphics struct properly. */
	g->delta_time_factor = 1.0f;
	g->think = think;
	g->render = render;
	g->frames.callback = fps_callback;

	/* Set up GLEW and glfw. */
	ret = graphics_libraries_init(g, window_width, window_height, window_mode, title);

	if(ret != GRAPHICS_OK) {
		return ret;
	}

	/* Set up OpenGL. */
	ret = graphics_opengl_init(g, view_width, view_height);

	if(ret != GRAPHICS_OK) {
		return ret;
	}

	return GRAPHICS_OK;
}

void graphics_free(struct graphics *g)
{
	/* Free resources. */
	glDeleteVertexArrays(1, &g->vao_rect);
	glDeleteBuffers(1, &g->vbo_rect);

	/* Shut down glfw. */
	glfwTerminate();
}

static void graphics_frames_register(struct frames *f, float delta_time)
{
	f->frames++;
	f->frame_time_min = fmin(delta_time, f->frame_time_min);
	f->frame_time_max = fmax(delta_time, f->frame_time_max);
	f->frame_time_sum += delta_time;

	if(now() - f->last_frame_report >= 1000.0) {
		f->last_frame_report = now();
		f->frame_time_avg = f->frame_time_sum / (float) f->frames;
		if(f->callback != NULL) {
			f->callback(f);
		}
		f->frame_time_max = FLT_MIN;
		f->frame_time_min = FLT_MAX;
		f->frame_time_sum = 0;
		f->frames = 0;
	}
}

/**
 * Returns the number of milliseconds since the program was started.
 */
double now()
{
	return glfwGetTime() * 1000.0;
}

void graphics_do_frame(struct graphics *g)
{
	double before = now();

	/* Delta-time. */
	float delta_time = 0;
	if(g->frames.last_frame != 0) {
		delta_time = (now() - g->frames.last_frame) * g->delta_time_factor;
	}
	g->frames.last_frame = now();

	/* Game loop. */
	g->think(g, delta_time);
	g->render(g, delta_time);

	/* Swap front and back buffers */
	glfwSwapBuffers(g->window);

	/* Poll for and process events */
	glfwPollEvents();

	/* Register that a frame has been drawn. */
	graphics_frames_register(&g->frames, now() - before);
}

#ifdef EMSCRIPTEN
void graphics_do_frame_emscripten()
{
	graphics_do_frame(graphics_global);
}
#endif

void graphics_loop(struct graphics *g)
{
	/* Sanity check. */
	if(!g->render || !g->think) {
		graphics_error("g->render() or g->think() not set!\n");
		return;
	}

#ifdef EMSCRIPTEN
	emscripten_set_main_loop(graphics_do_frame_emscripten, 0, 1);
#else
	while(!glfwWindowShouldClose(g->window)) {
		graphics_do_frame(g);
	}
#endif
}
