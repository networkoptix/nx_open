
/* license {{{ */
/*

wmctrl
A command line tool to interact with an EWMH/NetWM compatible X Window Manager.

Author, current maintainer: Tomas Styblo <tripie@cpan.org>

Copyright (C) 2003

This program is free software which I release under the GNU General Public
License. You may redistribute and/or modify this program under the terms
of that license as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

To get a copy of the GNU General Puplic License,  write to the
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
/* }}} */

#include "wmctrl.h"

#ifdef Q_WS_X11

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include <glib.h>
#include <QtCore/QString>

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

#define uc reinterpret_cast<const unsigned char*>

/* help {{{ */
#define HELP "wmctrl " VERSION "\n" \
"Usage: wmctrl [OPTION]...\n" \
"Actions:\n" \
"  -m                   Show information about the window manager and\n" \
"                       about the environment.\n" \
"  -l                   List windows managed by the window manager.\n" \
"  -d                   List desktops. The current desktop is marked\n" \
"                       with an asterisk.\n" \
"  -s <DESK>            Switch to the specified desktop.\n" \
"  -a <WIN>             Activate the window by switching to its desktop and\n" \
"                       raising it.\n" \
"  -c <WIN>             Close the window gracefully.\n" \
"  -R <WIN>             Move the window to the current desktop and\n" \
"                       activate it.\n" \
"  -r <WIN> -t <DESK>   Move the window to the specified desktop.\n" \
"  -r <WIN> -e <MVARG>  Resize and move the window around the desktop.\n" \
"                       The format of the <MVARG> argument is described below.\n" \
"  -r <WIN> -b <STARG>  Change the state of the window. Using this option it's\n" \
"                       possible for example to make the window maximized,\n" \
"                       minimized or fullscreen. The format of the <STARG>\n" \
"                       argument and list of possible states is given below.\n" \
"  -r <WIN> -N <STR>    Set the name (long title) of the window.\n" \
"  -r <WIN> -I <STR>    Set the icon name (short title) of the window.\n" \
"  -r <WIN> -T <STR>    Set both the name and the icon name of the window.\n" \
"  -k (on|off)          Activate or deactivate window manager's\n" \
"                       \"showing the desktop\" mode. Many window managers\n" \
"                       do not implement this mode.\n" \
"  -o <X>,<Y>           Change the viewport for the current desktop.\n" \
"                       The X and Y values are separated with a comma.\n" \
"                       They define the top left corner of the viewport.\n" \
"                       The window manager may ignore the request.\n" \
"  -n <NUM>             Change number of desktops.\n" \
"                       The window manager may ignore the request.\n" \
"  -g <W>,<H>           Change geometry (common size) of all desktops.\n" \
"                       The window manager may ignore the request.\n" \
"  -h                   Print help.\n" \
"\n" \
"Options:\n" \
"  -i                   Interpret <WIN> as a numerical window ID.\n" \
"  -p                   Include PIDs in the window list. Very few\n" \
"                       X applications support this feature.\n" \
"  -G                   Include geometry in the window list.\n" \
"  -x                   Include WM_CLASS in the window list or\n" \
"                       interpret <WIN> as the WM_CLASS name.\n" \
"  -u                   Override auto-detection and force UTF-8 mode.\n" \
"  -F                   Modifies the behavior of the window title matching\n" \
"                       algorithm. It will match only the full window title\n" \
"                       instead of a substring, when this option is used.\n" \
"                       Furthermore it makes the matching case sensitive.\n" \
"  -v                   Be verbose. Useful for debugging.\n" \
"  -w <WA>              Use a workaround. The option may appear multiple\n" \
"                       times. List of available workarounds is given below.\n" \
"\n" \
"Arguments:\n" \
"  <WIN>                This argument specifies the window. By default it's\n" \
"                       interpreted as a string. The string is matched\n" \
"                       against the window titles and the first matching\n" \
"                       window is used. The matching isn't case sensitive\n" \
"                       and the string may appear in any position\n" \
"                       of the title.\n" \
"\n" \
"                       The -i option may be used to interpret the argument\n" \
"                       as a numerical window ID represented as a decimal\n" \
"                       number. If it starts with \"0x\", then\n" \
"                       it will be interpreted as a hexadecimal number.\n" \
"\n" \
"                       The -x option may be used to interpret the argument\n" \
"                       as a string, which is matched against the window's\n" \
"                       class name (WM_CLASS property). Th first matching\n" \
"                       window is used. The matching isn't case sensitive\n" \
"                       and the string may appear in any position\n" \
"                       of the class name. So it's recommended to  always use\n" \
"                       the -F option in conjunction with the -x option.\n" \
"\n" \
"                       The special string \":SELECT:\" (without the quotes)\n" \
"                       may be used to instruct wmctrl to let you select the\n" \
"                       window by clicking on it.\n" \
"\n" \
"                       The special string \":ACTIVE:\" (without the quotes)\n" \
"                       may be used to instruct wmctrl to use the currently\n" \
"                       active window for the action.\n" \
"\n" \
"  <DESK>               A desktop number. Desktops are counted from zero.\n" \
"\n" \
"  <MVARG>              Specifies a change to the position and size\n" \
"                       of the window. The format of the argument is:\n" \
"\n" \
"                       <G>,<X>,<Y>,<W>,<H>\n" \
"\n" \
"                       <G>: Gravity specified as a number. The numbers are\n" \
"                          defined in the EWMH specification. The value of\n" \
"                          zero is particularly useful, it means \"use the\n" \
"                          default gravity of the window\".\n" \
"                       <X>,<Y>: Coordinates of new position of the window.\n" \
"                       <W>,<H>: New width and height of the window.\n" \
"\n" \
"                       The value of -1 may appear in place of\n" \
"                       any of the <X>, <Y>, <W> and <H> properties\n" \
"                       to left the property unchanged.\n" \
"\n" \
"  <STARG>              Specifies a change to the state of the window\n" \
"                       by the means of _NET_WM_STATE request.\n" \
"                       This option allows two properties to be changed\n" \
"                       simultaneously, specifically to allow both\n" \
"                       horizontal and vertical maximization to be\n" \
"                       altered together.\n" \
"\n" \
"                       The format of the argument is:\n" \
"\n" \
"                       (remove|add|toggle),<PROP1>[,<PROP2>]\n" \
"\n" \
"                       The EWMH specification defines the\n" \
"                       following properties:\n" \
"\n" \
"                           modal, sticky, maximized_vert, maximized_horz,\n" \
"                           shaded, skip_taskbar, skip_pager, hidden,\n" \
"                           fullscreen, above, below\n" \
"\n" \
"Workarounds:\n" \
"\n" \
"  DESKTOP_TITLES_INVALID_UTF8      Print non-ASCII desktop titles correctly\n" \
"                                   when using Window Maker.\n" \
"\n" \
"The format of the window list:\n" \
"\n" \
"  <window ID> <desktop ID> <client machine> <window title>\n" \
"\n" \
"The format of the desktop list:\n" \
"\n" \
"  <desktop ID> [-*] <geometry> <viewport> <workarea> <title>\n" \
"\n" \
"\n" \
"Author, current maintainer: Tomas Styblo <tripie@cpan.org>\n" \
"Released under the GNU General Public License.\n" \
"Copyright (C) 2003\n"
/* }}} */

#define MAX_PROPERTY_VALUE_LEN 4096
#define ACTIVE_WINDOW_MAGIC ":ACTIVE:"

#define p_verbose(...) if (options.verbose) { \
    fprintf(stderr, __VA_ARGS__); \
}

/* declarations of static functions *//*{{{*/
static gboolean wm_supports (Display *disp, const gchar *prop);
static Window *get_client_list (Display *disp, unsigned long *size);
static int client_msg(Display *disp, Window win, char *msg,
        unsigned long data0, unsigned long data1,
        unsigned long data2, unsigned long data3,
        unsigned long data4);
static int list_windows (Display *disp);
static int showing_desktop (Display *disp);
static int change_viewport (Display *disp);
static int change_geometry (Display *disp);
static int change_number_of_desktops (Display *disp);
static int switch_desktop (Display *disp);
static int wm_info (Display *disp);
static gchar *get_output_str (gchar *str, gboolean is_utf8);
static int action_window (Display *disp, Window win, char mode);
static int action_window_pid (Display *disp, char mode);
static int action_window_str (Display *disp, char mode);
static int activate_window (Display *disp, Window win,
        gboolean switch_desktop);
static int close_window (Display *disp, Window win);
static int window_to_desktop (Display *disp, Window win, int desktop);
static void window_set_title (Display *disp, Window win, char *str, char mode);
static gchar *get_window_title (Display *disp, Window win);
static gchar *get_window_class (Display *disp, Window win);
static gchar *get_property (Display *disp, Window win,
        Atom xa_prop_type, gchar *prop_name, unsigned long *size);
static void init_charset(void);
static int window_move_resize (Display *disp, Window win, char *arg);
static int window_state (Display *disp, Window win, char *arg);
static Window get_active_window(Display *dpy);

/*}}}*/

static struct {
    int verbose;
    int force_utf8;
    int show_class;
    int show_pid;
    int show_geometry;
    int match_by_id;
    int match_by_cls;
    int full_window_title_match;
    int wa_desktop_titles_invalid_utf8;
    char *param_window;
    char *param;
} options;

static gboolean envir_utf8;

int execute_wmctrl_inner (int argc, char **argv) { /* {{{ */
    int opt;
    int action = 0;
    int ret = EXIT_SUCCESS;
    Display *disp;

    int old_optint = optind;
    optind = 0;

    memset(&options, 0, sizeof(options)); /* just for sure */

    /* necessary to make g_get_charset() and g_locale_*() work */
    setlocale(LC_ALL, "");

    while ((opt = getopt(argc, argv, "FGvlupimxa:r:s:c:t:w:k:o:n:g:e:b:N:I:T:R:")) != -1) {
        switch (opt) {
            case 'F':
                options.full_window_title_match = 1;
                break;
            case 'G':
                options.show_geometry = 1;
                break;
            case 'i':
                options.match_by_id = 1;
                break;
            case 'v':
                options.verbose = 1;
                break;
            case 'u':
                options.force_utf8 = 1;
                break;
            case 'x':
                options.match_by_cls = 1;
                options.show_class = 1;
                break;
            case 'p':
                options.show_pid = 1;
                break;
            case 'a': case 'c': case 'R':
                options.param_window = optarg;
                action = opt;
                break;
            case 'r':
                options.param_window = optarg;
                break;
            case 't': case 'e': case 'b': case 'N': case 'I': case 'T':
                options.param = optarg;
                action = opt;
                break;
            case 's':
                options.param = optarg;
                action = opt;
                break;
            case 'w':
                if (strcmp(optarg, "DESKTOP_TITLES_INVALID_UTF8") == 0) {
                    options.wa_desktop_titles_invalid_utf8 = 1;
                }
                else {
                    fprintf(stderr, "Unknown workaround: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'k': case 'o': case 'n': case 'g':
                options.param = optarg;
                action = opt;
                break;
            case '?':
                return EXIT_FAILURE;
            default:
                action = opt;
        }
    }
    init_charset();

    if (! (disp = XOpenDisplay(NULL))) {
        fputs("Cannot open display.\n", stderr);
        return EXIT_FAILURE;
    }

    switch (action) {
        case 'l':
            ret = list_windows(disp);
            break;
        case 's':
            ret = switch_desktop(disp);
            break;
        case 'm':
            ret = wm_info(disp);
            break;
        case 'a': case 'c': case 'R':
        case 't': case 'e': case 'b': case 'N': case 'I': case 'T':
            if (! options.param_window) {
                fputs("No window was specified.\n", stderr);
                return EXIT_FAILURE;
            }
            if (options.match_by_id) {
                ret = action_window_pid(disp, action);
            }
            else {
                ret = action_window_str(disp, action);
            }
            break;
        case 'k':
            ret = showing_desktop(disp);
            break;
        case 'o':
            ret = change_viewport(disp);
            break;
        case 'n':
            ret = change_number_of_desktops(disp);
            break;
        case 'g':
            ret = change_geometry(disp);
            break;
    }

    XCloseDisplay(disp);

    optind = old_optint;

    return ret;
}
/* }}} */

int execute_wmctrl(QString cmd){
    QStringList input = cmd.split(QLatin1Char(' '));

    char **output;

    // Copy input to output
    output = new char*[input.size() + 1];
    for (int i = 0; i < input.size(); i++) {
        output[i] = new char[strlen(input.at(i).toStdString().c_str())+1];
        memcpy(output[i], input.at(i).toStdString().c_str(), strlen(input.at(i).toStdString().c_str())+1);
    }
    output[input.size()] = ((char)NULL);
    int result = execute_wmctrl_inner(input.size(), output);
    // Delete output
    int i = 0;
    while (output[i]) {
        delete output[i];
        i++;
    }
    delete output;
    return result;
}


static void init_charset (void) {/*{{{*/
    const gchar *charset; /* unused */
    gchar *lang = getenv("LANG") ? g_ascii_strup(getenv("LANG"), -1) : NULL;
    gchar *lc_ctype = getenv("LC_CTYPE") ? g_ascii_strup(getenv("LC_CTYPE"), -1) : NULL;

    /* this glib function doesn't work on my system ... */
    envir_utf8 = g_get_charset(&charset);

    /* ... therefore we will examine the environment variables */
    if (lc_ctype && (strstr(lc_ctype, "UTF8") || strstr(lc_ctype, "UTF-8"))) {
        envir_utf8 = TRUE;
    }
    else if (lang && (strstr(lang, "UTF8") || strstr(lang, "UTF-8"))) {
        envir_utf8 = TRUE;
    }

    g_free(lang);
    g_free(lc_ctype);

    if (options.force_utf8) {
        envir_utf8 = TRUE;
    }
    p_verbose("envir_utf8: %d\n", envir_utf8);
}/*}}}*/

static int client_msg(Display *disp, Window win, char *msg, /* {{{ */
        unsigned long data0, unsigned long data1,
        unsigned long data2, unsigned long data3,
        unsigned long data4) {
    XEvent event;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(disp, msg, False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
        return EXIT_SUCCESS;
    }
    else {
        fprintf(stderr, "Cannot send %s event.\n", msg);
        return EXIT_FAILURE;
    }
}/*}}}*/

static gchar *get_output_str (gchar *str, gboolean is_utf8) {/*{{{*/
    gchar *out;

    if (str == NULL) {
        return NULL;
    }

    if (envir_utf8) {
        if (is_utf8) {
            out = g_strdup(str);
        }
        else {
            if (! (out = g_locale_to_utf8(str, -1, NULL, NULL, NULL))) {
                p_verbose("Cannot convert string from locale charset to UTF-8.\n");
                out = g_strdup(str);
            }
        }
    }
    else {
        if (is_utf8) {
            if (! (out = g_locale_from_utf8(str, -1, NULL, NULL, NULL))) {
                p_verbose("Cannot convert string from UTF-8 to locale charset.\n");
                out = g_strdup(str);
            }
        }
        else {
            out = g_strdup(str);
        }
    }

    return out;
}/*}}}*/

static int wm_info (Display *disp) {/*{{{*/
    Window *sup_window = NULL;
    gchar *wm_name = NULL;
    gchar *wm_class = NULL;
    unsigned long *wm_pid = NULL;
    unsigned long *showing_desktop = NULL;
    gboolean name_is_utf8 = TRUE;
    gchar *name_out;
    gchar *class_out;

    if (! (sup_window = (Window *)get_property(disp, DefaultRootWindow(disp),
                    XA_WINDOW, "_NET_SUPPORTING_WM_CHECK", NULL))) {
        if (! (sup_window = (Window *)get_property(disp, DefaultRootWindow(disp),
                        XA_CARDINAL, "_WIN_SUPPORTING_WM_CHECK", NULL))) {
            fputs("Cannot get window manager info properties.\n"
                  "(_NET_SUPPORTING_WM_CHECK or _WIN_SUPPORTING_WM_CHECK)\n", stderr);
            return EXIT_FAILURE;
        }
    }

    /* WM_NAME */
    if (! (wm_name = get_property(disp, *sup_window,
            XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL))) {
        name_is_utf8 = FALSE;
        if (! (wm_name = get_property(disp, *sup_window,
                XA_STRING, "_NET_WM_NAME", NULL))) {
            p_verbose("Cannot get name of the window manager (_NET_WM_NAME).\n");
        }
    }
    name_out = get_output_str(wm_name, name_is_utf8);

    /* WM_CLASS */
    if (! (wm_class = get_property(disp, *sup_window,
            XInternAtom(disp, "UTF8_STRING", False), "WM_CLASS", NULL))) {
        name_is_utf8 = FALSE;
        if (! (wm_class = get_property(disp, *sup_window,
                XA_STRING, "WM_CLASS", NULL))) {
            p_verbose("Cannot get class of the window manager (WM_CLASS).\n");
        }
    }
    class_out = get_output_str(wm_class, name_is_utf8);


    /* WM_PID */
    if (! (wm_pid = (unsigned long *)get_property(disp, *sup_window,
                    XA_CARDINAL, "_NET_WM_PID", NULL))) {
        p_verbose("Cannot get pid of the window manager (_NET_WM_PID).\n");
    }

    /* _NET_SHOWING_DESKTOP */
    if (! (showing_desktop = (unsigned long *)get_property(disp, DefaultRootWindow(disp),
                    XA_CARDINAL, "_NET_SHOWING_DESKTOP", NULL))) {
        p_verbose("Cannot get the _NET_SHOWING_DESKTOP property.\n");
    }

    /* print out the info */
    printf("Name: %s\n", name_out ? name_out : "N/A");
    printf("Class: %s\n", class_out ? class_out : "N/A");

    if (wm_pid) {
        printf("PID: %lu\n", *wm_pid);
    }
    else {
        printf("PID: N/A\n");
    }

    if (showing_desktop) {
        printf("Window manager's \"showing the desktop\" mode: %s\n",
                *showing_desktop == 1 ? "ON" : "OFF");
    }
    else {
        printf("Window manager's \"showing the desktop\" mode: N/A\n");
    }

    g_free(name_out);
    g_free(sup_window);
    g_free(wm_name);
    g_free(wm_class);
    g_free(wm_pid);
    g_free(showing_desktop);

    return EXIT_SUCCESS;
}/*}}}*/

static int showing_desktop (Display *disp) {/*{{{*/
    unsigned long state;

    if (strcmp(options.param, "on") == 0) {
        state = 1;
    }
    else if (strcmp(options.param, "off") == 0) {
        state = 0;
    }
    else {
        fputs("The argument to the -k option must be either \"on\" or \"off\"\n", stderr);
        return EXIT_FAILURE;
    }

    return client_msg(disp, DefaultRootWindow(disp), "_NET_SHOWING_DESKTOP",
        state, 0, 0, 0, 0);
}/*}}}*/

static int change_viewport (Display *disp) {/*{{{*/
    unsigned long x, y;
    const char *argerr = "The -o option expects two integers separated with a comma.\n";

    if (sscanf(options.param, "%lu,%lu", &x, &y) == 2) {
        return client_msg(disp, DefaultRootWindow(disp), "_NET_DESKTOP_VIEWPORT",
            x, y, 0, 0, 0);
    }
    else {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }
}/*}}}*/

static int change_geometry (Display *disp) {/*{{{*/
    unsigned long x, y;
    const char *argerr = "The -g option expects two integers separated with a comma.\n";

    if (sscanf(options.param, "%lu,%lu", &x, &y) == 2) {
        return client_msg(disp, DefaultRootWindow(disp), "_NET_DESKTOP_GEOMETRY",
            x, y, 0, 0, 0);
    }
    else {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }
}/*}}}*/

static int change_number_of_desktops (Display *disp) {/*{{{*/
    unsigned long n;

    if (sscanf(options.param, "%lu", &n) != 1) {
        fputs("The -n option expects an integer.\n", stderr);
        return EXIT_FAILURE;
    }

    return client_msg(disp, DefaultRootWindow(disp), "_NET_NUMBER_OF_DESKTOPS",
        n, 0, 0, 0, 0);
}/*}}}*/

static int switch_desktop (Display *disp) {/*{{{*/
    int target = -1;

    target = atoi(options.param);
    if (target == -1) {
        fputs("Invalid desktop ID.\n", stderr);
        return EXIT_FAILURE;
    }

    return client_msg(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP",
            (unsigned long)target, 0, 0, 0, 0);
}/*}}}*/

static void window_set_title (Display *disp, Window win, /* {{{ */
        QString title, char mode) {

    if (mode == 'T' || mode == 'N') {
        /* set name */
        if (title.size() > 0) {
            XChangeProperty(disp, win, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
                uc(title.toLatin1().constData()), title.size());
        }
        else {
            XDeleteProperty(disp, win, XA_WM_NAME);
        }
        XChangeProperty(disp, win, XInternAtom(disp, "_NET_WM_NAME", False),
                XInternAtom(disp, "UTF8_STRING", False), 8, PropModeReplace,
                uc(title.toUtf8().constData()), title.size());
    }

    if (mode == 'T' || mode == 'I') {
        /* set icon name */
        if (title.size() > 0) {
            XChangeProperty(disp, win, XA_WM_ICON_NAME, XA_STRING, 8, PropModeReplace,
                    uc(title.toLatin1().constData()), title.size());
        }
        else {
            XDeleteProperty(disp, win, XA_WM_ICON_NAME);
        }
        XChangeProperty(disp, win, XInternAtom(disp, "_NET_WM_ICON_NAME", False),
                XInternAtom(disp, "UTF8_STRING", False), 8, PropModeReplace,
                uc(title.toUtf8().constData()), title.size());
    }
}/*}}}*/

static int window_to_desktop (Display *disp, Window win, int desktop) {/*{{{*/
    unsigned long *cur_desktop = NULL;
    Window root = DefaultRootWindow(disp);

    if (desktop == -1) {
        if (! (cur_desktop = (unsigned long *)get_property(disp, root,
                XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
            if (! (cur_desktop = (unsigned long *)get_property(disp, root,
                    XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
                fputs("Cannot get current desktop properties. "
                      "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)"
                      "\n", stderr);
                return EXIT_FAILURE;
            }
        }
        desktop = *cur_desktop;
    }
    g_free(cur_desktop);

    return client_msg(disp, win, "_NET_WM_DESKTOP", (unsigned long)desktop,
            0, 0, 0, 0);
}/*}}}*/

static int activate_window (Display *disp, Window win, /* {{{ */
        gboolean switch_desktop) {
    unsigned long *desktop;

    /* desktop ID */
    if ((desktop = (unsigned long *)get_property(disp, win,
            XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
        if ((desktop = (unsigned long *)get_property(disp, win,
                XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
            p_verbose("Cannot find desktop ID of the window.\n");
        }
    }

    if (switch_desktop && desktop) {
        if (client_msg(disp, DefaultRootWindow(disp),
                    "_NET_CURRENT_DESKTOP",
                    *desktop, 0, 0, 0, 0) != EXIT_SUCCESS) {
            p_verbose("Cannot switch desktop.\n");
        }
        g_free(desktop);
    }

    client_msg(disp, win, "_NET_ACTIVE_WINDOW",
            0, 0, 0, 0, 0);
    XMapRaised(disp, win);

    return EXIT_SUCCESS;
}/*}}}*/

static int close_window (Display *disp, Window win) {/*{{{*/
    return client_msg(disp, win, "_NET_CLOSE_WINDOW",
            0, 0, 0, 0, 0);
}/*}}}*/

static int window_state (Display *disp, Window win, char *arg) {/*{{{*/
    unsigned long action;
    Atom prop1 = 0;
    Atom prop2 = 0;
    char *p1, *p2;
    const char *argerr = "The -b option expects a list of comma separated parameters: \"(remove|add|toggle),<PROP1>[,<PROP2>]\"\n";

    if (!arg || strlen(arg) == 0) {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }

    if ((p1 = strchr(arg, ','))) {
        gchar *tmp_prop1, *tmp1;

        *p1 = '\0';

        /* action */
        if (strcmp(arg, "remove") == 0) {
            action = _NET_WM_STATE_REMOVE;
        }
        else if (strcmp(arg, "add") == 0) {
            action = _NET_WM_STATE_ADD;
        }
        else if (strcmp(arg, "toggle") == 0) {
            action = _NET_WM_STATE_TOGGLE;
        }
        else {
            fputs("Invalid action. Use either remove, add or toggle.\n", stderr);
            return EXIT_FAILURE;
        }
        p1++;

        /* the second property */
        if ((p2 = strchr(p1, ','))) {
            gchar *tmp_prop2, *tmp2;
            *p2 = '\0';
            p2++;
            if (strlen(p2) == 0) {
                fputs("Invalid zero length property.\n", stderr);
                return EXIT_FAILURE;
            }
            tmp_prop2 = g_strdup_printf("_NET_WM_STATE_%s", tmp2 = g_ascii_strup(p2, -1));
            p_verbose("State 2: %s\n", tmp_prop2);
            prop2 = XInternAtom(disp, tmp_prop2, False);
            g_free(tmp2);
            g_free(tmp_prop2);
        }

        /* the first property */
        if (strlen(p1) == 0) {
            fputs("Invalid zero length property.\n", stderr);
            return EXIT_FAILURE;
        }
        tmp_prop1 = g_strdup_printf("_NET_WM_STATE_%s", tmp1 = g_ascii_strup(p1, -1));
        p_verbose("State 1: %s\n", tmp_prop1);
        prop1 = XInternAtom(disp, tmp_prop1, False);
        g_free(tmp1);
        g_free(tmp_prop1);


        return client_msg(disp, win, "_NET_WM_STATE",
            action, (unsigned long)prop1, (unsigned long)prop2, 0, 0);
    }
    else {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }
}/*}}}*/

static gboolean wm_supports (Display *disp, const gchar *prop) {/*{{{*/
    Atom xa_prop = XInternAtom(disp, prop, False);
    Atom *list;
    unsigned long size;
    int i;

    if (! (list = (Atom *)get_property(disp, DefaultRootWindow(disp),
            XA_ATOM, "_NET_SUPPORTED", &size))) {
        p_verbose("Cannot get _NET_SUPPORTED property.\n");
        return FALSE;
    }

    for (i = 0; i < size / sizeof(Atom); i++) {
        if (list[i] == xa_prop) {
            g_free(list);
            return TRUE;
        }
    }

    g_free(list);
    return FALSE;
}/*}}}*/

static int window_move_resize (Display *disp, Window win, char *arg) {/*{{{*/
    signed long grav, x, y, w, h;
    unsigned long grflags;
    const char *argerr = "The -e option expects a list of comma separated integers: \"gravity,X,Y,width,height\"\n";

    if (!arg || strlen(arg) == 0) {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }

    if (sscanf(arg, "%ld,%ld,%ld,%ld,%ld", &grav, &x, &y, &w, &h) != 5) {
        fputs(argerr, stderr);
        return EXIT_FAILURE;
    }

    if (grav < 0) {
        fputs("Value of gravity mustn't be negative. Use zero to use the default gravity of the window.\n", stderr);
        return EXIT_FAILURE;
    }

    grflags = grav;
    if (x != -1) grflags |= (1 << 8);
    if (y != -1) grflags |= (1 << 9);
    if (w != -1) grflags |= (1 << 10);
    if (h != -1) grflags |= (1 << 11);

    p_verbose("grflags: %lu\n", grflags);

    if (wm_supports(disp, "_NET_MOVERESIZE_WINDOW")){
        return client_msg(disp, win, "_NET_MOVERESIZE_WINDOW",
            grflags, (unsigned long)x, (unsigned long)y, (unsigned long)w, (unsigned long)h);
    }
    else {
        p_verbose("WM doesn't support _NET_MOVERESIZE_WINDOW. Gravity will be ignored.\n");
        if ((w < 1 || h < 1) && (x >= 0 && y >= 0)) {
            XMoveWindow(disp, win, x, y);
        }
        else if ((x < 0 || y < 0) && (w >= 1 && h >= -1)) {
            XResizeWindow(disp, win, w, h);
        }
        else if (x >= 0 && y >= 0 && w >= 1 && h >= 1) {
            XMoveResizeWindow(disp, win, x, y, w, h);
        }
        return EXIT_SUCCESS;
    }
}/*}}}*/

static int action_window (Display *disp, Window win, char mode) {/*{{{*/
    p_verbose("Using window: 0x%.8lx\n", win);
    switch (mode) {
        case 'a':
            return activate_window(disp, win, TRUE);

        case 'c':
            return close_window(disp, win);

        case 'e':
            /* resize/move the window around the desktop => -r -e */
            return window_move_resize(disp, win, options.param);

        case 'b':
            /* change state of a window => -r -b */
            return window_state(disp, win, options.param);

        case 't':
            /* move the window to the specified desktop => -r -t */
            return window_to_desktop(disp, win, atoi(options.param));

        case 'R':
            /* move the window to the current desktop and activate it => -r */
            if (window_to_desktop(disp, win, -1) == EXIT_SUCCESS) {
                usleep(100000); /* 100 ms - make sure the WM has enough
                    time to move the window, before we activate it */
                return activate_window(disp, win, FALSE);
            }
            else {
                return EXIT_FAILURE;
            }

        case 'N': case 'I': case 'T':
            window_set_title(disp, win, QLatin1String(options.param), mode);
            return EXIT_SUCCESS;

        default:
            fprintf(stderr, "Unknown action: '%c'\n", mode);
            return EXIT_FAILURE;
    }
}/*}}}*/

static int action_window_pid (Display *disp, char mode) {/*{{{*/
    unsigned long wid;

    if (sscanf(options.param_window, "0x%lx", &wid) != 1 &&
            sscanf(options.param_window, "0X%lx", &wid) != 1 &&
            sscanf(options.param_window, "%lu", &wid) != 1) {
        fputs("Cannot convert argument to number.\n", stderr);
        return EXIT_FAILURE;
    }

    return action_window(disp, (Window)wid, mode);
}/*}}}*/

static int action_window_str (Display *disp, char mode) {/*{{{*/
    Window activate = 0;
    Window *client_list;
    unsigned long client_list_size;
    int i;

    if (strcmp(ACTIVE_WINDOW_MAGIC, options.param_window) == 0) {
        activate = get_active_window(disp);
        if (activate)
        {
            return action_window(disp, activate, mode);
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else {
        if ((client_list = get_client_list(disp, &client_list_size)) == NULL) {
            return EXIT_FAILURE;
        }

        for (i = 0; i < client_list_size / sizeof(Window); i++) {
            gchar *match_utf8;
            if (options.show_class) {
                match_utf8 = get_window_class(disp, client_list[i]); /* UTF8 */
            }
            else {
                match_utf8 = get_window_title(disp, client_list[i]); /* UTF8 */
            }
            if (match_utf8) {
                gchar *match;
                gchar *match_cf;
                gchar *match_utf8_cf = NULL;
                if (envir_utf8) {
                    match = g_strdup(options.param_window);
                    match_cf = g_utf8_casefold(options.param_window, -1);
                }
                else {
                    if (! (match = g_locale_to_utf8(options.param_window, -1, NULL, NULL, NULL))) {
                        match = g_strdup(options.param_window);
                    }
                    match_cf = g_utf8_casefold(match, -1);
                }

                if (!match || !match_cf) {
                    continue;
                }

                match_utf8_cf = g_utf8_casefold(match_utf8, -1);

                if ((options.full_window_title_match && strcmp(match_utf8, match) == 0) ||
                        (!options.full_window_title_match && strstr(match_utf8_cf, match_cf))) {
                    activate = client_list[i];
                    g_free(match);
                    g_free(match_cf);
                    g_free(match_utf8);
                    g_free(match_utf8_cf);
                    break;
                }
                g_free(match);
                g_free(match_cf);
                g_free(match_utf8);
                g_free(match_utf8_cf);
            }
        }
        g_free(client_list);

        if (activate) {
            return action_window(disp, activate, mode);
        }
        else {
            return EXIT_FAILURE;
        }
    }
}/*}}}*/

static Window *get_client_list (Display *disp, unsigned long *size) {/*{{{*/
    Window *client_list;

    if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
                    XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
        if ((client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
                        XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
            fputs("Cannot get client list properties. \n"
                  "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
                  "\n", stderr);
            return NULL;
        }
    }

    return client_list;
}/*}}}*/

static int list_windows (Display *disp) {/*{{{*/
    Window *client_list;
    unsigned long client_list_size;
    int i;
    int max_client_machine_len = 0;

    if ((client_list = get_client_list(disp, &client_list_size)) == NULL) {
        return EXIT_FAILURE;
    }

    /* find the longest client_machine name */
    for (i = 0; i < client_list_size / sizeof(Window); i++) {
        gchar *client_machine;
        if ((client_machine = get_property(disp, client_list[i],
                XA_STRING, "WM_CLIENT_MACHINE", NULL))) {
            max_client_machine_len = strlen(client_machine);
        }
        g_free(client_machine);
    }

    /* print the list */
    for (i = 0; i < client_list_size / sizeof(Window); i++) {
        gchar *title_utf8 = get_window_title(disp, client_list[i]); /* UTF8 */
        gchar *title_out = get_output_str(title_utf8, TRUE);
        gchar *client_machine;
        gchar *class_out = get_window_class(disp, client_list[i]); /* UTF8 */
        unsigned long *pid;
        unsigned long *desktop;
        int x, y, junkx, junky;
        unsigned int wwidth, wheight, bw, depth;
        Window junkroot;

        /* desktop ID */
        if ((desktop = (unsigned long *)get_property(disp, client_list[i],
                XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
            desktop = (unsigned long *)get_property(disp, client_list[i],
                    XA_CARDINAL, "_WIN_WORKSPACE", NULL);
        }

        /* client machine */
        client_machine = get_property(disp, client_list[i],
                XA_STRING, "WM_CLIENT_MACHINE", NULL);

        /* pid */
        pid = (unsigned long *)get_property(disp, client_list[i],
                XA_CARDINAL, "_NET_WM_PID", NULL);

        /* geometry */
        XGetGeometry (disp, client_list[i], &junkroot, &junkx, &junky,
                          &wwidth, &wheight, &bw, &depth);
        XTranslateCoordinates (disp, client_list[i], junkroot, junkx, junky,
                               &x, &y, &junkroot);

        /* special desktop ID -1 means "all desktops", so we
           have to convert the desktop value to signed long */
        printf("0x%.8lx %2ld", client_list[i],
                desktop ? (signed long)*desktop : 0);
        if (options.show_pid) {
           printf(" %-6lu", pid ? *pid : 0);
        }
        if (options.show_geometry) {
           printf(" %-4d %-4d %-4d %-4d", x, y, wwidth, wheight);
        }
        if (options.show_class) {
           printf(" %-20s ", class_out ? class_out : "N/A");
        }

        printf(" %*s %s\n",
              max_client_machine_len,
              client_machine ? client_machine : "N/A",
              title_out ? title_out : "N/A"
        );
        g_free(title_utf8);
        g_free(title_out);
        g_free(desktop);
        g_free(client_machine);
        g_free(class_out);
        g_free(pid);
    }
    g_free(client_list);

    return EXIT_SUCCESS;
}/*}}}*/

static gchar *get_window_class (Display *disp, Window win) {/*{{{*/
    gchar *class_utf8;
    gchar *wm_class;
    unsigned long size;

    wm_class = get_property(disp, win, XA_STRING, "WM_CLASS", &size);
    if (wm_class) {
        gchar *p_0 = strchr(wm_class, '\0');
        if (wm_class + size - 1 > p_0) {
            *(p_0) = '.';
        }
        class_utf8 = g_locale_to_utf8(wm_class, -1, NULL, NULL, NULL);
    }
    else {
        class_utf8 = NULL;
    }

    g_free(wm_class);

    return class_utf8;
}/*}}}*/

static gchar *get_window_title (Display *disp, Window win) {/*{{{*/
    gchar *title_utf8;
    gchar *wm_name;
    gchar *net_wm_name;

    wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
    net_wm_name = get_property(disp, win,
            XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

    if (net_wm_name) {
        title_utf8 = g_strdup(net_wm_name);
    }
    else {
        if (wm_name) {
            title_utf8 = g_locale_to_utf8(wm_name, -1, NULL, NULL, NULL);
        }
        else {
            title_utf8 = NULL;
        }
    }

    g_free(wm_name);
    g_free(net_wm_name);

    return title_utf8;
}/*}}}*/

static gchar *get_property (Display *disp, Window win, /*{{{*/
        Atom xa_prop_type, gchar *prop_name, unsigned long *size) {
    Atom xa_prop_name;
    Atom xa_ret_type;
    int ret_format;
    unsigned long ret_nitems;
    unsigned long ret_bytes_after;
    unsigned long tmp_size;
    unsigned char *ret_prop;
    gchar *ret;

    xa_prop_name = XInternAtom(disp, prop_name, False);

    /* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
     *
     * long_length = Specifies the length in 32-bit multiples of the
     *               data to be retrieved.
     */
    if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
            xa_prop_type, &xa_ret_type, &ret_format,
            &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        p_verbose("Cannot get %s property.\n", prop_name);
        return NULL;
    }

    if (xa_ret_type != xa_prop_type) {
        p_verbose("Invalid type of %s property.\n", prop_name);
        XFree(ret_prop);
        return NULL;
    }

    /* null terminate the result to make string handling easier */
    tmp_size = (ret_format / 8) * ret_nitems;
    ret = (gchar*)g_malloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) {
        *size = tmp_size;
    }

    XFree(ret_prop);
    return ret;
}/*}}}*/

static Window get_active_window(Display *disp) {/*{{{*/
    char *prop;
    unsigned long size;
    Window ret = (Window)0;

    prop = get_property(disp, DefaultRootWindow(disp), XA_WINDOW,
                        "_NET_ACTIVE_WINDOW", &size);
    if (prop) {
        ret = *((Window*)prop);
        g_free(prop);
    }

    return(ret);
}/*}}}*/
#else
int execute_wmctrl(QString cmd){
    return 1;
}

#endif
