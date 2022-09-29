#define _BSD_SOURCE
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>

/* macros */
#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

/* definitions */
typedef int FD;

struct mktimes {
	const char *fmt;
	const char *tzname;
};

struct getbattery {
	const char *base;
	const char *n_present, *n_descap, *n_remcap, *n_pownow, *n_status;

	FD fd_base;
};

struct gettemperature {
	const char *base;
	const char *n_sensor;

	FD fd_hwmon;
};


/* variables */
static Display *dpy;
#include "config.h"

/* functions */
static char *mktimes(char *strbuf, struct mktimes *ct);
static void setstatus(char *str);
static char *readfile(char *strbuf, int len, FD fd);
static char *getbattery(char *strbuf, struct getbattery *ct);
static char *gettemperature(char *strbuf, struct gettemperature *ct);

static char *
mktimes(char *strbuf, struct mktimes *ct)
{
	char buf[64] = {0};
	struct timespec tim;
	struct tm timtm;

	unsetenv("TZ");
	/* if ct->tzname == NULL, leave TZ unseted and use system localtime */
	if (ct->tzname)
		setenv("TZ", ct->tzname, 1);
	tzset();
	clock_gettime(CLOCK_REALTIME, &tim);
	localtime_r(&tim.tv_sec, &timtm);

	if (!strftime(buf, sizeof(buf)-1, ct->fmt, &timtm)) {
		*strbuf = '\0';
		return strbuf;
	}

	sprintf(strbuf, "%s", buf);
	return strbuf;
}

static void
setstatus(char *str)
{
#if 1
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
#else
	fprintf(stdout, "%s\n", str);
#endif
}

static char *
readfile(char *strbuf, int len, FD fd)
{
	ssize_t r;
	int rem = len - 1;
	char *strbuf_pos = strbuf;

	r = read(fd, strbuf_pos, rem);
	if (r >= 0)
		return strbuf;
	else
		return NULL;
}

static char *
getbattery(char *strbuf, struct getbattery *ct)
{
	char co[64] = {0};
	char status;
	int descap, remcap, pownow;
	FD fd_present, fd_descap, fd_remcap, fd_pownow, fd_status;

	descap = -1;
	remcap = -1;
	pownow = -1;

	/* Open battery dir if it wasn't */
	if (!ct->fd_base)
		ct->fd_base = open(ct->base, O_RDONLY | O_DIRECTORY);
	if (ct->fd_base < 0)
		goto error;

	/* BAT: present */
	if ((fd_present = openat(ct->fd_base, ct->n_present, O_RDONLY)) < 0)
		goto error;
	readfile(co, sizeof(co), fd_present);
	close(fd_present);
	if (co[0] != '1') {
		sprintf(strbuf, "not present");
		return strbuf;
	}
	memset(co, 0, sizeof(co));

	/* BAT: designed capacity */
	if ((fd_descap = openat(ct->fd_base, ct->n_descap, O_RDONLY)) < 0)
		goto error;
	readfile(co, sizeof(co), fd_descap);
	close(fd_descap);
	if (co == NULL || co[0] == '\0') {
		goto error;
	}
	sscanf(co, "%d", &descap);
	memset(co, 0, sizeof(co));

	if ((fd_remcap = openat(ct->fd_base, ct->n_remcap, O_RDONLY)) < 0)
		goto error;
	readfile(co, sizeof(co), fd_remcap);
	close(fd_remcap);
	if (co == NULL || co[0] == '\0') {
		goto error;
	}
	sscanf(co, "%d", &remcap);
	memset(co, 0, sizeof(co));

	if ((fd_pownow = openat(ct->fd_base, ct->n_pownow, O_RDONLY)) < 0)
		goto error;
	readfile(co, sizeof(co), fd_pownow);
	close(fd_pownow);
	if (co == NULL || co[0] == '\0') {
		goto error;
	}
	sscanf(co, "%d", &pownow);
	memset(co, 0, sizeof(co));

	if ((fd_status = openat(ct->fd_base, ct->n_status, O_RDONLY)) < 0)
		goto error;
	readfile(co, sizeof(co), fd_status);
	close(fd_status);
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '?';
	}

	if (remcap < 0 || descap < 0) {
		sprintf(strbuf, "invalid");
		return strbuf;
	}

	sprintf(strbuf, "%c %.1f %.2fW", status, ((float)remcap / (float)descap) * 100, (float)pownow / 1000000);
	return strbuf;

error:;
	*strbuf = '\0';
	return strbuf;
}

static char *
gettemperature(char *strbuf, struct gettemperature *ct)
{
	char co[64] = {0};
	FD fd_sensor;

	/* Find out what hwmon to watch */
	if (!ct->fd_hwmon) {
		DIR *d_hwmons;
		struct dirent *e_hwmons;
		char p_hwmons[PATH_MAX];

		sprintf(p_hwmons, "%s/hwmon", ct->base);
		if (!(d_hwmons = opendir(p_hwmons))) {
			ct->fd_hwmon = -1;
			goto error;
		}
		while ((e_hwmons = readdir(d_hwmons))) {
			if (e_hwmons->d_type == DT_DIR && e_hwmons->d_name[0] != '.')
				break;
		}
		if (!e_hwmons) {
			ct->fd_hwmon = -1;
			closedir(d_hwmons);
			goto error;
		}
		ct->fd_hwmon = openat(dirfd(d_hwmons), e_hwmons->d_name,
			O_RDONLY | O_DIRECTORY );
		closedir(d_hwmons);
	}
	if (ct->fd_hwmon < 0)
		goto error;

	/* Open and close sensor each call */
	if ((fd_sensor = openat(ct->fd_hwmon, ct->n_sensor, O_RDONLY)) < 0)
		goto error;
	readfile(co, sizeof(co), fd_sensor);
	close(fd_sensor);
	if (co == NULL)
		goto error;
	sprintf(strbuf, "%02.0f°C", atof(co) / 1000);
	return strbuf;

error:;
	*strbuf = '\0';
	return strbuf;
}

int
main(void)
{
	char status[512];
	char time[64];
	char bat[64];
	char tcpu[64];

	const long ns_timeout = 1 * (1000000000);
	const int hu_onclock = 2;
	struct timespec tp;
	struct timespec tpw;
	int hu = 0;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	nice(2);

	for (;;nanosleep(&tpw, NULL)) {
		mktimes(time, &clocks[0]);

		/* update hard stuff less often */
		if (!hu) {
			getbattery(bat, &batteries[0]);
			gettemperature(tcpu, &sensors[0]);
		}
		hu = (hu + 1) % hu_onclock;

		sprintf(status, " %s | %s | %s ",
				tcpu, bat, time);
		setstatus(status);

		/* Correct timeout to lineup with  0-second of each minute */
		clock_gettime(CLOCK_REALTIME, &tp);
		tpw.tv_nsec = ns_timeout - (tp.tv_nsec % ns_timeout);
		tpw.tv_sec = 0;
	}

	XCloseDisplay(dpy);

	return 0;
}

