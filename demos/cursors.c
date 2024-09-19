#include <gral.h>
#include <stdio.h>
#include <stdlib.h>

struct demo_application {
	struct gral_application *application;
};

struct demo_window {
	struct gral_window *window;
	int cursor;
};

static void destroy(void *user_data) {
	struct demo_window *window = user_data;
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
	add_rectangle(draw_context, 20.0f, 20.0f, 160.0f, 160.0f);
	add_rectangle(draw_context, 220.0f, 20.0f, 160.0f, 160.0f);
	add_rectangle(draw_context, 20.0f, 220.0f, 160.0f, 160.0f);
	add_rectangle(draw_context, 220.0f, 220.0f, 160.0f, 160.0f);
	gral_draw_context_fill(draw_context, 0.7f, 0.7f, 0.7f, 1.0f);
}

static void resize(int width, int height, void *user_data) {

}

static void mouse_enter(void *user_data) {

}

static void mouse_leave(void *user_data) {

}

static int point_inside_rectangle(float px, float py, float x, float y, float width, float height) {
	return px >= x && py >= y && px < (x + width) && py < (y + height);
}

static void mouse_move(float x, float y, void *user_data) {
	struct demo_window *window = user_data;
	int cursor = GRAL_CURSOR_DEFAULT;
	if (point_inside_rectangle(x, y, 20, 20, 160, 160))
		cursor = GRAL_CURSOR_TEXT;
	if (point_inside_rectangle(x, y, 220, 20, 160, 160))
		cursor = GRAL_CURSOR_HAND;
	if (point_inside_rectangle(x, y, 20, 220, 160, 160))
		cursor = GRAL_CURSOR_HORIZONTAL_ARROWS;
	if (point_inside_rectangle(x, y, 220, 220, 160, 160))
		cursor = GRAL_CURSOR_VERTICAL_ARROWS;
	if (cursor != window->cursor) {
		gral_window_set_cursor(window->window, cursor);
		window->cursor = cursor;
	}
}

static void mouse_move_relative(float dx, float dy, void *user_data) {
	printf("mouse move relative: {%f, %f}\n", dx, dy);
}

static void mouse_button_press(float x, float y, int button, int modifiers, void *user_data) {
	struct demo_window *window = user_data;
	gral_window_hide_cursor(window->window);
	gral_window_lock_pointer(window->window);
}

static void mouse_button_release(float x, float y, int button, void *user_data) {
	struct demo_window *window = user_data;
	gral_window_unlock_pointer(window->window);
	gral_window_show_cursor(window->window);
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
	struct gral_window_interface window_interface = {
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
	window->window = gral_window_create(application->application, 400, 400, "gral cursors demo", &window_interface, window);
	window->cursor = GRAL_CURSOR_DEFAULT;
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
	struct gral_application_interface application_interface = {&start, &open_empty, &open_file, &quit};
	application.application = gral_application_create("com.github.eyelash.libgral.demos.cursors", &application_interface, &application);
	int result = gral_application_run(application.application, argc, argv);
	gral_application_delete(application.application);
	return result;
}
