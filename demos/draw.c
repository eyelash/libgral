#include <gral.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

#define RAD(deg) ((deg) * ((float)M_PI / 180.0f))

#define FONT_SIZE 80.0f

struct demo_application {
	struct gral_application *application;
};

struct demo_window {
	struct gral_window *window;
	struct gral_text *text;
	float ascent;
	float descent;
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

static void add_arc(struct gral_draw_context *draw_context, float cx, float cy, float radius, float start_angle, float sweep_angle) {
	float h = 4.0f / 3.0f * tanf(sweep_angle / 4.0f);
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
	float radius = size / 2.0f;
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
	float radius = size / 2.0f;
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

static void add_shapes(struct gral_draw_context *draw_context) {
	add_circle(draw_context, 20.0f, 220.0f, 160.0f);
	add_rounded_rectangle(draw_context, 220.0f, 220.0f, 160.0f, 160.0f, 20.0f);
	add_star(draw_context, 420.0f, 220.0f, 160.0f);
}

static void draw(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data) {
	struct demo_window *window = user_data;
	float text_width = gral_text_get_width(window->text, draw_context);
	float text_height = window->ascent + window->descent;
	float text_x = (600.0f - text_width) / 2.0f;
	float text_y = (200.0f - text_height) / 2.0f + window->ascent;

	// background
	add_rectangle(draw_context, 0.0f, 0.0f, width, height);
	gral_draw_context_fill(draw_context, 0.2f, 0.2f, 0.2f, 1.0f);

	// grid
	add_rectangle(draw_context, 0.0f, roundf(text_y), 600.0f, 1.0f);
	add_rectangle(draw_context, 0.0f, roundf(text_y - window->ascent), 600.0f, 1.0f);
	add_rectangle(draw_context, 0.0f, roundf(text_y + window->descent), 600.0f, 1.0f);
	int i;
	for (i = 0; i <= 7; i++) {
		float x = gral_text_index_to_x(window->text, i);
		add_rectangle(draw_context, roundf(text_x + x), 0.f, 1.0f, 200.0f);
	}
	static struct gral_gradient_stop const grid_stops[] = {
		{0.0f, 0.4f, 0.4f, 0.4f, 1.0f},
		{1.0f, 0.2f, 0.2f, 0.2f, 1.0f}
	};
	gral_draw_context_fill_linear_gradient(draw_context, 0.0f, 0.0f, 0.0f, 200.0f, grid_stops, 2);

	// text and shapes
	gral_draw_context_add_text(draw_context, window->text, roundf(text_x), roundf(text_y));
	add_shapes(draw_context);
	static struct gral_gradient_stop const fill_stops[] = {
		{0.00f, 0.7f, 0.7f, 0.0f, 1.0f}, // yellow
		{0.35f, 0.7f, 0.4f, 0.0f, 1.0f}, // orange
		{0.65f, 0.7f, 0.0f, 0.0f, 1.0f}, // red
		{1.00f, 0.7f, 0.0f, 0.4f, 1.0f}  // purple
	};
	gral_draw_context_fill_linear_gradient(draw_context, 20.0f, 0.0f, 580.0f, 0.0f, fill_stops, 4);
	gral_draw_context_add_text(draw_context, window->text, roundf(text_x), roundf(text_y));
	add_shapes(draw_context);
	static struct gral_gradient_stop const stroke_stops[] = {
		{0.00f, 1.0f, 1.0f, 0.0f, 1.0f}, // yellow
		{0.35f, 1.0f, 0.5f, 0.0f, 1.0f}, // orange
		{0.65f, 1.0f, 0.0f, 0.0f, 1.0f}, // red
		{1.00f, 1.0f, 0.0f, 0.5f, 1.0f}  // purple
	};
	gral_draw_context_stroke_linear_gradient(draw_context, 2.0f, 20.0f, 0.0f, 580.0f, 0.0f, stroke_stops, 4);
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

static void key_release(int key, int key_code, void *user_data) {

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
		&focus_leave
	};
	window->window = gral_window_create(application->application, 600, 400, "gral draw demo", &window_interface, window);
	struct gral_font *font = gral_font_create_default(window->window, FONT_SIZE);
	window->text = gral_text_create(window->window, "libgral", font);
	gral_font_get_metrics(window->window, font, &window->ascent, &window->descent);
	gral_font_delete(font);
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
	application.application = gral_application_create("com.github.eyelash.libgral.demos.draw", &application_interface, &application);
	int result = gral_application_run(application.application, argc, argv);
	gral_application_delete(application.application);
	return result;
}
