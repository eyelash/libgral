#include <gral.h>

#define FREQUENCY 220.0f

static float saw(float x) {
	return x < 0.5f ? x * 2.0f : x * 2.0f - 2.0f;
}

static int callback(float *buffer, int frames, void *user_data) {
	static float x = 0.0f;
	static float position_x = 0.0f;
	static int played_frames = 0;
	if (played_frames + frames > 44100 * 5) frames = 44100 * 5 - played_frames;
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
	played_frames += frames;
	return frames;
}

static void start(void *user_data) {

}

static void open_empty(void *user_data) {

}

static void open_file(char const *path, void *user_data) {

}

static void quit(void *user_data) {

}

int main() {
	static struct gral_application_interface const application_interface = {&start, &open_empty, &open_file, &quit};
	struct gral_application *application = gral_application_create("com.github.eyelash.libgral.demos.audio", &application_interface, NULL);
	gral_audio_play(&callback, NULL);
	gral_application_delete(application);
	return 0;
}
