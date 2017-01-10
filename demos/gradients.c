#include <gral.h>

struct gral_demo {
	struct gral_application *application;
	struct gral_window *window;
};

static int close(void *user_data) {
	return 1;
}

static void add_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height) {
	gral_draw_context_move_to(draw_context, x, y);
	gral_draw_context_line_to(draw_context, x+width, y);
	gral_draw_context_line_to(draw_context, x+width, y+height);
	gral_draw_context_line_to(draw_context, x, y+height);
	gral_draw_context_close_path(draw_context);
}

static void draw(struct gral_draw_context *draw_context, void *user_data) {
	struct gral_demo *demo = user_data;
	struct gral_gradient_stop stops[] = {
		{0.f, 1.f, 0.f, 0.f, 1.f},
		{1.f, 0.f, 1.f, 0.f, 1.f}
	};
	add_rectangle(draw_context, 50.f, 50.f, 200.f, 200.f);
	gral_draw_context_fill_linear_gradient(draw_context, 50.f, 250.f, 150.f, 50.f, stops, 2);
}

static void resize(int width, int height, void *user_data) {

}

static void mouse_enter(void *user_data) {

}

static void mouse_leave(void *user_data) {

}

static void mouse_move(float x, float y, void *user_data) {

}

static void mouse_button_press(int button, void *user_data) {

}

static void mouse_button_release(int button, void *user_data) {

}

static void scroll(float dx, float dy, void *user_data) {

}

static void character(uint32_t c, void *user_data) {

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
	demo->window = gral_window_create(demo->application, 800, 600, "gral gradients demo", &interface, demo);
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
