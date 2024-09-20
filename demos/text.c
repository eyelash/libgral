#include <gral.h>
#include <stdlib.h>

struct demo_application {
	struct gral_application *application;
};

struct demo_window {
	struct gral_window *window;
	struct gral_text *text;
	float cursor_x;
	float ascent;
	float descent;
};

static void destroy(void *user_data) {
	struct demo_window *window = user_data;
	gral_text_delete(window->text);
	free(window);
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
	struct demo_window *window = user_data;
	float text_width = gral_text_get_width(window->text, draw_context);
	float text_height = window->ascent + window->descent;
	add_rectangle(draw_context, 50.0f, 50.0f, text_width, 1.0f);
	gral_draw_context_fill(draw_context, 1.0f, 0.0f, 0.0f, 1.0f);
	add_rectangle(draw_context, 50.0f, 50.0f - window->ascent, text_width, text_height);
	gral_draw_context_fill(draw_context, 1.0f, 0.0f, 0.0f, 0.2f);
	gral_draw_context_draw_text(draw_context, window->text, 50.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	add_rectangle(draw_context, 50.0f + window->cursor_x, 50.0f - window->ascent, 1.0f, text_height);
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
	struct demo_window *window = user_data;
	int index = gral_text_x_to_index(window->text, x - 50.0f);
	window->cursor_x = gral_text_index_to_x(window->text, index);
	gral_window_request_redraw(window->window, 0, 0, 600, 400);
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
	struct demo_application *application = user_data;
	struct demo_window *window = malloc(sizeof(struct demo_window));
	static struct gral_window_interface const window_interface = {
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
	window->window = gral_window_create(application->application, 600, 400, "gral text demo", &window_interface, window);
	struct gral_font *font = gral_font_create_default(window->window, 16.0f);
	window->text = gral_text_create(window->window, "gral text demo: bold italic", font);
	gral_text_set_bold(window->text, 16, 20);
	gral_text_set_italic(window->text, 21, 27);
	gral_text_set_color(window->text, 5, 9, 0.0f, 0.5f, 1.0f, 1.0f);
	window->cursor_x = 0.0f;
	gral_font_get_metrics(window->window, font, &window->ascent, &window->descent);
	gral_font_delete(font);
	gral_window_show(window->window);
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
	struct demo_application application;
	static struct gral_application_interface const application_interface = {&start, &open_empty, &open_file, &quit};
	application.application = gral_application_create("com.github.eyelash.libgral.demos.text", &application_interface, &application);
	int result = gral_application_run(application.application, argc, argv);
	gral_application_delete(application.application);
	return result;
}
