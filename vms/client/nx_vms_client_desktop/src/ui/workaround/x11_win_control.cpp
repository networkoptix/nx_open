#include "x11_win_control.h"
#include "qglobal.h"

#ifdef Q_WS_X11

#include <QtCore/QString>
#include <QX11Info>
#include <QtCore/QList>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */


/**
 * !!! Requires XFree() on returned result !!!
 */
static void* get_property (Window win, Atom type, QByteArray prop_name, unsigned long int *size = NULL) {
    Atom xa_prop_name = XInternAtom(QX11Info::display(), prop_name, False);
    Atom type_ret;
    int format_ret;
    unsigned long bytes_ret;
    unsigned long after_ret;
    unsigned char *prop_data = 0;

    int res = XGetWindowProperty(QX11Info::display(), win, xa_prop_name, 0, 0x7fffffff, False, type,
                                 &type_ret, &format_ret, &bytes_ret, &after_ret, &prop_data);
    if (res != 0)
        return NULL;

    if (type_ret != type) {
        XFree(prop_data);
        return NULL;
    }

    if(size)
        *size = bytes_ret;

    return prop_data;
}

static QList<Window> get_client_list () {
    unsigned long size;
    Window *client_list = reinterpret_cast<Window *>(get_property(DefaultRootWindow(QX11Info::display()), XA_WINDOW, "_NET_CLIENT_LIST", &size));
    if (!client_list)
        client_list = reinterpret_cast<Window *>(get_property(DefaultRootWindow(QX11Info::display()), XA_CARDINAL, "_WIN_CLIENT_LIST", &size));

    QList<Window> result;
    if (client_list){
        for (unsigned int i = 0; i < size; i++)
            result.append(client_list[i]);
        XFree(client_list);
    }
    return result;
}

static int action_event(Window win, unsigned long action) {
    Atom prop = XInternAtom(QX11Info::display(), "_NET_WM_STATE_BELOW", False);

    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(QX11Info::display(), "_NET_WM_STATE", False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = (unsigned long)action;
    event.xclient.data.l[1] = (unsigned long)prop;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    long mask = SubstructureRedirectMask | SubstructureNotifyMask;
    return XSendEvent(QX11Info::display(), DefaultRootWindow(QX11Info::display()), False, mask, &event);
}

static QString get_window_title (Window win) {
    QString title;

    char *net_wm_name = reinterpret_cast<char *> (get_property(win, XInternAtom(QX11Info::display(), "UTF8_STRING", False), "_NET_WM_NAME"));
    if (net_wm_name) {
        title = QString::fromUtf8(net_wm_name);
        XFree(net_wm_name);
        return title;
    }

    char *wm_name = reinterpret_cast<char *>(get_property(win, XA_STRING, "WM_NAME"));
    if (wm_name) {
        title = QLatin1String(wm_name);
        XFree(wm_name);
    }


    return title;
}

int setLauncherState(int action) {
    QList<Window> client_list = get_client_list();
    foreach(Window win, client_list){
        if (get_window_title(win).compare(QLatin1String("launcher")) == 0){
            return action_event(win, action);
        }
    }
    return 1;
}

int x11_showLauncher() {
    return setLauncherState(_NET_WM_STATE_REMOVE);
}

int x11_hideLauncher() {
    return setLauncherState(_NET_WM_STATE_ADD);
}

#else // Q_WS_X11

int x11_showLauncher() {
    return 1;
}

int x11_hideLauncher() {
    return 1;
}

#endif // Q_WS_X11
