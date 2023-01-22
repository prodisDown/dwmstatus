#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include "util.h"

typedef union Arg {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

enum ErrorNum {
	ERR_OK             = 0,
	ERR_FAILED         = 1,
	ERR_PANIC          = 2,
	ERR_INVALID_INPUT  = 9,
};

enum UpdateType {
	UP_ONCE,
	UP_ATLEAST,
	UP_WALLCLOCK,	/* wallclock.wait   -- update each `.wait` seconds
			 * wallclock.offset -- offset updates by `.offset` seconds from Unix-time */
};

typedef union UpdateArg {
	struct timespec ts;
	struct {
		time_t wait;
		time_t offset;
	} wallclock;
} UpdateArg;

typedef union UpdateCtx {
	struct {
		struct timespec last;
		struct timespec next;
	};
} UpdateCtx;

typedef struct Update {
	const enum UpdateType type;
	const UpdateArg arg;
} Update;

typedef struct Widget {
	ssize_t (*func)(char *restrict buf,
	                size_t buflen,
	                void *ctx,
	                const Arg arg);
	const size_t buflen;
	const size_t ctx_size;
	const Arg arg;
} Widget;

/* functions */
static void *ecalloc(size_t, size_t);
static void init_update(int, UpdateCtx **);
static void init_widget(int, char **, void **);
static void loop(void);
static void push_status(const char *restrict);
static void setup_signals(void);
static void sighandle_exitnow(int);
static void update_status(void);
void die(enum ErrorNum);

/* variables */
#ifndef DEBUG_NO_X11
static Display *dpy;
#endif
static char *restrict status;
static struct timespec now;
static volatile int exitnow;

static UpdateCtx **update_ctx;
static void **widget_ctx;
static char **widget_buf;

/* Add your widgets to `widgets.h` file */
#include "widgets.h"

/* Configuration, allows nested code to use variables above */
#include "config.h"
#if VERSION_MAJOR != CONFIG_VERSION_MAJOR \
 || VERSION_MINOR != CONFIG_VERSION_MINOR
#error "`config.h` has different API version from config.mk. Check your `config.h` for compatibility and change its version."
#endif

static void *ecalloc(size_t nmemb, size_t size)
{
	void *m = calloc(nmemb, size);
	if (m == NULL) die(ERR_PANIC);
	return m;
}

static void init_update(int u, UpdateCtx **u_ctx)
{
	switch(update[u].type) {
	case UP_WALLCLOCK:
		*u_ctx = (UpdateCtx *)ecalloc(1, sizeof(UpdateCtx));
		break;
	default:
		/* undefined */
		die(ERR_PANIC);
	}
}

static void init_widget(int w, char **w_buf, void **w_ctx)
{
	/* widget buffer */
	size_t w_buflen = widget[w].buflen;
	if (0 < w_buflen)
		*w_buf = (char *)ecalloc(w_buflen, sizeof(char));

	/* widget context */
	size_t w_ctx_size = widget[w].ctx_size;
	if (0 < w_ctx_size)
		*w_ctx = (void *)ecalloc(1, w_ctx_size);
}

static void update_status(void)
{
	char *cur = status;
	short off;

	if (status_begin) {
		off = sprintf(cur, "%s", status_begin);
		cur += off;
	}

	for(short w=0; COUNT(widget)>w; ++w)
	{
		if (NULL == widget_buf[w])
			continue;
		off = snprintf(cur, widget[w].buflen, "%s", widget_buf[w]);
		off = off > widget[w].buflen ? widget[w].buflen : off;
		cur += off;

		if (status_delim && COUNT(widget)-1 > w) {
			off = sprintf(cur, "%s", status_delim);
			cur += off;
		}
	}

	if (status_end) {
		off += sprintf(cur, "%s", status_end);
		cur += off;
	}

	*cur = '\0';
}

static void push_status(const char *restrict src)
{
#ifndef DEBUG_NO_X11
	XStoreName(dpy, DefaultRootWindow(dpy), src);
	XSync(dpy, False);
#endif
#ifdef DEBUG_STDOUT
	fprintf(stdout, "%s\n", src);
#endif
}


void die(enum ErrorNum code)
{
#ifndef DEBUG_NO_X11
	if (dpy)
		XCloseDisplay(dpy);
#endif

	exit(code);
}

static void sighandle_exitnow(int signal)
{
	exitnow = signal;
}

static void setup_signals(void)
{
	struct sigaction sa;

	sa.sa_handler = sighandle_exitnow;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
}

static void loop(void)
{
	/* default_lag := now + 60s */
	struct timespec default_lag = {.tv_sec=60, .tv_nsec=0};
	struct timespec closest;

	for(short u=0; COUNT(update)>u; ++u) {
		const Update *up = &update[u];
		UpdateCtx *u_ctx = update_ctx[u];
		const short *u_wd = (const short *)update_widgets[u];

		clock_gettime(CLOCK_REALTIME, &now);

		/* diff := next - now */
		{
			struct timespec diff;
			timespec_sub(&diff, &u_ctx->next, &now);
			if (0 <= diff.tv_sec)
				/* too early*/ continue;
		}

		memcpy(&u_ctx->last, &now, sizeof(now));
		for(short w = *u_wd; -1 < w; w = *(++u_wd))
		{
			const Widget *wd = &widget[w];
			void *w_ctx = widget_ctx[w];
			char *w_buf = widget_buf[w];
			(wd->func)(w_buf, wd->buflen, w_ctx, wd->arg);
		}

		switch(up->type) {
		case UP_WALLCLOCK:
			/* next := now + period - ((now - offset) % period) */
			if (up->arg.wallclock.wait <= 1) {
				u_ctx->next.tv_sec = now.tv_sec + 1;
			} else {
				struct timespec A = {0, 0};
				A.tv_sec = now.tv_sec - up->arg.wallclock.offset;
				A.tv_sec %= up->arg.wallclock.wait;
				A.tv_sec = up->arg.wallclock.wait - A.tv_sec;
				u_ctx->next.tv_sec = now.tv_sec + A.tv_sec;
			}
			u_ctx->next.tv_nsec = 0;
			break;
		default:
			continue;
		}
	}

	update_status();
	push_status(status);

	clock_gettime(CLOCK_REALTIME, &now);
	timespec_add(&closest, &now, &default_lag); /* default wait value */
	for(short u = 0; COUNT(update) > u; ++u) {
		UpdateCtx *u_ctx = update_ctx[u];
		if (u_ctx->next.tv_sec < closest.tv_sec
		|| u_ctx->next.tv_nsec < closest.tv_nsec)
			memcpy(&closest, &u_ctx->next, sizeof(u_ctx->next));
	}

	clock_gettime(CLOCK_REALTIME, &now);
	{
		struct timespec diff;
		timespec_sub(&diff, &closest, &now);
		if (0 <= diff.tv_sec)
			nanosleep(&diff, NULL);
	}
}

int main(void)
{
	/* updates initialization */
	update_ctx = ecalloc(COUNT(update), sizeof(void *));
	for(int u=0; COUNT(update)>u; ++u)
		init_update(u, &update_ctx[u]);
	/* widgets initialization */
	widget_buf = ecalloc(COUNT(widget), sizeof(void *));
	widget_ctx = ecalloc(COUNT(widget), sizeof(void *));
	for(int w=0; COUNT(widget)>w; ++w)
		init_widget(w, &widget_buf[w], &widget_ctx[w]);

	/* status string initialization */
	status = ecalloc(STATUS_BUFLEN, sizeof(char));

#ifndef DEBUG_NO_X11
	/* X initialization */
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		ERROR("XOpenDisplay returned error\n");
		die(ERR_PANIC);
	}
#endif

#ifdef NICE_LVL
	nice(NICE_LVL);
#endif

	exitnow = 0;
	setup_signals();

	for(;;)
	{
		loop();
		if (exitnow) goto sig_exitnow;
	}

sig_exitnow:
	/* Got SIGTERM, exit cleanly */
	die(ERR_OK);
}

