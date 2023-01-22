NAME = dwmstatus
VERSION_MAJOR = 1
VERSION_MINOR = 3

NICE_LVL = 9
#DEBUGFLAGS = -DDEBUG_NO_X11 -DDEBUG_STDOUT

# paths
PREFIX = ~/.local
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/include
X11LIB = /usr/lib

# includes and libs
INCS = -I. -I/usr/include -I$(X11INC)
LIBS = -L/usr/lib -lc -L$(X11LIB) -lX11

# flags
CPPFLAGS = $(DEBUGFLAGS) \
	   -DNAME=\"$(NAME)\" \
	   -DVERSION_MAJOR=$(VERSION_MAJOR) \
	   -DVERSION_MINOR=$(VERSION_MINOR) \
	   -DNICE_LVL=$(NICE_LVL) \
	   -D_DEFAULT_SOURCE
CFLAGS = -std=c11 -pedantic -Wall -Os $(INCS) $(CPPFLAGS)
LDFLAGS = -s $(LIBS)

# compiler and linker
CC = gcc

