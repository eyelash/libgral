#include <gral.h>
#include <stdio.h>

struct gral_demo {
	struct gral_application *application;
	struct gral_window *window;
};

static int close(void *user_data) {
	return 1;
}

static void draw(struct gral_draw_context *draw_context, void *user_data) {

}

static void resize(int width, int height, void *user_data) {

}

static void mouse_enter(void *user_data) {

}

static void mouse_leave(void *user_data) {

}

static void mouse_move(float x, float y, void *user_data) {

}

static void file_callback(const char *file, void *user_data) {
	printf("file: %s\n", file);
}

static void mouse_button_press(float x, float y, int button, void *user_data) {
	struct gral_demo *demo = user_data;
	if (button == GRAL_PRIMARY_MOUSE_BUTTON)
		gral_window_show_open_file_dialog(demo->window, &file_callback, demo);
	else if (button == GRAL_SECONDARY_MOUSE_BUTTON)
		gral_window_show_save_file_dialog(demo->window, &file_callback, demo);
}

static void mouse_button_release(float x, float y, int button, void *user_data) {

}

static void scroll(float dx, float dy, void *user_data) {

}

static void text(const char *s, void *user_data) {

}

static void paste(const char *text, void *user_data) {

}

static void initialize(void *user_data) {
	struct gral_demo *demo = user_data;
	struct gral_window_interface interface = {
		&close,
		&draw,
		&resize,
		&mouse_enter,
		&mouse_leave,
		&mouse_move,
		&mouse_button_press,
		&mouse_button_release,
		&scroll,
		&text,
		&paste
	};
	demo->window = gral_window_create(demo->application, 600, 400, "gral file dialog demo", &interface, demo);
}

int main(int argc, char **argv) {
	struct gral_demo demo;
	struct gral_application_interface interface = {&initialize};
	demo.application = gral_application_create("com.github.eyelash.libgral.demo", &interface, &demo);
	int result = gral_application_run(demo.application, argc, argv);
	gral_window_delete(demo.window);
	gral_application_delete(demo.application);
	return result;
}
