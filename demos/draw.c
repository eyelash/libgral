#include <gral.h>
#define _USE_MATH_DEFINES
#include <math.h>

#define RAD(deg) ((deg) * ((float)M_PI / 180.f))

struct demo {
	struct gral_application *application;
	struct gral_window *window;
};

static int close(void *user_data) {
	return 1;
}

static void add_arc(struct gral_draw_context *draw_context, float cx, float cy, float radius, float start_angle, float sweep_angle) {
	float h = 4.f / 3.f * tanf(sweep_angle / 4.f);
	float cos_start = cosf(start_angle) * radius;
	float sin_start = sinf(start_angle) * radius;
	float end_angle = start_angle + sweep_angle;
	float cos_end = cosf(end_angle) * radius;
	float sin_end = sinf(end_angle) * radius;
	float x = cx + cos_end;
	float y = cy + sin_end;
	float x1 = cx + cos_start - sin_start * h;
	float y1 = cy + sin_start + cos_start * h;
	float x2 = x + sin_end * h;
	float y2 = y - cos_end * h;
	gral_draw_context_curve_to(draw_context, x1, y1, x2, y2, x, y);
}

static void add_circle(struct gral_draw_context *draw_context, float x, float y, float size) {
	float radius = size / 2.f;
	gral_draw_context_move_to(draw_context, x, y + radius);
	add_arc(draw_context, x + radius, y + radius, radius, RAD(180), RAD(90));
	add_arc(draw_context, x + radius, y + radius, radius, RAD(270), RAD(90));
	add_arc(draw_context, x + radius, y + radius, radius, RAD(0), RAD(90));
	add_arc(draw_context, x + radius, y + radius, radius, RAD(90), RAD(90));
}

static void add_rounded_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height, float radius) {
	gral_draw_context_move_to(draw_context, x, y + radius);
	add_arc(draw_context, x + radius, y + radius, radius, RAD(180), RAD(90));
	gral_draw_context_line_to(draw_context, x + width - radius, y);
	add_arc(draw_context, x + width - radius, y + radius, radius, RAD(270), RAD(90));
	gral_draw_context_line_to(draw_context, x + width, y + height - radius);
	add_arc(draw_context, x + width - radius, y + height - radius, radius, RAD(0), RAD(90));
	gral_draw_context_line_to(draw_context, x + radius, y + height);
	add_arc(draw_context, x + radius, y + height - radius, radius, RAD(90), RAD(90));
	gral_draw_context_close_path(draw_context);
}

static void add_star(struct gral_draw_context *draw_context, float x, float y, float size) {
	float radius = size / 2.f;
	float cx = x + radius;
	float cy = y + radius;
	float a = RAD(270);
	gral_draw_context_move_to(draw_context, cx, y);
	int i;
	for (i = 1; i < 5; i++) {
		a += RAD(360 / 5 * 2);
		gral_draw_context_line_to(draw_context, cx + cosf(a)*radius, cy + sinf(a)*radius);
	}
	gral_draw_context_close_path(draw_context);
}

static void draw(struct gral_draw_context *draw_context, void *user_data) {
	gral_draw_context_add_rectangle(draw_context, 20.f, 20.f, 160.f, 160.f);
	gral_draw_context_fill(draw_context, .9f, .1f, .1f, 1.f);
	gral_draw_context_add_rectangle(draw_context, 220.f, 20.f, 160.f, 160.f);
	gral_draw_context_stroke(draw_context, 4.f, .1f, .1f, .9f, 1.f);
	gral_draw_context_add_rectangle(draw_context, 420.f, 20.f, 160.f, 160.f);
	struct gral_gradient_stop stops[] = {
		{0.f, .9f, .1f, .1f, 1.f},
		{.3f, .9f, .9f, .1f, 1.f},
		{1.f, .1f, .9f, .1f, 1.f}
	};
	gral_draw_context_fill_linear_gradient(draw_context, 420.f, 20.f, 580.f, 100.f, stops, 3);
	add_circle(draw_context, 20.f, 220.f, 160.f);
	add_rounded_rectangle(draw_context, 220.f, 220.f, 160.f, 160.f, 20.f);
	add_star(draw_context, 420.f, 220.f, 160.f);
	gral_draw_context_fill(draw_context, .1f, .1f, .9f, 1.f);
}

static void resize(int width, int height, void *user_data) {

}

static void mouse_enter(void *user_data) {

}

static void mouse_leave(void *user_data) {

}

static void mouse_move(float x, float y, void *user_data) {

}

static void mouse_button_press(float x, float y, int button, void *user_data) {

}

static void mouse_button_release(float x, float y, int button, void *user_data) {

}

static void scroll(float dx, float dy, void *user_data) {

}

static void text(const char *s, void *user_data) {

}

static void paste(const char *text, void *user_data) {

}

static int timer(void *user_data) {
	return 0;
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
		&scroll,
		&text,
		&paste,
		&timer
	};
	demo->window = gral_window_create(demo->application, 600, 400, "gral draw demo", &interface, demo);
	gral_window_set_minimum_size(demo->window, 600, 400);
	gral_window_show(demo->window);
}

int main(int argc, char **argv) {
	struct demo demo;
	struct gral_application_interface interface = {&initialize};
	demo.application = gral_application_create("com.github.eyelash.libgral.demos.draw", &interface, &demo);
	int result = gral_application_run(demo.application, argc, argv);
	gral_window_delete(demo.window);
	gral_application_delete(demo.application);
	return result;
}
