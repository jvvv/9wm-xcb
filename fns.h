/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <xcb/xcb.h>
#include <xcb/shape.h>

#ifdef	DEBUG
#define	trace(s, c, e)	dotrace((s), (c), (e))
#define eprintf(...) fprintf(stderr, "%s:%d:%s(): ", __FILE__, __LINE__, __func__);\
	fprintf(stderr, __VA_ARGS__)
#else
#define	trace(s, c, e)
#define eprintf(...)
#endif


/* main.c */
extern void *xalloc(size_t sz);
extern void initscreen(ScreenInfo *s, int i, int background);
extern ScreenInfo *getscreen(xcb_window_t w);
extern xcb_timestamp_t timestamp(void);
extern void sendcmessage(xcb_window_t w, xcb_atom_t a, long x, int isroot);
extern void sendconfig(Client *c);
extern void sighandler(int signum);
extern void sigchldhandler(int signum);
extern void cleanup(void);

/* xutil.c */
xcb_atom_t xinternatom(xcb_connection_t *c, char *name, uint8_t only_if_exists);
xcb_query_font_reply_t *xloadqueryfont (xcb_connection_t *c, char *fname, xcb_font_t *ret);
void xselectinput(xcb_connection_t *c, xcb_window_t w, uint32_t mask);
void xmovewindow(xcb_connection_t *c, xcb_window_t w, int x, int y);
void xresizewindow(xcb_connection_t *c, xcb_window_t w, uint32_t width, uint32_t height);
void xmoveresizewindow(xcb_connection_t *c, xcb_window_t w, int32_t x, int32_t y, uint32_t width, uint32_t height);
void xraisewindow(xcb_connection_t *c, xcb_window_t w);
void xmapraised(xcb_connection_t *c, xcb_window_t w);
xcb_window_t xcreatesimplewindow(xcb_connection_t *c, xcb_window_t parent, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t border_width, uint32_t border, uint32_t background);
uint32_t textwidth(xcb_connection_t *c, xcb_font_t font, int len, char *str);

/* event.c */
extern void mainloop(int shape_event);
extern void configurereq(xcb_configure_request_event_t *e);
extern void mapreq(xcb_map_request_event_t *e);
extern void unmap(xcb_unmap_notify_event_t *e);
extern void circulatereq(xcb_circulate_request_event_t *e);
extern void newwindow(xcb_create_notify_event_t *e);
extern void destroy(xcb_destroy_notify_event_t *e);
extern void clientmesg(xcb_client_message_event_t *e);
extern void cmap(xcb_colormap_notify_event_t *e);
extern void property(xcb_property_notify_event_t *e);
extern void reparent(xcb_reparent_notify_event_t *e);
#ifdef SHAPE
extern void shapenotify(xcb_shape_notify_event_t *e);
#endif
extern void enter(xcb_enter_notify_event_t *e);
extern void focusin(xcb_focus_in_event_t *e);

/* evq.c */
extern void evq_init(void);
extern void evq_destroy(void);
extern xcb_generic_event_t *getevent(xcb_connection_t *c);
extern xcb_generic_event_t *maskevent(xcb_connection_t *c, uint32_t mask);
extern xcb_generic_event_t *checkmaskevent(xcb_connection_t *c, uint32_t mask);
extern int evq_qlen(void);

/* manage.c */
extern int manage(Client *c, int mapped);
extern void scanwins(ScreenInfo *s);
extern void gettrans(Client *c);
extern void withdraw(Client *c);
extern void gravitate(Client *c, int invert);
extern void cmapfocus(Client *c);
extern void cmapnofocus(ScreenInfo *s);
extern void getcmaps(Client *c);
extern void setlabel(Client *c);
#ifdef SHAPE
extern void setshape(Client *c);
#endif
extern char *getprop(xcb_window_t w, xcb_atom_t a);
extern xcb_window_t getwprop(xcb_window_t w, xcb_atom_t a);
extern int getiprop(xcb_window_t w, xcb_atom_t a);
extern void set_state(Client *c, int state);
extern int get_state(xcb_window_t w, int *state);
extern void getproto(Client *c);

/* menu.c */
extern void button(xcb_button_press_event_t *e);
extern void spawn(ScreenInfo *s);
extern void reshape(Client *c);
extern void move(Client *c);
extern void delete(Client *c, int shift);
extern void hide(Client *c);
extern void unhide(int n, int map);
extern void unhidec(Client *c, int map);
extern void renamec(Client *c, char *name);

/* client.c */
extern void setactive(Client *c, int on);
extern void draw_border(Client *c, int active);
extern void active(Client *c);
extern void nofocus();
extern void top(Client *c);
extern Client * getclient(xcb_window_t w, int create);
extern void rmclient(Client *c);
#ifdef DEBUG
extern void dump_revert(void);
extern void dump_clients(void);
#endif

/* grab.c */
extern int menuhit(xcb_button_press_event_t *e, Menu *m);
extern Client * selectwin(int release, int *shift, ScreenInfo *s);
extern int sweep(Client *c);
extern int drag(Client *c);

/* error.c */
extern void fatal(char *s);
extern void handler(xcb_generic_error_t *e);
extern void flush_errors(void);
extern void graberror(char *f, int error);
#ifdef DEBUG
extern void dotrace(char *s, Client *c, xcb_generic_event_t *e);
#endif

/* cursor.c */
extern void initcurs(ScreenInfo *);

/* showevent.c */
extern char *gettype(xcb_generic_event_t *ev);
extern void showevent(xcb_generic_event_t *ev);
