#include <gral.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct demo_application {
	struct gral_application *application;
	struct gral_timer *timer;
	struct gral_midi *midi;
};

struct demo_window {
	struct gral_window *window;
	struct demo_application *application;
};

static void destroy(void *user_data) {
	printf("destroy\n");
	struct demo_window *window = user_data;
	free(window);
}

static int close(void *user_data) {
	printf("close\n");
	return 1;
}

static void draw(struct gral_draw_context *draw_context, int x, int y, int width, int height, void *user_data) {

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

static void mouse_move_relative(float dx, float dy, void *user_data) {
	printf("mouse move relative: {%f, %f}\n", dx, dy);
}

static void mouse_button_press(float x, float y, int button, int modifiers, void *user_data) {
	printf("mouse button press: {%f, %f} (modifiers: %X)\n", x, y, modifiers);
	struct demo_window *window = user_data;
	if (window->application->timer) {
		gral_timer_delete(window->application->timer);
		window->application->timer = 0;
	}
}

static void mouse_button_release(float x, float y, int button, void *user_data) {
	printf("mouse button release: {%f, %f}\n", x, y);
}

static void double_click(float x, float y, int button, int modifiers, void *user_data) {
	printf("double click: {%f, %f} (modifiers: %X)\n", x, y, modifiers);
}

static void scroll(float dx, float dy, void *user_data) {
	printf("scroll {%f, %f}\n", dx, dy);
}

static uint32_t utf8_get_next(char const **utf8) {
	char const *c = *utf8;
	if ((c[0] & 0x80) == 0x00) {
		*utf8 += 1;
		return c[0];
	}
	if ((c[0] & 0xE0) == 0xC0) {
		*utf8 += 2;
		return (c[0] & 0x1F) << 6 | (c[1] & 0x3F);
	}
	if ((c[0] & 0xF0) == 0xE0) {
		*utf8 += 3;
		return (c[0] & 0x0F) << 12 | (c[1] & 0x3F) << 6 | (c[2] & 0x3F);
	}
	if ((c[0] & 0xF8) == 0xF0) {
		*utf8 += 4;
		return (c[0] & 0x07) << 18 | (c[1] & 0x3F) << 12 | (c[2] & 0x3F) << 6 | (c[3] & 0x3F);
	}
	fprintf(stderr, "invalid UTF-8\n");
	return 0;
}

static void key_press(int key, int scan_code, int modifiers, void *user_data) {
	printf("key press: %X (%X) (modifiers: %X)\n", key, scan_code, modifiers);
}

static void key_release(int key, int scan_code, void *user_data) {
	printf("key release: %X (%X)\n", key, scan_code);
}

static void text(char const *s, void *user_data) {
	while (*s) {
		printf("text: U+%X\n", utf8_get_next(&s));
	}
}

static void focus_enter(void *user_data) {
	printf("focus enter\n");
}

static void focus_leave(void *user_data) {
	printf("focus leave\n");
}

static void timer(void *user_data) {
	printf("timer %f\n", gral_time_get_monotonic());
}

static void note_on(unsigned char note, unsigned char velocity, void *user_data) {
	printf("note on: %u (%u)\n", note, velocity);
}

static void note_off(unsigned char note, unsigned char velocity, void *user_data) {
	printf("note off: %u (%u)\n", note, velocity);
}

static void control_change(unsigned char controller, unsigned char value, void *user_data) {
	printf("control change: %u, %u\n", controller, value);
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
	window->window = gral_window_create(application->application, 600, 400, "gral events demo", &window_interface, window);
	window->application = application;
	gral_window_show(window->window);
}

static void start(void *user_data) {
	printf("start\n");
	struct demo_application *application = user_data;
	application->timer = gral_timer_create(1000, &timer, application);
	static struct gral_midi_interface const midi_interface = {&note_on, &note_off, &control_change};
	application->midi = gral_midi_create(application->application, "gral events demo", &midi_interface, application);
}

static void open_empty(void *user_data) {
	printf("open empty\n");
	create_window(user_data);
}

static void open_file(char const *path, void *user_data) {
	printf("open file: %s\n", path);
	create_window(user_data);
}

static void quit(void *user_data) {
	printf("quit\n");
	struct demo_application *application = user_data;
	gral_midi_delete(application->midi);
}

int main(int argc, char **argv) {
	struct demo_application application;
	static struct gral_application_interface const application_interface = {&start, &open_empty, &open_file, &quit};
	application.application = gral_application_create("com.github.eyelash.libgral.demos.events", &application_interface, &application);
	int result = gral_application_run(application.application, argc, argv);
	gral_application_delete(application.application);
	return result;
}
