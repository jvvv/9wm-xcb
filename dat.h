/* Copyright (c) 1994-1996 David Hogan, see README for licence details */

#define true		1
#define false		0

#define BORDER		_border
#define	INSET		_inset
#define MAXHIDDEN	32
#define B3FIXED 	5

#define AllButtonMask	(XCB_BUTTON_MASK_1|XCB_BUTTON_MASK_2 \
			|XCB_BUTTON_MASK_3|XCB_BUTTON_MASK_4 \
			|XCB_BUTTON_MASK_5)
#define ButtonMask	(XCB_EVENT_MASK_BUTTON_PRESS \
			|XCB_EVENT_MASK_BUTTON_RELEASE)
/*#define AllPointersMotion	(XCB_EVENT_MASK_POINTER_MOTION|\
				XCB_EVENT_MASK_POINTER_MOTION_HINT|\
		     		XCB_EVENT_MASK_BUTTON_MOTION)
#define AllButtonsMotion	(XCB_EVENT_MASK_BUTTON_1_MOTION|\
		    		XCB_EVENT_MASK_BUTTON_2_MOTION|\
				XCB_EVENT_MASK_BUTTON_3_MOTION|\
				XCB_EVENT_MASK_BUTTON_4_MOTION|\
				XCB_EVENT_MASK_BUTTON_5_MOTION)
#define AllMotion	(AllPointersMotion|AllButtonsMotion)
#define MenuMask	(ButtonMask|XCB_EVENT_MASK_BUTTON_MOTION \
			|XCB_EVENT_MASK_EXPOSURE|AllMotion)*/
#define MenuMask	(ButtonMask|XCB_EVENT_MASK_BUTTON_MOTION \
			|XCB_EVENT_MASK_EXPOSURE)
#define MenuGrabMask	(ButtonMask|XCB_EVENT_MASK_BUTTON_MOTION \
			|XCB_EVENT_MASK_STRUCTURE_NOTIFY)

#ifdef	Plan9
#define DEFSHELL	"/bin/rc"
#else
#define DEFSHELL	"/bin/sh"
#endif

typedef struct Client Client;
typedef struct Menu Menu;
typedef struct ScreenInfo ScreenInfo;
typedef struct TextItem TextItem;

struct Client {
	xcb_window_t		window;
	xcb_window_t		parent;
	xcb_window_t		trans;
	Client		*next;
	Client		*revert;

	int 		x;
	int 		y;
	int 		dx;
	int 		dy;
	int 		border;

	xcb_size_hints_t	size;
	int 		min_dx;
	int 		min_dy;

	int 		state;
	int 		init;
	int 		reparenting;
	int 		is9term;
	int 		hold;
	int 		proto;

	char		*label;
	char		*instance;
	char		*class;
	char		*name;
	char		*iconname;

	xcb_colormap_t	cmap;
	int 		ncmapwins;
	xcb_window_t		*cmapwins;
	xcb_colormap_t	*wmcmaps;
	ScreenInfo	*screen;
};

#define hidden(c)	((c)->state == XCB_ICCCM_WM_STATE_ICONIC)
#define withdrawn(c)	((c)->state == XCB_ICCCM_WM_STATE_WITHDRAWN)
#define normal(c)	((c)->state == XCB_ICCCM_WM_STATE_NORMAL)

/* c->proto */
#define Pdelete 	1
#define Ptakefocus	2

struct Menu {
	char	**item;
	char	*(*gen)();
	int	lasthit;
};

struct ScreenInfo {
	int		num;
	xcb_window_t	root;
	xcb_window_t	menuwin;
	xcb_colormap_t	def_cmap;
	xcb_gcontext_t	gc0;
	xcb_gcontext_t	gc1;
	uint32_t	black;
	uint32_t	white;
	int		min_cmaps;
	xcb_cursor_t	target;
	xcb_cursor_t	sweep0;
	xcb_cursor_t	boxcurs;
	xcb_cursor_t	arrow;
	xcb_pixmap_t	root_pixmap;
	char		display[256];	/* arbitrary limit */
};

struct TextItem
{
	uint8_t nchars;
	int8_t delta;
	uint8_t text[];
};

/* main.c */
extern xcb_connection_t	*dpy;
extern int		dpy_fd;
extern ScreenInfo	*screens;
extern int		num_screens;
extern int		initting;
extern xcb_font_t	font;
extern xcb_query_font_reply_t *font_info;
extern int		nostalgia;
extern char		**myargv;
extern uint8_t 		shape;
extern char 		*termprog;
extern char 		*shell;
extern char 		*version[];
extern int		_border;
extern int		_inset;
extern int		curtime;
extern int		debug;
extern int		signalled;

extern xcb_atom_t	exit_9wm;
extern xcb_atom_t	restart_9wm;
extern xcb_atom_t 	wm_state;
extern xcb_atom_t	wm_change_state;
extern xcb_atom_t 	_9wm_hold_mode;
extern xcb_atom_t 	wm_protocols;
extern xcb_atom_t 	wm_delete;
extern xcb_atom_t 	wm_take_focus;
extern xcb_atom_t 	wm_colormaps;

/* xutils.c */
extern uint16_t		max_char_width;

/* client.c */
extern Client		*clients;
extern Client		*current;

/* menu.c */
extern Client		*hiddenc[];
extern int 		numhidden;
extern char 		*b3items[];
extern Menu 		b3menu;

/* error.c */
extern int 		ignore_badwindow;
