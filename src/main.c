#include <sys/mman.h>
#include <sys/shm.h>

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include <libretro.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dlfcn.h>
#include <time.h>

typedef void (*retro_init_t)(void);
typedef void (*retro_deinit_t)(void);
typedef void (*retro_reset_t)(void);
typedef unsigned (*retro_api_version_t)(void);
typedef void (*retro_get_system_info_t)(struct retro_system_info *info);
typedef void (*retro_get_system_av_info_t)(struct retro_system_av_info *info);
typedef void (*retro_set_environment_t)(retro_environment_t);
typedef void (*retro_set_video_refresh_t)(retro_video_refresh_t);
typedef void (*retro_set_audio_sample_t)(retro_audio_sample_t);
typedef void (*retro_set_audio_sample_batch_t)(retro_audio_sample_batch_t);
typedef void (*retro_set_input_poll_t)(retro_input_poll_t);
typedef void (*retro_set_input_state_t)(retro_input_state_t);
typedef void (*retro_run_t)(void);
typedef bool (*retro_load_game_t)(const struct retro_game_info *info);
typedef void (*retro_unload_game_t)(void);
typedef unsigned (*retro_get_region_t)(void);
typedef bool (*retro_load_game_special_t)(unsigned type,
                                          const struct retro_game_info *info,
                                          size_t num);
typedef size_t (*retro_serialize_size_t)(void);
typedef bool (*retro_serialize_t)(void *data, size_t size);
typedef bool (*retro_unserialize_t)(const void *data, size_t size);
typedef void *(*retro_get_memory_data_t)(unsigned id);
typedef size_t (*retro_get_memory_size_t)(unsigned id);
typedef void (*retro_cheat_reset_t)(void);
typedef void (*retro_cheat_set_t)(unsigned index, bool enabled,
                                  const char *code);

static char sysdir[1024];

struct libretro_core
{
	void *handle;
	uint8_t *video_buf;
	uint8_t *audio_buf;
	struct retro_system_info system_info;
	struct retro_system_av_info system_av_info;
	struct retro_game_info game_info;
	uint8_t keys[(RETRO_DEVICE_ID_JOYPAD_MASK + 7) / 8];
	retro_init_t init;
	retro_deinit_t deinit;
	retro_reset_t reset;
	retro_api_version_t api_version;
	retro_get_system_info_t get_system_info;
	retro_get_system_av_info_t get_system_av_info;
	retro_set_environment_t set_environment;
	retro_set_video_refresh_t set_video_refresh;
	retro_set_audio_sample_t set_audio_sample;
	retro_set_audio_sample_batch_t set_audio_sample_batch;
	retro_set_input_poll_t set_input_poll;
	retro_set_input_state_t set_input_state;
	retro_run_t run;
	retro_load_game_t load_game;
	retro_unload_game_t unload_game;
	retro_get_region_t get_region;
	retro_load_game_special_t load_game_special;
	retro_serialize_size_t serialize_size;
	retro_serialize_t serialize;
	retro_unserialize_t unserialize;
	retro_get_memory_data_t get_memory_data;
	retro_get_memory_size_t get_memory_size;
	retro_cheat_reset_t cheat_reset;
	retro_cheat_set_t cheat_set;
};

struct window
{
	const char *progname;
	Display *display;
	Window window;
	int screen;
	int vsync;
	uint32_t width;
	uint32_t height;
	uint32_t scale;
	XVisualInfo vi;
	Window root;
	GC gc;
	XImage *image;
	XShmSegmentInfo shminfo;
};

static struct libretro_core *g_core;

static uint64_t nanotime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static void retro_log(enum retro_log_level level, const char *fmt, ...)
{
	switch (level)
	{
		case RETRO_LOG_DEBUG:
			fprintf(stderr, "[DEBUG] ");
			break;
		case RETRO_LOG_INFO:
			fprintf(stderr, "[INFO ] ");
			break;
		case RETRO_LOG_WARN:
			fprintf(stderr, "[WARN ] ");
			break;
		case RETRO_LOG_ERROR:
			fprintf(stderr, "[ERROR] ");
			break;
		default:
			fprintf(stderr, "[UNK  ] ");
			break;
	}
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

static bool environment(unsigned cmd, void *data)
{
	switch (cmd)
	{
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		{
			struct retro_input_descriptor *desc = data;
			(void)desc;
			/* XXX */
			return true;
		}
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
			/* XXX */
			return true;
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		{
			struct retro_log_callback *logging = data;
			logging->log = retro_log;
			return true;
		}
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
			*(const char**)data = sysdir;
			return true;
		default:
			return false;
	}
}

static void video_refresh(const void *data, unsigned width, unsigned height,
                          size_t pitch)
{
	if (width != g_core->system_av_info.geometry.base_width
	 || height != g_core->system_av_info.geometry.base_height)
		return;
	for (size_t y = 0; y < height; ++y)
	{
		memcpy(&g_core->video_buf[y * g_core->system_av_info.geometry.base_width * 4],
		       &((uint8_t*)data)[y * pitch], g_core->system_av_info.geometry.base_width * 4);
	}
}

static void audio_sample(int16_t left, int16_t right)
{
	(void)left;
	(void)right;
	/* XXX */
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
	(void)data;
	(void)frames;
	/* XXX */
	return 0;
}

static void input_poll(void)
{
	/* XXX */
}

static int16_t input_state(unsigned port, unsigned device, unsigned index,
                           unsigned id)
{
	(void)port;
	(void)device;
	(void)index;
	if (id >= RETRO_DEVICE_ID_JOYPAD_MASK)
		return 0;
	return !!(g_core->keys[id / 8] & (1 << (id % 8)));
}

static uint8_t *read_rom(const char *progname, const char *file,
                         size_t *rom_size)
{
	uint8_t *data = NULL;
	FILE *fp = fopen(file, "r");
	if (!fp)
	{
		fprintf(stderr, "%s: open: %s\n", progname, strerror(errno));
		return NULL;
	}
	*rom_size = 0;
	while (1)
	{
		uint8_t *newdata = realloc(data, *rom_size + 4096);
		if (!newdata)
		{
			fprintf(stderr, "%s: malloc: %s\n", progname, strerror(errno));
			goto err;
		}
		data = newdata;
		size_t ret = fread(&data[*rom_size], 1, 4096, fp);
		if (ferror(fp))
		{
			fprintf(stderr, "%s: read: %s\n", progname, strerror(errno));
			goto err;
		}
		*rom_size += ret;
		if (feof(fp))
			break;
	}
	fclose(fp);
	return data;

err:
	free(data);
	fclose(fp);
	return NULL;
}

static int load_core(const char *progname, struct libretro_core *core,
                     const char *file, const char *rom_path,
                     const uint8_t *rom_data, size_t rom_size)
{
	memset(core, 0, sizeof(*core));
	core->handle = dlopen(file, RTLD_LAZY);
	if (!core->handle)
	{
		fprintf(stderr, "%s: failed to open core: %s\n", progname, dlerror());
		return 1;
	}
#define LOAD_SYM(name) \
do \
{ \
	core->name = dlsym(core->handle, "retro_" #name); \
	if (!core->name) \
	{ \
		fprintf(stderr, "%s: dlsym(" #name "): %s\n", progname, dlerror()); \
		return 1; \
	} \
} \
while (0)

	LOAD_SYM(api_version);
	if (core->api_version() != RETRO_API_VERSION)
	{
		fprintf(stderr, "%s: invalid libretro API version\n", progname);
		return 1;
	}
	LOAD_SYM(init);
	LOAD_SYM(deinit);
	LOAD_SYM(reset);
	LOAD_SYM(get_system_info);
	LOAD_SYM(get_system_av_info);
	LOAD_SYM(set_environment);
	LOAD_SYM(set_video_refresh);
	LOAD_SYM(set_audio_sample);
	LOAD_SYM(set_audio_sample_batch);
	LOAD_SYM(set_input_poll);
	LOAD_SYM(set_input_state);
	LOAD_SYM(run);
	LOAD_SYM(load_game);
	LOAD_SYM(unload_game);
	LOAD_SYM(get_region);
	LOAD_SYM(load_game_special);
	LOAD_SYM(serialize_size);
	LOAD_SYM(serialize);
	LOAD_SYM(unserialize);
	LOAD_SYM(get_memory_data);
	LOAD_SYM(get_memory_size);
	LOAD_SYM(cheat_reset);
	LOAD_SYM(cheat_set);

#undef LOAD_SYM

	core->set_environment(&environment);
	core->init();

	core->set_video_refresh(&video_refresh);
	core->set_audio_sample(&audio_sample);
	core->set_audio_sample_batch(&audio_sample_batch);
	core->set_input_poll(&input_poll);
	core->set_input_state(&input_state);

	core->game_info.path = rom_path;
	core->game_info.data = rom_data;
	core->game_info.size = rom_size;
	core->game_info.meta = NULL;
	if (!core->load_game(&core->game_info))
	{
		fprintf(stderr, "%s: failed to load game\n", progname);
		return EXIT_FAILURE;
	}

	core->get_system_info(&core->system_info);
	core->get_system_av_info(&core->system_av_info);

	core->video_buf = malloc(core->system_av_info.geometry.base_width
	                       * core->system_av_info.geometry.base_height
	                       * 4);
	if (!core->video_buf)
	{
		fprintf(stderr, "%s: malloc: %s\n", progname, strerror(errno));
		return EXIT_FAILURE;
	}
	core->audio_buf = malloc(core->system_av_info.timing.sample_rate * 2);
	if (!core->audio_buf)
	{
		fprintf(stderr, "%s: malloc: %s\n", progname, strerror(errno));
		return EXIT_FAILURE;
	}
	return 0;
}

static int get_key_id(KeySym sym)
{
	switch (sym)
	{
		case XK_Left:
			return RETRO_DEVICE_ID_JOYPAD_LEFT;
		case XK_Right:
			return RETRO_DEVICE_ID_JOYPAD_RIGHT;
		case XK_Up:
			return RETRO_DEVICE_ID_JOYPAD_UP;
		case XK_Down:
			return RETRO_DEVICE_ID_JOYPAD_DOWN;
		case XK_x:
			return RETRO_DEVICE_ID_JOYPAD_A;
		case XK_z:
			return RETRO_DEVICE_ID_JOYPAD_B;
		case XK_Return:
			return RETRO_DEVICE_ID_JOYPAD_START;
		case XK_Shift_R:
			return RETRO_DEVICE_ID_JOYPAD_SELECT;
	}
	return -1;
}

static int create_shmimg(struct window *window)
{
	uint32_t width = g_core->system_av_info.geometry.base_width * window->scale;
	uint32_t height = g_core->system_av_info.geometry.base_height * window->scale;
	window->image = XShmCreateImage(window->display, window->vi.visual, 24,
	                        ZPixmap, NULL, &window->shminfo,
	                        (width + 3) & ~3, height);
	if (!window->image)
	{
		fprintf(stderr, "%s: failed to create image\n",
		        window->progname);
		return 1;
	}
	window->shminfo.shmid = shmget(IPC_PRIVATE,
	                               window->image->bytes_per_line
	                             * window->image->height,
	                               IPC_CREAT | 0777);
	if (window->shminfo.shmid == -1)
	{
		fprintf(stderr, "%s: shmget: %s\n", window->progname,
		        strerror(errno));
		return 1;
	}
	window->image->data = shmat(window->shminfo.shmid, 0, 0);
	if (!window->image->data)
	{
		fprintf(stderr, "%s: shmat: %s\n", window->progname,
		        strerror(errno));
		return 1;
	}
	window->shminfo.shmaddr = window->image->data;
	window->shminfo.readOnly = False;
	XShmAttach(window->display, &window->shminfo);
	XFlush(window->display);
	if (shmctl(window->shminfo.shmid, IPC_RMID, NULL) == -1)
	{
		fprintf(stderr, "%s: shmctl: %s\n", window->progname,
		        strerror(errno));
		return 1;
	}
	return 0;
}

static void handle_key_press(struct window *window, XKeyEvent *event)
{
	KeySym sym = XLookupKeysym(event, 0);
	if (sym == XK_space)
	{
		window->vsync = !window->vsync;
		return;
	}
	int id = get_key_id(sym);
	if (id < 0)
		return;
	g_core->keys[id / 8] |= 1 << (id % 8);
}

static void handle_key_release(struct window *window, XKeyEvent *event)
{
	(void)window;
	KeySym sym = XLookupKeysym(event, 0);
	int id = get_key_id(sym);
	if (id < 0)
		return;
	g_core->keys[id / 8] &= ~(1 << (id % 8));
}

static void handle_configure(struct window *window, XConfigureEvent *event)
{
	if ((uint32_t)event->width == window->width
	 && (uint32_t)event->height == window->height)
		return;
	XFillRectangle(window->display, window->window, window->gc, 0, 0,
	               event->width, event->height);
	window->width = event->width;
	window->height = event->height;
	uint32_t width_scale = window->width / g_core->system_av_info.geometry.base_width;
	uint32_t height_scale = window->height / g_core->system_av_info.geometry.base_height;
	uint32_t scale;
	if (!width_scale || !height_scale)
		scale = 1;
	else if (width_scale > height_scale)
		scale = height_scale;
	else
		scale = width_scale;
	if (scale == window->scale)
		return;
	window->scale = scale;
	XShmDetach(window->display, &window->shminfo);
	shmdt(window->image->data);
	XDestroyImage(window->image);
	if (create_shmimg(window))
		exit(EXIT_FAILURE);
}

static void handle_events(struct window *window)
{
	while (XPending(window->display))
	{
		XEvent event;
		XNextEvent(window->display, &event);
		switch (event.type)
		{
			case KeyPress:
				handle_key_press(window, &event.xkey);
				break;
			case KeyRelease:
				handle_key_release(window, &event.xkey);
				break;
			case ConfigureNotify:
				handle_configure(window, &event.xconfigure);
				break;
		}
	}
}

static int setup_window(const char *progname, struct window *window)
{
	window->vsync = 1;
	window->progname = progname;
	window->width = g_core->system_av_info.geometry.base_width;
	window->height = g_core->system_av_info.geometry.base_height;
	window->scale = 1;
	window->display = XOpenDisplay(NULL);
	if (!window->display)
	{
		fprintf(stderr, "%s: failed to open display\n", progname);
		return 1;
	}
	window->root = XRootWindow(window->display, 0);
	window->screen = DefaultScreen(window->display);
	if (!XMatchVisualInfo(window->display, window->screen, 24, TrueColor,
	                      &window->vi))
	{
		fprintf(stderr, "%s: failed to find visual\n", progname);
		return 1;
	}
	XSetWindowAttributes swa;
	swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;
	window->window = XCreateWindow(window->display, window->root, 0, 0,
	                               window->width, window->height, 0,
	                               window->vi.depth,
	                               InputOutput, window->vi.visual,
	                               CWEventMask, &swa);
	XGCValues gc_values;
	gc_values.foreground = 0;
	window->gc = XCreateGC(window->display, window->window,
	                       GCForeground, &gc_values);
	if (!window->gc)
	{
		fprintf(stderr, "%s: failed to create GC\n", progname);
		return 1;
	}
	if (create_shmimg(window))
		return 1;
	XMapWindow(window->display, window->window);
	return 0;
}

int main(int argc, char **argv)
{
#ifdef __eklat__
	snprintf(sysdir, sizeof(sysdir), "/lib");
#else
	snprintf(sysdir, sizeof(sysdir), "%s/.config/retroarch/system", getenv("HOME"));
#endif
	if (argc < 3)
	{
		fprintf(stderr, "%s: missing operand\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc > 3)
	{
		fprintf(stderr, "%s: extra operand\n", argv[0]);
		return EXIT_FAILURE;
	}

	size_t rom_size;
	uint8_t *rom_data = read_rom(argv[0], argv[2], &rom_size);
	if (!rom_data)
		return EXIT_FAILURE;

	struct libretro_core core;
	g_core = &core;
	if (load_core(argv[0], &core, argv[1], argv[2], rom_data, rom_size))
		return EXIT_FAILURE;

	struct window window;
	if (setup_window(argv[0], &window))
		return EXIT_FAILURE;

	uint64_t last_frame = nanotime();
	uint64_t frame_duration = 1000000000 / core.system_av_info.timing.fps;
	while (1)
	{
		handle_events(&window);
		core.run();
		if (window.scale > 1)
		{
			for (size_t y = 0; y < core.system_av_info.geometry.base_height; ++y)
			{
				for (size_t yy = 0; yy < window.scale; ++yy)
				{
					for (size_t x = 0; x < core.system_av_info.geometry.base_width; ++x)
					{
						for (size_t xx = 0; xx < window.scale; ++xx)
						{
							uint32_t *dst = (uint32_t*)&((uint8_t*)window.image->data)[(y * window.scale + yy) * window.image->bytes_per_line + (x * window.scale + xx) * 4];
							uint32_t *src = (uint32_t*)&core.video_buf[(core.system_av_info.geometry.base_width * y + x) * 4];
							*dst = *src;
						}
					}
				}
			}
		}
		else
		{
			for (size_t y = 0; y < core.system_av_info.geometry.base_height; ++y)
			{
				uint8_t *dst = &((uint8_t*)window.image->data)[y * window.image->bytes_per_line];
				uint8_t *src = &core.video_buf[core.system_av_info.geometry.base_width * 4 * y];
				memcpy(dst, src, core.system_av_info.geometry.base_width * 4);
			}
		}
		uint32_t dst_width = core.system_av_info.geometry.base_width * window.scale;
		uint32_t dst_height = core.system_av_info.geometry.base_height * window.scale;
		uint32_t dst_x = (window.width - dst_width) / 2;
		uint32_t dst_y = (window.height - dst_height) / 2;
		XShmPutImage(window.display, window.window, window.gc,
		             window.image, 0, 0, dst_x, dst_y,
		             dst_width, dst_height, False);
		XFlush(window.display);
		uint64_t current = nanotime();
		if (window.vsync)
		{
			if (current < last_frame + frame_duration)
			{
				uint64_t delta = frame_duration - (current - last_frame);
				struct timespec ts;
				ts.tv_sec = delta / 1000000000;
				ts.tv_nsec = delta % 1000000000;
				nanosleep(&ts, NULL);
			}
			last_frame += frame_duration;
		}
		else
		{
			last_frame = current;
		}
	}
	return EXIT_SUCCESS;
}
