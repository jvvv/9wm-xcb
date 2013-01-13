/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_event.h>
#include "dat.h"
#include "fns.h"

int	ignore_badwindow;

void fatal(char *s)
{
	fprintf(stderr, "9wm: ");
	perror(s);
	fprintf(stderr, "\n");
	exit(1);
}

void handler(xcb_generic_error_t *e)
{
	char msg[80], req[80], number[80];

	if (initting && (e->major_code == XCB_CHANGE_WINDOW_ATTRIBUTES)
				&& (e->error_code == XCB_ACCESS))
	{
		fprintf(stderr, "9wm: it looks like there's already a window manager running;  9wm not started\n");
		exit(1);
	}

	if (ignore_badwindow && (e->error_code == XCB_WINDOW
				|| e->error_code == XCB_COLORMAP))
		return;

	fprintf(stderr, "9wm: request=%s (major %d, minor %d),",
			xcb_event_get_request_label(e->major_code),
			e->major_code, e->minor_code);
	fprintf(stderr, " error=%s (%d), resource_id=%d\n",
		xcb_event_get_error_label(e->error_code),
		e->error_code, e->resource_id);

	if (initting)
	{
		fprintf(stderr, "9wm: failure during initialisation; aborting\n");
		exit(1);
	}
}

void flush_errors(void)
{
	xcb_generic_event_t *ev;

	while ((ev = checkmaskevent(dpy, 0)) != NULL)
		handler((xcb_generic_error_t *)ev);
}

void graberror(char *f, int err)
{
	char *s;

	switch (err)
	{
		case XCB_GRAB_STATUS_NOT_VIEWABLE:
			s = "not viewable";
			break;
		case XCB_GRAB_STATUS_ALREADY_GRABBED:
			s = "already grabbed";
			break;
		case XCB_GRAB_STATUS_FROZEN:
			s = "grab frozen";
			break;
		case XCB_GRAB_STATUS_INVALID_TIME:
			s = "invalid time";
			break;
		case XCB_GRAB_STATUS_SUCCESS:
			return;
		default:
			fprintf(stderr, "9wm: %s: grab error: %d\n", f, err);
			return;
	}
	fprintf(stderr, "9wm: %s: grab error: %s\n", f, s);
}

#ifdef	DEBUG

void dotrace(char *s, Client *c, xcb_generic_event_t *e)
{
	fprintf(stderr, "9wm: %s: c=0x%x", s, c);
	if (c)
		fprintf(stderr, " x %d y %d dx %d dy %d w 0x%x parent 0x%x",
			c->x, c->y, c->dx, c->dy, c->window, c->parent);
#ifdef	DEBUG_EV
	if (e)
	{
		fprintf(stderr, "\n\t");
		showevent(e);
	}
#endif
	fprintf(stderr, "\n");
}
#endif
