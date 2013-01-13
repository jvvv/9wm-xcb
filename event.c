/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_icccm.h>
#include <xcb/shape.h>
#include "dat.h"
#include "fns.h"
#include "patchlevel.h"

void mainloop(int shape_event)
{
	xcb_generic_event_t *ev;

	eprintf("shape_event=%d\n", shape_event);

	for (;;)
	{
		ev = getevent(dpy);

#ifdef	DEBUG_EV
		if (debug)
		{
			eprintf("evq_len=%d\n", evq_qlen());
			showevent(ev);
		}
#endif
		switch (ev->response_type & ~0x80)
		{
		default:
#ifdef	SHAPE
			if (shape && (ev->response_type & ~0x80) == shape_event)
				shapenotify((xcb_shape_notify_event_t *)ev);
			else
#endif
				fprintf(stderr, "9wm: unknown ev->response_type %d\n", ev->response_type);
		case 0:
			handler((xcb_generic_error_t *)ev);
			break;
		case XCB_BUTTON_PRESS:
			button((xcb_button_press_event_t *)ev);
			break;
		case XCB_BUTTON_RELEASE:
			break;
		case XCB_MAP_REQUEST:
			mapreq((xcb_map_request_event_t *)ev);
			break;
		case XCB_CONFIGURE_REQUEST:
			configurereq((xcb_configure_request_event_t *)ev);
			break;
		case XCB_CIRCULATE_REQUEST:
			circulatereq((xcb_circulate_request_event_t *)ev);
			break;
		case XCB_UNMAP_NOTIFY:
			unmap((xcb_unmap_notify_event_t *)ev);
			break;
		case XCB_CREATE_NOTIFY:
			newwindow((xcb_create_notify_event_t *)ev);
			break;
		case XCB_DESTROY_NOTIFY:
			destroy((xcb_destroy_notify_event_t *)ev);
			break;
		case XCB_CLIENT_MESSAGE:
			clientmesg((xcb_client_message_event_t *)ev);
			break;
		case XCB_COLORMAP_NOTIFY:
			cmap((xcb_colormap_notify_event_t *)ev);
			break;
		case XCB_PROPERTY_NOTIFY:
			property((xcb_property_notify_event_t *)ev);
			break;
		case XCB_SELECTION_CLEAR:
			fprintf(stderr, "9wm: XCB_SELECTION_CLEAR (this should not happen)\n");
			break;
		case XCB_SELECTION_NOTIFY:
			fprintf(stderr, "9wm: XCB_SELECTION_NOTIFY (this should not happen)\n");
			break;
		case XCB_SELECTION_REQUEST:
			fprintf(stderr, "9wm: XCB_SELECTION_REQUEST (this should not happen)\n");
			break;
		case XCB_ENTER_NOTIFY:
			enter((xcb_enter_notify_event_t *)ev);
			break;
		case XCB_REPARENT_NOTIFY:
			reparent((xcb_reparent_notify_event_t *)ev);
			break;
		case XCB_FOCUS_IN:
			focusin((xcb_focus_in_event_t *)ev);
			break;
		case XCB_MOTION_NOTIFY:
		case XCB_EXPOSE:
		case XCB_FOCUS_OUT:
		case XCB_CONFIGURE_NOTIFY:
		case XCB_MAP_NOTIFY:
		case XCB_MAPPING_NOTIFY:
			/* not interested */
			eprintf("%s\n", gettype(ev));
			trace("ignore", 0, ev);
			break;
		}
		xcb_flush(dpy);
		free(ev);
	}
}

void configurereq(xcb_configure_request_event_t *e)
{
	xcb_params_configure_window_t wc;
	Client *c;

	eprintf("e=0x%x\n", e);

	/* we don't set curtime as nothing here uses it */
	c = getclient(e->window, 0);

	trace("configurereq", c, e);

	e->value_mask &= ~XCB_CONFIG_WINDOW_SIBLING;

	if (c)
	{
		gravitate(c, 1);
		if (e->value_mask & XCB_CONFIG_WINDOW_X)
			c->x = e->x;
		if (e->value_mask & XCB_CONFIG_WINDOW_Y)
			c->y = e->y;
		if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
			c->dx = e->width;
		if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
			c->dy = e->height;
		if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
			c->border = e->border_width;
		gravitate(c, 0);
		if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
		{
//			if (wc.stack_mode == XCB_STACK_MODE_ABOVE)
			if (e->stack_mode == XCB_STACK_MODE_ABOVE)
				top(c);
			else
				e->value_mask &= ~XCB_CONFIG_WINDOW_STACK_MODE;
		}
		if ((c->parent != c->screen->root) && (c->window == e->window))
		{
			wc.x = c->x - BORDER;
			wc.y = c->y - BORDER;
			wc.width = c->dx + 2 * (BORDER - 1);	
			wc.height = c->dy + 2 * (BORDER - 1);
			wc.border_width = 1;
			wc.sibling = XCB_NONE;
			wc.stack_mode = e->stack_mode;
			xcb_aux_configure_window(dpy, c->parent, e->value_mask, &wc);
			sendconfig(c);
		}
	}

	if (c && c->init)
	{
		wc.x = BORDER - 1;
		wc.y = BORDER - 1;
	}
	else
	{
		wc.x = e->x;
		wc.y = e->y;
	}
	wc.width = e->width;
	wc.height = e->height;
	wc.border_width = 0;
	wc.sibling = XCB_NONE;
	wc.stack_mode = XCB_STACK_MODE_ABOVE;
	e->value_mask &= ~XCB_CONFIG_WINDOW_STACK_MODE;
	e->value_mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;

	xcb_aux_configure_window(dpy, e->window, e->value_mask, &wc);
}

void mapreq(xcb_map_request_event_t *e)
{
	Client *c;
	int i;

	eprintf("e=0x%x\n", e);

	curtime = XCB_CURRENT_TIME;
	c = getclient(e->window, 0);
	trace("mapreq", c, e);

	if ((c == 0) || (c->window != e->window))
	{
		/* workaround for stupid NCDware */
		fprintf(stderr, "9wm: bad mapreq c %x w %x, rescanning\n",
			c, e->window);
		for (i = 0; i < num_screens; i++)
			scanwins(&screens[i]);
		c = getclient(e->window, 0);
		if ((c == 0) || (c->window != e->window))
		{
			fprintf(stderr, "9wm: window not found after rescan\n");
			return;
		}
	}

	switch (c->state)
	{
		case XCB_ICCCM_WM_STATE_WITHDRAWN:
			if (c->parent == c->screen->root)
			{
				if (!manage(c, 0))
					return;
				break;
			}
			xcb_reparent_window(dpy, c->window, c->parent,
						BORDER-1, BORDER-1);
			xcb_change_save_set(dpy, XCB_SET_MODE_INSERT, c->window);
		/* fall through... */
		case XCB_ICCCM_WM_STATE_NORMAL:
			xcb_map_window(dpy, c->window);
			xmapraised(dpy, c->parent);
			top(c);
			set_state(c, XCB_ICCCM_WM_STATE_NORMAL);
			if ((c->trans != XCB_NONE) &&
				(current) && (c->trans == current->window))
				active(c);
			break;
		case XCB_ICCCM_WM_STATE_ICONIC:
			unhidec(c, 1);
			break;
	}	
}

void unmap(xcb_unmap_notify_event_t *e)
{
	Client *c;

	eprintf("e=0x%x\n", e);

	curtime = XCB_CURRENT_TIME;
	c = getclient(e->window, 0);
	if (c)
	{
		switch (c->state)
		{
			case XCB_ICCCM_WM_STATE_ICONIC:
				if (e->response_type & 0x80)
				{
					unhidec(c, 0);
					withdraw(c);
				}
				break;
			case XCB_ICCCM_WM_STATE_NORMAL:
				if (c == current)
					nofocus();
				if (!c->reparenting)
					withdraw(c);
				break;
		}
		c->reparenting = 0;
	}
}

void circulatereq(xcb_circulate_request_event_t *e)
{
	eprintf("e=0x%x\n", e);
}

void newwindow(xcb_create_notify_event_t *e)
{
	Client *c;
	ScreenInfo *s;

	eprintf("e=0x%x\n", e);
	/* we don't set curtime as nothing here uses it */
	if (e->override_redirect)
		return;
	c = getclient(e->window, 1);
	if (c && c->window == e->window && (s = getscreen(e->parent)))
	{
		c->x = e->x;
		c->y = e->y;
		c->dx = e->width;
		c->dy = e->height;
		c->border = e->border_width;
		c->screen = s;
		if (c->parent == XCB_NONE)
			c->parent = c->screen->root;
	}
}

void destroy(xcb_destroy_notify_event_t *e)
{
	Client *c;

	eprintf("e=0x%x\n", e);
	curtime = XCB_CURRENT_TIME;
	c = getclient(e->window, 0);
	if (c == 0)
		return;

	rmclient(c);

	/* flush any errors generated by the window's sudden demise */
	ignore_badwindow = 1;
	xcb_aux_sync(dpy);
	flush_errors();
	ignore_badwindow = 0;
}

void clientmesg(xcb_client_message_event_t *e)
{
	Client *c;

	eprintf("e=0x%x\n", e);
	curtime = XCB_CURRENT_TIME;
	if (e->type == exit_9wm)
	{
		cleanup();
		exit(0);
	}
	if (e->type == restart_9wm)
	{
		fprintf(stderr, "*** 9wm restarting ***\n");
		cleanup();
		execvp(myargv[0], myargv);
		perror("9wm: exec failed");
		exit(1);
	}
	if (e->type == wm_change_state) {
		c = getclient(e->window, 0);
		if ((e->format == 32) && 
			(e->data.data32[0] == XCB_ICCCM_WM_STATE_ICONIC) &&
			 (c != 0))
		{
			if (normal(c))
				hide(c);
		}
		else
		{
			fprintf(stderr, "9wm: WM_CHANGE_STATE: format %d data %d w 0x%x\n",
				e->format, e->data.data32[0], e->window);
		}
		return;
	}
	fprintf(stderr, "9wm: strange ClientMessage, type 0x%x window 0x%x\n",
		e->response_type, e->window);
}

void cmap(xcb_colormap_notify_event_t *e)
{
	Client *c;
	int i;

	eprintf("e=0x%x\n", e);
	/* we don't set curtime as nothing here uses it */
	if (e->_new)
	{
		c = getclient(e->window, 0);
		if (c)
		{
			c->cmap = e->colormap;
			if (c == current)
				cmapfocus(c);
		}
		else
		{
			for (c = clients; c; c = c->next)
			{
				for (i = 0; i < c->ncmapwins; i++)
				{
					if (c->cmapwins[i] == e->window)
					{
						c->wmcmaps[i] = e->colormap;
						if (c == current)
							cmapfocus(c);
						return;
					}
				}
			}
		}
	}
}

void property(xcb_property_notify_event_t *e)
{
	xcb_atom_t a;
	int delete;
	Client *c;

	eprintf("e=0x%x\n", e);
	/* we don't set curtime as nothing here uses it */
	a = e->atom;
	delete = (e->state == XCB_PROPERTY_DELETE);
	c = getclient(e->window, 0);
	if (c == 0)
		return;

	switch (a)
	{
		case XCB_ATOM_WM_ICON_NAME:
			c->iconname = delete ? 0 : getprop(c->window, a);
			setlabel(c);
			renamec(c, c->label);
			return;
		case XCB_ATOM_WM_NAME:
			c->name = delete ? 0 : getprop(c->window, a);
			setlabel(c);
			renamec(c, c->label);
			return;
		case XCB_ATOM_WM_TRANSIENT_FOR:
			gettrans(c);
			return;
	}
	if (a == _9wm_hold_mode)
	{
		c->hold = getiprop(c->window, _9wm_hold_mode);
		if (c == current)
			draw_border(c, 1);
	}
	else if (a == wm_colormaps)
	{
		getcmaps(c);
		if (c == current)
			cmapfocus(c);
	}
}

void reparent(xcb_reparent_notify_event_t *e)
{
	Client *c;
	xcb_get_geometry_cookie_t cookie;
	xcb_get_geometry_reply_t *reply;
	ScreenInfo *s;

	eprintf("e=0x%x\n", e);
	/* we don't set curtime as nothing here uses it */
	if (!getscreen(e->event) || e->override_redirect)
		return;
	if ((s = getscreen(e->parent)) != 0)
	{
		c = getclient(e->window, 1);
		if ((c != 0) && (c->dx == 0 || c->dy == 0))
		{
			cookie = xcb_get_geometry_unchecked(dpy, c->window);
			reply = xcb_get_geometry_reply(dpy, cookie, NULL);
			if (reply)
			{
				c->x = reply->x;
				c->y = reply->y;
				c->dx = reply->width;
				c->dy = reply->height;
				c->border = reply->border_width;
				free(reply);
			}
			else
			{
				fprintf(stderr, "reparent: get geometry failed\n");
			}
			c->screen = s;
			if (c->parent == XCB_NONE)
				c->parent = c->screen->root;
		}
	}
	else
	{
		c = getclient(e->window, 0);
		if ((c != 0) && (c->parent == c->screen->root || withdrawn(c)))
			rmclient(c);
	}
}

#ifdef	SHAPE
void shapenotify(xcb_shape_notify_event_t *e)
{
	Client *c;

	eprintf("e=0x%x\n", e);
	/* we don't set curtime as nothing here uses it */
	c = getclient(e->affected_window, 0);
	if (c == 0)
		return;

	setshape(c);
}
#endif

void enter(xcb_enter_notify_event_t *e)
{
	Client *c;
	uint32_t values[2];

	eprintf("e=0x%x\n", e);
	curtime = e->time;
	if ((e->mode != XCB_NOTIFY_MODE_GRAB) ||
		(e->detail != XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL))
		return;
	c = getclient(e->event, 0);
	if ((c != 0) && (c != current))
	{
		/* someone grabbed the pointer; make them current */
		xmapraised(dpy, c->parent);
		top(c);
		active(c);
	}
}

void focusin(xcb_focus_in_event_t *e)
{
	Client *c;
	uint32_t values[2];

	eprintf("e=0x%x\n", e);
	curtime = XCB_CURRENT_TIME;
	if (e->detail != XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL)
		return;
	c = getclient(e->event, 0);
	if ((c != 0) && (c->window == e->event) && (c != current))
	{
		/* someone grabbed keyboard or seized focus; make them current */
		xmapraised(dpy, c->parent);
		top(c);
		active(c);
	}
}
