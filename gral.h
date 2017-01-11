/*

Copyright (c) 2016-2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef GRAL_H
#define GRAL_H

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
	void (*mouse_button_press)(int button, void *user_data);
	void (*mouse_button_release)(int button, void *user_data);
	void (*scroll)(float dx, float dy, void *user_data);
	void (*character)(uint32_t c, void *user_data);
	void (*paste)(const char *text, void *user_data);
};


/*================
    APPLICATION
 ================*/

struct gral_application *gral_application_create(const char *id, struct gral_application_interface *interface, void *user_data);
void gral_application_delete(struct gral_application *application);
int gral_application_run(struct gral_application *application, int argc, char **argv);


/*============
    DRAWING
 ============*/

struct gral_text *gral_text_create(struct gral_window *window, const char *text, float size);
void gral_text_delete(struct gral_text *text);

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha);
void gral_draw_context_fill_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height, float red, float green, float blue, float alpha);
void gral_draw_context_new_path(struct gral_draw_context *draw_context);
void gral_draw_context_close_path(struct gral_draw_context *draw_context);
void gral_draw_context_move_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_line_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_curve_to(struct gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y);
void gral_draw_context_add_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height);
void gral_draw_context_add_arc(struct gral_draw_context *draw_context, float cx, float cy, float radius, float start_angle, float end_angle);
void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha);
void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop *stops, int count);
void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha);
void gral_draw_context_push_clip(struct gral_draw_context *draw_context);
void gral_draw_context_pop_clip(struct gral_draw_context *draw_context);


/*===========
    WINDOW
 ===========*/

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, const char *title, struct gral_window_interface *interface, void *user_data);
void gral_window_delete(struct gral_window *window);
void gral_window_request_redraw(struct gral_window *window);
void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data);
void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data);
void gral_window_clipboard_copy(struct gral_window *window, const char *text);
void gral_window_clipboard_request_paste(struct gral_window *window);


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int (*callback)(int16_t *buffer, int frames));

#ifdef __cplusplus
}
#endif

#endif
