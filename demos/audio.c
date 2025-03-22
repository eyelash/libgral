#include <gral.h>

#define FREQUENCY 220.0f

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

int main() {
	struct gral_audio *audio = gral_audio_create("gral audio demo", &callback, NULL);
	gral_sleep(5.0);
	gral_audio_delete(audio);
	return 0;
}
