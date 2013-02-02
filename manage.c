/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_icccm.h>
#include <xcb/shape.h>
#include "dat.h"
#include "fns.h"

int manage(Client *c, int mapped)
{
	int fixsize, dohide, doreshape, state;
	xcb_get_property_cookie_t prop_c;
	xcb_icccm_get_wm_class_reply_t cl_hint;
	xcb_icccm_wm_hints_t hints;
	xcb_size_hints_t nhints;
	xcb_params_configure_window_t wc;
	xcb_params_cw_t attr;
	uint32_t mask;
	uint32_t values[2];

	eprintf("c=0x%x mapped=%d (c->window=0x%x)\n", c, mapped, c->window);
	trace("manage", c, 0);
	xselectinput(dpy, c->window, (XCB_EVENT_MASK_COLOR_MAP_CHANGE |
					XCB_EVENT_MASK_ENTER_WINDOW |
					XCB_EVENT_MASK_PROPERTY_CHANGE |
					XCB_EVENT_MASK_FOCUS_CHANGE));

	/* Get loads of hints */

	prop_c = xcb_icccm_get_wm_class_unchecked(dpy, c->window);
	if (xcb_icccm_get_wm_class_reply(dpy, prop_c, &cl_hint, NULL))
	{
		c->instance = cl_hint.instance_name;
		c->class = cl_hint.class_name;
		c->is9term = (strcmp(c->class, "9term") == 0);
		xcb_icccm_get_wm_class_reply_wipe(&cl_hint);
	}
	else
	{
		c->instance = 0;
		c->class = 0;
		c->is9term = 0;
	}

	c->iconname = getprop(c->window, XCB_ATOM_WM_ICON_NAME);
	c->name = getprop(c->window, XCB_ATOM_WM_NAME);
	setlabel(c);

	prop_c = xcb_icccm_get_wm_hints_unchecked(dpy, c->window);
	xcb_icccm_get_wm_hints_reply(dpy, prop_c, &hints, NULL);
	
	prop_c = xcb_icccm_get_wm_normal_hints_unchecked(dpy, c->window);
	if (!xcb_icccm_get_wm_normal_hints_reply(dpy, prop_c, &c->size, NULL) || c->size.flags == 0)
		/* not specified - punt */
		c->size.flags = XCB_ICCCM_SIZE_HINT_P_SIZE;

	getcmaps(c);
	getproto(c);
	gettrans(c);
	if (c->is9term)
		c->hold = getiprop(c->window, _9wm_hold_mode);

	/* Figure out what to do with the window from hints */

	if (!get_state(c->window, &state))
		state = (hints.flags & XCB_ICCCM_WM_HINT_STATE) ?
			hints.initial_state : XCB_ICCCM_WM_STATE_NORMAL;
	dohide = (state == XCB_ICCCM_WM_STATE_ICONIC);

	fixsize = 0;
	if ((c->size.flags & (XCB_ICCCM_SIZE_HINT_US_SIZE | 
				XCB_ICCCM_SIZE_HINT_P_SIZE)))
		fixsize = 1;
	if (((c->size.flags & (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | 
			XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) ==
			((XCB_ICCCM_SIZE_HINT_P_MIN_SIZE |
			XCB_ICCCM_SIZE_HINT_P_MAX_SIZE))) &&
			(c->size.min_width == c->size.max_width) &&
			(c->size.min_height == c->size.max_height))
		fixsize = 1;
	doreshape = !mapped;
	if (fixsize)
	{
		if (c->size.flags & XCB_ICCCM_SIZE_HINT_US_POSITION)
			doreshape = 0;
		if (dohide && (c->size.flags & XCB_ICCCM_SIZE_HINT_P_POSITION))
			doreshape = 0;
		if (c->trans != XCB_NONE)
			doreshape = 0;
	}
	if (c->is9term)
		fixsize = 0;
	if (c->size.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE)
	{
		c->min_dx = c->size.base_width;
		c->min_dy = c->size.base_height;
	}
	else if (c->size.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)
	{
		c->min_dx = c->size.min_width;
		c->min_dy = c->size.min_height;
	}
	else if (c->is9term)
	{
		c->min_dx = 100;
		c->min_dy = 50;
	}
	else
		c->min_dx = c->min_dy = 0;

	/* Now do it!!! */

	if (doreshape) {
		if (current && current->screen == c->screen)
			cmapnofocus(c->screen);
		if (!(fixsize ? drag(c) : sweep(c)) && c->is9term)
		{
			fprintf(stderr, "manage: kill client!?!\n");
			xcb_kill_client(dpy, c->window);
			rmclient(c);
			if (current && current->screen == c->screen)
				cmapfocus(current);
			return 0;
		}
	}
	else
		gravitate(c, 0);

	c->parent = xcreatesimplewindow(dpy, c->screen->root,
			c->x - BORDER, c->y - BORDER,
			c->dx + 2*(BORDER-1), c->dy + 2*(BORDER-1),
			1, c->screen->black, c->screen->white);
	xselectinput(dpy, c->parent, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);

	if (mapped)
		c->reparenting = 1;
	if (doreshape && !fixsize)
		xresizewindow(dpy, c->window, c->dx, c->dy);
	mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	wc.border_width = 0;
	xcb_aux_configure_window(dpy, c->window, mask, &wc);

	xcb_reparent_window(dpy, c->window, c->parent, BORDER - 1, BORDER - 1);
#ifdef	SHAPE
	if (shape) {
		xcb_shape_select_input(dpy, c->window, 1L);
		ignore_badwindow = 1;		/* magic */
		setshape(c);
		flush_errors();
		ignore_badwindow = 0;
	}
#endif
	xcb_change_save_set(dpy, XCB_SET_MODE_INSERT, c->window);
	if (dohide)
		hide(c);
	else
	{
		xcb_map_window(dpy, c->window);
		xcb_map_window(dpy, c->parent);
		if (nostalgia || doreshape)
			active(c);
		else if ((c->trans != XCB_NONE) && (current) &&
				(current->window == c->trans))
			active(c);
		else
			setactive(c, 0);
		set_state(c, XCB_ICCCM_WM_STATE_NORMAL);
	}
	if (current && (current != c))
		cmapfocus(current);
	c->init = 1;
	return 1;
}

void scanwins(ScreenInfo *s)
{
	unsigned int i, nwins;
	Client *c;
	xcb_window_t *wins;
	xcb_query_tree_cookie_t qtr_c;
	xcb_query_tree_reply_t *qtr_r;
	xcb_get_window_attributes_cookie_t gatt_c;
	xcb_get_window_attributes_reply_t *gatt_r;
	xcb_get_geometry_cookie_t ggeo_c;
	xcb_get_geometry_reply_t *ggeo_r;

	eprintf("s=0x%x\n", s);
	qtr_c = xcb_query_tree_unchecked(dpy, s->root);
	qtr_r = xcb_query_tree_reply(dpy, qtr_c, NULL);
	if (qtr_r)
	{
		wins = xcb_query_tree_children(qtr_r);
		nwins = xcb_query_tree_children_length(qtr_r);
	}

	for (i = 0; i < nwins; i++)
	{
		if (wins[i] == s->menuwin)
			continue;
		gatt_c = xcb_get_window_attributes_unchecked(dpy, wins[i]);
		gatt_r = xcb_get_window_attributes_reply(dpy, gatt_c, NULL);
		if (gatt_r)
		{

			if (gatt_r->override_redirect)
				continue;
			c = getclient(wins[i], 1);
			ggeo_c = xcb_get_geometry_unchecked(dpy, wins[i]);
			ggeo_r = xcb_get_geometry_reply(dpy, ggeo_c, NULL);
			if (!ggeo_r)
			{
				fprintf(stderr, "9wm: get_geom failed\n");
				fprintf(stderr, " win = %d\n", wins[i]);
				free(gatt_r);
				continue;
			}
			if ((c != 0) && (c->window == wins[i]) && (!c->init))
			{
				c->x = ggeo_r->x;
				c->y = ggeo_r->y;
				c->dx = ggeo_r->width;
				c->dy = ggeo_r->height;
				c->border = ggeo_r->border_width;
				c->screen = s;
				c->parent = s->root;
				if (gatt_r->map_state == XCB_MAP_STATE_VIEWABLE)
					manage(c, 1);
			}
			free(ggeo_r);
			free(gatt_r);
		}
	}
	free(qtr_r);
}

void gettrans(Client *c)
{
	xcb_window_t trans;
	xcb_get_property_cookie_t prop_c;

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	prop_c = xcb_icccm_get_wm_transient_for_unchecked(dpy, c->window);
	if(xcb_icccm_get_wm_transient_for_reply(dpy, prop_c, &trans, NULL))
		c->trans = trans;
	else
		c->trans = XCB_NONE;
}

void withdraw(Client *c)
{
	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	xcb_unmap_window(dpy, c->parent);
	gravitate(c, 1);
	xcb_reparent_window(dpy, c->window, c->screen->root, c->x, c->y);
	gravitate(c, 0);
	xcb_change_save_set(dpy, XCB_SET_MODE_DELETE, c->window);
	set_state(c, XCB_ICCCM_WM_STATE_WITHDRAWN);

	/* flush any errors */
	ignore_badwindow = 1;
	flush_errors();
	ignore_badwindow = 0;
}

void gravitate(Client *c, int invert)
{
	int gravity, dx, dy, delta;

	eprintf("c=0x%x invert=%d (c->window=0x%x)\n", c, invert, c->window);
	gravity = XCB_GRAVITY_NORTH_WEST;
	if (c->size.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY)
		gravity = c->size.win_gravity;

	delta = c->border - BORDER;
	switch (gravity)
	{
		case XCB_GRAVITY_NORTH_WEST:
			dx = 0;
			dy = 0;
			break;
		case XCB_GRAVITY_NORTH:
			dx = delta;
			dy = 0;
			break;
		case XCB_GRAVITY_NORTH_EAST:
			dx = 2 * delta;
			dy = 0;
			break;
		case XCB_GRAVITY_WEST:
			dx = 0;
			dy = delta;
			break;
		case XCB_GRAVITY_CENTER:
		case XCB_GRAVITY_STATIC:
			dx = delta;
			dy = delta;
			break;
		case XCB_GRAVITY_EAST:
			dx = 2 * delta;
			dy = delta;
			break;
		case XCB_GRAVITY_SOUTH_WEST:
			dx = 0;
			dy = 2 * delta;
			break;
		case XCB_GRAVITY_SOUTH:
			dx = delta;
			dy = 2 * delta;
			break;
		case XCB_GRAVITY_SOUTH_EAST:
			dx = 2 * delta;
			dy = 2 * delta;
			break;
		default:
			fprintf(stderr, "9wm: bad window gravity %d for 0x%x\n", gravity, c->window);
			return;
	}
	dx += BORDER;
	dy += BORDER;
	if (invert) {
		dx = -dx;
		dy = -dy;
	}
	c->x += dx;
	c->y += dy;
}

static void installcmap(ScreenInfo *s, xcb_colormap_t cmap)
{
	eprintf("s=0x%x cmap=%d\n", s, cmap);
	if (cmap == XCB_NONE)
		xcb_install_colormap(dpy, s->def_cmap);
	else
		xcb_install_colormap(dpy, cmap);
}

void cmapfocus(Client *c)
{
	int i, found;
	Client *cc;

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	if (c == 0)
		return;
	else if (c->ncmapwins != 0) {
		found = 0;
		for (i = c->ncmapwins - 1; i >= 0; i--)
		{
			installcmap(c->screen, c->wmcmaps[i]);
			if (c->cmapwins[i] == c->window)
				found++;
		}
		if (!found)
			installcmap(c->screen, c->cmap);
	}
	else if ((c->trans != XCB_NONE) &&
			((cc = getclient(c->trans, 0)) != 0) &&
			(cc->ncmapwins != 0))
		cmapfocus(cc);
	else
		installcmap(c->screen, c->cmap);
}

void cmapnofocus(ScreenInfo *s)
{
	eprintf("s=0x%x\n", s);
	installcmap(s, XCB_NONE);
}

static int _getprop(xcb_window_t w, xcb_atom_t a, xcb_atom_t type, int32_t len, unsigned char **p)
{
	xcb_get_property_cookie_t prop_c;
	xcb_get_property_reply_t *prop_r;
	xcb_generic_error_t err, *errp;
	uint32_t length;

	*p = NULL;

	prop_c = xcb_get_property(dpy, 0, w, a, type, 0, len);
	prop_r = xcb_get_property_reply(dpy, prop_c, &errp);
	if (!prop_r)
	{
		handler(&err);
		return 1;
	}

	if (prop_r->type != XCB_NONE)
	{
		long nbytes, netbytes, len;
		void *pp;

		len = xcb_get_property_value_length(prop_r);
		pp = xcb_get_property_value(prop_r);

		switch (prop_r->format)
		{
			case 8:
				nbytes = netbytes = len;
				break;
			case 16:
				nbytes = len * sizeof(short);
				netbytes = len << 1;
				break;
			case 32:
				nbytes = len * sizeof(long);
				netbytes = len << 2;
				break;
			default:
				err.response_type = 0;
				err.error_code = XCB_IMPLEMENTATION;
				err.major_code = XCB_GET_PROPERTY;
				err.minor_code = 0;
				handler(&err);
				return 1;
				
		}

		if (nbytes + 1 > 0)
		{
			*p = xmalloc((unsigned)nbytes + 1);
			memcpy(*p, pp, netbytes);
			(*p)[nbytes] = '\0';
		}
	}

	if (prop_r->length == 0 && *p)
		xfree(*p);

	length = prop_r->length;
	xfree(prop_r);

	return length;
}

void getcmaps(Client *c)
{
	int n, i;
	xcb_window_t *cw;
	xcb_get_window_attributes_cookie_t gatt_c;
	xcb_get_window_attributes_reply_t *gatt_r;
	uint32_t value;

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	if (!c->init)
	{
		gatt_c = xcb_get_window_attributes_unchecked(dpy, c->window);
		gatt_r = xcb_get_window_attributes_reply(dpy, gatt_c, NULL);
		if (gatt_r)
		{
			c->cmap = gatt_r->colormap;
			free(gatt_r);
		}
	}
	n = _getprop(c->window, wm_colormaps, XCB_ATOM_WINDOW, 100L, (void **)&cw);
	if (c->ncmapwins != 0)
	{
		free(c->cmapwins);
		free(c->wmcmaps);
	}
	if (n <= 0)
	{
		c->ncmapwins = 0;
		return;
	}

	c->ncmapwins = n;
	c->cmapwins = cw;

	c->wmcmaps = (xcb_colormap_t *)malloc(n * sizeof(xcb_colormap_t));
	for (i = 0; i < n; i++)
	{
		if (cw[i] == c->window)
			c->wmcmaps[i] = c->cmap;
		else
		{
			value = XCB_EVENT_MASK_COLOR_MAP_CHANGE;
			xcb_change_window_attributes(dpy, cw[i],
					XCB_CW_EVENT_MASK, &value);
			gatt_c = xcb_get_window_attributes_unchecked(dpy, cw[i]);
			gatt_r =xcb_get_window_attributes_reply(dpy, gatt_c, NULL);
			if (gatt_r)
			{
				c->wmcmaps[i] = gatt_r->colormap;
				free(gatt_r);
			}
		}
	}
}

void setlabel(Client *c)
{
	char *label, *p;

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	if (c->iconname != 0)
		label = c->iconname;
	else if (c->name != 0)
		label = c->name;
	else if (c->instance != 0)
		label = c->instance;
	else if (c->class != 0)
		label = c->class;
	else
		label = "no label";
	if ((p = index(label, ':')) != 0)
		*p = '\0';
	c->label = label;
}

#ifdef	SHAPE
void setshape(Client *c)
{
	xcb_shape_get_rectangles_cookie_t shape_c;
	xcb_shape_get_rectangles_reply_t *shape_r;

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	shape_c = xcb_shape_get_rectangles_unchecked(dpy, c->window,
						XCB_SHAPE_SK_BOUNDING);
	shape_r = xcb_shape_get_rectangles_reply(dpy, shape_c, NULL);
	if (shape_r)
	{
	/* don't try to add a border if the window is non-rectangular */
		if (shape_r->rectangles_len > 1)
			xcb_shape_combine(dpy, XCB_SHAPE_SO_SET,
				XCB_SHAPE_SK_BOUNDING,
				XCB_SHAPE_SK_BOUNDING,
				c->parent, BORDER-1, BORDER-1, c->window);
		free(shape_r);
	}
}
#endif

char *getprop(xcb_window_t w, xcb_atom_t a)
{
	void *p;

	eprintf("w=0x%x a=%d\n", w, a);
	if (_getprop(w, a, XCB_ATOM_STRING, 100L, &p) <= 0)
		return 0;
	return (char *)p;
}

int get1prop(xcb_window_t w, xcb_atom_t a, xcb_atom_t type)
{
	void **p;
	int x;

	eprintf("w=0x%x a=%d type=%d\n", w, a, type);
	if (_getprop(w, a, type, 1L, p) <= 0)
		return 0;
	memcpy(&x, *p, sizeof(int));
	return x;
}

xcb_window_t getwprop(xcb_window_t w, xcb_atom_t a)
{
	eprintf("w=0x%x a=%d\n", w, a);
	return get1prop(w, a, XCB_ATOM_WINDOW);
}

int getiprop(xcb_window_t w, xcb_atom_t a)
{
	eprintf("w=0x%x a=%d\n", w, a);
	return get1prop(w, a, XCB_ATOM_INTEGER);
}

void set_state(Client *c, int state)
{
	uint32_t values[2];

	eprintf("c=0x%x state=%d (c->window=0x%x)\n", c, state, c->window);
	values[0] = state;
	values[1] = XCB_NONE;

	c->state = state;

	xcb_change_property(dpy, XCB_PROP_MODE_REPLACE, c->window, wm_state,
			wm_state, 32, 2, values);
}

int get_state(xcb_window_t w, int *state)
{
	void *p = 0;

	eprintf("w=0x%x state=0x%x\n", w, state);
	if (_getprop(w, wm_state, wm_state, 2L, &p) <= 0)
		return 0;

	memcpy(state, p, sizeof(int));
	return 1;
}

void getproto(Client *c)
{
	void *p;
	xcb_atom_t *t;
	int i;
	long n;
	xcb_window_t w;

	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);
	w = c->window;
	c->proto = 0;
	if ((n = _getprop(w, wm_protocols, XCB_ATOM_ATOM,
				20L, &p)) <= 0)
		return;

	for (i = 0, t = p; i < n; i++)
		if (t[i] == wm_delete)
			c->proto |= Pdelete;
		else if (t[i] == wm_take_focus)
			c->proto |= Ptakefocus;
}
