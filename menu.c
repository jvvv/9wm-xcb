/* Copyright (c) 1994-1996 David Hogan, see README for licence details */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include "dat.h"
#include "fns.h"

Client  *hiddenc[MAXHIDDEN];

int numhidden;

char    *b3items[B3FIXED+MAXHIDDEN+1] =
{
    "New",
    "Reshape",
    "Move",
    "Delete",
    "Hide",
    0,
};

Menu    b3menu =
{
    b3items,
};

Menu    egg =
{
    version,
};

void button(xcb_button_press_event_t *e)
{
    int n, shift;
    Client *c;
    xcb_window_t dw;
    ScreenInfo *s;
    uint32_t values[2];

    curtime = e->time;
    if (!(s = getscreen(e->root))) {
        fprintf(stderr, "button: getscreen failed\n");
        fprintf(stderr, " e->root=%d e->event=%d e->child=%d e->detail=%d\n",
                e->root, e->event, e->child, e->detail);
        return;
    }
    if (c = getclient(e->event, 0)) {
        e->event_x += c->x - BORDER + 1;
        e->event_y += c->y - BORDER + 1;
    }
    else if (e->event != e->root) {
        xcb_translate_coordinates_cookie_t xtrans_c;
        xcb_translate_coordinates_reply_t *xtrans_r;

        xtrans_c = xcb_translate_coordinates(dpy, e->event, s->root,
                                                            e->event_x, e->event_y);
        xtrans_r = xcb_translate_coordinates_reply(dpy, xtrans_c, NULL);
        if (xtrans_r) {
            e->event_x = xtrans_r->dst_x;
            e->event_y = xtrans_r->dst_y;
            free(xtrans_r);
        }
        else {
            fprintf(stderr, "9wm: button: failed to translate coords\n");
            fprintf(stderr, " window=%d root=%d child=%d\n",
                        e->event, e->root, e->child);
        }
    }

    switch (e->detail) {
        case XCB_BUTTON_INDEX_1:
            if (c) {
                xmapraised(dpy, c->parent);
                top(c);
                active(c);
            }
            return;
        case XCB_BUTTON_INDEX_2:
            if ((e->state & (XCB_MOD_MASK_SHIFT|XCB_MOD_MASK_CONTROL)) ==
                    (XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL))
                menuhit(e, &egg);
            return;
        case XCB_BUTTON_INDEX_3:
            break;
        default:
            return;
    }

    if (current && current->screen == s)
        cmapnofocus(s);

    switch (n = menuhit(e, &b3menu)) {
        case 0:     /* New */
            spawn(s);
            break;
        case 1:     /* Reshape */
            reshape(selectwin(1, 0, s));
            break;
        case 2:     /* Move */
            move(selectwin(0, 0, s));
            break;
        case 3:     /* Delete */
            shift = 0;
            c = selectwin(1, &shift, s);
            delete(c, shift);
            break;
        case 4:     /* Hide */
            hide(selectwin(1, 0, s));
            break;
        default:    /* unhide window */
            unhide(n - B3FIXED, 1);
            break;
        case -1:    /* nothing */
            break;
    }
    if (current && current->screen == s)
        cmapfocus(current);
}

void spawn(ScreenInfo *s)
{
    eprintf("s=0x%x\n", s);
    if (fork() == 0) {
        close(xcb_get_file_descriptor(dpy));
        if (s->display[0] != '\0')
            putenv(s->display);
        if (termprog != NULL) {
            execl(shell, shell, "-c", termprog, 0);
            fprintf(stderr, "9wm: exec %s", shell);
            perror(" failed");
        }
        execlp("9term", "9term", "-9wm", 0);
        execlp("xterm", "xterm", "-ut", 0);
        perror("9wm: exec 9term/xterm failed");
        exit(0);
    }
}

void reshape(Client *c)
{
    int odx, ody;
    uint32_t values[2];

    if (c == 0)
        return;
    odx = c->dx;
    ody = c->dy;
    if (sweep(c) == 0)
        return;
    active(c);
    top(c);
    xraisewindow(dpy, c->parent);
    xmoveresizewindow(dpy, c->parent, c->x-BORDER, c->y-BORDER,
                    c->dx+2*(BORDER-1), c->dy+2*(BORDER-1));
    if (c->dx == odx && c->dy == ody)
        sendconfig(c);
    else
        xmoveresizewindow(dpy, c->window, BORDER-1, BORDER-1, c->dx, c->dy);
}

void move(Client *c)
{
    uint32_t values[2];

    if (c == 0)
        return;
    if (drag(c) == 0)
        return;
    active(c);
    top(c);
    xraisewindow(dpy, c->parent);
    xmovewindow(dpy, c->parent, c->x - BORDER, c->y - BORDER);
    sendconfig(c);
}

void delete(Client *c, int shift)
{
    if (c == 0)
        return;
    if ((c->proto & Pdelete) && !shift)
        sendcmessage(c->window, wm_protocols, wm_delete, 0);
    else
        xcb_kill_client(dpy, c->window);        /* let event clean up */
}

void hide(Client *c)
{
    if (c == 0 || numhidden == MAXHIDDEN)
        return;
    if (hidden(c)) {
        fprintf(stderr, "9wm: already hidden: %s\n", c->label);
        return;
    }
    xcb_unmap_window(dpy, c->parent);
    xcb_unmap_window(dpy, c->window);
    set_state(c, XCB_ICCCM_WM_STATE_ICONIC);
    if (c == current)
        nofocus();
    hiddenc[numhidden] = c;
    b3items[B3FIXED+numhidden] = c->label;
    numhidden++;
    b3items[B3FIXED+numhidden] = 0;
}

void unhide(int n, int map)
{
    Client *c;
    int i;
    uint32_t values[2];

    if (n >= numhidden) {
        fprintf(stderr, "9wm: unhide: n %d numhidden %d\n", n, numhidden);
        return;
    }
    c = hiddenc[n];
    if (!hidden(c)) {
        fprintf(stderr, "9wm: unhide: not hidden: %s(0x%x)\n",
            c->label, c->window);
        return;
    }

    if (map) {
        xcb_map_window(dpy, c->window);
        xmapraised(dpy, c->parent);
        set_state(c, XCB_ICCCM_WM_STATE_NORMAL);
        active(c);
        top(c);
    }

    numhidden--;
    for (i = n; i < numhidden; i ++) {
        hiddenc[i] = hiddenc[i+1];
        b3items[B3FIXED+i] = b3items[B3FIXED+i+1];
    }
    b3items[B3FIXED+numhidden] = 0;
}

void unhidec(Client *c, int map)
{
    int i;

    for (i = 0; i < numhidden; i++)
        if (c == hiddenc[i]) {
            unhide(i, map);
            return;
        }
    fprintf(stderr, "9wm: unhidec: not hidden: %s(0x%x)\n",
        c->label, c->window);
}

void renamec(Client *c, char *name)
{
    int i;

    if (name == 0)
        name = "???";
    c->label = name;
    if (!hidden(c))
        return;
    for (i = 0; i < numhidden; i++)
        if (c == hiddenc[i]) {
            b3items[B3FIXED+i] = name;
            return;
        }
}
