#include <gral.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

static struct gral_window *window;
static int inside = 0;
static struct gral_text *text;

static int close(void *user_data) {
	return 1;
}

static void draw_star(struct gral_painter *painter, float x, float y, float size) {
	float radius = size / 2.0f;
	float cx = x + radius;
	float cy = y + radius;
	gral_painter_move_to(painter, cx, y);
	int i;
	for (i = 1; i < 5; i++) {
		float a = (float)M_PI * 3.0f / 2.0f + (float)M_PI * 2.0f * 2.0f / 5.0f * i;
		gral_painter_line_to(painter, cx + cosf(a)*radius, cy + sinf(a)*radius);
	}
	gral_painter_close_path(painter);
}

static void draw(struct gral_painter *painter, void *user_data) {
	draw_star(painter, 20.0f, 20.0f, 160.0f);
	if (inside) gral_painter_fill(painter, 0.0f, 1.0f, 0.0f, 1.0f);
	else gral_painter_fill(painter, 1.0f, 0.0f, 0.0f, 1.0f);
	gral_painter_new_path(painter);
	draw_star(painter, 220.0f, 20.0f, 160.0f);
	gral_painter_stroke(painter, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	gral_painter_draw_text(painter, text, 50.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void resize(int width, int height, void *user_data) {
	printf("resize: %dx%d\n", width, height);
}

static void mouse_enter(void *user_data) {
	inside = 1;
	gral_window_request_redraw(window);
}

static void mouse_leave(void *user_data) {
	inside = 0;
	gral_window_request_redraw(window);
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

static void character(uint32_t c, void *user_data) {
	printf("character: U+%X\n", c);
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
		&scroll,
		&character
	};
	window = gral_window_create(800, 600, "test", &interface, 0);
	text = gral_text_create(window, "Test", 16.0f);
	return gral_run();
}
