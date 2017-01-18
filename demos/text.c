#include <gral.h>

struct gral_demo {
	struct gral_application *application;
	struct gral_window *window;
	struct gral_text *text;
};

static int close(void *user_data) {
	return 1;
}

static void draw(struct gral_draw_context *draw_context, void *user_data) {
	struct gral_demo *demo = user_data;
	float width = gral_text_get_width(demo->text);
	gral_draw_context_fill_rectangle(draw_context, 50.f, 50.f, width, 1.f, 1.f, 0.f, 0.f, 1.f);
	gral_draw_context_fill_rectangle(draw_context, 50.f, 35.f, width, 25.f, 1.f, 0.f, 0.f, .2f);
	gral_draw_context_draw_text(draw_context, demo->text, 50.f, 50.f, 0.f, 0.f, 1.f, 1.f);
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
	demo->window = gral_window_create(demo->application, 800, 600, "gral text demo", &interface, demo);
	demo->text = gral_text_create(demo->window, "gral text demo", 16.f);
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
