#include <gral.h>
#include <stdio.h>

static int close(void *user_data) {
	return 1;
}

static void draw(struct gral_painter *painter, void *user_data) {
	gral_painter_set_color(painter, 1.0f, 0.0f, 0.0f, 1.0f);
	gral_painter_draw_rectangle(painter, 10, 10, 100, 100);
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

static void mouse_button_press(int button, void *user_data) {
	printf("mouse button press\n");
}

static void mouse_button_release(int button, void *user_data) {
	printf("mouse button release\n");
}

static void scroll(float dx, float dy, void *user_data) {
	printf("scroll {%f, %f}\n", dx, dy);
}

int main(int argc, char **argv) {
	gral_init(&argc, &argv);
	struct gral_window_interface interface = {
		&close,
		&draw,
		&resize,
		&mouse_enter,
		&mouse_leave,
		&mouse_move,
		&mouse_button_press,
		&mouse_button_release,
		&scroll
	};
	gral_window_create(800, 600, "test", &interface, 0);
	return gral_run();
}
