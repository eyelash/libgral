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

static gboolean window_delete_event_callback(GtkWidget *window, GdkEvent *event, gpointer user_data) {
	int (*close)(void) = user_data;
	return !close();
}

static gboolean area_draw_callback(GtkWidget *area, cairo_t *cr, gpointer user_data) {
	void (*draw)(cairo_t *) = user_data;
	draw(cr);
	return GDK_EVENT_STOP;
}

static void area_size_allocate_callback(GtkWidget *area, GdkRectangle *allocation, gpointer user_data) {
	void (*resize)(int width, int height) = user_data;
	resize(allocation->width, allocation->height);
}

struct gral_window *gral_window_create(int width, int height, const char *title, struct gral_window_interface *handler) {
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_set_title(GTK_WINDOW(window), title);
	g_signal_connect(window, "delete-event", G_CALLBACK(window_delete_event_callback), handler->close);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	GtkWidget *area = gtk_drawing_area_new();
	g_signal_connect(area, "draw", G_CALLBACK(area_draw_callback), handler->draw);
	g_signal_connect(area, "size-allocate", G_CALLBACK(area_size_allocate_callback), handler->resize);
	gtk_container_add(GTK_CONTAINER(window), area);
	gtk_widget_show_all(window);
	return (struct gral_window *)window;
}
