#include <gral.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

struct gral_demo {
	struct gral_application *application;
	struct gral_window *window;
	int mouse_inside;
	struct gral_text *text;
};

static int close(void *user_data) {
	return 1;
}

static void draw_star(struct gral_draw_context *draw_context, float x, float y, float size) {
	float radius = size / 2.0f;
	float cx = x + radius;
	float cy = y + radius;
	gral_draw_context_move_to(draw_context, cx, y);
	int i;
	for (i = 1; i < 5; i++) {
		float a = (float)M_PI * 3.0f / 2.0f + (float)M_PI * 2.0f * 2.0f / 5.0f * i;
		gral_draw_context_line_to(draw_context, cx + cosf(a)*radius, cy + sinf(a)*radius);
	}
	gral_draw_context_close_path(draw_context);
}

static void draw(struct gral_draw_context *draw_context, void *user_data) {
	struct gral_demo *demo = user_data;
	draw_star(draw_context, 20.0f, 20.0f, 160.0f);
	if (demo->mouse_inside) gral_draw_context_fill(draw_context, 0.0f, 1.0f, 0.0f, 1.0f);
	else gral_draw_context_fill(draw_context, 1.0f, 0.0f, 0.0f, 1.0f);
	gral_draw_context_new_path(draw_context);
	draw_star(draw_context, 220.0f, 20.0f, 160.0f);
	gral_draw_context_stroke(draw_context, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	gral_draw_context_draw_text(draw_context, demo->text, 50.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void resize(int width, int height, void *user_data) {
	printf("resize: %dx%d\n", width, height);
}

static void mouse_enter(void *user_data) {
	struct gral_demo *demo = user_data;
	demo->mouse_inside = 1;
	gral_window_request_redraw(demo->window);
}

static void mouse_leave(void *user_data) {
	struct gral_demo *demo = user_data;
	demo->mouse_inside = 0;
	gral_window_request_redraw(demo->window);
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
		&character
	};
	demo->window = gral_window_create(demo->application, 800, 600, "test", &interface, demo);
	demo->mouse_inside = 0;
	demo->text = gral_text_create(demo->window, "Test", 16.0f);
}

int main(int argc, char **argv) {
	struct gral_demo demo;
	struct gral_application_interface interface = {&initialize};
	demo.application = gral_application_create("com.github.eyelash.libgral.demo", &interface, &demo);
	int result = gral_application_run(demo.application, argc, argv);
	gral_text_delete(demo.text);
	gral_window_delete(demo.window);
	gral_application_delete(demo.application);
	return result;
}
