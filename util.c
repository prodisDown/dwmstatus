#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "util.h"


void eprintf(const char *filename, int line, const char *funcname, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:%s:", filename, line, funcname);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}

/* @return int 0 - success, path ready
 * @return int 1 - failure, check errno */
int makepath(int atfd, const char *path)
{
	char tmp[PATH_MAX] = {0};
	size_t len;
	char *p;
	struct stat st;

	strcpy(tmp, path);
	len = strlen(path);
	if (len > 1 && tmp[len - 1] == '/')
		tmp[len - 1] = '\0';
	errno = 0;
	if (!fstatat(atfd, tmp, &st, 0)) {
		if (!S_ISDIR(st.st_mode)) {
			errno = ENOTDIR;
			ERROR("File exists, but it isn't a directory.");
			NOTE("%s", tmp);
			return -1;
		} else
			return 0;
	}
	if (errno != ENOENT) {
		ERROR("%s", strerror(errno));
		return -1;
	}

	for (p = tmp + 1; *p != '\0'; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		errno = 0;
		if (mkdirat(atfd, tmp, S_IRWXU) && errno != EEXIST) {
			ERROR("Can't create a directiory. %s", strerror(errno));
			NOTE("%s", tmp);
			return -1;
		}
		*p = '/';
	}
	errno = 0;
	if (mkdirat(atfd, tmp, S_IRWXU) && errno != EEXIST) {
		ERROR("Can't create a directiory. %s", strerror(errno));
		NOTE("%s", tmp);
		return 1;
	}
	return 0;
}

/* A minus B (both must be positive) */
void timespec_sub(struct timespec *dest, const struct timespec *A, const struct timespec *B)
{
	struct timespec X = {
		.tv_sec = A->tv_sec - B->tv_sec,
		.tv_nsec = A->tv_nsec % 1000000000 - B->tv_nsec % 1000000000
	};

	if (0 > X.tv_nsec) {
		X.tv_nsec += 1000000000;
		--X.tv_sec;
	}

	memcpy(dest, &X, sizeof(X));
}

/* A plus B (both must be positive) */
void timespec_add(struct timespec *dest, const struct timespec *A, const struct timespec *B)
{
	struct timespec X = {
		.tv_sec = A->tv_sec + B->tv_sec,
		.tv_nsec = A->tv_nsec % 1000000000 + B->tv_nsec % 1000000000
	};

	if (1000000000 <= X.tv_nsec) {
		X.tv_nsec -= 1000000000;
		++X.tv_sec;
	}

	memcpy(dest, &X, sizeof(X));
}
