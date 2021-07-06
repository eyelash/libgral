/*

Copyright (c) 2016-2021 Elias Aebi

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
enum {
	GRAL_KEY_ENTER = 0x0D,
	GRAL_KEY_TAB = 0x09,
	GRAL_KEY_BACKSPACE = 0x232B,
	GRAL_KEY_DELETE = 0x2326,
	GRAL_KEY_ARROW_LEFT = 0x2190,
	GRAL_KEY_ARROW_UP = 0x2191,
	GRAL_KEY_ARROW_RIGHT = 0x2192,
	GRAL_KEY_ARROW_DOWN = 0x2193,
	GRAL_KEY_PAGE_UP = 0x21DE,
	GRAL_KEY_PAGE_DOWN = 0x21DF,
	GRAL_KEY_HOME = 0x2196,
	GRAL_KEY_END = 0x2198,
	GRAL_KEY_ESCAPE = 0x1B
};
enum {
	GRAL_MODIFIER_CONTROL = 1 << 0,
	GRAL_MODIFIER_ALT = 1 << 1,
	GRAL_MODIFIER_SHIFT = 1 << 2
};
enum {
	GRAL_CURSOR_DEFAULT = 1,
	GRAL_CURSOR_HAND = 2,
	GRAL_CURSOR_TEXT = 3,
	GRAL_CURSOR_HORIZONTAL_ARROWS = 4,
	GRAL_CURSOR_VERTICAL_ARROWS = 5,
	GRAL_CURSOR_NONE = 0
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
	void (*draw)(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data);
	void (*resize)(int width, int height, void *user_data);
	void (*mouse_enter)(void *user_data);
	void (*mouse_leave)(void *user_data);
	void (*mouse_move)(float x, float y, void *user_data);
	void (*mouse_button_press)(float x, float y, int button, int modifiers, void *user_data);
	void (*mouse_button_release)(float x, float y, int button, void *user_data);
	void (*double_click)(float x, float y, int button, int modifiers, void *user_data);
	void (*scroll)(float dx, float dy, void *user_data);
	void (*key_press)(int key, int scan_code, int modifiers, void *user_data);
	void (*key_release)(int key, int scan_code, void *user_data);
	void (*text)(char const *s, void *user_data);
};
struct gral_timer;
struct gral_file;


/*================
    APPLICATION
 ================*/

struct gral_application *gral_application_create(char const *id, struct gral_application_interface const *interface, void *user_data);
void gral_application_delete(struct gral_application *application);
int gral_application_run(struct gral_application *application, int argc, char **argv);


/*============
    DRAWING
 ============*/

struct gral_text *gral_text_create(struct gral_window *window, char const *text, float size);
void gral_text_delete(struct gral_text *text);
void gral_text_set_bold(struct gral_text *text, int start_index, int end_index);
void gral_text_set_italic(struct gral_text *text, int start_index, int end_index);
void gral_text_set_color(struct gral_text *text, int start_index, int end_index, float red, float green, float blue, float alpha);
float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context);
float gral_text_index_to_x(struct gral_text *text, int index);
int gral_text_x_to_index(struct gral_text *text, float x);

void gral_font_get_metrics(struct gral_window *window, float size, float *ascent, float *descent);

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha);
void gral_draw_context_close_path(struct gral_draw_context *draw_context);
void gral_draw_context_move_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_line_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_curve_to(struct gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y);
void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha);
void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count);
void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha);
void gral_draw_context_draw_clipped(struct gral_draw_context *draw_context, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data);
void gral_draw_context_draw_transformed(struct gral_draw_context *draw_context, float a, float b, float c, float d, float e, float f, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data);


/*===========
    WINDOW
 ===========*/

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, char const *title, struct gral_window_interface const *interface, void *user_data);
void gral_window_delete(struct gral_window *window);
void gral_window_request_redraw(struct gral_window *window, int x, int y, int width, int height);
void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height);
void gral_window_set_cursor(struct gral_window *window, int cursor);
void gral_window_warp_cursor(struct gral_window *window, float x, float y);
void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data);
void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data);
void gral_window_clipboard_copy(struct gral_window *window, char const *text);
void gral_window_clipboard_paste(struct gral_window *window, void (*callback)(char const *text, void *user_data), void *user_data);
struct gral_timer *gral_window_create_timer(struct gral_window *window, int milliseconds, int (*callback)(void *user_data), void *user_data);
void gral_window_delete_timer(struct gral_window *window, struct gral_timer *timer);
void gral_window_run_on_main_thread(struct gral_window *window, void (*callback)(void *user_data), void *user_data);


/*=========
    FILE
 =========*/

struct gral_file *gral_file_open_read(char const *path);
struct gral_file *gral_file_open_write(char const *path);
struct gral_file *gral_file_get_stdin(void);
struct gral_file *gral_file_get_stdout(void);
struct gral_file *gral_file_get_stderr(void);
void gral_file_close(struct gral_file *file);
size_t gral_file_read(struct gral_file *file, void *buffer, size_t size);
void gral_file_write(struct gral_file *file, void const *buffer, size_t size);
size_t gral_file_get_size(struct gral_file *file);


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int (*callback)(float *buffer, int frames, void *user_data), void *user_data);

#ifdef __cplusplus
}
#endif

#endif
