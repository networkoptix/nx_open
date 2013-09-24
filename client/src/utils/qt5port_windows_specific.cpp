#include "qt5port_windows_specific.h"

#ifdef Q_OS_WIN

#include <QtGui/QGuiApplication>
#include <QtGui/5.1.1/QtGui/qpa/qplatformnativeinterface.h>

QWindow *windowForWidget(const QWidget *widget) {
    if (QWindow *window = widget->windowHandle())
        return window;
    if (const QWidget *nativeParent = widget->nativeParentWidget())
        return nativeParent->windowHandle();
    return NULL;
}

HWND hwndForWidget(const QWidget *widget) {
    if (QWindow *window = windowForWidget(widget))
        if (window->handle())
            return static_cast<HWND>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(QByteArrayLiteral("handle"), window));
    return NULL;
}
#endif

HWND widToHwnd(WId id) {
    return reinterpret_cast<HWND>(id);
}

WId hwndToWid(HWND id) {
    return reinterpret_cast<WId>(id);
}



