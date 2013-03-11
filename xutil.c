/* Copyright (c) 2013 John Vogel, see README for licence details */
/* xutil.c -- x utility functions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_icccm.h>
#include "dat.h"
#include "fns.h"

xcb_atom_t xinternatom(xcb_connection_t *c, char *name, uint8_t only_if_exists)
{
	xcb_atom_t atom;
	xcb_intern_atom_cookie_t cookie;
	xcb_intern_atom_reply_t *reply;
	xcb_generic_error_t *errorp;

	eprintf("name=%s only_if_exists=%d\n", name, only_if_exists);
	cookie = xcb_intern_atom(c, only_if_exists, strlen(name), name);
	reply = xcb_intern_atom_reply(c, cookie, &errorp);
	if (reply)
	{
		return reply->atom;
		free(reply);
	}
	else if (errorp)
		handler(errorp);

	return 0;
}

xcb_query_font_reply_t *xloadqueryfont (xcb_connection_t *c, char *fname, xcb_font_t *ret)
{
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *errorp;
	xcb_query_font_cookie_t qf_c;
	xcb_query_font_reply_t *qf_r;
	xcb_font_t font;

	eprintf("fname=%s ret=0x%x\n", fname, ret);
	font = xcb_generate_id(c);
	cookie = xcb_open_font_checked(c, font, strlen(fname), fname);
	errorp = xcb_request_check(c, cookie);
	if (errorp)
	{
		handler(errorp);
		return NULL;
	}

	qf_c = xcb_query_font(c, font);
	qf_r = xcb_query_font_reply(c, qf_c, &errorp);
	if (!qfreply)
	{
		xcb_close_font(c, font);
		if(errorp)
			handler(errorp);
	}
	else
		*ret = font;

	return qf_r;
}

void xselectinput(xcb_connection_t *c, xcb_window_t w, uint32_t mask)
{
	eprintf("w=0x%x mask=%d\n", w, mask);
	xcb_change_window_attributes(c, w, XCB_CW_EVENT_MASK, &mask);
}

void xmovewindow(xcb_connection_t *c, xcb_window_t w, int x, int y)
{
	xcb_params_configure_window_t wc;

	eprintf("w=0x%x x=%d y=%d\n", w, x, y);
	wc.x = x;
	wc.y = y;
	xcb_aux_configure_window(c, w, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &wc);
}

void xresizewindow(xcb_connection_t *c, xcb_window_t w, uint32_t width, uint32_t height)
{
	xcb_params_configure_window_t wc;

	eprintf("w=0x%x width=%d height=%d\n", w, width, height);
	wc.width = width;
	wc.height = height;
	xcb_aux_configure_window(c, w, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &wc);
}

void xmoveresizewindow(xcb_connection_t *c, xcb_window_t w,
	int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	xcb_params_configure_window_t wc;

	eprintf("w=0x%x x=%d y=%d width=%d height=%d\n", w, x, y, width, height);
	wc.x = x;
	wc.y = y;
	wc.width = width;
	wc.height = height;

	xcb_aux_configure_window(c, w,
				(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
				XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT), &wc);
}

void xmapraised(xcb_connection_t *c, xcb_window_t w)
{
	uint32_t value = XCB_STACK_MODE_ABOVE;
	eprintf("w=0x%x\n", w);
	xcb_configure_window(c, w, XCB_CONFIG_WINDOW_STACK_MODE, &value);
	xcb_map_window(c, w);
}

void xraisewindow(xcb_connection_t *c, xcb_window_t w)
{
	uint32_t value = XCB_STACK_MODE_ABOVE;
	eprintf("w=0x%x\n", w);
	xcb_configure_window(c, w, XCB_CONFIG_WINDOW_STACK_MODE, &value);
}

xcb_window_t xcreatesimplewindow(xcb_connection_t *c, xcb_window_t parent, uint32_t x, uint32_t y,
		uint32_t width, uint32_t height, uint32_t border_width, uint32_t border, uint32_t background)
{
	xcb_window_t wid = xcb_generate_id(c);
	uint32_t values[] = { background, border };

	xcb_create_window(c, 0, wid, parent, x, y, width, height, border_width,
		XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
		(XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL), values);

	return wid;
}

uint32_t textwidth(xcb_connection_t *c, xcb_font_t font, int len, char *str)
{
	xcb_query_text_extents_cookie_t qte_c;
	xcb_query_text_extents_reply_t *qte_r;
	xcb_query_font_cookie_t qf_c;
	xcb_query_font_reply_t *qf_r;
	xcb_generic_error_t *errorp;
	int i;
	uint32_t width;
	xcb_char2b_t *wstr;

	eprintf("font=0x%x len=%d str=%s\n", font, len, str);
	wstr = (xcb_char2b_t *)xalloc(len * sizeof(xcb_char2b_t));

	for (i = 0; i < len; i++)
	{
		wstr[i].byte1 = 0;
		wstr[i].byte2 = str[i];
	}

	qte_c = xcb_query_text_extents(c, font, len, wstr);
	qte_r = xcb_query_text_extents_reply(c, qte_c, &errorp);
	if (qte_r)
	{
		width = qte_r->overall_width;
		free(qte_r);
	}
	else
	{
		fprintf(stderr, "9wm: xtextwidth: failed to get text extents\n");
		if (errorp)
			handler(errorp);

		qf_c = xcb_query_font(c, font);
		qf_r = xcb_query_font_reply(c, qf_c, NULL);
		if (!qf_r)
		{
			fprintf(stderr, "9wm: xtextwidth: query font failed\n");
			if (errorp)
				handler(errorp);
			return 0;
		}

		width = len * qf_r->max_bounds.character_width;
		free(qf_r);
	}

	free(wstr);

	return width;
}
