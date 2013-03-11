/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_icccm.h>
#include "dat.h"
#include "fns.h"

static int nobuttons(xcb_button_press_event_t *e)	/* Einstuerzende */ 
{
	int state;

	eprintf("e=0x%x\n", e);
	state = (e->state & AllButtonMask);
	return ((e->response_type == XCB_BUTTON_RELEASE) &&
			((state & (state - 1)) == 0));
}

static int grab(xcb_window_t w, xcb_window_t constrain, int mask, xcb_cursor_t curs, int t)
{
	int status = -1;
	xcb_grab_pointer_cookie_t cookie;
	xcb_grab_pointer_reply_t *reply;
	xcb_generic_error_t *errp;

	eprintf("w=0x%x constrain=0x%x mask=%d curs=%d t=%d\n", w, constrain, mask, curs, t);

	if (!t)
		t = timestamp();

	gp_c = xcb_grab_pointer(dpy, 0, w, mask,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
		constrain, curs, t);
	gp_r = xcb_grab_pointer_reply(dpy, gp_c, &errorp);
	if (gp_r)
	{
		status = gp_r->status;
		free(gp_r);
	}
	else if(errorp)
	{
		status = errorp->error_code;
	}

	return status;
}

static void ungrab(xcb_button_press_event_t *e)
{
	xcb_generic_event_t *ev = NULL;
	xcb_button_press_event_t *bpe = e;
	xcb_generic_error_t *err;
	
	eprintf("e=0x%x\n", e);
	if (!nobuttons(bpe))
	{
		for (;;)
		{
			ev = maskevent(dpy, (ButtonMask | XCB_EVENT_MASK_BUTTON_MOTION));
			if ((ev->response_type & ~0x80) == XCB_MOTION_NOTIFY)
				continue;
			bpe = (xcb_button_press_event_t *)ev;
			if (nobuttons(bpe))
				break;
		}
	}
	xcb_ungrab_pointer(dpy, bpe->time);
	curtime = bpe->time;
	free(ev);
}

static void getmouse(int *x, int *y, ScreenInfo *s)
{
	xcb_query_pointer_cookie_t qp_c;
	xcb_query_pointer_reply_t *qp_r;
	xcb_generic_error_t *errorp;

	eprintf("x=%d y=%d s=0x%x\n", x, y, s);
	qp_c = xcb_query_pointer(dpy, s->root);
	qp_r = xcb_query_pointer_reply(dpy, qp_c, &errorp);
	if (!qp_r || !qp_r->same_screen)
	{
		free(qp_r);
		return;
	}

	*x = qp_r->root_x;
	*y = qp_r->root_y;
	free(qp_r);
}

static void setmouse(int x, int y, ScreenInfo *s)
{
	eprintf("x=%d y=%d s=0x%x\n", x, y, s);
	xcb_warp_pointer(dpy, XCB_NONE, s->root, 0, 0, 0, 0, x, y);
}

int menuhit(xcb_button_press_event_t *e, Menu *m)
{
	xcb_generic_event_t *ev;
	int i, n, cur, old, wide, high, status, drawn, warp;
	int x, y, dx, dy, xmax, ymax;
	int tx, ty;
	ScreenInfo *s;
	xcb_screen_t *scr;
	xcb_rectangle_t rect;
	xcb_params_configure_window_t wc;
	xcb_params_cw_t attr;
	xcb_params_gc_t gv;

	eprintf("e=0x%x m=0x%x\n", e, m);
	if (font == 0)
		return -1;
	s = getscreen(e->root);

	/* ugly event mangling */
	if ((s == 0) || (e->event == s->menuwin))
		return -1;

	dx = 0;
	for (n = 0; m->item[n]; n++)
	{
		wide = textwidth(dpy, font, strlen(m->item[n]), m->item[n]) + 4;
		if (wide > dx)
			dx = wide;
	}
	wide = dx;
	cur = m->lasthit;
	if (cur >= n)
		cur = n - 1;

	high = font_info->font_ascent + font_info->font_descent + 1;
	dy = n * high;
	x = e->event_x - wide / 2;
	y = e->event_y - cur * high - high / 2;
	warp = 0;
	scr = xcb_aux_get_screen(dpy, s->num);
	xmax = scr->width_in_pixels;
	ymax = scr->height_in_pixels;
	if (x < 0)
	{
		e->event_x -= x;
		x = 0;
		warp++;
	}
	if (x+wide >= xmax)
	{
		e->event_x -= x + wide - xmax;
		x = xmax - wide;
		warp++;
	}
	if (y < 0)
	{
		e->event_y -= y;
		y = 0;
		warp++;
	}
	if ((y + dy) >= ymax)
	{
		e->event_y -= y + dy - ymax;
		y = ymax - dy;
		warp++;
	}
	if (warp)
		setmouse(e->event_x, e->event_y, s);

	xmoveresizewindow(dpy, s->menuwin, x, y, dx, dy);
	xselectinput(dpy, s->menuwin, MenuMask);
	xmapraised(dpy, s->menuwin);
	status = grab(s->menuwin, XCB_NONE, MenuGrabMask, XCB_NONE, e->time);
	if (status != XCB_GRAB_STATUS_SUCCESS)
	{
		graberror("menuhit", status);
		xcb_unmap_window(dpy, s->menuwin);
		return -1;
	}
	drawn = 0;
	for (;;)
	{
		ev = maskevent(dpy, MenuMask);
		switch (ev->response_type & ~0x80)
		{
		case XCB_BUTTON_PRESS:
			free(ev);
			break;
		case XCB_BUTTON_RELEASE:
			if (((xcb_button_release_event_t *)ev)->detail != e->detail)
				break;
			x = ((xcb_button_release_event_t *)ev)->event_x;
			y = ((xcb_button_release_event_t *)ev)->event_y;
			i = y/high;
			if ((cur >= 0) && (y >= cur * high - 3) &&
				(y < (cur + 1) * high + 3))
				i = cur;
			if ((x < 0) || (x > wide) || (y < -3))
				i = -1;
			else if ((i < 0) || (i >= n))
				i = -1;
			else
				m->lasthit = i;
			if (!nobuttons((xcb_button_press_event_t *)ev))
				i = -1;
			ungrab((xcb_button_press_event_t *)ev);
			xcb_unmap_window(dpy, s->menuwin);
			xcb_flush(dpy);
			free(ev);
			return i;
		case XCB_MOTION_NOTIFY:
			if (!drawn)
				break;
			x = ((xcb_motion_notify_event_t *)ev)->event_x;
			y = ((xcb_motion_notify_event_t *)ev)->event_y;
			old = cur;
			cur = y / high;
			if ((old >= 0) && (y >= old * high - 3) &&
				(y < (old + 1) * high + 3))
				cur = old;
			if ((x < 0) || (x > wide) || (y < -3))
				cur = -1;
			else if ((cur < 0) || (cur >= n))
				cur = -1;
			free(ev);
			if (cur == old)
				break;
			rect.x = 0;
			rect.width = wide;
			rect.height = high;
			if ((old >= 0) && (old < n))
			{
				rect.y = old * high;
				xcb_poly_fill_rectangle(dpy, s->menuwin, s->gc0, 1, &rect);
			}
			if ((cur >= 0) && (cur < n))
			{
				rect.y = cur * high;
				xcb_poly_fill_rectangle(dpy, s->menuwin, s->gc0, 1, &rect);
			}
			xcb_flush(dpy);
			break;
		case XCB_EXPOSE:
			xcb_clear_area(dpy,0,s->menuwin,0,0,0,0);
			for (i = 0; i < n; i++)
			{
				
				tx = (wide - textwidth(dpy, font, strlen(m->item[i]), m->item[i]))/2;
				ty = i * high + font_info->font_ascent + 1;
				xcb_image_text_8(dpy, strlen(m->item[i]), s->menuwin, s->gc1, tx, ty, m->item[i]);
			}
			if (cur >= 0 && cur < n)
			{
				rect.x = 0;
				rect.y = cur * high;
				rect.width = wide;
				rect.height = high;
				xcb_poly_fill_rectangle(dpy, s->menuwin, s->gc0, 1, &rect);
			}
			drawn = 1;
			xcb_flush(dpy);
			free(ev);
			break;
		default:
			fprintf(stderr, "9wm: menuhit: unknown ev.type %d\n",
						ev->response_type);
			free(ev);
			break;
		}
	}
}

Client * selectwin(int release, int *shift, ScreenInfo *s)
{
	xcb_button_press_event_t *ev;
	int status;
	xcb_window_t w;
	Client *c;

	eprintf("release=%d shift=0x%x s=0x%x\n", release, shift, s);
	status = grab(s->root, s->root, ButtonMask, s->target, 0);
	if (status != XCB_GRAB_STATUS_SUCCESS)
	{
		graberror("selectwin", status); /* */
		return 0;
	}
	w = XCB_NONE;
	for (;;)
	{
		ev = (xcb_button_press_event_t *)maskevent(dpy, ButtonMask);
		switch (ev->response_type & ~0x80)
		{
			case XCB_BUTTON_PRESS:
				if (ev->detail != XCB_BUTTON_INDEX_3)
				{
					ungrab(ev);
					free(ev);
					return 0;
				}
				w = ev->child;
				if (!release)
				{
					c = getclient(w, 0);
					if (c == 0)
						ungrab(ev);
					if (shift != 0)
						*shift = (ev->state & XCB_MOD_MASK_SHIFT) != 0;
					free(ev);
					return c;
				}
				break;
			case XCB_BUTTON_RELEASE:
				ungrab(ev);
				if ((ev->detail != XCB_BUTTON_INDEX_3) ||
					(ev->child != w))
				{
					free(ev);
					return 0;
				}
				if (shift != 0)
					*shift = (ev->state & XCB_MOD_MASK_SHIFT) != 0;
				free(ev);
				return getclient(w, 0);
		}
	}
}

static void sweepcalc(Client *c, int x, int y)
{
	int dx, dy, sx, sy;

	eprintf("c=0x%x x=%d y=%d\n", c, x, y);
	dx = x - c->x;
	dy = y - c->y;
	sx = sy = 1;
	if (dx < 0)
	{
		dx = -dx;
		sx = -1;
	}
	if (dy < 0)
	{
		dy = -dy;
		sy = -1;
	}

	dx -= 2 * BORDER;
	dy -= 2 * BORDER;

	if (!c->is9term)
	{
		if (dx < c->min_dx)
			dx = c->min_dx;
		if (dy < c->min_dy)
			dy = c->min_dy;
	}

	if (c->size.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)
	{
		dx = c->min_dx + (dx-c->min_dx)/c->size.width_inc*c->size.width_inc;
		dy = c->min_dy + (dy-c->min_dy)/c->size.height_inc*c->size.height_inc;
	}

	if (c->size.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)
	{
		if (dx > c->size.max_width)
			dx = c->size.max_width;
		if (dy > c->size.max_height)
			dy = c->size.max_height;
	}
	c->dx = sx*(dx + 2*BORDER);
	c->dy = sy*(dy + 2*BORDER);
}

static void dragcalc(Client *c, int x, int y)
{
	eprintf("c=0x%x x=%d y=%d\n", c, x, y);
	c->x = x;
	c->y = y;
}

static void drawbound(Client *c)
{
	int x, y, dx, dy;
	ScreenInfo *s;
	xcb_rectangle_t rects[2];

	eprintf("c=0x%x\n", c);
	s = c->screen;
	x = c->x;
	y = c->y;
	dx = c->dx;
	dy = c->dy;

	if (dx < 0)
	{
		x += dx;
		dx = -dx;
	}
	if (dy < 0)
	{
		y += dy;
		dy = -dy;
	}
	if ((dx <= 2) || (dy <= 2))
		return;

	rects[0].x = x;
	rects[0].y = y;
	rects[0].width = dx - 1;
	rects[0].height = dy - 1;
	rects[1].x = x + 1;
	rects[1].y = y + 1;
	rects[1].width = dx - 3;
	rects[1].height = dy - 3;

	xcb_poly_rectangle(dpy,s->root, s->gc0, 2, rects);
}

static void misleep(int msec)
{
	struct timeval t;

	eprintf("msec=%d\n", msec);
	t.tv_sec = msec / 1000;
	t.tv_usec = (msec % 1000) * 1000;
	select(0, 0, 0, 0, &t);
}

static int sweepdrag(Client *c, xcb_button_press_event_t *e0, void (*recalc)())
{
	xcb_generic_event_t *ev;
	xcb_button_press_event_t *e;
	int idle;
	int cx, cy, rx, ry;
	int ox, oy, odx, ody;

	eprintf("c=0x%x e0=0x%x recalc=0x%x\n", c, e0, recalc);
	ox = c->x;
	oy = c->y;
	odx = c->dx;
	ody = c->dy;
	c->x -= BORDER;
	c->y -= BORDER;
	c->dx += 2 * BORDER;
	c->dy += 2 * BORDER;

	if (e0)
	{
		c->x = cx = e0->event_x;
		c->y = cy = e0->event_y;
		recalc(c, e0->event_x, e0->event_y);
	}
	else
		getmouse(&cx, &cy, c->screen);

	xcb_grab_server(dpy);
	drawbound(c);
	idle = 0;

	for (;;)
	{
		if ((ev = checkmaskevent(dpy, ButtonMask)) == 0)
		{
			getmouse(&rx, &ry, c->screen);
			if ((rx != cx) || (ry != cy) || (++idle > 300))
			{
				drawbound(c);
				if ((rx == cx) && (ry == cy))
				{
					xcb_ungrab_server(dpy);
					xcb_flush(dpy);
					misleep(500);
					xcb_grab_server(dpy);
					idle = 0;
				}
				recalc(c, rx, ry);
				cx = rx;
				cy = ry;
				drawbound(c);
				xcb_flush(dpy);
			}
			misleep(50);
			continue;
		}
		e = (xcb_button_press_event_t *)ev;
		switch (e->response_type & ~0x80)
		{
			case XCB_BUTTON_PRESS:
			case XCB_BUTTON_RELEASE:
				drawbound(c);
				ungrab(e);
				xcb_ungrab_server(dpy);
				if ((e->detail != XCB_BUTTON_INDEX_3) &&
							(c->init))
					goto bad;
				recalc(c, e->event_x, e->event_y);
				if (c->dx < 0)
				{
					c->x += c->dx;
					c->dx = -c->dx;
				}
				if (c->dy < 0)
				{
					c->y += c->dy;
					c->dy = -c->dy;
				}	
				c->x += BORDER;
				c->y += BORDER;
				c->dx -= 2 * BORDER;
				c->dy -= 2 * BORDER;
				if ((c->dx < 4) || (c->dy < 4) ||
					(c->dx < c->min_dx) ||
					(c->dy < c->min_dy))
					goto bad;
				free(ev);
				return 1;
		}
	}
bad:
	free(e);
	c->x = ox;
	c->y = oy;
	c->dx = odx;
	c->dy = ody;
	return 0;
}

int sweep(Client *c)
{
	int status;
	xcb_button_press_event_t *e;
	ScreenInfo *s;

	eprintf("c=0x%x\n", c);
	s = c->screen;
	status = grab(s->root, s->root, ButtonMask, s->sweep0, 0);
	if (status != XCB_GRAB_STATUS_SUCCESS)
	{
		graberror("sweep", status);
		return 0;
	}

	e = (xcb_button_press_event_t *)maskevent(dpy, ButtonMask);
	if (e->detail != XCB_BUTTON_INDEX_3)
	{
		ungrab(e);
		free(e);
		return 0;
	}
	if (c->size.flags & (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE
				|XCB_ICCCM_SIZE_HINT_BASE_SIZE))
		 setmouse(e->event_x+c->min_dx, e->event_y+c->min_dy, s);
	xcb_change_active_pointer_grab(dpy, s->boxcurs, e->time, ButtonMask);

	return sweepdrag(c, e, sweepcalc);
}

int drag(Client *c)
{
	int status;
	ScreenInfo *s;

	eprintf("c=0x%x\n", c);
	s = c->screen;
	if (c->init)
		setmouse(c->x - BORDER, c->y - BORDER, s);
	else {
		getmouse(&c->x, &c->y, s);		   /* start at current mouse pos */
		c->x += BORDER;
		c->y += BORDER;
	}
	status = grab(s->root, s->root, ButtonMask, s->boxcurs, 0);
	if (status != XCB_GRAB_STATUS_SUCCESS)
	{
		graberror("drag", status); 
		return 0;
	}
	return sweepdrag(c, 0, dragcalc);
}
