NAME = dwmstatus
VERSION = 1.1
NICE_LVL = 9
#DEBUGFLAGS = -DDEBUG_NORENAME -DDEBUG_STDOUT

# paths
PREFIX = ~/.local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11

# flags
CPPFLAGS = ${DEBUGFLAGS} \
	   -DVERSION=\"${VERSION}\" \
	   -DNAME=\"${NAME}\" \
	   -DNICE_LVL=${NICE_LVL} \
	   -D_DEFAULT_SOURCE
CFLAGS = -std=c11 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -s ${LIBS}

# compiler and linker
CC = gcc

