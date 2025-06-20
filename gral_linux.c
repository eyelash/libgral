/*

Copyright (c) 2016-2025 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>
#include <alsa/asoundlib.h>


/*================
    APPLICATION
 ================*/

#define GRAL_TYPE_APPLICATION gral_application_get_type()
G_DECLARE_FINAL_TYPE(GralApplication, gral_application, GRAL, APPLICATION, GtkApplication)
struct _GralApplication {
	GtkApplication parent_instance;
	struct gral_application_interface const *interface;
	void *user_data;
};
G_DEFINE_TYPE(GralApplication, gral_application, GTK_TYPE_APPLICATION)

static void gral_application_startup(GApplication *gapplication) {
	G_APPLICATION_CLASS(gral_application_parent_class)->startup(gapplication);
	GralApplication *application = GRAL_APPLICATION(gapplication);
	application->interface->start(application->user_data);
}
static void gral_application_activate(GApplication *gapplication) {
	GralApplication *application = GRAL_APPLICATION(gapplication);
	application->interface->open_empty(application->user_data);
}
static void gral_application_open(GApplication *gapplication, GFile **files, gint n_files, gchar const *hint) {
	GralApplication *application = GRAL_APPLICATION(gapplication);
	for (gint i = 0; i < n_files; i++) {
		char *path = g_file_get_path(files[i]);
		application->interface->open_file(path, application->user_data);
		g_free(path);
	}
}
static void gral_application_shutdown(GApplication *gapplication) {
	G_APPLICATION_CLASS(gral_application_parent_class)->shutdown(gapplication);
	GralApplication *application = GRAL_APPLICATION(gapplication);
	application->interface->quit(application->user_data);
}
static void gral_application_init(GralApplication *application) {

}
static void gral_application_class_init(GralApplicationClass *class) {
	GApplicationClass *application_class = G_APPLICATION_CLASS(class);
	application_class->startup = gral_application_startup;
	application_class->activate = gral_application_activate;
	application_class->open = gral_application_open;
	application_class->shutdown = gral_application_shutdown;
}

struct gral_application *gral_application_create(char const *id, struct gral_application_interface const *interface, void *user_data) {
	GralApplication *application = g_object_new(GRAL_TYPE_APPLICATION, "application-id", id, "flags", G_APPLICATION_NON_UNIQUE|G_APPLICATION_HANDLES_OPEN, NULL);
	application->interface = interface;
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

static void pixbuf_destroy(guchar *pixels, gpointer data) {
	free(pixels);
}

struct gral_image *gral_image_create(int width, int height, void *data) {
	return (struct gral_image *)gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE, 8, width, height, width * 4, pixbuf_destroy, NULL);
}

void gral_image_delete(struct gral_image *image) {
	g_object_unref(image);
}

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
	GSettingsSchemaSource *schema_source = g_settings_schema_source_get_default();
	if (schema_source) {
		GSettingsSchema *settings_schema = g_settings_schema_source_lookup(schema_source, "org.gnome.desktop.interface", TRUE);
		if (settings_schema) {
			GSettings *settings = g_settings_new_full(settings_schema, NULL, NULL);
			gchar *monospace_font_name = g_settings_get_string(settings, "monospace-font-name");
			PangoFontDescription *font = pango_font_description_from_string(monospace_font_name);
			pango_font_description_set_absolute_size(font, pango_units_from_double(size));
			g_free(monospace_font_name);
			g_object_unref(settings);
			g_settings_schema_unref(settings_schema);
			return (struct gral_font *)font;
		}
	}
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
	PangoAttrList *attr_list = pango_attr_list_new();
	pango_layout_set_attributes(layout, attr_list);
	pango_attr_list_unref(attr_list);
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
	PangoAttribute *attribute = pango_attr_foreground_new(red * 65535.0f, green * 65535.0f, blue * 65535.0f);
	attribute->start_index = start_index;
	attribute->end_index = end_index;
	PangoAttrList *attr_list = pango_layout_get_attributes(PANGO_LAYOUT(text));
	pango_attr_list_change(attr_list, attribute);
	attribute = pango_attr_foreground_alpha_new(alpha * 65535.0f);
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

void gral_draw_context_draw_image(struct gral_draw_context *draw_context, struct gral_image *image, float x, float y) {
	int width = gdk_pixbuf_get_width((GdkPixbuf *)image);
	int height = gdk_pixbuf_get_height((GdkPixbuf *)image);
	cairo_rectangle((cairo_t *)draw_context, x, y, width, height);
	gdk_cairo_set_source_pixbuf((cairo_t *)draw_context, (GdkPixbuf *)image, x, y);
	cairo_fill((cairo_t *)draw_context);
}

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	cairo_move_to((cairo_t *)draw_context, x, y);
	cairo_set_source_rgba((cairo_t *)draw_context, red, green, blue, alpha);
	PangoLayoutLine *line = pango_layout_get_line_readonly(PANGO_LAYOUT(text), 0);
	pango_cairo_show_layout_line((cairo_t *)draw_context, line);
}

void gral_draw_context_add_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y) {
	cairo_move_to((cairo_t *)draw_context, x, y);
	PangoLayoutLine *line = pango_layout_get_line_readonly(PANGO_LAYOUT(text), 0);
	pango_cairo_layout_line_path((cairo_t *)draw_context, line);
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

void gral_draw_context_stroke_linear_gradient(struct gral_draw_context *draw_context, float line_width, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count) {
	cairo_set_line_width((cairo_t *)draw_context, line_width);
	cairo_set_line_cap((cairo_t *)draw_context, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join((cairo_t *)draw_context, CAIRO_LINE_JOIN_ROUND);
	cairo_pattern_t *gradient = cairo_pattern_create_linear(start_x, start_y, end_x, end_y);
	int i;
	for (i = 0; i < count; i++) {
		cairo_pattern_add_color_stop_rgba(gradient, stops[i].position, stops[i].red, stops[i].green, stops[i].blue, stops[i].alpha);
	}
	cairo_set_source((cairo_t *)draw_context, gradient);
	cairo_stroke((cairo_t *)draw_context);
	cairo_pattern_destroy(gradient);
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
	struct gral_window_interface const *interface;
	void *user_data;
	gboolean is_cursor_hidden;
	int cursor;
	gboolean is_pointer_locked;
	gint locked_pointer_x, locked_pointer_y;
	guint last_key;
};
G_DEFINE_TYPE(GralWindow, gral_window, GTK_TYPE_APPLICATION_WINDOW)

static gboolean gral_window_delete_event(GtkWidget *widget, GdkEventAny *event) {
	GralWindow *window = GRAL_WINDOW(widget);
	return !window->interface->close(window->user_data);
}
static void gral_window_finalize(GObject *object) {
	GralWindow *window = GRAL_WINDOW(object);
	window->interface->destroy(window->user_data);
	G_OBJECT_CLASS(gral_window_parent_class)->finalize(object);
}
static void gral_window_activate_menu_item(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
	GralWindow *window = GRAL_WINDOW(user_data);
	window->interface->activate_menu_item(g_variant_get_int32(parameter), window->user_data);
}
static void gral_window_init(GralWindow *window) {
	GSimpleAction *action = g_simple_action_new("activate-menu-item", G_VARIANT_TYPE_INT32);
	g_signal_connect_object(action, "activate", G_CALLBACK(gral_window_activate_menu_item), window, 0);
	g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
	g_object_unref(action);
}
static void gral_window_class_init(GralWindowClass *class) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	widget_class->delete_event = gral_window_delete_event;
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	object_class->finalize = gral_window_finalize;
}

#define GRAL_TYPE_WIDGET gral_widget_get_type()
G_DECLARE_FINAL_TYPE(GralWidget, gral_widget, GRAL, WIDGET, GtkDrawingArea)
struct _GralWidget {
	GtkDrawingArea parent_instance;
	GtkIMContext *im_context;
};
G_DEFINE_TYPE(GralWidget, gral_widget, GTK_TYPE_DRAWING_AREA)

static void gral_widget_realize(GtkWidget *widget) {
	GTK_WIDGET_CLASS(gral_widget_parent_class)->realize(widget);
}
static void gral_widget_unrealize(GtkWidget *widget) {
	GTK_WIDGET_CLASS(gral_widget_parent_class)->unrealize(widget);
}
static gboolean gral_widget_draw(GtkWidget *widget, cairo_t *cr) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	GdkRectangle clip_rectangle;
	gdk_cairo_get_clip_rectangle(cr, &clip_rectangle);
	window->interface->draw((struct gral_draw_context *)cr, clip_rectangle.x, clip_rectangle.y, clip_rectangle.width, clip_rectangle.height, window->user_data);
	return GDK_EVENT_STOP;
}
static void gral_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
	GTK_WIDGET_CLASS(gral_widget_parent_class)->size_allocate(widget, allocation);
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	window->interface->resize(allocation->width, allocation->height, window->user_data);
}
static gboolean gral_widget_enter_notify_event(GtkWidget *widget, GdkEventCrossing *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	window->interface->mouse_enter(window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_leave_notify_event(GtkWidget *widget, GdkEventCrossing *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	window->interface->mouse_leave(window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_motion_notify_event(GtkWidget *widget, GdkEventMotion *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	if (window->is_pointer_locked) {
		GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(window));
		GdkDisplay *display = gdk_screen_get_display(screen);
		GdkDevice *pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(display));
		gint x, y;
		gdk_device_get_position(pointer, NULL, &x, &y);
		if (x != window->locked_pointer_x || y != window->locked_pointer_y) {
			window->interface->mouse_move_relative(x - window->locked_pointer_x, y - window->locked_pointer_y, window->user_data);
			if (GDK_IS_WAYLAND_DISPLAY(display)) {
				// gdk_device_warp does not work on Wayland
				window->locked_pointer_x = x;
				window->locked_pointer_y = y;
			}
			else {
				gdk_device_warp(pointer, screen, window->locked_pointer_x, window->locked_pointer_y);
			}
		}
	}
	else {
		window->interface->mouse_move(event->x, event->y, window->user_data);
	}
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_button_press_event(GtkWidget *widget, GdkEventButton *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	if (event->type == GDK_BUTTON_PRESS) {
		window->interface->mouse_button_press(event->x, event->y, event->button, get_modifiers(event->state), window->user_data);
	}
	else if (event->type == GDK_2BUTTON_PRESS) {
		window->interface->double_click(event->x, event->y, event->button, get_modifiers(event->state), window->user_data);
	}
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_button_release_event(GtkWidget *widget, GdkEventButton *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	window->interface->mouse_button_release(event->x, event->y, event->button, window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_scroll_event(GtkWidget *widget, GdkEventScroll *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	gdouble delta_x, delta_y;
	gdk_event_get_scroll_deltas((GdkEvent *)event, &delta_x, &delta_y);
	window->interface->scroll(-delta_x, -delta_y, window->user_data);
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
	default:
		{
			GdkKeymap *keymap = gdk_keymap_get_for_display(gdk_window_get_display(event->window));
			GdkKeymapKey keymap_key;
			keymap_key.keycode = event->hardware_keycode;
			keymap_key.group = 0;
			keymap_key.level = 0;
			return gdk_keyval_to_unicode(gdk_keymap_lookup_key(keymap, &keymap_key));
		}
	}
}
static int get_key_code(GdkEventKey *event) {
	int key_code = event->hardware_keycode - 8;
	if (key_code <= 0x58) {
		return key_code;
	}
	switch (key_code) {
	case 0x60: return 0xE01C; // NUMPAD_ENTER
	case 0x61: return 0xE01D; // CONTROL_RIGHT
	case 0x62: return 0xE035; // NUMPAD_DEVIDE
	case 0x64: return 0xE038; // ALT_RIGHT
	case 0x66: return 0xE047; // HOME
	case 0x67: return 0xE048; // ARROW_UP
	case 0x68: return 0xE049; // PAGE_UP
	case 0x69: return 0xE04B; // ARROW_LEFT
	case 0x6A: return 0xE04D; // ARROW_RIGHT
	case 0x6B: return 0xE04F; // END
	case 0x6C: return 0xE050; // ARROW_DOWN
	case 0x6D: return 0xE051; // PAGE_DOWN
	case 0x6E: return 0xE052; // INSERT
	case 0x6F: return 0xE053; // DELETE
	case 0x75: return 0x59;   // NUMPAD_EQUAL
	case 0x7D: return 0xE05B; // META_LEFT
	case 0x7E: return 0xE05C; // META_RIGHT
	case 0x7F: return 0xE05D; // CONTEXT_MENU
	case 0xB7: return 0x64;   // F13
	case 0xB8: return 0x65;   // F14
	case 0xB9: return 0x66;   // F15
	case 0xBA: return 0x67;   // F16
	case 0xBB: return 0x68;   // F17
	case 0xBC: return 0x69;   // F18
	case 0xBD: return 0x6A;   // F19
	default: return 0;
	}
}
static gboolean gral_widget_key_press_event(GtkWidget *widget, GdkEventKey *event) {
	GralWidget *area = GRAL_WIDGET(widget);
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	gtk_im_context_filter_keypress(area->im_context, event);
	window->interface->key_press(get_key(event), get_key_code(event), get_modifiers(event->state), event->keyval == window->last_key, window->user_data);
	window->last_key = event->keyval;
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_key_release_event(GtkWidget *widget, GdkEventKey *event) {
	GralWidget *area = GRAL_WIDGET(widget);
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	gtk_im_context_filter_keypress(area->im_context, event);
	window->interface->key_release(get_key(event), get_key_code(event), window->user_data);
	if (event->keyval == window->last_key) {
		window->last_key = GDK_KEY_VoidSymbol;
	}
	return GDK_EVENT_STOP;
}
static void gral_widget_commit(GtkIMContext *context, gchar *str, gpointer user_data) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(user_data)));
	window->interface->text(str, window->user_data);
}
static gboolean gral_widget_focus_in_event(GtkWidget *widget, GdkEventFocus *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	window->interface->focus_enter(window->user_data);
	return GDK_EVENT_STOP;
}
static gboolean gral_widget_focus_out_event(GtkWidget *widget, GdkEventFocus *event) {
	GralWindow *window = GRAL_WINDOW(gtk_widget_get_toplevel(widget));
	window->interface->focus_leave(window->user_data);
	return GDK_EVENT_STOP;
}
static void gral_widget_dispose(GObject *object) {
	GralWidget *widget = GRAL_WIDGET(object);
	g_clear_object(&widget->im_context);
	G_OBJECT_CLASS(gral_widget_parent_class)->dispose(object);
}
static void gral_widget_init(GralWidget *widget) {
	widget->im_context = gtk_im_multicontext_new();
	g_signal_connect_object(widget->im_context, "commit", G_CALLBACK(gral_widget_commit), widget, 0);
	gtk_widget_add_events(GTK_WIDGET(widget), GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_SCROLL_MASK|GDK_SMOOTH_SCROLL_MASK);
	gtk_widget_set_can_focus(GTK_WIDGET(widget), TRUE);
}
static void gral_widget_class_init(GralWidgetClass *class) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	widget_class->realize = gral_widget_realize;
	widget_class->unrealize = gral_widget_unrealize;
	widget_class->draw = gral_widget_draw;
	widget_class->size_allocate = gral_widget_size_allocate;
	widget_class->enter_notify_event = gral_widget_enter_notify_event;
	widget_class->leave_notify_event = gral_widget_leave_notify_event;
	widget_class->motion_notify_event = gral_widget_motion_notify_event;
	widget_class->button_press_event = gral_widget_button_press_event;
	widget_class->button_release_event = gral_widget_button_release_event;
	widget_class->scroll_event = gral_widget_scroll_event;
	widget_class->key_press_event = gral_widget_key_press_event;
	widget_class->key_release_event = gral_widget_key_release_event;
	widget_class->focus_in_event = gral_widget_focus_in_event;
	widget_class->focus_out_event = gral_widget_focus_out_event;
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	object_class->dispose = gral_widget_dispose;
}

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, char const *title, struct gral_window_interface const *interface, void *user_data) {
	GralWindow *window = g_object_new(GRAL_TYPE_WINDOW, "application", application, NULL);
	window->interface = interface;
	window->user_data = user_data;
	window->is_cursor_hidden = FALSE;
	window->cursor = GRAL_CURSOR_DEFAULT;
	window->is_pointer_locked = FALSE;
	window->last_key = GDK_KEY_VoidSymbol;
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
	gtk_window_set_title(GTK_WINDOW(window), title);
	GtkWidget *widget = g_object_new(GRAL_TYPE_WIDGET, NULL);
	gtk_container_add(GTK_CONTAINER(window), widget);
	return (struct gral_window *)window;
}

void gral_window_show(struct gral_window *window) {
	gtk_widget_show_all(GTK_WIDGET(window));
}

void gral_window_set_title(struct gral_window *window, char const *title) {
	gtk_window_set_title(GTK_WINDOW(window), title);
}

void gral_window_request_redraw(struct gral_window *window, int x, int y, int width, int height) {
	GtkWidget *widget = gtk_bin_get_child(GTK_BIN(window));
	gtk_widget_queue_draw_area(widget, x, y, width, height);
}

void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height) {
	GtkWidget *widget = gtk_bin_get_child(GTK_BIN(window));
	gtk_widget_set_size_request(widget, minimum_width, minimum_height);
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
static void update_cursor(GralWindow *window) {
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
	GdkCursor *gdk_cursor;
	if (window->is_cursor_hidden) {
		gdk_cursor = gdk_cursor_new_from_name(gdk_window_get_display(gdk_window), "none");
	}
	else {
		gdk_cursor = gdk_cursor_new_from_name(gdk_window_get_display(gdk_window), get_cursor_name(window->cursor));
	}
	gdk_window_set_cursor(gdk_window, gdk_cursor);
	g_object_unref(gdk_cursor);
}
void gral_window_set_cursor(struct gral_window *window_, int cursor) {
	GralWindow *window = GRAL_WINDOW(window_);
	window->cursor = cursor;
	if (!window->is_cursor_hidden) {
		update_cursor(window);
	}
}

void gral_window_hide_cursor(struct gral_window *window_) {
	GralWindow *window = GRAL_WINDOW(window_);
	window->is_cursor_hidden = TRUE;
	update_cursor(window);
}

void gral_window_show_cursor(struct gral_window *window_) {
	GralWindow *window = GRAL_WINDOW(window_);
	window->is_cursor_hidden = FALSE;
	update_cursor(window);
}

void gral_window_warp_cursor(struct gral_window *window, float x, float y) {
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
	GdkScreen *screen = gdk_window_get_screen(gdk_window);
	GdkDevice *pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_screen_get_display(screen)));
	gint root_x, root_y;
	gdk_window_get_root_coords(gdk_window, x, y, &root_x, &root_y);
	gdk_device_warp(pointer, screen, root_x, root_y);
}

void gral_window_lock_pointer(struct gral_window *window_) {
	GralWindow *window = GRAL_WINDOW(window_);
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(window));
	GdkDisplay *display = gdk_screen_get_display(screen);
	GdkDevice *pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(display));
	gdk_device_get_position(pointer, NULL, &window->locked_pointer_x, &window->locked_pointer_y);
	window->is_pointer_locked = TRUE;
}

void gral_window_unlock_pointer(struct gral_window *window_) {
	GralWindow *window = GRAL_WINDOW(window_);
	window->is_pointer_locked = FALSE;
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

void gral_window_show_context_menu(struct gral_window *window, struct gral_menu *menu, float x, float y) {
	GtkWidget *widget = gtk_bin_get_child(GTK_BIN(window));
	if (gtk_menu_get_attach_widget(GTK_MENU(menu)) == NULL) {
		gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
	}
	gtk_widget_show_all(GTK_WIDGET(menu));
	GdkRectangle rect = {x, y, 1, 1};
	gtk_menu_popup_at_rect(GTK_MENU(menu), gtk_widget_get_window(widget), &rect, GDK_GRAVITY_SOUTH_EAST, GDK_GRAVITY_NORTH_WEST, NULL);
}

struct gral_menu *gral_menu_create(void) {
	GtkWidget *menu = gtk_menu_new();
	g_object_ref_sink(menu);
	return (struct gral_menu *)menu;
}

void gral_menu_delete(struct gral_menu *menu) {
	g_object_unref(menu);
}

void gral_menu_append_item(struct gral_menu *menu, char const *text, int id) {
	GtkWidget *item = gtk_menu_item_new_with_label(text);
	gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "win.activate-menu-item");
	gtk_actionable_set_action_target(GTK_ACTIONABLE(item), "i", id);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

void gral_menu_append_separator(struct gral_menu *menu) {
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

void gral_menu_append_submenu(struct gral_menu *menu, char const *text, struct gral_menu *submenu) {
	GtkWidget *item = gtk_menu_item_new_with_label(text);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), GTK_WIDGET(submenu));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_unref(submenu);
}

typedef struct {
	void (*callback)(void *user_data);
	void *user_data;
} TimerCallbackData;
static gboolean timer_callback(gpointer user_data) {
	TimerCallbackData *callback_data = user_data;
	callback_data->callback(callback_data->user_data);
	return G_SOURCE_CONTINUE;
}
static void timer_destroy(gpointer user_data) {
	TimerCallbackData *callback_data = user_data;
	g_slice_free(TimerCallbackData, callback_data);
}

struct gral_timer *gral_timer_create(int milliseconds, void (*callback)(void *user_data), void *user_data) {
	TimerCallbackData *callback_data = g_slice_new(TimerCallbackData);
	callback_data->callback = callback;
	callback_data->user_data = user_data;
	return (struct gral_timer *)(intptr_t)g_timeout_add_full(G_PRIORITY_DEFAULT, milliseconds, timer_callback, callback_data, timer_destroy);
}

void gral_timer_delete(struct gral_timer *timer) {
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
void gral_run_on_main_thread(void (*callback)(void *user_data), void *user_data) {
	IdleCallbackData *callback_data = g_slice_new(IdleCallbackData);
	callback_data->callback = callback;
	callback_data->user_data = user_data;
	gdk_threads_add_idle_full(G_PRIORITY_DEFAULT_IDLE, idle_callback, callback_data, idle_destroy);
}


/*=========
    FILE
 =========*/

#include <sys/inotify.h>
#include <glib-unix.h>

typedef struct {
	void (*callback)(void *user_data);
	void *user_data;
	int fd;
} InotifyCallbackData;
static gboolean inotify_callback(gint fd, GIOCondition condition, gpointer user_data) {
	InotifyCallbackData *callback_data = user_data;
	char buffer[4096];
	ssize_t size = read(fd, buffer, sizeof(buffer));
	struct inotify_event *event;
	for (ssize_t i = 0; i < size; i += sizeof(*event) + event->len) {
		event = (struct inotify_event *)(buffer + i);
		callback_data->callback(callback_data->user_data);
	}
	return G_SOURCE_CONTINUE;
}
static void inotify_destroy(gpointer user_data) {
	InotifyCallbackData *callback_data = user_data;
	close(callback_data->fd);
	g_slice_free(InotifyCallbackData, callback_data);
}

struct gral_directory_watcher *gral_directory_watch(char const *path, void (*callback)(void *user_data), void *user_data) {
	InotifyCallbackData *callback_data = g_slice_new(InotifyCallbackData);
	callback_data->callback = callback;
	callback_data->user_data = user_data;
	callback_data->fd = inotify_init1(IN_NONBLOCK);
	inotify_add_watch(callback_data->fd, path, IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
	return (struct gral_directory_watcher *)(intptr_t)g_unix_fd_add_full(G_PRIORITY_DEFAULT, callback_data->fd, G_IO_IN, inotify_callback, callback_data, inotify_destroy);
}

void gral_directory_watcher_delete(struct gral_directory_watcher *directory_watcher) {
	g_source_remove((guint)(intptr_t)directory_watcher);
}


/*==========
    AUDIO
 ==========*/

#define FRAMES 512

struct gral_audio {
	void (*callback)(float *buffer, int frames, void *user_data);
	void *user_data;
	pa_threaded_mainloop *mainloop;
	pa_context *context;
	pa_stream *stream;
};

static void stream_state_callback(pa_stream *stream, void *user_data) {
	struct gral_audio *audio = user_data;
	pa_threaded_mainloop_signal(audio->mainloop, 0);
}

static void write_callback(pa_stream *stream, size_t n_bytes, void *user_data) {
	struct gral_audio *audio = user_data;
	void *buffer = NULL;
	pa_stream_begin_write(stream, &buffer, &n_bytes);
	int frames = n_bytes / (2 * sizeof(float));
	audio->callback(buffer, frames, audio->user_data);
	pa_stream_write(stream, buffer, n_bytes, NULL, 0, PA_SEEK_RELATIVE);
}

static void underflow_callback(pa_stream *stream, void *user_data) {
	fprintf(stderr, "libgral audio underrun\n");
}

static void context_state_callback(pa_context *context, void *user_data) {
	struct gral_audio *audio = user_data;
	if (pa_context_get_state(context) == PA_CONTEXT_READY) {
		pa_sample_spec sample_spec;
		sample_spec.format = PA_SAMPLE_FLOAT32NE;
		sample_spec.rate = 44100;
		sample_spec.channels = 2;
		audio->stream = pa_stream_new(context, "libgral", &sample_spec, NULL);
		pa_stream_set_state_callback(audio->stream, &stream_state_callback, audio);
		pa_stream_set_write_callback(audio->stream, &write_callback, audio);
		pa_stream_set_underflow_callback(audio->stream, &underflow_callback, audio);
		pa_buffer_attr buffer_attr;
		buffer_attr.maxlength = -1;
		buffer_attr.tlength = FRAMES * 2 * sizeof(float);
		buffer_attr.prebuf = -1;
		buffer_attr.minreq = -1;
		buffer_attr.fragsize = -1;
		pa_stream_connect_playback(audio->stream, NULL, &buffer_attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL);
	}
	pa_threaded_mainloop_signal(audio->mainloop, 0);
}

struct gral_audio *gral_audio_create(char const *name, void (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	struct gral_audio *audio = malloc(sizeof(struct gral_audio));
	audio->callback = callback;
	audio->user_data = user_data;
	audio->mainloop = pa_threaded_mainloop_new();
	audio->context = pa_context_new(pa_threaded_mainloop_get_api(audio->mainloop), "libgral");
	pa_context_set_state_callback(audio->context, &context_state_callback, audio);
	pa_context_connect(audio->context, NULL, PA_CONTEXT_NOFLAGS, NULL);
	audio->stream = NULL;
	pa_threaded_mainloop_start(audio->mainloop);
	return audio;
}

void gral_audio_delete(struct gral_audio *audio) {
	pa_threaded_mainloop_stop(audio->mainloop);
	if (audio->stream) {
		pa_stream_disconnect(audio->stream);
		pa_stream_unref(audio->stream);
	}
	pa_context_disconnect(audio->context);
	pa_context_unref(audio->context);
	pa_threaded_mainloop_free(audio->mainloop);
	free(audio);
}


/*=========
    MIDI
 =========*/

struct gral_midi {
	struct gral_midi_interface const *interface;
	void *user_data;
	snd_seq_t *seq;
	int port;
	guint source_id;
};

static void midi_connect_port(struct gral_midi *midi, int client, snd_seq_port_info_t *port_info) {
	int port = snd_seq_port_info_get_port(port_info);
	unsigned int capability = snd_seq_port_info_get_capability(port_info);
	//int direction = snd_seq_port_info_get_direction(port_info);
	if (capability & SND_SEQ_PORT_CAP_NO_EXPORT) {
		return;
	}
	if ((capability & SND_SEQ_PORT_CAP_READ) && (capability & SND_SEQ_PORT_CAP_SUBS_READ)) {
		snd_seq_connect_from(midi->seq, midi->port, client, port);
	}
}

static gboolean midi_callback(gint fd, GIOCondition condition, gpointer user_data) {
	struct gral_midi *midi = user_data;
	snd_seq_event_t *event;
	while (snd_seq_event_input(midi->seq, &event) > 0) {
		switch (event->type) {
		case SND_SEQ_EVENT_NOTEON:
			midi->interface->note_on(event->data.note.note, event->data.note.velocity, midi->user_data);
			break;
		case SND_SEQ_EVENT_NOTEOFF:
			midi->interface->note_off(event->data.note.note, event->data.note.velocity, midi->user_data);
			break;
		case SND_SEQ_EVENT_CONTROLLER:
			midi->interface->control_change(event->data.control.param, event->data.control.value, midi->user_data);
			break;
		case SND_SEQ_EVENT_PORT_START:
			{
				snd_seq_port_info_t *port_info;
				snd_seq_port_info_alloca(&port_info);
				snd_seq_get_any_port_info(midi->seq, event->data.addr.client, event->data.addr.port, port_info);
				midi_connect_port(midi, event->data.addr.client, port_info);
				break;
			}
		default:
			break;
		}
	}
	return G_SOURCE_CONTINUE;
}

struct gral_midi *gral_midi_create(struct gral_application *application, char const *name, struct gral_midi_interface const *interface, void *user_data) {
	struct gral_midi *midi = malloc(sizeof(struct gral_midi));
	midi->interface = interface;
	midi->user_data = user_data;
	snd_seq_open(&midi->seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
	snd_seq_set_client_name(midi->seq, name);
	int client_id = snd_seq_client_id(midi->seq);
	midi->port = snd_seq_create_simple_port(midi->seq, name, SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	snd_seq_client_info_t *client_info;
	snd_seq_client_info_alloca(&client_info);
	snd_seq_port_info_t *port_info;
	snd_seq_port_info_alloca(&port_info);
	snd_seq_client_info_set_client(client_info, -1);
	while (snd_seq_query_next_client(midi->seq, client_info) >= 0) {
		int client = snd_seq_client_info_get_client(client_info);
		snd_seq_port_info_set_client(port_info, client);
		snd_seq_port_info_set_port(port_info, -1);
		while (snd_seq_query_next_port(midi->seq, port_info) >= 0) {
			midi_connect_port(midi, client, port_info);
		}
	}
	struct pollfd pfd;
	snd_seq_poll_descriptors(midi->seq, &pfd, 1, POLLIN);
	midi->source_id = g_unix_fd_add(pfd.fd, pfd.events, &midi_callback, midi);
	return midi;
}

void gral_midi_delete(struct gral_midi *midi) {
	g_source_remove(midi->source_id);
	snd_seq_close(midi->seq);
	free(midi);
}
