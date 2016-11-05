/*

Copyright (c) 2016, Elias Aebi
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

void gral_init(int *argc, char ***argv) {
	gtk_init(argc, argv);
}

int gral_run(void) {
	gtk_main();
	return 0;
}


/*=============
    PAINTING
 =============*/

void gral_painter_new_path(struct gral_painter *painter) {
	cairo_new_path((cairo_t *)painter);
}

void gral_painter_move_to(struct gral_painter *painter, float x, float y) {
	cairo_move_to((cairo_t *)painter, x, y);
}

void gral_painter_close_path(struct gral_painter *painter) {
	cairo_close_path((cairo_t *)painter);
}

void gral_painter_line_to(struct gral_painter *painter, float x, float y) {
	cairo_line_to((cairo_t *)painter, x, y);
}

void gral_painter_curve_to(struct gral_painter *painter, float x1, float y1, float x2, float y2, float x, float y) {
	cairo_curve_to((cairo_t *)painter, x1, y1, x2, y2, x, y);
}

void gral_painter_fill(struct gral_painter *painter, float red, float green, float blue, float alpha) {
	cairo_set_source_rgba((cairo_t *)painter, red, green, blue, alpha);
	cairo_fill_preserve((cairo_t *)painter);
}

void gral_painter_stroke(struct gral_painter *painter, float line_width, float red, float green, float blue, float alpha) {
	cairo_set_line_width((cairo_t *)painter, line_width);
	cairo_set_line_cap((cairo_t *)painter, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join((cairo_t *)painter, CAIRO_LINE_JOIN_ROUND);
	cairo_set_source_rgba((cairo_t *)painter, red, green, blue, alpha);
	cairo_stroke_preserve((cairo_t *)painter);
}

void gral_painter_set_color(struct gral_painter *painter, float red, float green, float blue, float alpha) {
	cairo_t *cr = (cairo_t *)painter;
	cairo_set_source_rgba(cr, red, green, blue, alpha);
}

void gral_painter_draw_rectangle(struct gral_painter *painter, float x, float y, float width, float height) {
	cairo_t *cr = (cairo_t *)painter;
	cairo_rectangle(cr, x, y, width, height);
	cairo_fill(cr);
}


/*===========
    WINDOW
 ===========*/

#define GRAL_TYPE_WINDOW gral_window_get_type()
#define GRAL_WINDOW(obj) G_TYPE_CHECK_INSTANCE_CAST((obj), GRAL_TYPE_WINDOW, GralWindow)
struct _GralWindowClass {
	GtkWindowClass parent_class;
};
struct _GralWindow {
	GtkWindow parent_instance;
	struct gral_window_interface interface;
	void *user_data;
};
typedef struct _GralWindowClass GralWindowClass;
typedef struct _GralWindow GralWindow;
G_DEFINE_TYPE(GralWindow, gral_window, GTK_TYPE_WINDOW)

static gboolean gral_window_delete_event(GtkWidget *widget, GdkEventAny *event) {
	GralWindow *window = GRAL_WINDOW(widget);
	return !window->interface.close(window->user_data);
}
static void gral_window_destroy(GtkWidget *widget) {
	GTK_WIDGET_CLASS(gral_window_parent_class)->destroy(widget);
	gtk_main_quit();
}
static void gral_window_init(GralWindow *window) {

}
static void gral_window_class_init(GralWindowClass *class) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	widget_class->delete_event = gral_window_delete_event;
	widget_class->destroy = gral_window_destroy;
}
static GralWindow *gral_window_new(void) {
	return g_object_new(GRAL_TYPE_WINDOW, "type", GTK_WINDOW_TOPLEVEL, NULL);
}

#define GRAL_TYPE_AREA gral_area_get_type()
#define GRAL_AREA(obj) G_TYPE_CHECK_INSTANCE_CAST((obj), GRAL_TYPE_AREA, GralArea)
struct _GralAreaClass {
	GtkDrawingAreaClass parent_class;
};
struct _GralArea {
	GtkDrawingArea parent_instance;
	struct gral_window_interface interface;
	void *user_data;
};
typedef struct _GralAreaClass GralAreaClass;
typedef struct _GralArea GralArea;
G_DEFINE_TYPE(GralArea, gral_area, GTK_TYPE_DRAWING_AREA)

static gboolean gral_area_draw(GtkWidget *widget, cairo_t *cr) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.draw((struct gral_painter *)cr, window->user_data);
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
static void gral_area_init(GralArea *area) {
	gtk_widget_add_events(GTK_WIDGET(area), GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_SCROLL_MASK);
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
}
static GtkWidget *gral_area_new() {
	return g_object_new(GRAL_TYPE_AREA, NULL);
}

struct gral_window *gral_window_create(int width, int height, const char *title, struct gral_window_interface *interface, void *user_data) {
	GralWindow *window = gral_window_new();
	window->interface = *interface;
	window->user_data = user_data;
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_set_title(GTK_WINDOW(window), title);
	GtkWidget *area = gral_area_new();
	gtk_container_add(GTK_CONTAINER(window), area);
	gtk_widget_show_all(GTK_WIDGET(window));
	return (struct gral_window *)window;
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
