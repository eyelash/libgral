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

static char const *key_code_to_string(int key_code) {
	switch (key_code) {
	case 0x01: return "Escape";
	case 0x02: return "Digit1";
	case 0x03: return "Digit2";
	case 0x04: return "Digit3";
	case 0x05: return "Digit4";
	case 0x06: return "Digit5";
	case 0x07: return "Digit6";
	case 0x08: return "Digit7";
	case 0x09: return "Digit8";
	case 0x0A: return "Digit9";
	case 0x0B: return "Digit0";
	case 0x0C: return "Minus";
	case 0x0D: return "Equal";
	case 0x0E: return "Backspace";
	case 0x0F: return "Tab";
	case 0x10: return "KeyQ";
	case 0x11: return "KeyW";
	case 0x12: return "KeyE";
	case 0x13: return "KeyR";
	case 0x14: return "KeyT";
	case 0x15: return "KeyY";
	case 0x16: return "KeyU";
	case 0x17: return "KeyI";
	case 0x18: return "KeyO";
	case 0x19: return "KeyP";
	case 0x1A: return "BracketLeft";
	case 0x1B: return "BracketRight";
	case 0x1C: return "Enter";
	case 0x1D: return "ControlLeft";
	case 0x1E: return "KeyA";
	case 0x1F: return "KeyS";
	case 0x20: return "KeyD";
	case 0x21: return "KeyF";
	case 0x22: return "KeyG";
	case 0x23: return "KeyH";
	case 0x24: return "KeyJ";
	case 0x25: return "KeyK";
	case 0x26: return "KeyL";
	case 0x27: return "Semicolon";
	case 0x28: return "Quote";
	case 0x29: return "Backquote";
	case 0x2A: return "ShiftLeft";
	case 0x2B: return "Backslash";
	case 0x2C: return "KeyZ";
	case 0x2D: return "KeyX";
	case 0x2E: return "KeyC";
	case 0x2F: return "KeyV";
	case 0x30: return "KeyB";
	case 0x31: return "KeyN";
	case 0x32: return "KeyM";
	case 0x33: return "Comma";
	case 0x34: return "Period";
	case 0x35: return "Slash";
	case 0x36: return "ShiftRight";
	case 0x37: return "NumpadMultiply";
	case 0x38: return "AltLeft";
	case 0x39: return "Space";
	case 0x3A: return "CapsLock";
	case 0x3B: return "F1";
	case 0x3C: return "F2";
	case 0x3D: return "F3";
	case 0x3E: return "F4";
	case 0x3F: return "F5";
	case 0x40: return "F6";
	case 0x41: return "F7";
	case 0x42: return "F8";
	case 0x43: return "F9";
	case 0x44: return "F10";
	case 0x45: return "NumLock";
	case 0x47: return "Numpad7";
	case 0x48: return "Numpad8";
	case 0x49: return "Numpad9";
	case 0x4A: return "NumpadSubtract";
	case 0x4B: return "Numpad4";
	case 0x4C: return "Numpad5";
	case 0x4D: return "Numpad6";
	case 0x4E: return "NumpadAdd";
	case 0x4F: return "Numpad1";
	case 0x50: return "Numpad2";
	case 0x51: return "Numpad3";
	case 0x52: return "Numpad0";
	case 0x53: return "NumpadDecimal";
	case 0x57: return "F11";
	case 0x58: return "F12";
	case 0x59: return "NumpadEqual";
	case 0xE01C: return "NumpadEnter";
	case 0xE01D: return "ControlRight";
	case 0xE035: return "NumpadDevide";
	case 0xE038: return "AltRight";
	case 0xE047: return "Home";
	case 0xE048: return "ArrowUp";
	case 0xE049: return "PageUp";
	case 0xE04B: return "ArrowLeft";
	case 0xE04D: return "ArrowRight";
	case 0xE04F: return "End";
	case 0xE050: return "ArrowDown";
	case 0xE051: return "PageDown";
	case 0xE052: return "Insert";
	case 0xE053: return "Delete";
	case 0xE05B: return "MetaLeft";
	case 0xE05C: return "MetaRight";
	case 0xE05D: return "ContextMenu";
	default: return "";
	}
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

static void key_press(int key, int key_code, int modifiers, int is_repeat, void *user_data) {
	printf("key press:   %X %X %s (modifiers: %X) %s\n", key, key_code, key_code_to_string(key_code), modifiers, is_repeat ? "(repeat)" : "");
}

static void key_release(int key, int key_code, void *user_data) {
	printf("key release: %X %X %s\n", key, key_code, key_code_to_string(key_code));
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
