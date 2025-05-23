#include <gral.h>
#include <stdlib.h>

struct demo_application {
	struct gral_application *application;
};

struct demo_window {
	struct gral_window *window;
	struct gral_image *image;
};

static void destroy(void *user_data) {
	struct demo_window *window = user_data;
	gral_image_delete(window->image);
	free(window);
}

static int close(void *user_data) {
	return 1;
}

static void draw(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data) {
	struct demo_window *window = user_data;
	gral_draw_context_draw_image(draw_context, window->image, 250.0f, 150.0f);
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

}

static void mouse_button_release(float x, float y, int button, void *user_data) {

}

static void double_click(float x, float y, int button, int modifiers, void *user_data) {

}

static void scroll(float dx, float dy, void *user_data) {

}

static void key_press(int key, int key_code, int modifiers, int is_repeat, void *user_data) {

}

static void key_release(int key, int scan_code, void *user_data) {

}

static void text(char const *s, void *user_data) {

}

static void focus_enter(void *user_data) {

}

static void focus_leave(void *user_data) {

}

static void activate_menu_item(int id, void *user_data) {

}

static struct gral_image *create_image(void) {
	unsigned char *image_data = malloc(100 * 100 * 4);
	int x, y;
	for (y = 0; y < 100; y++) {
		for (x = 0; x < 100; x++) {
			int i = (100 * y + x) * 4;
			image_data[i + 0] = y == 10 ? 255 : x + 50;
			image_data[i + 1] = 128;
			image_data[i + 2] = 255;
			image_data[i + 3] = 255;
		}
	}
	return gral_image_create(100, 100, image_data);
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
		&focus_leave,
		&activate_menu_item
	};
	window->window = gral_window_create(application->application, 600, 400, "gral image demo", &window_interface, window);
	window->image = create_image();
	gral_window_set_minimum_size(window->window, 600, 400);
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
	application.application = gral_application_create("com.github.eyelash.libgral.demos.image", &application_interface, &application);
	int result = gral_application_run(application.application, argc, argv);
	gral_application_delete(application.application);
	return result;
}
