/*

Copyright (c) 2016-2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "gral.h"
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>


/*================
    APPLICATION
 ================*/

#define GRAL_TYPE_APPLICATION gral_application_get_type()
#define GRAL_APPLICATION(obj) G_TYPE_CHECK_INSTANCE_CAST((obj), GRAL_TYPE_APPLICATION, GralApplication)
struct _GralApplicationClass {
	GtkApplicationClass parent_class;
};
struct gral_application {
	GtkWindow parent_instance;
	struct gral_application_interface interface;
	void *user_data;
};
typedef struct _GralApplicationClass GralApplicationClass;
typedef struct gral_application GralApplication;
G_DEFINE_TYPE(GralApplication, gral_application, GTK_TYPE_APPLICATION)

static void gral_application_activate(GApplication *gapplication) {
	G_APPLICATION_CLASS(gral_application_parent_class)->activate(gapplication);
	GralApplication *application = GRAL_APPLICATION(gapplication);
	application->interface.initialize(application->user_data);
}
static void gral_application_init(GralApplication *application) {

}
static void gral_application_class_init(GralApplicationClass *class) {
	GApplicationClass *application_class = G_APPLICATION_CLASS(class);
	application_class->activate = gral_application_activate;
}

struct gral_application *gral_application_create(const char *id, struct gral_application_interface *interface, void *user_data) {
	GralApplication *application = g_object_new(GRAL_TYPE_APPLICATION, "application-id", id, NULL);
	application->interface = *interface;
	application->user_data = user_data;
	return application;
}

void gral_application_delete(struct gral_application *application) {
	g_object_unref(application);
}

int gral_application_run(struct gral_application *application, int argc, char **argv) {
	return g_application_run(G_APPLICATION(application), argc, argv);
}


/*============
    DRAWING
 ============*/

struct gral_text *gral_text_create(struct gral_window *window, const char *text, float size) {
	PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(window), text);
	PangoFontDescription *font_description = pango_font_description_new();
	pango_font_description_set_absolute_size(font_description, size*PANGO_SCALE);
	pango_layout_set_font_description(layout, font_description);
	pango_font_description_free(font_description);
	return (struct gral_text *)layout;
}

void gral_text_delete(struct gral_text *text) {
	g_object_unref(text);
}

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	cairo_move_to((cairo_t *)draw_context, x, y);
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	PangoLayoutLine *line = pango_layout_get_line_readonly((PangoLayout *)text, 0);
	pango_cairo_show_layout_line((cairo_t *)draw_context, line);
}

void gral_draw_context_fill_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height, float red, float green, float blue, float alpha) {
	cairo_rectangle((cairo_t *)draw_context, x, y, width, height);
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	cairo_fill((cairo_t *)draw_context);
}

void gral_draw_context_new_path(struct gral_draw_context *draw_context) {
	cairo_new_path((cairo_t *)draw_context);
}

void gral_draw_context_close_path(struct gral_draw_context *draw_context) {
	cairo_close_path((cairo_t *)draw_context);
}

void gral_draw_context_move_to(struct gral_draw_context *draw_context, float x, float y) {
	cairo_move_to((cairo_t *)draw_context, x, y);
}

void gral_draw_context_line_to(struct gral_draw_context *draw_context, float x, float y) {
	cairo_line_to((cairo_t *)draw_context, x, y);
}

void gral_draw_context_curve_to(struct gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y) {
	cairo_curve_to((cairo_t *)draw_context, x1, y1, x2, y2, x, y);
}

void gral_draw_context_add_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height) {
	cairo_rectangle((cairo_t *)draw_context, x, y, width, height);
}

void gral_draw_context_add_arc(struct gral_draw_context *draw_context, float cx, float cy, float radius, float start_angle, float end_angle) {
	cairo_arc((cairo_t *)draw_context, cx, cy, radius, start_angle, end_angle);
}

void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	cairo_fill((cairo_t *)draw_context);
}

void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop *stops, int count) {
	cairo_pattern_t *gradient = cairo_pattern_create_linear(start_x, start_y, end_x, end_y);
	int i;
	for (i = 0; i < count; i++) {
		cairo_pattern_add_color_stop_rgba(gradient, stops[i].position, stops[i].red, stops[i].green, stops[i].blue, stops[i].alpha);
	}
	cairo_set_source((cairo_t *)draw_context, gradient);
	cairo_fill((cairo_t *)draw_context);
	cairo_pattern_destroy(gradient);
}

void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha) {
	cairo_set_line_width((cairo_t *)draw_context, line_width);
	cairo_set_line_cap((cairo_t *)draw_context, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join((cairo_t *)draw_context, CAIRO_LINE_JOIN_ROUND);
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	cairo_stroke((cairo_t *)draw_context);
}

void gral_draw_context_push_clip(struct gral_draw_context *draw_context) {
	cairo_save((cairo_t *)draw_context);
	cairo_clip((cairo_t *)draw_context);
}

void gral_draw_context_pop_clip(struct gral_draw_context *draw_context) {
	cairo_restore((cairo_t *)draw_context);
	cairo_new_path((cairo_t *)draw_context);
}


/*===========
    WINDOW
 ===========*/

#define GRAL_TYPE_WINDOW gral_window_get_type()
#define GRAL_WINDOW(obj) G_TYPE_CHECK_INSTANCE_CAST((obj), GRAL_TYPE_WINDOW, GralWindow)
struct _GralWindowClass {
	GtkApplicationWindowClass parent_class;
};
struct gral_window {
	GtkApplicationWindow parent_instance;
	struct gral_window_interface interface;
	void *user_data;
};
typedef struct _GralWindowClass GralWindowClass;
typedef struct gral_window GralWindow;
G_DEFINE_TYPE(GralWindow, gral_window, GTK_TYPE_APPLICATION_WINDOW)

static gboolean gral_window_delete_event(GtkWidget *widget, GdkEventAny *event) {
	GralWindow *window = GRAL_WINDOW(widget);
	return !window->interface.close(window->user_data);
}
static void gral_window_init(GralWindow *window) {

}
static void gral_window_class_init(GralWindowClass *class) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	widget_class->delete_event = gral_window_delete_event;
}

#define GRAL_TYPE_AREA gral_area_get_type()
#define GRAL_AREA(obj) G_TYPE_CHECK_INSTANCE_CAST((obj), GRAL_TYPE_AREA, GralArea)
struct _GralAreaClass {
	GtkDrawingAreaClass parent_class;
};
struct _GralArea {
	GtkDrawingArea parent_instance;
	GtkIMContext *im_context;
};
typedef struct _GralAreaClass GralAreaClass;
typedef struct _GralArea GralArea;
G_DEFINE_TYPE(GralArea, gral_area, GTK_TYPE_DRAWING_AREA)

static gboolean gral_area_draw(GtkWidget *widget, cairo_t *cr) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.draw((struct gral_draw_context *)cr, window->user_data);
	return GDK_EVENT_STOP;
}
static void gral_area_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
	GTK_WIDGET_CLASS(gral_area_parent_class)->size_allocate(widget, allocation);
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.resize(allocation->width, allocation->height, window->user_data);
}
static gboolean gral_area_enter_notify_event(GtkWidget *widget, GdkEventCrossing *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.mouse_enter(window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_leave_notify_event(GtkWidget *widget, GdkEventCrossing *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.mouse_leave(window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_motion_notify_event(GtkWidget *widget, GdkEventMotion *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.mouse_move(event->x, event->y, window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_button_press_event(GtkWidget *widget, GdkEventButton *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.mouse_button_press(event->button, window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_button_release_event(GtkWidget *widget, GdkEventButton *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.mouse_button_release(event->button, window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_scroll_event(GtkWidget *widget, GdkEventScroll *event) {
	// TODO: implement
	return GDK_EVENT_STOP;
}
static gboolean gral_area_key_press_event(GtkWidget *widget, GdkEventKey *event) {
	GralArea *area = GRAL_AREA(widget);
	gtk_im_context_filter_keypress(area->im_context, event);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_key_release_event(GtkWidget *widget, GdkEventKey *event) {
	GralArea *area = GRAL_AREA(widget);
	gtk_im_context_filter_keypress(area->im_context, event);
	return GDK_EVENT_STOP;
}
static void gral_area_commit(GtkIMContext *context, gchar *str, gpointer user_data) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(GTK_WIDGET(user_data)));
	while (*str != '\0') {
		window->interface.character(g_utf8_get_char(str), window->user_data);
		str = g_utf8_next_char(str);
	}
}
static void gral_area_dispose(GObject *object) {
	GralArea *area = GRAL_AREA(object);
	g_clear_object(&area->im_context);
	G_OBJECT_CLASS(gral_area_parent_class)->dispose(object);
}
static void gral_area_init(GralArea *area) {
	area->im_context = gtk_im_multicontext_new();
	g_signal_connect(area->im_context, "commit", G_CALLBACK(gral_area_commit), area);
	gtk_widget_add_events(GTK_WIDGET(area), GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_SCROLL_MASK);
	gtk_widget_set_can_focus(GTK_WIDGET(area), TRUE);
}
static void gral_area_class_init(GralAreaClass *class) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	widget_class->draw = gral_area_draw;
	widget_class->size_allocate = gral_area_size_allocate;
	widget_class->enter_notify_event = gral_area_enter_notify_event;
	widget_class->leave_notify_event = gral_area_leave_notify_event;
	widget_class->motion_notify_event = gral_area_motion_notify_event;
	widget_class->button_press_event = gral_area_button_press_event;
	widget_class->button_release_event = gral_area_button_release_event;
	widget_class->scroll_event = gral_area_scroll_event;
	widget_class->key_press_event = gral_area_key_press_event;
	widget_class->key_release_event = gral_area_key_release_event;
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	object_class->dispose = gral_area_dispose;
}

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, const char *title, struct gral_window_interface *interface, void *user_data) {
	GralWindow *window = g_object_new(GRAL_TYPE_WINDOW, "application", application, NULL);
	g_object_ref_sink(window);
	window->interface = *interface;
	window->user_data = user_data;
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_set_title(GTK_WINDOW(window), title);
	GtkWidget *area = g_object_new(GRAL_TYPE_AREA, NULL);
	gtk_container_add(GTK_CONTAINER(window), area);
	gtk_widget_show_all(GTK_WIDGET(window));
	return window;
}

void gral_window_delete(struct gral_window *window) {
	g_object_unref(window);
}

void gral_window_request_redraw(struct gral_window *window) {
	gtk_widget_queue_draw(GTK_WIDGET(window));
}

void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Open", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		callback(filename, user_data);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}

void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Save", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		callback(filename, user_data);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}

void gral_window_clipboard_copy(struct gral_window *window, const char *text) {
	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, text, -1);
}

static void gral_window_text_received(GtkClipboard *clipboard, const gchar *text, gpointer data) {
	GralWindow *window = GRAL_WINDOW(data);
	window->interface.paste(text, window->user_data);
}
void gral_window_clipboard_request_paste(struct gral_window *window) {
	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_text(clipboard, gral_window_text_received, window);
}


/*==========
    AUDIO
 ==========*/

#define FRAMES 1024

static void play_buffer(snd_pcm_t *pcm, int16_t *buffer, int frames) {
	while (frames > 0) {
		int frames_written = snd_pcm_writei(pcm, buffer, frames);
		if (frames_written < 0) {
			fprintf(stderr, "audio error: %s\n", snd_strerror(frames_written));
			snd_pcm_recover(pcm, frames_written, 0);
			snd_pcm_prepare(pcm);
			continue;
		}
		buffer += 2 * frames_written;
		frames -= frames_written;
	}
}

void gral_audio_play(int (*callback)(int16_t *buffer, int frames)) {
	snd_pcm_t *pcm;
	snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
	snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 44100, 1, 1000000*FRAMES/44100);
	snd_pcm_prepare(pcm);
	int16_t buffer[FRAMES*2];
	while (callback(buffer, FRAMES)) {
		play_buffer(pcm, buffer, FRAMES);
	}
	snd_pcm_drain(pcm);
	snd_pcm_close(pcm);
}
