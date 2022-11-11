#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
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
	/* Special meaning for `struct timespec` fields.
	 * - update each `.tv_sec` seconds
	 * - offset updates by `.tv_nsec` seconds from Unix-time
	 * */
	UP_WALLCLOCK,
	UP_ONDEMAND,
//	UP_FORKED,
//	UP_STATIC,
};

typedef union UpdateArg {
	struct timespec tp;
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


/* status specific structs */

struct mktimes_arg {
	const char *fmt;
	const char *tzname;
};

struct getbattery_arg {
	const char *dir;
	const char *n_present,
	           *n_energy_design,
	           *n_energy_now,
	           *n_power_now,
	           *n_status;
	const char *(*status_txt);
};

struct getbattery_ctx {
	int fd_dir;
};

struct gettemperature_arg {
	const char *dir;
	const char *n_sensor;
};

struct gettemperature_ctx {
	int fd_hwmon;
};


/* functions */
ssize_t mktimes(char *restrict, size_t, void *, const Arg);
ssize_t getbattery(char *restrict, size_t, void *, const Arg);
ssize_t gettemperature(char *restrict, size_t, void *, const Arg);
static void *ecalloc(size_t, size_t);
static void init_updates(void);
static void init_widgets(void);
static void update_status(void);
static void push_status(const char *restrict);
void die(enum ErrorNum);

/* variables */
static Display *dpy;
static char *restrict status;

/* configuration, allows nested code to use variables above */
#include "config.h"

UpdateCtx *(update_ctx[COUNT(update)]) = {0};
void *(widget_ctx[COUNT(widget)]) = {0};
char *(widget_buf[COUNT(widget)]) = {0};

ssize_t mktimes(char *restrict buf, size_t buflen, void *ctx, const Arg arg)
{
	(void)ctx;
	struct mktimes_arg *s = (struct mktimes_arg *)arg.v;

	struct timespec ts;
	struct tm tm;

	/* If ct->tzname == NULL, leave TZ undefined and use system localtime.
	 */
	unsetenv("TZ");
	if (s->tzname != NULL)
		setenv("TZ", s->tzname, 1);
	tzset();

	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);

	size_t ws = strftime(buf, buflen, s->fmt, &tm);
	if (0 >= ws)
		goto error;

	return (ssize_t)ws;

error:
	return -1;
}

ssize_t getbattery(char *restrict buf, size_t buflen, void *ctx, const Arg arg)
{
	struct getbattery_arg *s = (struct getbattery_arg *)arg.v;
	struct getbattery_ctx *c = (struct getbattery_ctx *)ctx;

	short status_idx;
	long energy_design = -1,
	     energy_now = -1,
	     power_now = -1;

	/* Open battery directory if it wasn't */
	if (c->fd_dir == 0)
		c->fd_dir = open(s->dir, O_RDONLY|O_DIRECTORY);
	if (0 > c->fd_dir) {
		goto error;
	}

	/* BAT: present */
	{
		char rbuf;
		int fd_present = openat(c->fd_dir, s->n_present, O_RDONLY);
		if (0 > fd_present) {
			goto error;
		}

		read(fd_present, &rbuf, 1);
		close(fd_present);

		if (rbuf != '1') {
			sprintf(buf, "no battery");
			return 11;
		}
	}
	/* BAT: energy designed capacity */
	{
		char rbuf[32] = {0};
		int fd_energy_design = openat(c->fd_dir,
		                              s->n_energy_design,
		                              O_RDONLY);
		if (0 > fd_energy_design) {
			goto error;
		}

		read(fd_energy_design, rbuf, COUNT(rbuf));
		close(fd_energy_design);

		if (rbuf[COUNT(rbuf)-1] != '\0') {
			goto error;
		}
		if (0 >= sscanf(rbuf, "%ld", &energy_design)) {
			goto error;
		}
	}
	/* BAT: energy remaining capacity */
	{
		char rbuf[32] = {0};
		int fd_energy_now = openat(c->fd_dir,
		                              s->n_energy_now,
		                              O_RDONLY);
		if (0 > fd_energy_now) {
			goto error;
		}

		read(fd_energy_now, rbuf, COUNT(rbuf));
		close(fd_energy_now);

		if (rbuf[COUNT(rbuf)-1] != '\0') {
			goto error;
		}
		if (0 >= sscanf(rbuf, "%ld", &energy_now)) {
			goto error;
		}
	}
	/* BAT: current power consumption */
	{
		char rbuf[32] = {0};
		int fd_power_now = openat(c->fd_dir,
		                              s->n_power_now,
		                              O_RDONLY);
		if (0 > fd_power_now) {
			goto error;
		}

		read(fd_power_now, rbuf, COUNT(rbuf));
		close(fd_power_now);

		if (rbuf[COUNT(rbuf)-1] != '\0') {
			goto error;
		}
		if (0 >= sscanf(rbuf, "%ld", &power_now)) {
			goto error;
		}
	}
	/* BAT: current status of the battery */
	{
		char rbuf[32] = {0};
		int fd_status = openat(c->fd_dir,
		                              s->n_status,
		                              O_RDONLY);
		if (0 > fd_status) {
			goto error;
		}

		read(fd_status, rbuf, COUNT(rbuf));
		close(fd_status);

		if (rbuf[COUNT(rbuf)-1] != '\0')
			goto error;

		if (!strcmp(rbuf,
				"Discharging\n"))
			status_idx=2;
		else if (!strcmp(rbuf,
				"Charging\n"))
			status_idx=1;
		else
			status_idx=0;
	}

	if (0 > energy_now || 0 > energy_design)
		goto error;

	int psize = snprintf(buf, buflen, "%s %.2f %.2f",
	        s->status_txt[status_idx],
	        ((float)energy_now / (float)energy_design) * 100.0,
	        (float)power_now / 1e6f);
	if (psize >= buflen)
		goto error;

	return (ssize_t)psize;
error:
	return -1;
}

ssize_t gettemperature(char *restrict buf, const size_t buflen, void *ctx, const Arg arg)
{
	struct gettemperature_arg *s = (struct gettemperature_arg *)arg.v;
	struct gettemperature_ctx *c = (struct gettemperature_ctx *)ctx;

	/* Find out what hwmon to watch */
	if (0 >= c->fd_hwmon) {
		DIR *d_hwmons;
		struct dirent *e_hwmons;
		char p_hwmons[PATH_MAX];

		sprintf(p_hwmons, "%s/hwmon", s->dir);
		d_hwmons = opendir(p_hwmons);
		if (NULL == d_hwmons)
		{
			c->fd_hwmon = -1;
			goto error;
		}

		while (NULL != (e_hwmons = readdir(d_hwmons)))
		{
			if (e_hwmons->d_type == DT_DIR
			  && e_hwmons->d_name[0] != '.')
				break;
		}
		if (e_hwmons == NULL) {
			c->fd_hwmon = -1;
			closedir(d_hwmons);
			goto error;
		}

		c->fd_hwmon = openat(dirfd(d_hwmons),
		                     e_hwmons->d_name,
		                     O_RDONLY|O_DIRECTORY);
		closedir(d_hwmons);
	}
	if (c->fd_hwmon < 0)
		goto error;

	/* Read sensor */
	int psize;
	{
		char rbuf[21] = {0};
		int fd_sensor = openat(c->fd_hwmon, s->n_sensor, O_RDONLY);
		if (0 > fd_sensor)
			goto error;

		read(fd_sensor, rbuf, 21);
		close(fd_sensor);

		if (rbuf[20] != '\0')
			goto error;
		psize = snprintf(buf, buflen,
		                 "%2.0f°C",
		                 atof(rbuf) / 1e3f);
	}
	if (psize >= buflen)
		goto error;

	return psize;
error:;
	buf[0] = '\0';
	return -1;
}

static void *ecalloc(size_t nmemb, size_t size)
{
	void *m = calloc(nmemb, size);
	if (!m)
		die(ERR_PANIC);
	return m;
}

static void init_updates(void) {
for(int u=0; COUNT(update)>u; ++u)
{
	switch(update[u].type) {
	case UP_WALLCLOCK:
	case UP_ONDEMAND:
		update_ctx[u] = (UpdateCtx *)ecalloc(1, sizeof(UpdateCtx));
		break;

	default:
		/* undefined */
		die(ERR_PANIC);
	}
}
}

static void init_widgets(void) {
for(int w=0; COUNT(widget)>w; ++w)
{
	/* widget_buf */
	if (0 < widget[w].buflen)
		widget_buf[w] = (char *)ecalloc(widget[w].buflen, sizeof(char));

	/* widget_ctx */
	if (0 < widget[w].ctx_size)
		widget_ctx[w] = ecalloc(1, widget[w].ctx_size);
}
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
#ifndef DEBUG_NORENAME
	XStoreName(dpy, DefaultRootWindow(dpy), src);
	XSync(dpy, False);
#endif
#ifdef DEBUG_STDOUT
	fprintf(stdout, "%s\n", src);
#endif
}


/* Cleanup and exit
 */
void die(enum ErrorNum code)
{
	if (dpy)
		XCloseDisplay(dpy);

	exit(code);
}

int main(void)
{
	status = ecalloc(STATUS_BUFLEN, sizeof(char));

	init_updates();
	init_widgets();

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		ERROR("Error in XOpenDisplay\n");
		die(ERR_PANIC);
	}

#ifdef NICE_LVL
	nice(NICE_LVL);
#endif

/* Main loop */
	struct timespec now;
loop_again:
	;
	/* default_lag := now + 60s */
	struct timespec default_lag = {.tv_sec=60, .tv_nsec=0};
	struct timespec closest;

	clock_gettime(CLOCK_REALTIME, &now);

	for(short u=0; COUNT(update)>u; ++u) {
		const Update up = update[u];
		UpdateCtx *up_ctx = update_ctx[u];
		const short *up_wid = (const short *)update_widgets[u];
		struct timespec diff;

		/* diff := next - now */
		timespec_sub(&diff, &up_ctx->next, &now);

		if (0 > diff.tv_sec) {
			memcpy(&up_ctx->last, &now, sizeof(now));

			for(short w=*up_wid; -1<w; w=*(++up_wid)) {
				const Widget wg = widget[w];
				void *wg_ctx = widget_ctx[w];
				char *wg_buf = widget_buf[w];

				(wg.func)(wg_buf, wg.buflen, wg_ctx, wg.arg);
			}

			switch(up.type) {
			case UP_WALLCLOCK:
				;
				/* next := now + period - ((now - offset) % period) */
				if (up.arg.tp.tv_sec <= 1) {
					up_ctx->next.tv_sec = now.tv_sec + 1;
				} else {
					struct timespec A={0};
					A.tv_sec = now.tv_sec - (time_t)up.arg.tp.tv_nsec;
					A.tv_sec %= up.arg.tp.tv_sec;
					A.tv_sec = up.arg.tp.tv_sec - A.tv_sec;
					up_ctx->next.tv_sec = now.tv_sec + A.tv_sec;
				}
				up_ctx->next.tv_nsec = 0;
				break;

			case UP_ONDEMAND:
				/* next := now + period */
				timespec_add(&up_ctx->next, &now, &up.arg.tp);
				break;

			default:
				continue;
			}
//			DEBUG("u=%hd:next=% .2lld.% .9lld",u,up_ctx->next.tv_sec,up_ctx->next.tv_nsec);

			clock_gettime(CLOCK_REALTIME, &now);
		}
	}

	update_status();
	push_status(status);

	timespec_add(&closest, &now, &default_lag);
	for(short u=0; COUNT(update)>u; ++u) {
		UpdateCtx *up_ctx = update_ctx[u];
		if (up_ctx->next.tv_sec < closest.tv_sec
			|| up_ctx->next.tv_nsec < closest.tv_nsec)
			memcpy(&closest, &up_ctx->next, sizeof(up_ctx->next));
	}
//	DEBUG("closest=% .2lld.% .9lld",closest.tv_sec,closest.tv_nsec);

	clock_gettime(CLOCK_REALTIME, &now);
	{
		struct timespec diff;

		timespec_sub(&diff, &closest, &now);
		if (0 <= diff.tv_sec) {
//			DEBUG("now: % .2lld.% .9lld", now.tv_sec, now.tv_nsec);
//			DEBUG("nanosleep: % .2lld.% .9lld", diff.tv_sec, diff.tv_nsec);
			nanosleep(&diff, NULL);
		}

//		DEBUG("------------------------------");
		goto loop_again;
	}

	/* unreachable */
	die(ERR_OK);
}
