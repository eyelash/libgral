/*

Copyright (c) 2016-2025 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef GRAL_H
#define GRAL_H

#include <stddef.h>

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
enum {
	GRAL_FILE_TYPE_REGULAR,
	GRAL_FILE_TYPE_DIRECTORY,
	GRAL_FILE_TYPE_INVALID
};

struct gral_application;
struct gral_application_interface {
	void (*start)(void *user_data);
	void (*open_empty)(void *user_data);
	void (*open_file)(char const *path, void *user_data);
	void (*quit)(void *user_data);
};
struct gral_image;
struct gral_font;
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
	void (*destroy)(void *user_data);
	int (*close)(void *user_data);
	void (*draw)(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data);
	void (*resize)(int width, int height, void *user_data);
	void (*mouse_enter)(void *user_data);
	void (*mouse_leave)(void *user_data);
	void (*mouse_move)(float x, float y, void *user_data);
	void (*mouse_move_relative)(float dx, float dy, void *user_data);
	void (*mouse_button_press)(float x, float y, int button, int modifiers, void *user_data);
	void (*mouse_button_release)(float x, float y, int button, void *user_data);
	void (*double_click)(float x, float y, int button, int modifiers, void *user_data);
	void (*scroll)(float dx, float dy, void *user_data);
	void (*key_press)(int key, int key_code, int modifiers, int is_repeat, void *user_data);
	void (*key_release)(int key, int key_code, void *user_data);
	void (*text)(char const *s, void *user_data);
	void (*focus_enter)(void *user_data);
	void (*focus_leave)(void *user_data);
	void (*activate_menu_item)(int id, void *user_data);
};
struct gral_menu;
struct gral_timer;
struct gral_file;
struct gral_directory_watcher;
struct gral_audio;
struct gral_midi;
struct gral_midi_interface {
	void (*note_on)(unsigned char note, unsigned char velocity, void *user_data);
	void (*note_off)(unsigned char note, unsigned char velocity, void *user_data);
	void (*control_change)(unsigned char controller, unsigned char value, void *user_data);
};


/*================
    APPLICATION
 ================*/

struct gral_application *gral_application_create(char const *id, struct gral_application_interface const *interface, void *user_data);
void gral_application_delete(struct gral_application *application);
int gral_application_run(struct gral_application *application, int argc, char **argv);


/*============
    DRAWING
 ============*/

struct gral_image *gral_image_create(int width, int height, void *data);
void gral_image_delete(struct gral_image *image);

struct gral_font *gral_font_create(struct gral_window *window, char const *name, float size);
struct gral_font *gral_font_create_default(struct gral_window *window, float size);
struct gral_font *gral_font_create_monospace(struct gral_window *window, float size);
void gral_font_delete(struct gral_font *font);
void gral_font_get_metrics(struct gral_window *window, struct gral_font *font, float *ascent, float *descent);
struct gral_text *gral_text_create(struct gral_window *window, char const *text, struct gral_font *font);
void gral_text_delete(struct gral_text *text);
void gral_text_set_bold(struct gral_text *text, int start_index, int end_index);
void gral_text_set_italic(struct gral_text *text, int start_index, int end_index);
void gral_text_set_color(struct gral_text *text, int start_index, int end_index, float red, float green, float blue, float alpha);
float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context);
float gral_text_index_to_x(struct gral_text *text, int index);
int gral_text_x_to_index(struct gral_text *text, float x);

void gral_draw_context_draw_image(struct gral_draw_context *draw_context, struct gral_image *image, float x, float y);
void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha);
void gral_draw_context_add_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y);
void gral_draw_context_close_path(struct gral_draw_context *draw_context);
void gral_draw_context_move_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_line_to(struct gral_draw_context *draw_context, float x, float y);
void gral_draw_context_curve_to(struct gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y);
void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha);
void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count);
void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha);
void gral_draw_context_stroke_linear_gradient(struct gral_draw_context *draw_context, float line_width, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count);
void gral_draw_context_draw_clipped(struct gral_draw_context *draw_context, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data);
void gral_draw_context_draw_transformed(struct gral_draw_context *draw_context, float a, float b, float c, float d, float e, float f, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data);


/*===========
    WINDOW
 ===========*/

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, char const *title, struct gral_window_interface const *interface, void *user_data);
void gral_window_show(struct gral_window *window);
void gral_window_set_title(struct gral_window *window, char const *title);
void gral_window_request_redraw(struct gral_window *window, int x, int y, int width, int height);
void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height);
void gral_window_set_cursor(struct gral_window *window, int cursor);
void gral_window_hide_cursor(struct gral_window *window);
void gral_window_show_cursor(struct gral_window *window);
void gral_window_warp_cursor(struct gral_window *window, float x, float y);
void gral_window_lock_pointer(struct gral_window *window);
void gral_window_unlock_pointer(struct gral_window *window);
void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data);
void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data);
void gral_window_clipboard_copy(struct gral_window *window, char const *text);
void gral_window_clipboard_paste(struct gral_window *window, void (*callback)(char const *text, void *user_data), void *user_data);
void gral_window_show_context_menu(struct gral_window *window, struct gral_menu *menu, float x, float y);

struct gral_menu *gral_menu_create(void);
void gral_menu_delete(struct gral_menu *menu);
void gral_menu_append_item(struct gral_menu *menu, char const *text, int id);
void gral_menu_append_separator(struct gral_menu *menu);
void gral_menu_append_submenu(struct gral_menu *menu, char const *text, struct gral_menu *submenu);

struct gral_timer *gral_timer_create(int milliseconds, void (*callback)(void *user_data), void *user_data);
void gral_timer_delete(struct gral_timer *timer);

void gral_run_on_main_thread(void (*callback)(void *user_data), void *user_data);


/*=========
    FILE
 =========*/

struct gral_file *gral_file_open_read(char const *path);
struct gral_file *gral_file_open_write(char const *path);
struct gral_file *gral_file_get_standard_input(void);
struct gral_file *gral_file_get_standard_output(void);
struct gral_file *gral_file_get_standard_error(void);
void gral_file_close(struct gral_file *file);
size_t gral_file_read(struct gral_file *file, void *buffer, size_t size);
void gral_file_write(struct gral_file *file, void const *buffer, size_t size);
size_t gral_file_get_size(struct gral_file *file);
void *gral_file_map(struct gral_file *file, size_t size);
void gral_file_unmap(void *address, size_t size);
int gral_file_get_type(char const *path);
void gral_file_rename(char const *old_path, char const *new_path);
void gral_file_remove(char const *path);
void gral_directory_create(char const *path);
void gral_directory_iterate(char const *path, void (*callback)(char const *name, void *user_data), void *user_data);
void gral_directory_remove(char const *path);
char *gral_get_current_working_directory(void);
struct gral_directory_watcher *gral_directory_watch(char const *path, void (*callback)(void *user_data), void *user_data);
void gral_directory_watcher_delete(struct gral_directory_watcher *directory_watcher);


/*=========
    TIME
 =========*/

double gral_time_get_monotonic(void);
void gral_sleep(double seconds);


/*==========
    AUDIO
 ==========*/

struct gral_audio *gral_audio_create(char const *name, void (*callback)(float *buffer, int frames, void *user_data), void *user_data);
void gral_audio_delete(struct gral_audio *audio);


/*=========
    MIDI
 =========*/

struct gral_midi *gral_midi_create(struct gral_application *application, char const *name, struct gral_midi_interface const *interface, void *user_data);
void gral_midi_delete(struct gral_midi *midi);


#ifdef __cplusplus
}
#endif

#endif
