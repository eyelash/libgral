#include <gral.h>

struct demo {
	struct gral_application *application;
	struct gral_window *window;
	struct gral_text *text;
	float cursor_x;
	float ascent;
	float descent;
};

static void destroy(void *user_data) {

}

static int close(void *user_data) {
	return 1;
}

static void add_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height) {
	gral_draw_context_move_to(draw_context, x, y);
	gral_draw_context_line_to(draw_context, x + width, y);
	gral_draw_context_line_to(draw_context, x + width, y + height);
	gral_draw_context_line_to(draw_context, x, y + height);
	gral_draw_context_close_path(draw_context);
}

static void draw(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data) {
	struct demo *demo = user_data;
	float text_width = gral_text_get_width(demo->text, draw_context);
	float text_height = demo->ascent + demo->descent;
	add_rectangle(draw_context, 50.0f, 50.0f, text_width, 1.0f);
	gral_draw_context_fill(draw_context, 1.0f, 0.0f, 0.0f, 1.0f);
	add_rectangle(draw_context, 50.0f, 50.0f - demo->ascent, text_width, text_height);
	gral_draw_context_fill(draw_context, 1.0f, 0.0f, 0.0f, 0.2f);
	gral_draw_context_draw_text(draw_context, demo->text, 50.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	add_rectangle(draw_context, 50.0f + demo->cursor_x, 50.0f - demo->ascent, 1.0f, text_height);
	gral_draw_context_fill(draw_context, 1.0f, 0.0f, 0.0f, 1.0f);
}

static void resize(int width, int height, void *user_data) {

}

static void mouse_enter(void *user_data) {

}

static void mouse_leave(void *user_data) {

}

static void mouse_move(float x, float y, void *user_data) {

}

static void mouse_move_relative(float dx, float dy, void *user_data) {

}

static void mouse_button_press(float x, float y, int button, int modifiers, void *user_data) {
	struct demo *demo = user_data;
	int index = gral_text_x_to_index(demo->text, x - 50.0f);
	demo->cursor_x = gral_text_index_to_x(demo->text, index);
	gral_window_request_redraw(demo->window, 0, 0, 600, 400);
}

static void mouse_button_release(float x, float y, int button, void *user_data) {

}

static void double_click(float x, float y, int button, int modifiers, void *user_data) {

}

static void scroll(float dx, float dy, void *user_data) {

}

static void key_press(int key, int scan_code, int modifiers, void *user_data) {

}

static void key_release(int key, int scan_code, void *user_data) {

}

static void text(char const *s, void *user_data) {

}

static void focus_enter(void *user_data) {

}

static void focus_leave(void *user_data) {

}

static void create_window(void *user_data) {
	struct demo *demo = user_data;
	struct gral_window_interface interface = {
		&destroy,
		&close,
		&draw,
		&resize,
		&mouse_enter,
		&mouse_leave,
		&mouse_move,
		&mouse_move_relative,
		&mouse_button_press,
		&mouse_button_release,
		&double_click,
		&scroll,
		&key_press,
		&key_release,
		&text,
		&focus_enter,
		&focus_leave
	};
	demo->window = gral_window_create(demo->application, 600, 400, "gral text demo", &interface, demo);
	struct gral_font *font = gral_font_create_default(demo->window, 16.0f);
	demo->text = gral_text_create(demo->window, "gral text demo: bold italic", font);
	gral_text_set_bold(demo->text, 16, 20);
	gral_text_set_italic(demo->text, 21, 27);
	gral_text_set_color(demo->text, 5, 9, 0.0f, 0.5f, 1.0f, 1.0f);
	demo->cursor_x = 0.0f;
	gral_font_get_metrics(demo->window, font, &demo->ascent, &demo->descent);
	gral_font_delete(font);
}

static void start(void *user_data) {

}

static void open_empty(void *user_data) {
	create_window(user_data);
}

static void open_file(char const *path, void *user_data) {
	create_window(user_data);
}

static void quit(void *user_data) {

}

int main(int argc, char **argv) {
	struct demo demo;
	struct gral_application_interface interface = {&start, &open_empty, &open_file, &quit};
	demo.application = gral_application_create("com.github.eyelash.libgral.demos.text", &interface, &demo);
	int result = gral_application_run(demo.application, argc, argv);
	gral_text_delete(demo.text);
	gral_application_delete(demo.application);
	return result;
}
