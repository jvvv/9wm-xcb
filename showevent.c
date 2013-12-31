/* Original code from ShowEvent shar archive posted to comp.sources.x
 * Ported to xcb by John Vogel <jvogel4@stny.rr.com>
 */

/* Original README from comp.sources.x/volume2/showevent/part01.gz
 * shar archive:

There are times during debugging when it would be real useful to be able to
print the fields of an event in a human readable form.  Too many times I found 
myself scrounging around in section 8 of the Xlib manual looking for the valid 
fields for the events I wanted to see, then adding printf's to display the 
numeric values of the fields, and then scanning through X.h trying to decode
the cryptic detail and state fields.  After playing with xev, I decided to
write a couple of standard functions that I could keep in a library and call
on whenever I needed a little debugging verbosity.  The first function,
GetType(), is useful for returning the string representation of the type of
an event.  The second function, ShowEvent(), is used to display all the fields
of an event in a readable format.  The functions are not complicated, in fact,
they are mind-numbingly boring - but that's just the point nobody wants to
spend the time writing functions like this, they just want to have them when
they need them.

A simple, sample program is included which does little else but to demonstrate
the use of these two functions.  These functions have saved me many an hour 
during debugging and I hope you find some benefit to these.  If you have any
comments, suggestions, improvements, or if you find any blithering errors you 
can get it touch with me at the following location:

            ken@richsun.UUCP
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

typedef uint8_t Boolean;
#define True 1
#define False 0

Boolean use_separate_lines = True;
static char *sep;
extern xcb_connection_t *dpy;

/******************************************************************************/
/**** Miscellaneous routines to convert values to their string equivalents ****/
/******************************************************************************/

/* Returns the string equivalent of a boolean parameter */
static char *TorF(int bool)
{
    switch (bool) {
        case True:
            return ("True");
        case False:
            return ("False");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a property notify state */
static char *PropertyState(int state)
{
    switch (state) {
        case XCB_PROPERTY_NEW_VALUE:
            return ("PropertyNewValue");
        case XCB_PROPERTY_DELETE:
            return ("PropertyDelete");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a visibility notify state */
static char *VisibilityState(int state)
{
    switch (state) {
        case XCB_VISIBILITY_UNOBSCURED:
            return ("VisibilityUnobscured");
        case XCB_VISIBILITY_PARTIALLY_OBSCURED:
            return ("VisibilityPartiallyObscured");
        case XCB_VISIBILITY_FULLY_OBSCURED:
            return ("VisibilityFullyObscured");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a timestamp */
static char *ServerTime(xcb_timestamp_t time)
{
    uint32_t msec;
    uint32_t sec;
    uint32_t min;
    uint32_t hr;
    uint32_t day;
    char buffer[32];

    msec = time % 1000;
    time /= 1000;
    sec = time % 60;
    time /= 60;
    min = time % 60;
    time /= 60;
    hr = time % 24;
    time /= 24;
    day = time;

    sprintf(buffer, "%d day%s %02d:%02d:%02d.%03d",
                day, day == 1 ? "" : "(s)", hr, min, sec, msec);
    return (buffer);
}

/* Simple structure to ease the interpretation of masks */
typedef struct _MaskType
{
    unsigned int value;
    char *string;
} MaskType;

/* Returns the string equivalent of a mask of buttons and/or modifier keys */
static char *ButtonAndOrModifierState(unsigned int state)
{
    char buffer[256];
    static MaskType masks[] = {
        { XCB_BUTTON_MASK_1, "Button1Mask" },
        { XCB_BUTTON_MASK_2, "Button2Mask" },
        { XCB_BUTTON_MASK_3, "Button3Mask" },
        { XCB_BUTTON_MASK_4, "Button4Mask" },
        { XCB_BUTTON_MASK_5, "Button5Mask" },
        { XCB_MOD_MASK_SHIFT, "ShiftMask" },
        { XCB_MOD_MASK_LOCK, "LockMask" },
        { XCB_MOD_MASK_CONTROL, "ControlMask" },
        { XCB_MOD_MASK_1, "Mod1Mask" },
        { XCB_MOD_MASK_2, "Mod2Mask" },
        { XCB_MOD_MASK_3, "Mod3Mask" },
        { XCB_MOD_MASK_4, "Mod4Mask" },
        { XCB_MOD_MASK_5, "Mod5Mask" },
    };
    int num_masks = sizeof(masks) / sizeof(MaskType);
    int i;
    Boolean first = True;

    buffer[0] = 0;

    for (i = 0; i < num_masks; i++) {
        if (state & masks[i].value)
            if (first) {
                first = False;
                strcpy(buffer, masks[i].string);
            }
            else {
                strcat(buffer, " | ");
                strcat(buffer, masks[i].string);
            }
    }

    return (buffer);
}

/* Returns the string equivalent of a mask of configure window values */
static char *ConfigureValueMask(unsigned int valuemask)
{
    char buffer[256];
    static MaskType masks[] = {
        { XCB_CONFIG_WINDOW_X, "CWX" },
        { XCB_CONFIG_WINDOW_Y, "CWY" },
        { XCB_CONFIG_WINDOW_WIDTH, "CWWidth" },
        { XCB_CONFIG_WINDOW_HEIGHT, "CWHeight" },
        { XCB_CONFIG_WINDOW_BORDER_WIDTH, "CWBorderWidth" },
        { XCB_CONFIG_WINDOW_SIBLING, "CWSibling" },
        { XCB_CONFIG_WINDOW_STACK_MODE, "CWStackMode" },
    };
    int num_masks = sizeof(masks) / sizeof(MaskType);
    int i;
    Boolean first = True;

    buffer[0] = 0;

    for (i = 0; i < num_masks; i++)
        if (valuemask & masks[i].value)
            if (first) {
                first = False;
                strcpy(buffer, masks[i].string);
            }
            else {
                strcat(buffer, " | ");
                strcat(buffer, masks[i].string);
            }

    return (buffer);
}

/* Returns the string equivalent of a motion hint */
static char *IsHint(char is_hint)
{
    switch (is_hint) {
        case XCB_MOTION_NORMAL:
            return ("NotifyNormal");
        case XCB_MOTION_HINT:
            return ("NotifyHint");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of an id or the value "None" */
static char *MaybeNone(int value)
{
    char buffer[16];

    if (value == XCB_NONE)
        return ("None");
    sprintf(buffer, "0x%x", value);
    return (buffer);
}

/* Returns the string equivalent of a colormap state */
static char *ColormapState(int state)
{
    switch (state) {
        case XCB_COLORMAP_STATE_INSTALLED:
            return ("ColormapInstalled");
        case XCB_COLORMAP_STATE_UNINSTALLED:
            return ("ColormapUninstalled");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a crossing detail */
static char *CrossingDetail(int detail)
{
    switch (detail) {
        case XCB_NOTIFY_DETAIL_ANCESTOR:
            return ("NotifyAncestor");
        case XCB_NOTIFY_DETAIL_INFERIOR:
            return ("NotifyInferior");
        case XCB_NOTIFY_DETAIL_VIRTUAL:
            return ("NotifyVirtual");
        case XCB_NOTIFY_DETAIL_NONLINEAR:
            return ("NotifyNonlinear");
        case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL:
            return ("NotifyNonlinearVirtual");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a focus change detail */
static char *FocusChangeDetail(int detail)
{
    switch (detail) {
        case XCB_NOTIFY_DETAIL_ANCESTOR:
            return ("NotifyAncestor");
        case XCB_NOTIFY_DETAIL_INFERIOR:
            return ("NotifyInferior");
        case XCB_NOTIFY_DETAIL_VIRTUAL:
            return ("NotifyVirtual");
        case XCB_NOTIFY_DETAIL_NONLINEAR:
            return ("NotifyNonlinear");
        case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL:
            return ("NotifyNonlinearVirtual");
        case XCB_NOTIFY_DETAIL_POINTER:
            return ("NotifyPointer");
        case XCB_NOTIFY_DETAIL_POINTER_ROOT:
            return ("NotifyPointerRoot");
        case XCB_NOTIFY_DETAIL_NONE:
            return ("NotifyDetailNone");
        default:
            return ("?");
        }
}

/* Returns the string equivalent of a configure detail */
static char *ConfigureDetail(int detail)
{
    switch (detail) {
        case XCB_STACK_MODE_ABOVE:
            return ("Above");
        case XCB_STACK_MODE_BELOW:
            return ("Below");
        case XCB_STACK_MODE_TOP_IF:
            return ("TopIf");
        case XCB_STACK_MODE_BOTTOM_IF:
            return ("BottomIf");
        case XCB_STACK_MODE_OPPOSITE:
            return ("Opposite");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a grab mode */
static char *GrabMode(int mode)
{
    switch (mode) {
        case XCB_NOTIFY_MODE_NORMAL:
            return ("NotifyNormal");
        case XCB_NOTIFY_MODE_GRAB:
            return ("NotifyGrab");
        case XCB_NOTIFY_MODE_UNGRAB:
            return ("NotifyUngrab");
        case XCB_NOTIFY_MODE_WHILE_GRABBED:
            return ("NotifyWhileGrabbed");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a mapping request */
static char *MappingRequest(int request)
{
    switch (request) {
        case XCB_MAPPING_MODIFIER:
            return ("MappingModifier");
        case XCB_MAPPING_KEYBOARD:
            return ("MappingKeyboard");
        case XCB_MAPPING_POINTER:
            return ("MappingPointer");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a stacking order place */
static char *Place(int place)
{
    switch (place) {
        case XCB_PLACE_ON_TOP:
            return ("PlaceOnTop");
        case XCB_PLACE_ON_BOTTOM:
            return ("PlaceOnBottom");
        default:
            return ("?");
    }
}

/* Returns the string equivalent of a major code */
static char *MajorCode(int code)
{
    char buffer[32];

    switch (code) {
        case XCB_COPY_AREA:
            return ("X_CopyArea");
        case XCB_COPY_PLANE:
            return ("X_CopyPlane");
        default:
            sprintf(buffer, "0x%x", code);
            return (buffer);
    }
}

/* Returns the string equivalent the keycode contained in the key event */
/*static char *Keycode(xcb_key_press_event_t *ev)
{
    char buffer[256];
    KeySym keysym_str;
    char *keysym_name;
    char string[256];

    XLookupString(ev, string, 64, &keysym_str, NULL);

    if (keysym_str == NoSymbol)
        keysym_name = "NoSymbol";
    else if (!(keysym_name = XKeysymToString(keysym_str)))
        keysym_name = "(no name)";

    sprintf(buffer, "%u (keysym 0x%x \"%s\")",
    ev->keycode, keysym_str, keysym_name);

    return (buffer);
}*/

/* Returns the string equivalent of an atom or "None"*/
static char *AtomName(xcb_connection_t *c, xcb_atom_t atom)
{
    char buffer[256];
    char *name;
    int name_len;
    xcb_get_atom_name_cookie_t gan_c;
    xcb_get_atom_name_reply_t *gan_r;

    if (atom == XCB_NONE)
        return ("None");

    gan_c = xcb_get_atom_name_unchecked(c, atom);
    gan_r = xcb_get_atom_name_reply(c, gan_c, NULL);
    if (!gan_r)
        return NULL;
    name = xcb_get_atom_name_name(gan_r);
    name_len = xcb_get_atom_name_name_length(gan_r);
    free(gan_r);
    if (name_len > sizeof(buffer))
        name_len = sizeof(buffer);
    memcpy(buffer, name, name_len - 1);
    buffer[name_len - 1] = 0;

    return buffer;
}

/******************************************************************************/
/**** Routines to print out readable values for the field of various events ***/
/******************************************************************************/

static void VerbMotion(xcb_motion_notify_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->event, sep);
    fprintf(stderr,"root=0x%x%s", ev->root, sep);
    fprintf(stderr,"subwindow=0x%x%s", ev->child, sep);
    fprintf(stderr,"time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr,"x=%d y=%d%s", ev->event_x, ev->event_y, sep);
    fprintf(stderr,"x_root=%d y_root=%d%s", ev->root_x, ev->root_y, sep);
    fprintf(stderr,"state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr,"is_hint=%s%s", IsHint(ev->detail), sep);
    fprintf(stderr,"same_screen=%s\n", TorF(ev->same_screen));
}

static void VerbButton(xcb_button_press_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->event, sep);
    fprintf(stderr,"root=0x%x%s", ev->root, sep);
    fprintf(stderr,"subwindow=0x%x%s", ev->child, sep);
    fprintf(stderr,"time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr,"x=%d y=%d%s", ev->event_x, ev->event_y, sep);
    fprintf(stderr,"x_root=%d y_root=%d%s", ev->root_x, ev->root_y, sep);
    fprintf(stderr,"state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr,"button=%s%s", ButtonAndOrModifierState(ev->detail), sep);
    fprintf(stderr,"same_screen=%s\n", TorF(ev->same_screen));
}

static void VerbColormap(xcb_colormap_notify_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"colormap=%s%s", MaybeNone(ev->colormap), sep);
    fprintf(stderr,"new=%s%s", TorF(ev->_new), sep);
    fprintf(stderr,"state=%s\n", ColormapState(ev->state));
}

static void VerbCrossing(xcb_enter_notify_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->event, sep);
    fprintf(stderr,"root=0x%x%s", ev->root, sep);
    fprintf(stderr,"subwindow=0x%x%s", ev->child, sep);
    fprintf(stderr,"time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr,"x=%d y=%d%s", ev->event_x, ev->event_y, sep);
    fprintf(stderr,"x_root=%d y_root=%d%s", ev->root_x, ev->root_y, sep);
    fprintf(stderr,"mode=%s%s", GrabMode(ev->mode), sep);
    fprintf(stderr,"detail=%s%s", CrossingDetail(ev->detail), sep);
    fprintf(stderr,"same_screen_focus=%s%s", TorF(ev->same_screen_focus), sep);
    fprintf(stderr,"state=%s\n", ButtonAndOrModifierState(ev->state));
}

static void VerbExpose(xcb_expose_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr,"width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr,"count=%d\n", ev->count);
}

static void VerbGraphicsExpose(xcb_graphics_exposure_event_t *ev)
{
    fprintf(stderr,"drawable=0x%x%s", ev->drawable, sep);
    fprintf(stderr,"x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr,"width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr,"major_opcode=%s%s", MajorCode(ev->major_opcode), sep);
    fprintf(stderr,"minor_opcode=%d\n", ev->minor_opcode);
}

static void VerbNoExpose(xcb_no_exposure_event_t *ev)
{
    fprintf(stderr,"drawable=0x%x%s", ev->drawable, sep);
    fprintf(stderr,"major_opcode=%s%s", MajorCode(ev->major_opcode), sep);
    fprintf(stderr,"minor_opcode=%d\n", ev->minor_opcode);
}

static void VerbFocus(xcb_focus_in_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->event, sep);
    fprintf(stderr,"mode=%s%s", GrabMode(ev->mode), sep);
    fprintf(stderr,"detail=%s\n", FocusChangeDetail(ev->detail));
}

static void VerbKeymap(xcb_keymap_notify_event_t *ev)
{
    int i;

    fprintf(stderr,"key_vector=");

    for (i = 0; i < 31; i++)
        fprintf(stderr,"%02x", ev->keys[i]);

    fprintf(stderr,"\n");
}

static void VerbKey(xcb_key_press_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->event, sep);
    fprintf(stderr,"root=0x%x%s", ev->root, sep);
    fprintf(stderr,"subwindow=0x%x%s", ev->child, sep);
    fprintf(stderr,"time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr,"x=%d y=%d%s", ev->event_x, ev->event_y, sep);
    fprintf(stderr,"x_root=%d y_root=%d%s", ev->root_x, ev->root_y, sep);
    fprintf(stderr,"state=%s%s", ButtonAndOrModifierState(ev->state), sep);
    fprintf(stderr,"keycode=%d%s", ev->detail, sep);
    fprintf(stderr,"same_screen=%s\n", TorF(ev->same_screen));
}

static void VerbProperty(xcb_property_notify_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"atom=%s%s", AtomName(dpy, ev->atom), sep);
    fprintf(stderr,"time=%s%s", ServerTime(ev->time), sep);
    fprintf(stderr,"state=%s\n", PropertyState(ev->state));
}

static void VerbResizeRequest(xcb_resize_request_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"width=%d height=%d\n", ev->width, ev->height);
}

static void VerbCirculate(xcb_circulate_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"place=%s\n", Place(ev->place));
}

static void VerbConfigure(xcb_configure_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr,"width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr,"border_width=%d%s", ev->border_width, sep);
    fprintf(stderr,"above=%s%s", MaybeNone(ev->above_sibling), sep);
    fprintf(stderr,"override_redirect=%s\n", TorF(ev->override_redirect));
}

static void VerbCreateWindow(xcb_create_notify_event_t *ev)
{
    fprintf(stderr,"parent=0x%x%s", ev->parent, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr,"width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr,"border_width=%d%s", ev->border_width, sep);
    fprintf(stderr,"override_redirect=%s\n", TorF(ev->override_redirect));
}

static void VerbDestroyWindow(xcb_destroy_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x\n", ev->window);
}

static void VerbGravity(xcb_gravity_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"x=%d y=%d\n", ev->x, ev->y);
}

static void VerbMap(xcb_map_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"override_redirect=%s\n", TorF(ev->override_redirect));
}

static void VerbReparent(xcb_reparent_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"parent=0x%x%s", ev->parent, sep);
    fprintf(stderr,"x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr,"override_redirect=%s\n", TorF(ev->override_redirect));
}

static void VerbUnmap(xcb_unmap_notify_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"from_configure=%s\n", TorF(ev->from_configure));
}

static void VerbCirculateRequest(xcb_circulate_request_event_t *ev)
{
    fprintf(stderr,"event=0x%x%s", ev->event, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"place=%s\n", Place(ev->place));
}

static void VerbConfigureRequest(xcb_configure_request_event_t *ev)
{
    fprintf(stderr,"parent=0x%x%s", ev->parent, sep);
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"x=%d y=%d%s", ev->x, ev->y, sep);
    fprintf(stderr,"width=%d height=%d%s", ev->width, ev->height, sep);
    fprintf(stderr,"border_width=%d%s", ev->border_width, sep);
    fprintf(stderr,"sibling=%s%s", MaybeNone(ev->sibling), sep);
    fprintf(stderr,"stack_mode=0x%x%s", ConfigureDetail(ev->stack_mode), sep);
    fprintf(stderr,"value_mask=%s\n", ConfigureValueMask(ev->value_mask));
}

static void VerbMapRequest(xcb_map_request_event_t *ev)
{
    fprintf(stderr,"parent=0x%x%s", ev->parent, sep);
    fprintf(stderr,"window=0x%x\n", ev->window);
}

static void VerbClient(xcb_client_message_event_t *ev)
{
    int i;

    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"type=%s%s", AtomName(dpy, ev->type), sep);
    fprintf(stderr,"format=%d\n", ev->format);
    fprintf(stderr,"data (shown as longs)=");

    for (i = 0; i < 5; i++)
        fprintf(stderr," 0x%08x", ev->data.data32[i]);

    fprintf(stderr,"\n");
}

static void VerbMapping(xcb_mapping_notify_event_t *ev)
{
    fprintf(stderr,"request=0x%x%s", MappingRequest(ev->request), sep);
    fprintf(stderr,"first_keycode=0x%x%s", ev->first_keycode, sep);
    fprintf(stderr,"count=0x%x\n", ev->count);
}

static void VerbSelectionClear(xcb_selection_clear_event_t *ev)
{
    fprintf(stderr,"owner=0x%x%s", ev->owner, sep);
    fprintf(stderr,"selection=%s%s", AtomName(dpy, ev->selection), sep);
    fprintf(stderr,"time=%s\n", ServerTime(ev->time));
}

static void VerbSelection(xcb_selection_notify_event_t *ev)
{
    fprintf(stderr,"requestor=0x%x%s", ev->requestor, sep);
    fprintf(stderr,"selection=%s%s", AtomName(dpy, ev->selection), sep);
    fprintf(stderr,"target=%s%s", AtomName(dpy, ev->target), sep);
    fprintf(stderr,"property=%s%s", AtomName(dpy, ev->property), sep);
    fprintf(stderr,"time=%s\n", ServerTime(ev->time));
}

static void VerbSelectionRequest(xcb_selection_request_event_t *ev)
{
    fprintf(stderr,"owner=0x%x%s", ev->owner, sep);
    fprintf(stderr,"requestor=0x%x%s", ev->requestor, sep);
    fprintf(stderr,"selection=%s%s", AtomName(dpy, ev->selection), sep);
    fprintf(stderr,"target=%s%s", AtomName(dpy, ev->target), sep);
    fprintf(stderr,"property=%s%s", AtomName(dpy, ev->property), sep);
    fprintf(stderr,"time=%s\n", ServerTime(ev->time));
}

static void VerbVisibility(xcb_visibility_notify_event_t *ev)
{
    fprintf(stderr,"window=0x%x%s", ev->window, sep);
    fprintf(stderr,"state=%s\n", VisibilityState(ev->state));
}

/******************************************************************************/
/************ Return the string representation for type of an event ***********/
/******************************************************************************/

char *gettype(xcb_generic_event_t *ev)
{
    switch (ev->response_type & ~0x80) {
        case XCB_KEY_PRESS:
            return ("KeyPress");
        case XCB_KEY_RELEASE:
            return ("KeyRelease");
        case XCB_BUTTON_PRESS:
            return ("ButtonPress");
        case XCB_BUTTON_RELEASE:
            return ("ButtonRelease");
        case XCB_MOTION_NOTIFY:
            return ("MotionNotify");
        case XCB_ENTER_NOTIFY:
            return ("EnterNotify");
        case XCB_LEAVE_NOTIFY:
            return ("LeaveNotify");
        case XCB_FOCUS_IN:
            return ("FocusIn");
        case XCB_FOCUS_OUT:
            return ("FocusOut");
        case XCB_KEYMAP_NOTIFY:
            return ("KeymapNotify");
        case XCB_EXPOSE:
            return ("Expose");
        case XCB_GRAPHICS_EXPOSURE:
            return ("GraphicsExpose");
        case XCB_NO_EXPOSURE:
            return ("NoExpose");
        case XCB_VISIBILITY_NOTIFY:
            return ("VisibilityNotify");
        case XCB_CREATE_NOTIFY:
            return ("CreateNotify");
        case XCB_DESTROY_NOTIFY:
            return ("DestroyNotify");
        case XCB_UNMAP_NOTIFY:
            return ("UnmapNotify");
        case XCB_MAP_NOTIFY:
            return ("MapNotify");
        case XCB_MAP_REQUEST:
            return ("MapRequest");
        case XCB_REPARENT_NOTIFY:
            return ("ReparentNotify");
        case XCB_CONFIGURE_NOTIFY:
            return ("ConfigureNotify");
        case XCB_CONFIGURE_REQUEST:
            return ("ConfigureRequest");
        case XCB_GRAVITY_NOTIFY:
            return ("GravityNotify");
        case XCB_RESIZE_REQUEST:
            return ("ResizeRequest");
        case XCB_CIRCULATE_NOTIFY:
            return ("CirculateNotify");
        case XCB_CIRCULATE_REQUEST:
            return ("CirculateRequest");
        case XCB_PROPERTY_NOTIFY:
            return ("PropertyNotify");
        case XCB_SELECTION_CLEAR:
            return ("SelectionClear");
        case XCB_SELECTION_REQUEST:
            return ("SelectionRequest");
        case XCB_SELECTION_NOTIFY:
            return ("SelectionNotify");
        case XCB_COLORMAP_NOTIFY:
            return ("ColormapNotify");
        case XCB_CLIENT_MESSAGE:
            return ("ClientMessage");
        case XCB_MAPPING_NOTIFY:
            return ("MappingNotify");
    }
}

/******************************************************************************/
/**************** Print the values of all fields for any event ****************/
/******************************************************************************/

void showevent(xcb_generic_event_t *ev)
{
    /* determine which field separator to use */
    sep = (use_separate_lines) ? "\n" : " ";

    fprintf(stderr,"response_type=%s%s", gettype(ev), sep);
    fprintf(stderr,"sequence=%d%s", ev->sequence, sep);
    fprintf(stderr,"full_sequence=%d%s", ev->full_sequence, sep);
    fprintf(stderr,"send_event=%s%s", TorF(ev->response_type & 0x80), sep);

    switch (ev->response_type & ~0x80) {
        case XCB_MOTION_NOTIFY:
            VerbMotion((xcb_motion_notify_event_t *)ev);
            break;

        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE:
            VerbButton((xcb_button_press_event_t *)ev);
            break;

        case XCB_COLORMAP_NOTIFY:
            VerbColormap((xcb_colormap_notify_event_t *)ev);
            break;

        case XCB_ENTER_NOTIFY:
        case XCB_LEAVE_NOTIFY:
            VerbCrossing((xcb_enter_notify_event_t *)ev);
            break;

        case XCB_EXPOSE:
            VerbExpose((xcb_expose_event_t *)ev);
            break;

        case XCB_GRAPHICS_EXPOSURE:
            VerbGraphicsExpose((xcb_graphics_exposure_event_t *)ev);
            break;

        case XCB_NO_EXPOSURE:
            VerbNoExpose((xcb_no_exposure_event_t *)ev);
            break;

        case XCB_FOCUS_IN:
        case XCB_FOCUS_OUT:
            VerbFocus((xcb_focus_in_event_t *)ev);
            break;

        case XCB_KEYMAP_NOTIFY:
            VerbKeymap((xcb_keymap_notify_event_t *)ev);
            break;

        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
            VerbKey((xcb_key_press_event_t *)ev);
            break;

        case XCB_PROPERTY_NOTIFY:
            VerbProperty((xcb_property_notify_event_t *)ev);
            break;

        case XCB_RESIZE_REQUEST:
            VerbResizeRequest((xcb_resize_request_event_t *)ev);
            break;

        case XCB_CIRCULATE_NOTIFY:
            VerbCirculate((xcb_circulate_notify_event_t *)ev);
            break;

        case XCB_CONFIGURE_NOTIFY:
            VerbConfigure((xcb_configure_notify_event_t *)ev);
            break;

        case XCB_CREATE_NOTIFY:
            VerbCreateWindow((xcb_create_notify_event_t *)ev);
            break;

        case XCB_DESTROY_NOTIFY:
            VerbDestroyWindow((xcb_destroy_notify_event_t *)ev);
            break;

        case XCB_GRAVITY_NOTIFY:
            VerbGravity((xcb_gravity_notify_event_t *)ev);
            break;

        case XCB_MAP_NOTIFY:
            VerbMap((xcb_map_notify_event_t *)ev);
            break;

        case XCB_REPARENT_NOTIFY:
            VerbReparent((xcb_reparent_notify_event_t *)ev);
            break;

        case XCB_UNMAP_NOTIFY:
            VerbUnmap((xcb_unmap_notify_event_t *)ev);
            break;

        case XCB_CIRCULATE_REQUEST:
            VerbCirculateRequest((xcb_circulate_request_event_t *)ev);
            break;

        case XCB_CONFIGURE_REQUEST:
            VerbConfigureRequest((xcb_configure_request_event_t *)ev);
            break;

        case XCB_MAP_REQUEST:
            VerbMapRequest((xcb_map_request_event_t *)ev);
            break;

        case XCB_CLIENT_MESSAGE:
            VerbClient((xcb_client_message_event_t *)ev);
            break;

        case XCB_MAPPING_NOTIFY:
            VerbMapping((xcb_mapping_notify_event_t *)ev);
            break;

        case XCB_SELECTION_CLEAR:
            VerbSelectionClear((xcb_selection_clear_event_t *)ev);
            break;

        case XCB_SELECTION_NOTIFY:
            VerbSelection((xcb_selection_notify_event_t *)ev);
            break;

        case XCB_SELECTION_REQUEST:
            VerbSelectionRequest((xcb_selection_request_event_t *)ev);
            break;

        case XCB_VISIBILITY_NOTIFY:
            VerbVisibility((xcb_visibility_notify_event_t *)ev);
            break;

    }
}
