#include <gral.h>
#include <stdio.h>

struct demo {
	struct gral_application *application;
	struct gral_window *window;
	int cursor;
};

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

static void draw(struct gral_draw_context *draw_context, void *user_data) {
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
	struct demo *demo = user_data;
	int cursor = GRAL_CURSOR_DEFAULT;
	if (point_inside_rectangle(x, y, 20, 20, 160, 160))
		cursor = GRAL_CURSOR_TEXT;
	if (point_inside_rectangle(x, y, 220, 20, 160, 160))
		cursor = GRAL_CURSOR_NONE;
	if (point_inside_rectangle(x, y, 20, 220, 160, 160))
		cursor = GRAL_CURSOR_HORIZONTAL_ARROWS;
	if (point_inside_rectangle(x, y, 220, 220, 160, 160))
		cursor = GRAL_CURSOR_VERTICAL_ARROWS;
	if (cursor != demo->cursor) {
		gral_window_set_cursor(demo->window, cursor);
		demo->cursor = cursor;
	}
}

static void mouse_button_press(float x, float y, int button, int modifiers, void *user_data) {
	struct demo *demo = user_data;
	gral_window_warp_cursor(demo->window, 200, 200);
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

static void text(const char *s, void *user_data) {

}

static void initialize(void *user_data) {
	struct demo *demo = user_data;
	struct gral_window_interface interface = {
		&close,
		&draw,
		&resize,
		&mouse_enter,
		&mouse_leave,
		&mouse_move,
		&mouse_button_press,
		&mouse_button_release,
		&double_click,
		&scroll,
		&key_press,
		&key_release,
		&text
	};
	demo->window = gral_window_create(demo->application, 400, 400, "gral cursors demo", &interface, demo);
	demo->cursor = GRAL_CURSOR_DEFAULT;
}

int main(int argc, char **argv) {
	struct demo demo;
	struct gral_application_interface interface = {&initialize};
	demo.application = gral_application_create("com.github.eyelash.libgral.demos.cursors", &interface, &demo);
	int result = gral_application_run(demo.application, argc, argv);
	gral_window_delete(demo.window);
	gral_application_delete(demo.application);
	return result;
}
