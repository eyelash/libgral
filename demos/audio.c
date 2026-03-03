#include <gral.h>
#include <stdlib.h>

#define FREQUENCY 220.0f

struct demo_application {
	struct gral_application *application;
	struct gral_audio *audio;
};

struct demo_window {
	struct gral_window *window;
	struct demo_application *application;
};

static float saw(float x) {
	return x < 0.5f ? x * 2.0f : x * 2.0f - 2.0f;
}

static void callback(float *buffer, int frames, void *user_data) {
	static float x = 0.0f;
	static float position_x = 0.0f;
	int t;
	for (t = 0; t < frames; t++) {
		float value = saw(x) * 0.2f;
		float position = saw(position_x) * 0.5f + 0.5f;
		buffer[2*t] = value * (1.0f - position);
		buffer[2*t+1] = value * position;
		x += FREQUENCY / 44100.0f;
		if (x > 1.0f) x -= 1.0f;
		position_x += 1.0f / 44100.0f;
		if (position_x > 1.0f) position_x -= 1.0f;
	}
}

static void destroy(void *user_data) {
	struct demo_window *window = user_data;
	free(window);
}

static int close(void *user_data) {
	return 1;
}

static void draw(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data) {

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
	struct demo_window *window = user_data;
	// space bar toggles the audio playback
	if (key == ' ') {
		if (window->application->audio) {
			gral_audio_delete(window->application->audio);
			window->application->audio = 0;
		}
		else {
			window->application->audio = gral_audio_create(window->application->application, "gral audio demo", &callback, NULL);
		}
	}
}

static void key_release(int key, int key_code, void *user_data) {

}

static void text(char const *s, void *user_data) {

}

static void focus_enter(void *user_data) {

}

static void focus_leave(void *user_data) {

}

static void activate_menu_item(int id, void *user_data) {

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
	window->window = gral_window_create(application->application, 600, 400, "gral audio demo", &window_interface, window);
	window->application = application;
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
	struct demo_application *application = user_data;
	if (application->audio) {
		gral_audio_delete(application->audio);
	}
}

int main(int argc, char **argv) {
	struct demo_application application;
	static struct gral_application_interface const application_interface = {&start, &open_empty, &open_file, &quit};
	application.application = gral_application_create("com.github.eyelash.libgral.demos.audio", &application_interface, &application);
	application.audio = 0;
	int result = gral_application_run(application.application, argc, argv);
	gral_application_delete(application.application);
	return result;
}
