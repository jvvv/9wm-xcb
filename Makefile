# Makefile for 9wm-xcb
CFLAGS = -DSHAPE
CFLAGS_DBG = -g -O0 -DDEBUG -DDEBUG_EV $(CFLAGS)
LDFLAGS = -lxcb -lxcb-util -lxcb-icccm -lxcb-image -lxcb-shape
PREFIX=/opt
BIN = $(PREFIX)/bin

MANDIR = $(PREFIX)/man/man1
MANSUFFIX = 1

SRCS = client.c cursor.c error.c event.c evq.c grab.c main.c manage.c menu.c xutil.c
SRCS_DBG = client.c cursor.c error.c event.c evq.c grab.c main.c manage.c menu.c xutil.c showevent.c
OBJS = $(SRCS:.c=.o)
OBJS_DBG = $(SRCS_DBG:.c=.g)
HDRS = dat.h fns.h patchlevel.h

all: 9wm-xcb

debug: 9wm-xcb.dbg

.SUFFIXES : .g

.c.g:
	$(CC) $(CFLAGS_DBG) -c $< -o $*.g

9wm-xcb.dbg: $(OBJS_DBG)
	$(CC) $(CFLAGS_DBG) -o $@ $(OBJS_DBG) $(LDFLAGS)

9wm-xcb: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): $(HDRS)

$(OBJS_DBG): $(HDRS)

install: 9wm-xcb
	cp 9wm-xcb $(BIN)/9wm

install.man:
	cp 9wm.man $(MANDIR)/9wm.$(MANSUFFIX)

trout: 9wm.man
	troff -man 9wm.man >trout

vu: trout
	xditview trout

clean:
	rm -f 9wm-xcb 9wm-xcb.dbg *.o *.g core trout
