/* Copyright (c) 2013 John Vogel, see README for licence details */
/* evq.c - event queue routines */
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_icccm.h>
#include "dat.h"
#include "fns.h"

extern int signalled;

struct evqnode
{
    struct evqnode *next, *prev;
    xcb_generic_event_t *event;
};

static struct evqnode *evq_head;
static struct evqnode *evq_tail;
static int evq_len;

int evq_qlen(void)
{
    return evq_len;
}

void evq_init(void)
{
    evq_head = xalloc(sizeof(struct evqnode));
    evq_tail = xalloc(sizeof(struct evqnode));
    evq_head->event = NULL;
    evq_tail->event = NULL;
    evq_head->prev = evq_head;
    evq_head->next = evq_tail;
    evq_tail->prev = evq_head;
    evq_tail->next = evq_tail;
    evq_len = 0;
}

void evq_destroy(void)
{
    struct evqnode *n, *t;

    for (n = evq_head->next; n->next != n; ) {
        t = n;
        n = n->next;
        free(t->event);
        free(t);
        evq_len--;
        if (evq_len < 0) {
            fprintf(stderr, "evq_destroy: evq underflow\n");
            exit(EXIT_FAILURE);
        }
    }
    free(evq_head);
    free(evq_tail);
    evq_head = NULL;
    evq_tail = NULL;
    evq_len = 0;
}

static void evq_enq(xcb_generic_event_t *ev)
{
    struct evqnode *n, *p;

    n = xalloc(sizeof(struct evqnode));
    n->event = ev;
    p = evq_tail->prev;
    p->next = n;
    n->next = evq_tail;
    n->prev = p;
    evq_tail->prev = n;

    evq_len++;
}

xcb_generic_event_t *evq_deq(struct evqnode *node)
{
    struct evqnode *n, *p;
    xcb_generic_event_t *ev;

    if ((evq_len == 0) || (evq_head->next == evq_tail)) {
        fprintf(stderr, "evq_deq: empty queue dequeue\n");
        exit(EXIT_FAILURE);
    }

    if (node == NULL) {
        p = evq_head->next;
        ev = p->event;
        n = p->next;
        evq_head->next = n;
        n->prev = evq_head;
        free(p);
    }
    else {
        ev = node->event;
        n = node->next;
        p = node->prev;
        p->next = n;
        n->prev = p;
        free(node);
    }

    evq_len--;
    if (evq_len < 0) {
        fprintf(stderr, "evq_deq: evq underflow\n");
        exit(EXIT_FAILURE);
    }

    return ev;
}

xcb_generic_event_t *getevent(xcb_connection_t *c)
{
    xcb_generic_event_t *ev;

    if (evq_len != 0)
        return evq_deq(NULL);

    while (!signalled)
        if ((ev = xcb_wait_for_event(c)) != NULL)
            return ev;

    fprintf(stderr, "9wm: exiting on signal\n");
    cleanup();
    exit(EXIT_FAILURE);
}

/*

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

/* Retreived _event_to_mask array from xlib sources and converted to xcb libs. */

/* LASTEvent must be bigger than any event. Xlib uses this and since
    some day ge_event might need to use 35, use the same as Xlib. */
#define LASTEvent 36

/*
 * This array can be used given an event type to determine the mask bits
 * that could have generated it.
 */
long const _event_to_mask [LASTEvent] = {
    0,              /* no event 0 */
    0,              /* no event 1 */
    XCB_EVENT_MASK_KEY_PRESS,   /* KeyPress 2 */
    XCB_EVENT_MASK_KEY_RELEASE, /* KeyRelease  3*/
    XCB_EVENT_MASK_BUTTON_PRESS,    /* ButtonPress  4*/
    XCB_EVENT_MASK_BUTTON_RELEASE,  /* ButtonRelease  5*/
    XCB_EVENT_MASK_POINTER_MOTION|XCB_EVENT_MASK_POINTER_MOTION_HINT|
        XCB_EVENT_MASK_BUTTON_1_MOTION|XCB_EVENT_MASK_BUTTON_2_MOTION|
        XCB_EVENT_MASK_BUTTON_3_MOTION|XCB_EVENT_MASK_BUTTON_4_MOTION|
        XCB_EVENT_MASK_BUTTON_5_MOTION|XCB_EVENT_MASK_BUTTON_MOTION,    /* MotionNotify 6 */
    XCB_EVENT_MASK_ENTER_WINDOW,    /* EnterNotify 7 */
    XCB_EVENT_MASK_LEAVE_WINDOW,    /* LeaveNotify 8 */
    XCB_EVENT_MASK_FOCUS_CHANGE,    /* FocusIn 9 */
    XCB_EVENT_MASK_FOCUS_CHANGE,    /* FocusOut 10 */
    XCB_EVENT_MASK_KEYMAP_STATE,    /* KeymapNotify 11 */
    XCB_EVENT_MASK_EXPOSURE,    /* Expose 12 */
    XCB_EVENT_MASK_EXPOSURE,    /* GraphicsExpose 13 */
    XCB_EVENT_MASK_EXPOSURE,    /* NoExpose 14 */
    XCB_EVENT_MASK_VISIBILITY_CHANGE,   /* VisibilityNotify 15 */
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, /* CreateNotify 16 */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, /* DestroyNotify 17 */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, /* UnmapNotify 18 */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, /* MapNotify 19 */
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,   /* MapRequest 20 */
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_STRUCTURE_NOTIFY, /* ReparentNotify 21 */
    XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, /* ConfigureNotify 22 */
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,   /* ConfigureRequest 23 */
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_STRUCTURE_NOTIFY, /* GravityNotify 24 */
    XCB_EVENT_MASK_RESIZE_REDIRECT,     /* ResizeRequest 25 */
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_STRUCTURE_NOTIFY, /* CirculateNotify 26 */
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,   /* CirculateRequest 26 */
    XCB_EVENT_MASK_PROPERTY_CHANGE,     /* PropertyNotify 27 */
    0,                  /* SelectionClear 28 */
    0,                  /* SelectionRequest 29 */
    0,                  /* SelectionNotify 30 */
    XCB_EVENT_MASK_COLOR_MAP_CHANGE,    /* ColormapNotify 31 */
    0,                  /* ClientMessage 32 */
    0,                  /* MappingNotify 33 */
};

static int evq_check_mask(uint8_t rtype, uint32_t mask)
{
    uint8_t idx = rtype & ~0x80;

    if ((mask == 0) && (idx == 0)) /* caller is looking for error events */
        return 1;

    return ((idx >= LASTEvent) ? 0 : _event_to_mask[idx] & mask);
}

xcb_generic_event_t *_maskevent(xcb_connection_t *c, uint32_t mask, int poll)
{
    xcb_generic_event_t *ev;
    struct evqnode *n;

    if (evq_len != 0)
        for (n = evq_head->next; n != evq_tail; n = n->next)
            if (evq_check_mask(n->event->response_type, mask))
                return evq_deq(n);

    while (!signalled) {
        if ((ev = xcb_poll_for_event(c)) == NULL) {
            if (poll)
                return NULL;
            else
                continue;
        }
        if (evq_check_mask(ev->response_type, mask))
            return ev;
        evq_enq(ev);
    }

    fprintf(stderr, "9wm: exiting on signal\n");
    cleanup();
    exit(EXIT_FAILURE);
}

xcb_generic_event_t *maskevent(xcb_connection_t *c, uint32_t mask)
{
    return _maskevent(c, mask, 0);
}

xcb_generic_event_t *checkmaskevent(xcb_connection_t *c, uint32_t mask)
{
    return _maskevent(c, mask, 1);
}
