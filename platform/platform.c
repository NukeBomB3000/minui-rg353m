// rg353m (RK3566, dArkOS, KMS/DRM)
// Based on gkdpixel2 platform — key differences:
//   - SDL_VIDEODRIVER=kmsdrm (set via systemd env, not here)
//   - Gamepad on event4 (retrogame_joypad / singleadc-joypad)
//   - Volume buttons on event3 (gpio-keys)
//   - Power on event0 (rk805 pwrkey)
//   - RAW_MENU = 316 (BTN_MODE)
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////

// event4: retrogame_joypad (main gamepad)
#define RAW_UP		544 // BTN_DPAD_UP
#define RAW_DOWN	545 // BTN_DPAD_DOWN
#define RAW_LEFT	546 // BTN_DPAD_LEFT
#define RAW_RIGHT	547 // BTN_DPAD_RIGHT
#define RAW_A		305 // BTN_EAST
#define RAW_B		304 // BTN_SOUTH
#define RAW_X		307 // BTN_NORTH
#define RAW_Y		308 // BTN_WEST
#define RAW_START	315 // BTN_START
#define RAW_SELECT	314 // BTN_SELECT
#define RAW_MENU	316 // BTN_MODE (F button)
#define RAW_L1		310 // BTN_TL
#define RAW_L2		312 // BTN_TL2
#define RAW_L3		317 // BTN_THUMBL
#define RAW_R1		311 // BTN_TR
#define RAW_R2		313 // BTN_TR2
#define RAW_R3		318 // BTN_THUMBR

// event3: gpio-keys (volume)
#define RAW_PLUS	115 // KEY_VOLUMEUP
#define RAW_MINUS	114 // KEY_VOLUMEDOWN

// event0: rk805 pwrkey
#define RAW_POWER	116 // KEY_POWER

// Analog stick axis codes (event4, EV_ABS)
#define RAW_LSX		0  // ABS_X
#define RAW_LSY		1  // ABS_Y
#define RAW_RSX		3  // ABS_RX
#define RAW_RSY		4  // ABS_RY

// event0=power, event3=volume, event4=gamepad
#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[2] = open("/dev/input/event4", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
}
void PLAT_quitInput(void) {
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

// from <linux/input.h> — BTN_ constants conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY	0x01
#define EV_ABS	0x03

void PLAT_pollInput(void) {
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn;
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0;
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;

			if (type==EV_KEY) {
				if (value>1) continue; // ignore key repeat events

				pressed = value;
				LOG_info("key event: %i (%i)\n", code, pressed);
					 if (code==RAW_UP)		{ btn = BTN_DPAD_UP;	id = BTN_ID_DPAD_UP; }
				else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN;	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT;	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT;	id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_A)		{ btn = BTN_A;			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B;			id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X;			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y;			id = BTN_ID_Y; }
				else if (code==RAW_START)	{ btn = BTN_START;		id = BTN_ID_START; }
				else if (code==RAW_SELECT)	{ btn = BTN_SELECT;		id = BTN_ID_SELECT; }
				else if (code==RAW_MENU)	{ btn = BTN_MENU;		id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1;			id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2;			id = BTN_ID_L2; }
				else if (code==RAW_L3)		{ btn = BTN_L3;			id = BTN_ID_L3; }
				else if (code==RAW_R1)		{ btn = BTN_R1;			id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2;			id = BTN_ID_R2; }
				else if (code==RAW_R3)		{ btn = BTN_R3;			id = BTN_ID_R3; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS;		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS;		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER;		id = BTN_ID_POWER; }
			}
			else if (type==EV_ABS) {
				LOG_info("abs event: %i (%i)\n", code, value);
					 if (code==RAW_LSX) { pad.laxis.x = (value * 32767) / 1800; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_LSY) { pad.laxis.y = (value * 32767) / 1800; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_RSX) pad.raxis.x = (value * 32767) / 1800;
				else if (code==RAW_RSY) pad.raxis.y = (value * 32767) / 1800;
				btn = BTN_NONE;
			}

			if (btn==BTN_NONE) continue;

			if (!pressed) {
				pad.is_pressed		&= ~btn;
				pad.just_repeated	&= ~btn;
				pad.just_released	|= btn;
			}
			else if ((pad.is_pressed & btn)==BTN_NONE) {
				pad.just_pressed	|= btn;
				pad.just_repeated	|= btn;
				pad.is_pressed		|= btn;
				pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
			}
		}
	}
}

int PLAT_shouldWake(void) {
	static uint32_t sleep_start = 0;
	int input;
	static struct input_event event;

	// Record when we first entered sleep so we can ignore buffered events
	if (sleep_start == 0) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		sleep_start = (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
	}

	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			uint32_t event_ms = (uint32_t)(event.time.tv_sec * 1000 + event.time.tv_usec / 1000);
			// Ignore events that were buffered before we went to sleep
			if (event_ms < sleep_start) continue;
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				sleep_start = 0;
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Texture* effect;

	SDL_Surface* buffer;
	SDL_Surface* screen;

	GFX_Renderer* blit;

	int width;
	int height;
	int pitch;
	int sharpness;
} vid;

static int device_width;
static int device_height;
static int device_pitch;

SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	PLAT_enableBacklight(1);

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	vid.texture = SDL_CreateTexture(vid.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
	vid.target  = NULL;

	vid.buffer  = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen  = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width   = w;
	vid.height  = h;
	vid.pitch   = p;

	device_width  = w;
	device_height = h;
	device_pitch  = p;

	vid.sharpness = SHARPNESS_SOFT;

	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0);
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {}

static int hard_scale = 4;

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;

	if (w>=device_width && h>=device_height) hard_scale = 1;
	else if (h>=160) hard_scale = 2;
	else hard_scale = 4;

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);

	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);

	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w*hard_scale, h*hard_scale);
	}
	else {
		vid.target = NULL;
	}

	vid.buffer = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.width  = w;
	vid.height = h;
	vid.pitch  = p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w, h, p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {}
void PLAT_setNearestNeighbor(int enabled) {}

void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.width, vid.height, p);
}

static struct FX_Context {
	int scale, type, color;
	int next_scale, next_type, next_color;
	int live_type;
} effect = {
	.scale=1, .next_scale=1,
	.type=EFFECT_NONE, .next_type=EFFECT_NONE, .live_type=EFFECT_NONE,
	.color=0, .next_color=0,
};

static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
	uint8_t red   = (rgb565 >> 11) & 0x1F;
	uint8_t green = (rgb565 >> 5)  & 0x3F;
	uint8_t blue  =  rgb565        & 0x1F;
	*r = (red   << 3) | (red   >> 2);
	*g = (green << 2) | (green >> 4);
	*b = (blue  << 3) | (blue  >> 2);
}

static void updateEffect(void) {
	if (effect.next_scale==effect.scale && effect.next_type==effect.type && effect.next_color==effect.color) return;

	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type  = effect.next_type;
	effect.color = effect.next_color;

	if (effect.type==EFFECT_NONE) return;
	if (effect.type==effect.live_type && effect.scale==live_scale && effect.color==live_color) return;

	char* effect_path;
	int opacity = 128;
	if (effect.type==EFFECT_LINE) {
		if      (effect.scale<3) effect_path = RES_PATH "/line-2.png";
		else if (effect.scale<4) effect_path = RES_PATH "/line-3.png";
		else if (effect.scale<5) effect_path = RES_PATH "/line-4.png";
		else if (effect.scale<6) effect_path = RES_PATH "/line-5.png";
		else if (effect.scale<8) effect_path = RES_PATH "/line-6.png";
		else                     effect_path = RES_PATH "/line-8.png";
	}
	else if (effect.type==EFFECT_GRID) {
		if      (effect.scale<3)  { effect_path = RES_PATH "/grid-2.png";  opacity = 64; }
		else if (effect.scale<4)  { effect_path = RES_PATH "/grid-3.png";  opacity = 112; }
		else if (effect.scale<5)  { effect_path = RES_PATH "/grid-4.png";  opacity = 144; }
		else if (effect.scale<6)  { effect_path = RES_PATH "/grid-5.png";  opacity = 160; }
		else if (effect.scale<8)  { effect_path = RES_PATH "/grid-6.png";  opacity = 112; }
		else if (effect.scale<11) { effect_path = RES_PATH "/grid-8.png";  opacity = 144; }
		else                      { effect_path = RES_PATH "/grid-11.png"; opacity = 136; }
	}

	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		if (effect.type==EFFECT_GRID && effect.color) {
			uint8_t r, g, b;
			rgb565_to_rgb888(effect.color, &r, &g, &b);
			uint32_t* pixels = (uint32_t*)tmp->pixels;
			for (int y=0; y<tmp->h; y++) {
				for (int x=0; x<tmp->w; x++) {
					uint8_t _,a;
					SDL_GetRGBA(pixels[y*tmp->w+x], tmp->format, &_,&_,&_,&a);
					if (a) pixels[y*tmp->w+x] = SDL_MapRGBA(tmp->format, r,g,b,a);
				}
			}
		}
		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
}

void PLAT_setEffect(int next_type)      { effect.next_type  = next_type; }
void PLAT_setEffectColor(int next_color){ effect.next_color = next_color; }
void PLAT_vsync(int remaining) { if (remaining>0) SDL_Delay(remaining); }

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w, vid.blit->true_h, vid.blit->src_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (!vid.blit) {
		resizeVideo(device_width, device_height, FIXED_PITCH);
		SDL_UpdateTexture(vid.texture, NULL, vid.screen->pixels, vid.screen->pitch);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}

	SDL_UpdateTexture(vid.texture, NULL, vid.blit->src, vid.blit->src_p);

	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer, vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		SDL_SetRenderTarget(vid.renderer, NULL);
		x *= hard_scale; y *= hard_scale;
		w *= hard_scale; h *= hard_scale;
		target = vid.target;
	}

	SDL_Rect* src_rect = &(SDL_Rect){x,y,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};
	if (vid.blit->aspect==0) {
		int bw = vid.blit->src_w * vid.blit->scale;
		int bh = vid.blit->src_h * vid.blit->scale;
		dst_rect->x = (device_width  - bw) / 2;
		dst_rect->y = (device_height - bh) / 2;
		dst_rect->w = bw;
		dst_rect->h = bh;
	}
	else if (vid.blit->aspect>0) {
		int bh = device_height;
		int bw = bh * vid.blit->aspect;
		if (bw>device_width) {
			double ratio = 1.0 / vid.blit->aspect;
			bw = device_width;
			bh = bw * ratio;
		}
		dst_rect->x = (device_width  - bw) / 2;
		dst_rect->y = (device_height - bh) / 2;
		dst_rect->w = bw;
		dst_rect->h = bh;
	}

	SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
	updateEffect();
	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect)
		SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,dst_rect->w,dst_rect->h}, dst_rect);

	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

int PLAT_supportsOverscan(void) { return 1; }

///////////////////////////////

#define OVERLAY_WIDTH  PILL_SIZE
#define OVERLAY_HEIGHT PILL_SIZE
#define OVERLAY_BPP    4
#define OVERLAY_DEPTH  16
#define OVERLAY_PITCH  (OVERLAY_WIDTH * OVERLAY_BPP)
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000
static struct OVL_Context { SDL_Surface* overlay; } ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT), OVERLAY_DEPTH, OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void)      { if (ovl.overlay) SDL_FreeSurface(ovl.overlay); }
void PLAT_enableOverlay(int e)   {}

///////////////////////////////

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/ac/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;
}

void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight/bl_power", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
}

void PLAT_powerOff(void) {
	sleep(2);
	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	FILE* f = fopen("/tmp/poweroff", "w"); if (f) fclose(f);
	exit(0);
}

///////////////////////////////

// RK3566 CPU frequency scaling
#define GOVERNOR_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU:        freq =  600000; break;
		case CPU_SPEED_POWERSAVE:   freq = 1200000; break;
		case CPU_SPEED_NORMAL:      freq = 1608000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1800000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

void PLAT_setVolume(int volume) {
	SetVolume(volume);
}
int PLAT_getVolume(void) {
	return GetVolume();
}

void PLAT_setRumble(int strength) {}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

static char model[256];
char* PLAT_getModel(void) {
	char buffer[256];
	getFile("/proc/device-tree/model", buffer, 256);
	char* tmp = strrchr(buffer, ' ');
	if (tmp) strcpy(model, tmp+1);
	else strcpy(model, "RG353M");
	return model;
}

int PLAT_isOnline(void) { return 0; }
