# See LICENSE file for copyright and license details.

include config.mk
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)

SRC = main.c util.c
OBJ = $(SRC:.c=.o)

all: options $(NAME)

options:
	@printf '%s\n' '$(NAME) build options:'
	@printf '%s\n' 'CFLAGS   = $(CFLAGS)'
	@printf '%s\n' 'LDFLAGS  = $(LDFLAGS)'
	@printf '%s\n' 'CC       = $(CC)'

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OBJ): config.mk config.h util.h

$(NAME): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	@printf '%s\n' 'cleaning'
	rm -f $(NAME) $(OBJ) $(NAME)-$(VERSION).tar.gz

dist: clean
	@printf '%s\n' 'creating tar-archive for distrbution'
	mkdir -p $(NAME)-$(VERSION)
	cp -R -t $(NAME)-$(VERSION) Makefile LICENSE config.mk \
		$(SRC)
	tar -cf $(NAME)-$(VERSION).tar $(NAME)-$(VERSION)
	gzip $(NAME)-$(VERSION).tar
	rm -rf $(NAME)-$(VERSION)

install: all
	@printf '%s\n' 'installing executable file to $(DESTDIR)$(PREFIX)/bin'
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(NAME) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(NAME)

uninstall:
	@printf '%s\n' 'removing executable file from $(DESTDIR)$(PREFIX)/bin'
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)

.PHONY: all options clean dist install uninstall
