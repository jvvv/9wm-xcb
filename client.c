/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include "dat.h"
#include "fns.h"

Client	*clients;
Client	*current;

void setactive(Client *c, int on)
{
	if (!c)
	{
		eprintf("null client\n");
		return;
	}

	eprintf("c=0x%x on=%d (c->window=0x%x)\n", c, on, c->window);

	if (c->parent == c->screen->root)
	{
		fprintf(stderr, "9wm: bad parent in setactive; dumping core\n");
		abort();
	}
	if (on)
	{
		xcb_ungrab_button(dpy, XCB_BUTTON_INDEX_ANY, c->parent,
					XCB_BUTTON_MASK_ANY);
		xcb_set_input_focus(dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
					c->window, timestamp());
		if (c->proto & Ptakefocus)
			sendcmessage(c->window, wm_protocols, wm_take_focus, 0);
		cmapfocus(c);
	}
	else
	{
		xcb_grab_button(dpy, 0, c->parent, ButtonMask,
			XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_SYNC,
			XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_ANY,
			XCB_MOD_MASK_ANY);
	}
	draw_border(c, on);
}

void draw_border(Client *c, int active)
{
	xcb_rectangle_t rect;
	uint32_t value;

	if (!c)
	{
		eprintf("null client\n");
		return;
	}

	eprintf("c=0x%x active=%d (c->window=0x%x)\n", c, active, c->window);

	value =	active ? c->screen->black : c->screen->white;
	xcb_change_window_attributes(dpy, c->parent, XCB_CW_BACK_PIXEL, &value);
	xcb_clear_area(dpy, 0, c->parent, 0, 0, 0, 0);
	if (c->hold && active)
	{
		rect.x = INSET;
		rect.y = INSET;
		rect.width = c->dx + BORDER - INSET;
		rect.height = c->dy + BORDER - INSET;
		xcb_poly_rectangle(dpy, c->parent, c->screen->gc0, 1, &rect);
	}
}

void active(Client *c)
{
	Client *cc;

	if (!c)
	{
		eprintf("null client\n");
		return;
	}

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);

	if (c == current)
		return;
	if (current)
	{
		setactive(current, 0);
		if (current->screen != c->screen)
			cmapnofocus(current->screen);
	}
	setactive(c, 1);
	for (cc = clients; cc; cc = cc->next)
		if (cc->revert == c)
			cc->revert = c->revert;
	c->revert = current;
	while (c->revert && !normal(c->revert))
		c->revert = c->revert->revert;
	current = c;
#ifdef	DEBUG
	if (debug)
		dump_revert();
#endif
}

void nofocus()
{
	static xcb_window_t w = 0;
	uint32_t mask;
	uint32_t values[2];
	Client *c;

	if (current) {
		setactive(current, 0);
		for (c = current->revert; c; c = c->revert)
			if (normal(c)) {
				active(c);
				return;
			}
		cmapnofocus(current->screen);
		/* if no candidates to revert to, fall through */
	}
	current = 0;
	if (w == 0)
	{
		mask = XCB_CW_OVERRIDE_REDIRECT;
		values[0] = 1;
		w = xcb_generate_id(dpy);
		xcb_create_window(dpy, XCB_COPY_FROM_PARENT, w,
			screens[0].root, 0, 0, 1, 1, 0,
			XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT,
			mask, values);
		xcb_map_window(dpy, w);
	}
	xcb_set_input_focus(dpy, XCB_INPUT_FOCUS_POINTER_ROOT, w,
				XCB_CURRENT_TIME);
}

void top(Client *c)
{
	Client **l, *cc;

	if (!c)
	{
		eprintf("null client\n");
		return;
	}

	eprintf("c=0x%x (c->window=0x%x\n", c, c->window);

	l = &clients;
	for (cc = *l; cc; cc = *l)
	{
		if (cc == c)
		{
			*l = c->next;
			c->next = clients;
			clients = c;
			return;
		}
		l = &cc->next;
	}
	fprintf(stderr, "9wm: %x not on client list in top()\n", c);
}

Client * getclient(xcb_window_t w, int create)
{
	Client *c = NULL;

	eprintf("w=0x%x create=%d\n", w, create);

	if (w == 0 || getscreen(w))
		return 0;

	for (c = clients; c; c = c->next)
		if (c->window == w || c->parent == w)
			return c;

	if (create)
	{
		c = (Client *)xalloc(sizeof(Client));
		c->window = w;
		/* c->parent will be set by the caller */
		c->state = XCB_ICCCM_WM_STATE_WITHDRAWN;
		c->next = clients;
		clients = c;
	}
	
	return c;
}

void rmclient(Client *c)
{
	Client *cc;

	if (!c)
	{
		eprintf("null client\n");
		return;
	}

	eprintf("c=0x%x (c->window=0x%x)\n");

	for (cc = current; cc && cc->revert; cc = cc->revert)
		if (cc->revert == c)
			cc->revert = cc->revert->revert;

	if (c == clients)
		clients = c->next;
	for (cc = clients; cc && cc->next; cc = cc->next)
		if (cc->next == c)
			cc->next = cc->next->next;

	if (hidden(c))
		unhidec(c, 0);

	if (c->parent != c->screen->root)
		xcb_destroy_window(dpy, c->parent);

	c->parent = c->window = XCB_NONE;		/* paranoia */
	if (current == c)
	{
		current = c->revert;
		if (current == 0)
			nofocus();
		else
		{
			if (current->screen != c->screen)
				cmapnofocus(c->screen);
			setactive(current, 1);
		}
	}
	if (c->instance)
		free(c->instance);
	if (c->class)
		free(c->class);
	if (c->name)
		free(c->name);
	if (c->iconname)
		free(c->iconname);
	if (c->ncmapwins != 0)
	{
		free((char *)c->cmapwins);
		free((char *)c->wmcmaps);
	}
	memset(c, 0, sizeof(Client));		/* paranoia */
	free(c);
}

#ifdef	DEBUG
void dump_revert(void)
{
	Client *c;
	int i;

	eprintf("(dump follows)\n");

	i = 0;
	for (c = current; c; c = c->revert)
	{
		fprintf(stderr, "%s(%x:%d)", c->label ? c->label : "?", c->window, c->state);
		if (i++ > 100)
			break;
		if (c->revert)
			fprintf(stderr, " -> ");
	}
	if (current == 0)
		fprintf(stderr, "empty");
	fprintf(stderr, "\n");
}

void dump_clients(void)
{
	Client *c;

	eprintf("dump follows\n");
	for (c = clients; c; c = c->next)
		fprintf(stderr, "w 0x%x parent 0x%x @ (%d, %d)\n", c->window, c->parent, c->x, c->y);
}
#endif
