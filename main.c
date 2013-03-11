/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_icccm.h>
#include <xcb/shape.h>
#include "dat.h"
#include "fns.h"
#include "patchlevel.h"

char	*version[] =
{
	"9wm version 1.2, Copyright (c) 1994-1996 David Hogan", 0,
};

xcb_connection_t	*dpy;
int			dpy_fd;
ScreenInfo		*screens;
int			initting;
xcb_font_t		font;
xcb_query_font_reply_t	*font_info;
int 			nostalgia;
char			**myargv;
char			*termprog;
char			*shell;
uint8_t			shape;
int 			_border = 4;
int 			_inset = 1;
int 			curtime;
int 			debug;
int 			signalled;
int 			num_screens;

xcb_atom_t		exit_9wm;
xcb_atom_t		restart_9wm;
xcb_atom_t		wm_state;
xcb_atom_t		wm_change_state;
xcb_atom_t		wm_protocols;
xcb_atom_t		wm_delete;
xcb_atom_t		wm_take_focus;
xcb_atom_t		wm_colormaps;
xcb_atom_t		_9wm_running;
xcb_atom_t		_9wm_hold_mode;

char *fontlist[] = {
	"lucm.latin1.9",
	"blit",
	"9x15bold",
	"lucidasanstypewriter-12",
	"fixed",
	"*",
	NULL,
};

void *xalloc(size_t sz)
{
	void *p = calloc(1, sz ? sz : 1);
	if (!p)
	{
		fprintf(stderr, "Failed to allocate %d bytes\n", sz);
		exit(EXIT_FAILURE);
	}
	return p;
}

void usage()
{
	fprintf(stderr, "usage: 9wm [-grey] [-version] [-font fname] [-term prog] [exit|restart]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int i, background, do_exit, do_restart;
	char *fname;
	int shape_event, dummy;
	int defscrno;
	xcb_screen_t *defscr;

	myargv = argv;			/* for restart */

	background = do_exit = do_restart = 0;
	font = 0;
	fname = 0;
	for (i = 1; i < argc; i++)
		if (strcmp(argv[i], "-nostalgia") == 0)
			nostalgia++;
		else if (strcmp(argv[i], "-grey") == 0)
			background = 1;
		else if (strcmp(argv[i], "-debug") == 0)
			debug++;
		else if (strcmp(argv[i], "-font") == 0 && i+1<argc)
		{
			i++;
			fname = argv[i];
		}
		else if (strcmp(argv[i], "-term") == 0 && i+1<argc)
			termprog = argv[++i];
		else if (strcmp(argv[i], "-version") == 0)
		{
			fprintf(stderr, "%s", version[0]);
			if (PATCHLEVEL > 0)
				fprintf(stderr, "; patch level %d", PATCHLEVEL);
			fprintf(stderr, "\n");
			exit(0);
		}
		else if (argv[i][0] == '-')
			usage();
		else
			break;
	for (; i < argc; i++)
		if (strcmp(argv[i], "exit") == 0)
			do_exit++;
		else if (strcmp(argv[i], "restart") == 0)
			do_restart++;
		else
			usage();

	if (do_exit && do_restart)
		usage();

	shell = (char *)getenv("SHELL");
	if (shell == NULL)
		shell = DEFSHELL;

	dpy = xcb_connect(NULL, &defscrno);
	if (xcb_connection_has_error(dpy))
		fatal("can't open display");

	initting = 1;
	evq_init();

	if (signal(SIGTERM, sighandler) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);
	if (signal(SIGINT, sighandler) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, sighandler) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);
	if (signal(SIGCHLD, sigchldhandler) == SIG_ERR)
	{
		fprintf(stderr, "9wm: failed to install SIGCHLD handler, quitting\n");
		xcb_disconnect(dpy);
		exit(1);
	}

	exit_9wm = xinternatom(dpy, "9WM_EXIT", 0);
	restart_9wm = xinternatom(dpy, "9WM_RESTART", 0);

	curtime = -1;		/* don't care */
	defscr = xcb_aux_get_screen(dpy, defscrno);
	if (do_exit)
	{
		sendcmessage(defscr->root, exit_9wm, 0L, 1);
		xcb_aux_sync(dpy);
		exit(0);
	}
	if (do_restart)
	{
		sendcmessage(defscr->root, restart_9wm, 0L, 1);
		xcb_aux_sync(dpy);
		exit(0);
	}

	wm_state = xinternatom(dpy, "WM_STATE", 0);
	wm_change_state = xinternatom(dpy, "WM_CHANGE_STATE", 0);
	wm_protocols = xinternatom(dpy, "WM_PROTOCOLS", 0);
	wm_delete = xinternatom(dpy, "WM_DELETE_WINDOW", 0);
	wm_take_focus = xinternatom(dpy, "WM_TAKE_FOCUS", 0);
	wm_colormaps = xinternatom(dpy, "WM_COLORMAP_WINDOWS", 0);
	_9wm_running = xinternatom(dpy, "_9WM_RUNNING", 0);
	_9wm_hold_mode = xinternatom(dpy, "_9WM_HOLD_MODE", 0);

	if (fname)
		if (!(font_info = xloadqueryfont(dpy, fname, &font)))
			fprintf(stderr, "9wm: warning: can't load font %s\n", fname);

	if (!font_info)
	{
		for (i =  0; ; i++)
		{
			if (!fontlist[i] || !*fontlist[i])
			{
				fprintf(stderr, "9wm: warning: can't find a font\n");
				break;
			}
			if (font_info = xloadqueryfont(dpy, fontlist[i], &font))
				break;
		}
	}

	if (nostalgia)
	{
		_border--;
		_inset--;
	}

#ifdef	SHAPE
	shape = false;
	const xcb_query_extension_reply_t *ext_reply;
	xcb_shape_query_version_cookie_t shape_ver_cookie;
	xcb_shape_query_version_reply_t *shape_ver_reply;

	xcb_prefetch_extension_data(dpy, &xcb_shape_id);
	ext_reply = xcb_get_extension_data(dpy, &xcb_shape_id);
	if (ext_reply)
	{
		shape_ver_cookie = xcb_shape_query_version_unchecked(dpy);
		shape_ver_reply = xcb_shape_query_version_reply(dpy, shape_ver_cookie, NULL);
		if (shape_ver_reply)
		{
			shape = true;
			shape_event = ext_reply->first_event;
			free(shape_ver_reply);
		}
	}
#endif

	num_screens = xcb_setup_roots_length(xcb_get_setup(dpy));
	screens = (ScreenInfo *)xalloc(sizeof(ScreenInfo) * num_screens);

	for (i = 0; i < num_screens; i++)
		initscreen(&screens[i], i, background);

	/* set selection so that 9term knows we're running */
	curtime = XCB_CURRENT_TIME;
	xcb_set_selection_owner(dpy, screens[0].menuwin, _9wm_running, timestamp());

	xcb_aux_sync(dpy);
	initting = 0;

	nofocus();

	for (i = 0; i < num_screens; i++)
		scanwins(&screens[i]);
	
	mainloop(shape_event);
}

void initscreen(ScreenInfo *s, int i, int background)
{
	char *ds, *colon, *dot1;
	uint32_t mask;
	xcb_params_gc_t gv;
	xcb_params_cw_t attr;
	xcb_screen_t *scr;

	eprintf("s=%d i=%d background=%d\n", s, i, background);
	
	scr = xcb_aux_get_screen(dpy, i);
	s->num = i;
	s->root = scr->root;
	s->def_cmap = scr->default_colormap;
	s->min_cmaps = scr->min_installed_maps;

	ds = getenv("DISPLAY");
	colon = rindex(ds, ':');
	if (colon && num_screens > 1)
	{
		strcpy(s->display, "DISPLAY=");
		strcat(s->display, ds);
		colon = s->display + 8 + (colon - ds);	/* use version in buf */
		dot1 = index(colon, '.');	/* first period after colon */
		if (!dot1)
			dot1 = colon + strlen(colon);	/* if not there, append */
		sprintf(dot1, ".%d", i);
	}
	else
		s->display[0] = '\0';

	s->black = scr->black_pixel;
	s->white = scr->white_pixel;

	gv.function = XCB_GX_XOR;
	gv.foreground = s->black ^ s->white;
	gv.background = s->white;
	gv.line_width = 0;
	gv.subwindow_mode = XCB_SUBWINDOW_MODE_INCLUDE_INFERIORS;
	mask = XCB_GC_FUNCTION | XCB_GC_FOREGROUND | XCB_GC_BACKGROUND |
                   XCB_GC_LINE_WIDTH | XCB_GC_SUBWINDOW_MODE;
	if (font_info && font)
	{
		gv.font = font;
		mask |= XCB_GC_FONT;
	}
	s->gc = xcb_generate_id(dpy);
	xcb_aux_create_gc(dpy, s->gc, s->root, mask, &gv);

	initcurs(s);

	attr.cursor = s->arrow;
	attr.event_mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
			| XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
			| XCB_EVENT_MASK_COLOR_MAP_CHANGE
			| XCB_EVENT_MASK_BUTTON_PRESS
			| XCB_EVENT_MASK_BUTTON_RELEASE
			| XCB_EVENT_MASK_PROPERTY_CHANGE;
	mask = XCB_CW_CURSOR |XCB_CW_EVENT_MASK;
	if (background)
	{
		attr.back_pixmap = s->root_pixmap;
		mask |= XCB_CW_BACK_PIXMAP;
	}
	xcb_aux_change_window_attributes(dpy, s->root, mask, &attr);
	xcb_clear_area(dpy, 0, s->root, 0, 0, 0, 0);
	xcb_aux_sync(dpy);

	s->menuwin = xcreatesimplewindow(dpy, s->root, 0, 0, 1, 1, 1, s->black, s->white);
}

ScreenInfo *getscreen(xcb_window_t w)
{
	int i;

	eprintf("w=0x%x\n", w);

	for (i = 0; i < num_screens; i++)
		if (screens[i].root == w)
			return &screens[i];

	return 0;
}

xcb_timestamp_t timestamp(void)
{
	xcb_generic_event_t *ev;

	if (curtime == XCB_CURRENT_TIME)
	{
		xcb_change_property(dpy, XCB_PROP_MODE_APPEND,
					screens[0].root, _9wm_running,
					_9wm_running, 8, 0, "");
		xcb_aux_sync(dpy);
		ev = maskevent(dpy, XCB_EVENT_MASK_PROPERTY_CHANGE);
		curtime = ((xcb_property_notify_event_t *)ev)->time;
	}
	eprintf("curtime=%d\n", curtime);
	return curtime;
}

void sendcmessage(xcb_window_t w, xcb_atom_t a, long x, int isroot)
{
	xcb_client_message_event_t ev;
	uint32_t mask;

	eprintf("w=0x%x a=0x%x x=0x%x isroot=%d\n", w, a, x, isroot);

	memset(&ev, 0, sizeof(xcb_client_message_event_t));
	ev.response_type = XCB_CLIENT_MESSAGE;
	ev.window = w;
	ev.type = a;
	ev.format = 32;
	ev.data.data32[0] = x;
	ev.data.data32[1] = timestamp();
	mask = 0L;
	if (isroot)
		mask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
	xcb_send_event(dpy, 0, w, mask, (const char *)&ev);
}

void sendconfig(Client *c)
{
	xcb_configure_notify_event_t ce;

	if (!c)
		return;
		
	eprintf("c=0x%x (c->window=0x%x)\n", c, c->window);

	ce.response_type = XCB_CONFIGURE_NOTIFY;
	ce.event = c->window;
	ce.window = c->window;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->dx;
	ce.height = c->dy;
	ce.border_width = c->border;
	ce.above_sibling = XCB_NONE;
	ce.override_redirect = 0;
	xcb_send_event(dpy, 0, c->window,
		XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char*)&ce);
}

void sighandler(int signum)
{
	eprintf("signum=%d\n", signum);
	signalled = 1;
}

void sigchldhandler(int signum)
{
	eprintf("signum=%d\n", signum);
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

void cleanup()
{
	Client *c, *cc[2], *next;
	uint32_t values[2];
	xcb_params_configure_window_t wc;
	int i;

	eprintf("\n");

	/* order of un-reparenting determines final stacking order... */
	cc[0] = cc[1] = 0;
	for (c = clients; c; c = next)
	{
		next = c->next;
		i = normal(c);
		c->next = cc[i];
		cc[i] = c;
	}

	for (i = 0; i < 2; i++)
	{
		for (c = cc[i]; c; c = c->next)
		{
			if (!withdrawn(c))
			{
				gravitate(c, 1);
				xcb_reparent_window(dpy, c->window,
						c->screen->root, c->x, c->y);
			}
			wc.border_width = c->border;
			xcb_aux_configure_window(dpy, c->window,
					XCB_CONFIG_WINDOW_BORDER_WIDTH, &wc);
		}
	}

	xcb_set_input_focus(dpy, XCB_INPUT_FOCUS_POINTER_ROOT,
				XCB_INPUT_FOCUS_POINTER_ROOT , XCB_CURRENT_TIME);
	for (i = 0; i < num_screens; i++)
		cmapnofocus(&screens[i]);
	if (font_info)
		free(font_info);
	evq_destroy();
	xcb_disconnect(dpy);
}
