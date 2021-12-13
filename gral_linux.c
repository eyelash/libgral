/*

Copyright (c) 2016-2021 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>


/*================
    APPLICATION
 ================*/

#define GRAL_TYPE_APPLICATION gral_application_get_type()
G_DECLARE_FINAL_TYPE(GralApplication, gral_application, GRAL, APPLICATION, GtkApplication)
struct _GralApplication {
	GtkApplication parent_instance;
	struct gral_application_interface interface;
	void *user_data;
};
G_DEFINE_TYPE(GralApplication, gral_application, GTK_TYPE_APPLICATION)

static void gral_application_activate(GApplication *gapplication) {
	GralApplication *application = GRAL_APPLICATION(gapplication);
	application->interface.open_empty(application->user_data);
}
static void gral_application_open(GApplication *gapplication, GFile **files, gint n_files, const gchar *hint) {
	GralApplication *application = GRAL_APPLICATION(gapplication);
	for (gint i = 0; i < n_files; i++) {
		char *path = g_file_get_path(files[i]);
		application->interface.open_file(path, application->user_data);
		g_free(path);
	}
}
static void gral_application_shutdown(GApplication *gapplication) {
	G_APPLICATION_CLASS(gral_application_parent_class)->shutdown(gapplication);
	GralApplication *application = GRAL_APPLICATION(gapplication);
	application->interface.quit(application->user_data);
}
static void gral_application_init(GralApplication *application) {

}
static void gral_application_class_init(GralApplicationClass *class) {
	GApplicationClass *application_class = G_APPLICATION_CLASS(class);
	application_class->activate = gral_application_activate;
	application_class->open = gral_application_open;
	application_class->shutdown = gral_application_shutdown;
}

struct gral_application *gral_application_create(char const *id, struct gral_application_interface const *interface, void *user_data) {
	GralApplication *application = g_object_new(GRAL_TYPE_APPLICATION, "application-id", id, "flags", G_APPLICATION_NON_UNIQUE|G_APPLICATION_HANDLES_OPEN, NULL);
	application->interface = *interface;
	application->user_data = user_data;
	return (struct gral_application *)application;
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

struct gral_font *gral_font_create(struct gral_window *window, char const *name, float size) {
	PangoContext *context = gtk_widget_get_pango_context(GTK_WIDGET(window));
	PangoFontDescription *font = pango_font_description_copy(pango_context_get_font_description(context));
	pango_font_description_set_family(font, name);
	pango_font_description_set_absolute_size(font, pango_units_from_double(size));
	return (struct gral_font *)font;
}

struct gral_font *gral_font_create_default(struct gral_window *window, float size) {
	PangoContext *context = gtk_widget_get_pango_context(GTK_WIDGET(window));
	PangoFontDescription *font = pango_font_description_copy(pango_context_get_font_description(context));
	pango_font_description_set_absolute_size(font, pango_units_from_double(size));
	return (struct gral_font *)font;
}

struct gral_font *gral_font_create_monospace(struct gral_window *window, float size) {
	PangoContext *context = gtk_widget_get_pango_context(GTK_WIDGET(window));
	PangoFontDescription *font = pango_font_description_copy(pango_context_get_font_description(context));
	pango_font_description_set_family_static(font, "monospace");
	pango_font_description_set_absolute_size(font, pango_units_from_double(size));
	return (struct gral_font *)font;
}

void gral_font_delete(struct gral_font *font) {
	pango_font_description_free((PangoFontDescription *)font);
}

void gral_font_get_metrics(struct gral_window *window, struct gral_font *font, float *ascent, float *descent) {
	PangoContext *context = gtk_widget_get_pango_context(GTK_WIDGET(window));
	PangoFontMetrics *metrics = pango_context_get_metrics(context, (PangoFontDescription *)font, NULL);
	if (ascent) *ascent = pango_units_to_double(pango_font_metrics_get_ascent(metrics));
	if (descent) *descent = pango_units_to_double(pango_font_metrics_get_descent(metrics));
	pango_font_metrics_unref(metrics);
}

struct gral_text *gral_text_create(struct gral_window *window, char const *text, struct gral_font *font) {
	PangoContext *context = gtk_widget_get_pango_context(GTK_WIDGET(window));
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_font_description(layout, (PangoFontDescription *)font);
	pango_layout_set_attributes(layout, pango_attr_list_new());
	return (struct gral_text *)layout;
}

void gral_text_delete(struct gral_text *text) {
	g_object_unref(text);
}

void gral_text_set_bold(struct gral_text *text, int start_index, int end_index) {
	PangoAttribute *attribute = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
	attribute->start_index = start_index;
	attribute->end_index = end_index;
	PangoAttrList *attr_list = pango_layout_get_attributes(PANGO_LAYOUT(text));
	pango_attr_list_change(attr_list, attribute);
}

void gral_text_set_italic(struct gral_text *text, int start_index, int end_index) {
	PangoAttribute *attribute = pango_attr_style_new(PANGO_STYLE_ITALIC);
	attribute->start_index = start_index;
	attribute->end_index = end_index;
	PangoAttrList *attr_list = pango_layout_get_attributes(PANGO_LAYOUT(text));
	pango_attr_list_change(attr_list, attribute);
}

void gral_text_set_color(struct gral_text *text, int start_index, int end_index, float red, float green, float blue, float alpha) {
	PangoAttribute *attribute = pango_attr_foreground_new(red * 65535.f, green * 65535.f, blue * 65535.f);
	attribute->start_index = start_index;
	attribute->end_index = end_index;
	PangoAttrList *attr_list = pango_layout_get_attributes(PANGO_LAYOUT(text));
	pango_attr_list_change(attr_list, attribute);
	attribute = pango_attr_foreground_alpha_new(alpha * 65535.f);
	attribute->start_index = start_index;
	attribute->end_index = end_index;
	pango_attr_list_change(attr_list, attribute);
}

float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context) {
	PangoRectangle extents;
	pango_layout_get_extents(PANGO_LAYOUT(text), &extents, NULL);
	return pango_units_to_double(extents.width);
}

float gral_text_index_to_x(struct gral_text *text, int index) {
	PangoLayoutLine *line = pango_layout_get_line_readonly(PANGO_LAYOUT(text), 0);
	int x;
	pango_layout_line_index_to_x(line, index, FALSE, &x);
	return pango_units_to_double(x);
}

int gral_text_x_to_index(struct gral_text *text, float x) {
	PangoLayoutLine *line = pango_layout_get_line_readonly(PANGO_LAYOUT(text), 0);
	int index, trailing;
	pango_layout_line_x_to_index(line, pango_units_from_double(x), &index, &trailing);
	char const *str = pango_layout_get_text(PANGO_LAYOUT(text));
	return g_utf8_offset_to_pointer(str + index, trailing) - str;
}

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	cairo_move_to((cairo_t *)draw_context, x, y);
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	PangoLayoutLine *line = pango_layout_get_line_readonly(PANGO_LAYOUT(text), 0);
	pango_cairo_show_layout_line((cairo_t *)draw_context, line);
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

void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	cairo_fill((cairo_t *)draw_context);
}

void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count) {
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

void gral_draw_context_draw_clipped(struct gral_draw_context *draw_context, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data) {
	cairo_save((cairo_t *)draw_context);
	cairo_clip((cairo_t *)draw_context);
	callback(draw_context, user_data);
	cairo_restore((cairo_t *)draw_context);
	cairo_new_path((cairo_t *)draw_context);
}

void gral_draw_context_draw_transformed(struct gral_draw_context *draw_context, float a, float b, float c, float d, float e, float f, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data) {
	cairo_save((cairo_t *)draw_context);
	cairo_matrix_t matrix = {a, b, c, d, e, f};
	cairo_transform((cairo_t *)draw_context, &matrix);
	callback(draw_context, user_data);
	cairo_restore((cairo_t *)draw_context);
}


/*===========
    WINDOW
 ===========*/

static int get_modifiers(guint state) {
	int modifiers = 0;
	if (state & GDK_CONTROL_MASK) modifiers |= GRAL_MODIFIER_CONTROL;
	if (state & GDK_MOD1_MASK) modifiers |= GRAL_MODIFIER_ALT;
	if (state & GDK_SHIFT_MASK) modifiers |= GRAL_MODIFIER_SHIFT;
	return modifiers;
}

#define GRAL_TYPE_WINDOW gral_window_get_type()
G_DECLARE_FINAL_TYPE(GralWindow, gral_window, GRAL, WINDOW, GtkApplicationWindow)
struct _GralWindow {
	GtkApplicationWindow parent_instance;
	struct gral_window_interface interface;
	void *user_data;
};
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
G_DECLARE_FINAL_TYPE(GralArea, gral_area, GRAL, AREA, GtkDrawingArea)
struct _GralArea {
	GtkDrawingArea parent_instance;
	GtkIMContext *im_context;
};
G_DEFINE_TYPE(GralArea, gral_area, GTK_TYPE_DRAWING_AREA)

static gboolean gral_area_draw(GtkWidget *widget, cairo_t *cr) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	GdkRectangle clip_rectangle;
	gdk_cairo_get_clip_rectangle(cr, &clip_rectangle);
	window->interface.draw((struct gral_draw_context *)cr, clip_rectangle.x, clip_rectangle.y, clip_rectangle.width, clip_rectangle.height, window->user_data);
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
	if (event->type == GDK_BUTTON_PRESS) {
		window->interface.mouse_button_press(event->x, event->y, event->button, get_modifiers(event->state), window->user_data);
	}
	else if (event->type == GDK_2BUTTON_PRESS) {
		window->interface.double_click(event->x, event->y, event->button, get_modifiers(event->state), window->user_data);
	}
	return GDK_EVENT_STOP;
}
static gboolean gral_area_button_release_event(GtkWidget *widget, GdkEventButton *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	window->interface.mouse_button_release(event->x, event->y, event->button, window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_area_scroll_event(GtkWidget *widget, GdkEventScroll *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	gdouble delta_x, delta_y;
	gdk_event_get_scroll_deltas((GdkEvent *)event, &delta_x, &delta_y);
	window->interface.scroll(-delta_x, -delta_y, window->user_data);
	return GDK_EVENT_STOP;
}
static int get_key(GdkEventKey *event) {
	switch (event->keyval) {
	case GDK_KEY_Return:
		return GRAL_KEY_ENTER;
	case GDK_KEY_Tab:
		return GRAL_KEY_TAB;
	case GDK_KEY_BackSpace:
		return GRAL_KEY_BACKSPACE;
	case GDK_KEY_Delete:
		return GRAL_KEY_DELETE;
	case GDK_KEY_Left:
		return GRAL_KEY_ARROW_LEFT;
	case GDK_KEY_Up:
		return GRAL_KEY_ARROW_UP;
	case GDK_KEY_Right:
		return GRAL_KEY_ARROW_RIGHT;
	case GDK_KEY_Down:
		return GRAL_KEY_ARROW_DOWN;
	case GDK_KEY_Page_Up:
		return GRAL_KEY_PAGE_UP;
	case GDK_KEY_Page_Down:
		return GRAL_KEY_PAGE_DOWN;
	case GDK_KEY_Home:
		return GRAL_KEY_HOME;
	case GDK_KEY_End:
		return GRAL_KEY_END;
	case GDK_KEY_Escape:
		return GRAL_KEY_ESCAPE;
	default: {
		GdkKeymap *keymap = gdk_keymap_get_for_display(gdk_window_get_display(event->window));
		GdkKeymapKey keymap_key;
		keymap_key.keycode = event->hardware_keycode;
		keymap_key.group = 0;
		keymap_key.level = 0;
		return gdk_keyval_to_unicode(gdk_keymap_lookup_key(keymap, &keymap_key));
	}
	}
}
static gboolean gral_area_key_press_event(GtkWidget *widget, GdkEventKey *event) {
	GralArea *area = GRAL_AREA(widget);
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	gtk_im_context_filter_keypress(area->im_context, event);
	int key = get_key(event);
	if (key) {
		window->interface.key_press(key, event->hardware_keycode - 8, get_modifiers(event->state), window->user_data);
	}
	return GDK_EVENT_STOP;
}
static gboolean gral_area_key_release_event(GtkWidget *widget, GdkEventKey *event) {
	GralArea *area = GRAL_AREA(widget);
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(widget));
	gtk_im_context_filter_keypress(area->im_context, event);
	int key = get_key(event);
	if (key) {
		window->interface.key_release(key, event->hardware_keycode - 8, window->user_data);
	}
	return GDK_EVENT_STOP;
}
static void gral_area_commit(GtkIMContext *context, gchar *str, gpointer user_data) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_parent(GTK_WIDGET(user_data)));
	window->interface.text(str, window->user_data);
}
static void gral_area_dispose(GObject *object) {
	GralArea *area = GRAL_AREA(object);
	g_clear_object(&area->im_context);
	G_OBJECT_CLASS(gral_area_parent_class)->dispose(object);
}
static void gral_area_init(GralArea *area) {
	area->im_context = gtk_im_multicontext_new();
	g_signal_connect_object(area->im_context, "commit", G_CALLBACK(gral_area_commit), area, 0);
	gtk_widget_add_events(GTK_WIDGET(area), GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_SCROLL_MASK|GDK_SMOOTH_SCROLL_MASK);
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

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, char const *title, struct gral_window_interface const *interface, void *user_data) {
	GralWindow *window = g_object_new(GRAL_TYPE_WINDOW, "application", application, NULL);
	g_object_ref_sink(window);
	window->interface = *interface;
	window->user_data = user_data;
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_set_title(GTK_WINDOW(window), title);
	GtkWidget *area = g_object_new(GRAL_TYPE_AREA, NULL);
	gtk_container_add(GTK_CONTAINER(window), area);
	gtk_widget_show_all(GTK_WIDGET(window));
	return (struct gral_window *)window;
}

void gral_window_delete(struct gral_window *window) {
	g_object_unref(window);
}

void gral_window_set_title(struct gral_window *window, char const *title) {
	gtk_window_set_title(GTK_WINDOW(window), title);
}

void gral_window_request_redraw(struct gral_window *window, int x, int y, int width, int height) {
	GtkWidget *area = gtk_bin_get_child(GTK_BIN(window));
	gtk_widget_queue_draw_area(area, x, y, width, height);
}

void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height) {
	GdkGeometry geometry;
	geometry.min_width = minimum_width;
	geometry.min_height = minimum_height;
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry, GDK_HINT_MIN_SIZE);
}

static char const *get_cursor_name(int cursor) {
	switch (cursor) {
	case GRAL_CURSOR_DEFAULT:
		return "default";
	case GRAL_CURSOR_HAND:
		return "pointer";
	case GRAL_CURSOR_TEXT:
		return "text";
	case GRAL_CURSOR_HORIZONTAL_ARROWS:
		return "ew-resize";
	case GRAL_CURSOR_VERTICAL_ARROWS:
		return "ns-resize";
	case GRAL_CURSOR_NONE:
		return "none";
	}
}
void gral_window_set_cursor(struct gral_window *window, int cursor) {
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
	GdkCursor *gdk_cursor = gdk_cursor_new_from_name(gdk_window_get_display(gdk_window), get_cursor_name(cursor));
	gdk_window_set_cursor(gdk_window, gdk_cursor);
	g_object_unref(gdk_cursor);
}

void gral_window_warp_cursor(struct gral_window *window, float x, float y) {
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
	GdkScreen *screen = gdk_window_get_screen(gdk_window);
	GdkDevice *pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_screen_get_display(screen)));
	gint root_x, root_y;
	gdk_window_get_root_coords(gdk_window, x, y, &root_x, &root_y);
	gdk_device_warp(pointer, screen, root_x, root_y);
}

void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data) {
	GtkFileChooserNative *dialog = gtk_file_chooser_native_new(NULL, GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		callback(filename, user_data);
		g_free(filename);
	}
	g_object_unref(dialog);
}

void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data) {
	GtkFileChooserNative *dialog = gtk_file_chooser_native_new(NULL, GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		callback(filename, user_data);
		g_free(filename);
	}
	g_object_unref(dialog);
}

void gral_window_clipboard_copy(struct gral_window *window, char const *text) {
	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, text, -1);
}

typedef struct {
	void (*callback)(char const *text, void *user_data);
	void *user_data;
} PasteCallbackData;
static void paste_callback(GtkClipboard *clipboard, gchar const *text, gpointer user_data) {
	PasteCallbackData *callback_data = user_data;
	callback_data->callback(text, callback_data->user_data);
	g_slice_free(PasteCallbackData, callback_data);
}
void gral_window_clipboard_paste(struct gral_window *window, void (*callback)(char const *text, void *user_data), void *user_data) {
	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);
	PasteCallbackData *callback_data = g_slice_new(PasteCallbackData);
	callback_data->callback = callback;
	callback_data->user_data = user_data;
	gtk_clipboard_request_text(clipboard, paste_callback, callback_data);
}

typedef struct {
	int (*callback)(void *user_data);
	void (*destroy)(void *user_data);
	void *user_data;
} TimerCallbackData;
static gboolean timer_callback(gpointer user_data) {
	TimerCallbackData *callback_data = user_data;
	return callback_data->callback(callback_data->user_data);
}
static void timer_destroy(gpointer user_data) {
	TimerCallbackData *callback_data = user_data;
	callback_data->destroy(callback_data->user_data);
	g_slice_free(TimerCallbackData, callback_data);
}
struct gral_timer *gral_window_create_timer(struct gral_window *window, int milliseconds, int (*callback)(void *user_data), void (*destroy)(void *user_data), void *user_data) {
	TimerCallbackData *callback_data = g_slice_new(TimerCallbackData);
	callback_data->callback = callback;
	callback_data->destroy = destroy;
	callback_data->user_data = user_data;
	return (struct gral_timer *)(intptr_t)g_timeout_add_full(G_PRIORITY_DEFAULT, milliseconds, timer_callback, callback_data, timer_destroy);
}

void gral_window_delete_timer(struct gral_window *window, struct gral_timer *timer) {
	g_source_remove((guint)(intptr_t)timer);
}

typedef struct {
	void (*callback)(void *user_data);
	void *user_data;
} IdleCallbackData;
static gboolean idle_callback(gpointer user_data) {
	IdleCallbackData *callback_data = user_data;
	callback_data->callback(callback_data->user_data);
	return G_SOURCE_REMOVE;
}
static void idle_destroy(gpointer user_data) {
	g_slice_free(IdleCallbackData, user_data);
}
void gral_window_run_on_main_thread(struct gral_window *window, void (*callback)(void *user_data), void *user_data) {
	IdleCallbackData *callback_data = g_slice_new(IdleCallbackData);
	callback_data->callback = callback;
	callback_data->user_data = user_data;
	gdk_threads_add_idle_full(G_PRIORITY_DEFAULT_IDLE, idle_callback, callback_data, idle_destroy);
}


/*=========
    FILE
 =========*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

struct gral_file *gral_file_open_read(char const *path) {
	int fd = open(path, O_RDONLY);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_open_write(char const *path) {
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_get_stdin(void) {
	return (struct gral_file *)STDIN_FILENO;
}

struct gral_file *gral_file_get_stdout(void) {
	return (struct gral_file *)STDOUT_FILENO;
}

struct gral_file *gral_file_get_stderr(void) {
	return (struct gral_file *)STDERR_FILENO;
}

void gral_file_close(struct gral_file *file) {
	close((int)(intptr_t)file);
}

size_t gral_file_read(struct gral_file *file, void *buffer, size_t size) {
	return read((int)(intptr_t)file, buffer, size);
}

void gral_file_write(struct gral_file *file, void const *buffer, size_t size) {
	write((int)(intptr_t)file, buffer, size);
}

size_t gral_file_get_size(struct gral_file *file) {
	struct stat stat;
	fstat((int)(intptr_t)file, &stat);
	return stat.st_size;
}


/*==========
    AUDIO
 ==========*/

#define FRAMES 1024

static void play_buffer(snd_pcm_t *pcm, void const *buffer, snd_pcm_uframes_t frames) {
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

void gral_audio_play(int (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	snd_pcm_t *pcm;
	snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
	unsigned int latency = FRAMES * 3 / 44100.0 * 1e6 + 0.5;
	snd_pcm_set_params(pcm, SND_PCM_FORMAT_FLOAT, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 44100, 1, latency);
	snd_pcm_prepare(pcm);
	float buffer[FRAMES*2];
	int frames = callback(buffer, FRAMES, user_data);
	while (frames > 0) {
		play_buffer(pcm, buffer, frames);
		frames = callback(buffer, FRAMES, user_data);
	}
	snd_pcm_drain(pcm);
	snd_pcm_close(pcm);
}
