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
	printf("resize: %dx%d\n", width, height);
}

static void mouse_enter(void *user_data) {
	printf("mouse enter\n");
}

static void mouse_leave(void *user_data) {
	printf("mouse leave\n");
}

static void mouse_move(float x, float y, void *user_data) {
	printf("mouse move: {%f, %f}\n", x, y);
}

static void mouse_button_press(float x, float y, int button, void *user_data) {
	printf("mouse button press: {%f, %f}\n", x, y);
}

static void mouse_button_release(float x, float y, int button, void *user_data) {
	printf("mouse button release: {%f, %f}\n", x, y);
}

static void scroll(float dx, float dy, void *user_data) {
	printf("scroll {%f, %f}\n", dx, dy);
}

static void character(uint32_t c, void *user_data) {
	printf("character: U+%X\n", c);
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
		&character,
		&paste
	};
	demo->window = gral_window_create(demo->application, 800, 600, "gral events demo", &interface, demo);
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
