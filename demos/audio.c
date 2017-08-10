#include <gral.h>

#define FREQUENCY 220.f

#define INT16(x) ((int16_t)((x) * 32767.f + .5f))

static float saw(float x) {
	return x < .5f ? x * 2.f : x * 2.f - 2.f;
}

static int callback(int16_t *buffer, int frames) {
	static float x = 0.f;
	static float position_x = 0.f;
	static int played_frames = 0;
	if (played_frames + frames > 44100 * 5) frames = 44100 * 5 - played_frames;
	int t;
	for (t = 0; t < frames; t++) {
		float value = saw(x) * .4f;
		float position = saw(position_x) * .5f + .5f;
		buffer[2*t] = INT16(value * (1.f - position));
		buffer[2*t+1] = INT16(value * position);
		x += FREQUENCY / 44100.f;
		if (x > 1.f) x -= 1.f;
		position_x += 1.f / 44100.f;
		if (position_x > 1.f) position_x -= 1.f;
	}
	played_frames += frames;
	return frames;
}

int main() {
	gral_audio_play(&callback);
	return 0;
}
