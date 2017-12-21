/*

Copyright (c) 2016-2017 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef GRAL_H
#define GRAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	GRAL_PRIMARY_MOUSE_BUTTON = 1,
	GRAL_MIDDLE_MOUSE_BUTTON = 2,
	GRAL_SECONDARY_MOUSE_BUTTON = 3
};

struct gral_application;
struct gral_application_interface {
	void (*initialize)(void *user_data);
};
struct gral_text;
struct gral_gradient_stop {
	float position;
	float red;
	float green;
	float blue;
	float alpha;
};
struct gral_draw_context;
struct gral_window;
struct gral_window_interface {
	int (*close)(void *user_data);
	void (*draw)(struct gral_draw_context *draw_context, void *user_data);
	void (*resize)(int width, int height, void *user_data);
	void (*mouse_enter)(void *user_data);
	void (*mouse_leave)(void *user_data);
	void (*mouse_move)(float x, float y, void *user_data);
	void (*mouse_button_press)(float x, float y, int button, void *user_data);
	void (*mouse_button_release)(float x, float y, int button, void *user_data);
	void (*scroll)(float dx, float dy, void *user_data);
	void (*character)(uint32_t c, void *user_data);
	void (*paste)(const char *text, void *user_data);
};


/*================
    APPLICATION
 ================*/

struct gral_application *gral_application_create(const char *id, const struct gral_application_interface *interface, void *user_data);
void gral_application_delete(struct gral_application *application);
int gral_application_run(struct gral_application *application, int argc, char **argv);


/*============
    DRAWING
 ============*/

struct gral_text *gral_text_create(struct gral_window *window, const char *text, float size);
void gral_text_delete(struct gral_text *text);
float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context);

void gral_font_get_metrics(struct gral_window *window, float size, float *ascent, float *descent);

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha);
void gral_draw_context_close_path(struct gral_draw_context *draw_context);
void gral_draw_context_move_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_line_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_curve_to(struct gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y);
void gral_draw_context_add_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height);
void gral_draw_context_add_arc(struct gral_draw_context *draw_context, float cx, float cy, float radius, float start_angle, float sweep_angle);
void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha);
void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, const struct gral_gradient_stop *stops, int count);
void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha);
void gral_draw_context_push_clip(struct gral_draw_context *draw_context);
void gral_draw_context_pop_clip(struct gral_draw_context *draw_context);


/*===========
    WINDOW
 ===========*/

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, const char *title, const struct gral_window_interface *interface, void *user_data);
void gral_window_delete(struct gral_window *window);
void gral_window_request_redraw(struct gral_window *window);
void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height);
void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data);
void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data);
void gral_window_clipboard_copy(struct gral_window *window, const char *text);
void gral_window_clipboard_request_paste(struct gral_window *window);


/*=========
    FILE
 =========*/

void gral_file_read(const char *file, void (*callback)(const char *data, size_t size, void *user_data), void *user_data);
void gral_file_write(const char *file, const char *data, size_t size);


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int (*callback)(int16_t *buffer, int frames));

#ifdef __cplusplus
}
#endif

#endif
