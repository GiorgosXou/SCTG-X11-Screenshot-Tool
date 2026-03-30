#include <X11/Xlib.h>
#include <stdio.h>
#include "notify.h"


static Window get_active_window(Display *d) {
    Atom prop = XInternAtom(d, "_NET_ACTIVE_WINDOW", True);
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;

    if (XGetWindowProperty(d, DefaultRootWindow(d), prop,
                           0, (~0L), False, AnyPropertyType,
                           &type, &format, &nitems, &bytes_after,
                           &data) != Success || !data) {
        return 0;
    }

    Window w = *(Window *)data;
    XFree(data);
    return w;
}


static void get_title(Display *display, Window window, unsigned char** title_buf) {
    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8 = XInternAtom(display, "UTF8_STRING", False);

    Atom type;
    int format;
    unsigned long nitems, bytes_after;

    XGetWindowProperty(display, window, net_wm_name, 0, (~0L), False,
        utf8, &type, &format, &nitems, &bytes_after, title_buf);
}


static void get_mouse(Display *display, int* y, int* x){
    Window root = DefaultRootWindow(display);
    Window ret_root, ret_child;
    int win_x, win_y;
    unsigned int mask;

    if (!XQueryPointer(display, root, &ret_root, &ret_child,
                       x, y, &win_x, &win_y, &mask))
        CHECK_NOTIFY(False, "Pointer query failed\n");
}


static void get_infos_from(Display* display, unsigned char** title_buf, int* y, int* x) {
    Window window = get_active_window(display);
    *title_buf = NULL;

    get_mouse(display, y, x);

    if (!window)
        return;

    get_title(display, window, title_buf);
}
