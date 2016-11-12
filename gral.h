/*

Copyright (c) 2016, Elias Aebi
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

struct gral_text;
struct gral_painter;
struct gral_window;
struct gral_window_interface {
	int (*close)(void *user_data);
	void (*draw)(struct gral_painter *painter, void *user_data);
	void (*resize)(int width, int height, void *user_data);
	void (*mouse_enter)(void *user_data);
	void (*mouse_leave)(void *user_data);
	void (*mouse_move)(float x, float y, void *user_data);
	void (*mouse_button_press)(int button, void *user_data);
	void (*mouse_button_release)(int button, void *user_data);
	void (*scroll)(float dx, float dy, void *user_data);
	void (*character)(uint32_t c, void *user_data);
};

void gral_init(int *argc, char ***argv);
int gral_run(void);


/*============
    DRAWING
 ============*/

struct gral_text *gral_text_create(struct gral_window *window, const char *text, float size);
void gral_text_delete(struct gral_text *text);

void gral_painter_draw_text(struct gral_painter *painter, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha);
void gral_painter_new_path(struct gral_painter *painter);
void gral_painter_move_to(struct gral_painter *painter, float x, float y);
void gral_painter_close_path(struct gral_painter *painter);
void gral_painter_line_to(struct gral_painter *painter, float x, float y);
void gral_painter_curve_to(struct gral_painter *painter, float x1, float y1, float x2, float y2, float x, float y);
void gral_painter_fill(struct gral_painter *painter, float red, float green, float blue, float alpha);
void gral_painter_stroke(struct gral_painter *painter, float line_width, float red, float green, float blue, float alpha);


/*===========
    WINDOW
 ===========*/

struct gral_window *gral_window_create(int width, int height, const char *title, struct gral_window_interface *interface, void *user_data);
void gral_window_delete(struct gral_window *window);
void gral_window_request_redraw(struct gral_window *window);


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int (*callback)(int16_t *buffer, int frames));

#ifdef __cplusplus
}
#endif

#endif
