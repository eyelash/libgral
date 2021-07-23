#include <gral.h>
#include <stdio.h>

struct demo {
	struct gral_application *application;
	struct gral_window *window;
};

static int close(void *user_data) {
	return 1;
}

static void draw(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data) {

}

static void resize(int width, int height, void *user_data) {

}

static void mouse_enter(void *user_data) {

}

static void mouse_leave(void *user_data) {

}

static void mouse_move(float x, float y, void *user_data) {

}

static void paste_callback(char const *text, void *user_data) {
	printf("paste: %s\n", text);
}

static void mouse_button_press(float x, float y, int button, int modifiers, void *user_data) {
	struct demo *demo = user_data;
	if (button == GRAL_PRIMARY_MOUSE_BUTTON)
		gral_window_clipboard_copy(demo->window, "gral clipboard test");
	else if (button == GRAL_SECONDARY_MOUSE_BUTTON)
		gral_window_clipboard_paste(demo->window, paste_callback, NULL);
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

static void create_window(void *user_data) {
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
	demo->window = gral_window_create(demo->application, 800, 600, "gral clipboard demo", &interface, demo);
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
	struct gral_application_interface interface = {&open_empty, &open_file, &quit};
	demo.application = gral_application_create("com.github.eyelash.libgral.demos.clipboard", &interface, &demo);
	int result = gral_application_run(demo.application, argc, argv);
	gral_window_delete(demo.window);
	gral_application_delete(demo.application);
	return result;
}
