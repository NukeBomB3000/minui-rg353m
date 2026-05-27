// keymon for rg353m (dArkOS/KMS)
// Monitors volume/brightness keys and headphone jack
// event0=power, event3=gpio-keys (volume), event4=retrogame_joypad
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <sys/time.h>

#include <msettings.h>

#define RELEASED 0
#define PRESSED  1
#define REPEAT   2

#define CODE_MENU  316 // BTN_MODE (F button on RG353M)
#define CODE_PLUS  115 // KEY_VOLUMEUP  (event3 gpio-keys)
#define CODE_MINUS 114 // KEY_VOLUMEDOWN (event3 gpio-keys)

#define VOLUME_MIN     0
#define VOLUME_MAX     20
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 10

// event0=power, event3=volume, event4=gamepad
#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];
static struct input_event ev;

static pthread_t ports_pt;
#define JACK_STATE_PATH "/sys/bus/platform/devices/singleadc-joypad/hp"

int getInt(char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (file) { fscanf(file, "%i", &i); fclose(file); }
	return i;
}

static void* watchPorts(void* arg) {
	int has_headphones, had_headphones;
	has_headphones = had_headphones = getInt(JACK_STATE_PATH);
	SetJack(has_headphones);

	while (1) {
		sleep(1);
		has_headphones = getInt(JACK_STATE_PATH);
		if (had_headphones != has_headphones) {
			had_headphones = has_headphones;
			SetJack(has_headphones);
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {
	printf("keymon\n"); fflush(stdout);
	InitSettings();
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);

	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[2] = open("/dev/input/event4", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	uint32_t val;
	uint32_t menu_pressed = 0;

	uint32_t up_pressed = 0, up_just_pressed = 0, up_repeat_at = 0;
	uint32_t down_pressed = 0, down_just_pressed = 0, down_repeat_at = 0;

	uint8_t ignore;
	uint32_t then, now;
	struct timeval tod;

	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000;
	ignore = 0;

	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		if (now - then > 1000) ignore = 1;

		for (int i = 0; i < INPUT_COUNT; i++) {
			while (read(inputs[i], &ev, sizeof(ev)) == sizeof(ev)) {
				if (ignore) continue;
				val = ev.value;
				if (ev.type != EV_KEY || val > REPEAT) continue;
				switch (ev.code) {
					case CODE_MENU:
						menu_pressed = val;
						break;
					case CODE_PLUS:
						up_pressed = up_just_pressed = val;
						if (val) up_repeat_at = now + 300;
						break;
					case CODE_MINUS:
						down_pressed = down_just_pressed = val;
						if (val) down_repeat_at = now + 300;
						break;
				}
			}
		}

		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = down_repeat_at = 0;
		}

		if (up_just_pressed || (up_pressed && now >= up_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val < BRIGHTNESS_MAX) SetBrightness(++val);
			} else {
				val = GetVolume();
				if (val < VOLUME_MAX) SetVolume(++val);
			}
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}

		if (down_just_pressed || (down_pressed && now >= down_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val > BRIGHTNESS_MIN) SetBrightness(--val);
			} else {
				val = GetVolume();
				if (val > VOLUME_MIN) SetVolume(--val);
			}
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}

		then = now;
		ignore = 0;
		usleep(16666); // ~60fps
	}
}
