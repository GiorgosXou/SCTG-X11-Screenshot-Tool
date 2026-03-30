#define UI_LINUX
#define UI_IMPLEMENTATION
#include <stdlib.h>
#include <stdio.h>
#include "winfos.h"
#include "notify.h"
#include "portgui.h"
#include "screenshot.h"


#define SCTG_VERSION "v0.0.1-beta"

#if !defined(SCREENSHOT_KEY)
    #define SCREENSHOT_KEY XK_Print
#endif

UIWindow *window;
UIPanel  *panel;

bool crop = false;
int X1=0, Y1=0, X2=0, Y2=0;
int preX=0, preY=0;

int y,x;
unsigned char* title;
bool is_storable=true;


unsigned char *tags=NULL;
void ShowTagDialog(void* cp) {
	const char *result = UIDialogShow(
	    window, 0,
	    "Add Extra Tags\n\n%l\n\n%t\n\n%l\n\n%f%B%C",
	    &tags, "Add", "Cancel"
	);

    // if result is "Cancel" free(tags)
	if (*result == 'C'){
	    free(tags);
	    tags = NULL;
	}
}


void PaintImage(UIElement *element, UIMessage message, UIPainter* painter){
    if (message != UI_MSG_PAINT_FOREGROUND) return;

    for (int y = 0; y < window->height; ++y) 
        for (int x = 0; x < window->width; ++x) // NOTE: window not image & not UIDrawPixel(...)
            painter->bits[y * painter->width + x] = XGetPixel(image, x, y);

	if (crop){
        X1 = MIN(element->window->cursorX, preX);
        X2 = MAX(element->window->cursorX, preX);
        Y1 = MIN(element->window->cursorY, preY);
        Y2 = MAX(element->window->cursorY, preY);
	}

	UIDrawBorder(painter, UI_RECT_4(X1, X2, Y1, Y2), 0xFF5F00, UI_RECT_1(2));
}

bool is_left_down;
void HandleMouse(UIElement *element, UIMessage message){
    if (message == UI_MSG_RIGHT_UP){
	    UIMenu *menu = UIMenuCreate(element, 0);
	    UIMenuAddItem(menu, 0, "[t] TAG"   , -1, ShowTagDialog, (void *) NULL);
	    // UIMenuAddItem(menu, 0, "[b] BOX"   , -1, NULL         , (void *) NULL);
	    // UIMenuAddItem(menu, 0, "[i] TEXT"  , -1, NULL         , (void *) NULL);
	    // UIMenuAddItem(menu, 0, "[a] ARROW" , -1, NULL         , (void *) NULL);
	    // UIMenuAddItem(menu, 0, "[s] SELECT", -1, NULL         , (void *) NULL);
        menu->pointX = window->cursorX;
        menu->pointY = window->cursorY;
	    UIMenuShow(menu);

    }else if(message == UI_MSG_LEFT_DOWN){
        is_left_down = True;
        preX = window->cursorX;
        preY = window->cursorY;

    }else if(message == UI_MSG_MOUSE_DRAG && is_left_down){
        crop = True;
        UIElementRefresh(&panel->e);

    }else if(message == UI_MSG_LEFT_UP){
        is_left_down = False;
        crop = False;
    }
}


int PanelMessage(UIElement *element, UIMessage message, int di, void *dp) {
    HandleMouse(element, message);
    PaintImage (element, message, (UIPainter *)dp);

    return 0;
}


void ui_abort_image(void *cp){
    preX = preY = 0;
	ui.quit = true;
}


void ui_quit(void *cp){
    is_storable = true;
    ui_abort_image((void*)NULL);
}


void register_shortcuts(){ // .ctrl = True, if needed
	UIWindowRegisterShortcut(window, (UIShortcut) {
	    .code = UI_KEYCODE_ESCAPE,  
	    .invoke = ui_abort_image, 
	    .cp = NULL
	});
	UIWindowRegisterShortcut(window, (UIShortcut) {
	    .code = UI_KEYCODE_ENTER,  
	    .invoke = ui_quit, 
	    .cp = NULL
	});
	UIWindowRegisterShortcut(window, (UIShortcut) {
	    .code = UI_KEYCODE_LETTER('T') ,  
	    .invoke = ShowTagDialog, 
	    .cp = NULL
	});
}


void open_editor_gui() {
	ui.quit = false;
    is_storable = false; // set it to false here cause the user may exit in all short of ways eg. [i3] $mod+Shift+q
	window = UIWindowCreate(0, UI_WINDOW_FULLSCREEN, 0, 0, 0);
	panel  = UIPanelCreate(&window->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
	panel ->e.messageUser = PanelMessage;
    register_shortcuts();
	UIMessageLoop();
	UIElementDestroy(&window->e);
	XUnmapWindow(ui.display, window->os->window);
	XFlush(ui.display);
}


void get_tags_with_title(){  // meh... but whatever... won't do C-string surgery :P
    int needed = snprintf(NULL, 0,
        "%s%s| %s%s(%d,%d)",
        (title ? (char*)title : ""),
        (title ? " " : ""),
        (tags  ? (char*)tags  : ""),
        (tags  ? " " : ""),
        x, y
    ) + 1;

    char *new_tags = (char*)malloc(needed);
    CHECK_NOTIFY(new_tags, "Failed to malloc new_tags.");

    snprintf((char*)new_tags, needed,
        "%s%s| %s%s(%d,%d)",
        (title ? (char*)title : ""),
        (title ? " " : ""),
        (tags  ? (char*)tags  : ""),
        (tags  ? " " : ""),
        x, y
    );

    free(tags);
    tags = (unsigned char*)new_tags;
}


int main(int argc, char **argv) {
    if (argc < 2 || argc > 2){
        printf("SCTG - screenshot utility %s\n", SCTG_VERSION);
        printf("  use: sctg <path>\n");
        return EXIT_FAILURE;
    }

    // initialize luigi/portgui for later use 
	UIInitialise();
	ui.theme = uiThemeDark;

    // Get default root X11 window
    Window root = DefaultRootWindow(ui.display);
    // Get keycode for PrintScreen
    KeyCode keycode = XKeysymToKeycode(ui.display, SCREENSHOT_KEY);
    // Grab the key globally + AnyModifier
    XGrabKey(ui.display, keycode, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    // Set key to fire on press + AnyModifier
    XSelectInput(ui.display, root, KeyPressMask);

    // Monitor key events
    while (1) {
        XEvent ev;
        XNextEvent(ui.display, &ev);

        if ((ev.type != KeyPress) ||
            (ev.xkey.keycode != keycode))
            continue;

        capture_image_from(ui.display);
        get_infos_from(ui.display, &title, &y, &x);

        if (ev.xkey.state & ControlMask)
            open_editor_gui();

        get_tags_with_title();
        XFree(title); 

        if(is_storable){
            save(
                (ev.xkey.state & ShiftMask) ? 
                INTO_CLIPBOARD : argv[1], // path
                tags, image, X1, Y1, X2-X1, Y2-Y1
            );
            notify(
                "-=[ SCTG SCREENSHOT ]=-", 
                (char*)tags, 5
            );
        }

        // Reset/Clean stuff
        capture_image_clean(ui.display);
	    free(tags);
	    tags = NULL;
        is_storable = true; 
        X1 = X2 = Y1 = Y2 = 0;
    }

    // XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}
