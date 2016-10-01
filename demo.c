#include <gral.h>
#include <stdio.h>

static int close(void) {
	return 1;
}

static void draw(struct gral_painter *painter) {
	gral_painter_set_color(painter, 1.0f, 0.0f, 0.0f, 1.0f);
	gral_painter_draw_rectangle(painter, 10, 10, 100, 100);
}

static void resize(int width, int height) {
	printf("resize: %dx%d\n", width, height);
}

int main(int argc, char **argv) {
	gral_init(&argc, &argv);
	struct gral_window_interface handler = {&close, &draw, &resize};
	gral_window_create(800, 600, "test", &handler);
	return gral_run();
}
