#ifndef UTIL_H
#define UTIL_H

/* stringify macro name */
#define __STRINGIFY_QUOTE__(...) #__VA_ARGS__
#define STRINGIFY(defd_name) __STRINGIFY_QUOTE__(defd_name)

#define COUNT(x) (sizeof(x) / sizeof((x)[0]))

//#define NULLOBJ(t) ((t *)0)

/* functions */

void eprintf(const char *filename, int line, const char *funcname, const char *fmt, ...);
#define ERROR(...) ((eprintf)(__FILE__, __LINE__, __func__, "ERROR: " __VA_ARGS__))
#define NOTE(...)  ((eprintf)(__FILE__, __LINE__, __func__, "NOTE: " __VA_ARGS__))
#define INFO(...)  ((eprintf)(__FILE__, __LINE__, __func__, "INFO: " __VA_ARGS__))
#define DEBUG(...) ((eprintf)(__FILE__, __LINE__, __func__, "DEBUG: " __VA_ARGS__))

/* @return 0 - success, path presented
 * @return 1 - failure, check errno */
int makepath(int atfd, const char *path);

/* A minus B (both must be positive) */
void timespec_sub(struct timespec *dest, const struct timespec *A, const struct timespec *B);

/* A plus B (both must be positive) */
void timespec_add(struct timespec *dest, const struct timespec *A, const struct timespec *B);

#endif /* UTIL_H */
